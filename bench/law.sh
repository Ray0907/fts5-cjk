#!/bin/sh
# Reproduces the Chinese results table in the README and checks cjk against a
# LIKE scan on Taiwan's national law database (law.moj.gov.tw open data).
# Exits non-zero if any cjk count differs from the LIKE ground truth.
#
#   make && bench/law.sh
set -e
cd "$(dirname "$0")"
SQLITE3=${SQLITE3:-$(ls /opt/homebrew/opt/sqlite/bin/sqlite3 2>/dev/null || command -v sqlite3)}
ext=$(ls ../cjk.dylib ../cjk.so 2>/dev/null | head -1)
[ -n "$ext" ] || { echo "run make first"; exit 1; }
mkdir -p corpus

if [ ! -f corpus/law.db ]; then
  curl -sSL -o corpus/law.zip https://law.moj.gov.tw/api/Ch/Law/JSON
  unzip -o -q corpus/law.zip -d corpus
  python3 - <<'PY'
import json, sqlite3
laws = json.load(open('corpus/ChLaw.json', encoding='utf-8-sig'))['Laws']
db = sqlite3.connect('corpus/law.db')
db.execute('CREATE TABLE a(body TEXT)')
db.executemany('INSERT INTO a VALUES (?)',
  ((x['ArticleContent'],) for l in laws for x in l['LawArticles']
   if x['ArticleType'] == 'A' and x['ArticleContent']))
db.commit()
PY
fi

sq() { "$SQLITE3" corpus/law.db -cmd ".load ${ext%.*} sqlite3_cjk_init" "$@"; }

start=$(date +%s)
sq "DROP TABLE IF EXISTS u; DROP TABLE IF EXISTS tg; DROP TABLE IF EXISTS c; DROP TABLE IF EXISTS cu;
    CREATE VIRTUAL TABLE u  USING fts5(body, tokenize='unicode61');
    CREATE VIRTUAL TABLE tg USING fts5(body, tokenize='trigram');
    CREATE VIRTUAL TABLE c  USING fts5(body, tokenize='cjk');
    CREATE VIRTUAL TABLE cu USING fts5(body, tokenize='cjk unigram 1');
    INSERT INTO u(rowid,body)  SELECT rowid,body FROM a;
    INSERT INTO tg(rowid,body) SELECT rowid,body FROM a;
    INSERT INTO c(rowid,body)  SELECT rowid,body FROM a;
    INSERT INTO cu(rowid,body) SELECT rowid,body FROM a;"
echo "articles: $(sq 'SELECT count(*) FROM a'), four tables built in $(( $(date +%s) - start ))s"
for t in u tg c cu; do echo "index $t: $(sq "SELECT sum(length(block)) FROM ${t}_data") bytes"; done

fail=0
n() { sq "SELECT count(*) FROM $1 WHERE $1 MATCH '$2'"; }
like() { sq "SELECT count(*) FROM a WHERE $1"; }
check() { # label got want
  [ "$2" = "$3" ] || { echo "FAIL $1: cjk $2, LIKE $3"; fail=1; }
}

echo
echo "| query | \`unicode61\` | \`trigram\` | \`cjk\` |"
echo "|---|---|---|---|"
row() { # query like-expr
  c=$(n c "$1")
  echo "| $1 | $(n u "$1") | $(n tg "$1") | $c |"
  check "$1" "$c" "$(like "$2")"
}
for q in 勞工 退休 公司 保險; do row "$q" "body LIKE '%$q%'"; done
row 臺灣 "body LIKE '%臺灣%' OR body LIKE '%台灣%'"
for q in 特別休假 有期徒刑; do row "$q" "body LIKE '%$q%'"; done
row '勞工 退休' "body LIKE '%勞工%' AND body LIKE '%退休%'"
echo

# No-space query is a substring match, with and without unigram.
for q in 勞工退休 勞工保險條例; do
  w=$(like "body LIKE '%$q%'")
  check "$q" "$(n c $q)" "$w"; check "$q (unigram)" "$(n cu $q)" "$w"
done
# Variant folding works in both directions.
check "台灣" "$(n c 台灣)" "$(n c 臺灣)"
# Single characters need unigram 1.
for q in 稅 罰 茶; do
  echo "single char $q: cjk $(n c $q), cjk unigram 1 $(n cu $q)"
  check "$q (unigram)" "$(n cu $q)" "$(like "body LIKE '%$q%'")"
done

[ $fail = 0 ] && echo "PASS: every cjk count equals the LIKE scan" || { echo FAIL; exit 1; }
