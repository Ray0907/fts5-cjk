.mode list
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
INSERT INTO t(rowid, body) VALUES
  (1, '日月潭紅茶產於南投縣魚池鄉'),
  (2, '阿里山高山茶產於嘉義縣'),
  (3, 'Go 語言和 SQLite 全文檢索：Résumé'),
  (4, '東京都渋谷区のカフェ'),
  (5, '서울특별시 강남구'),
  (6, '茶');
-- tokens as fts5 sees them
CREATE VIRTUAL TABLE v USING fts5vocab(t, 'row');
SELECT '# vocab'; SELECT term FROM v ORDER BY term;
SELECT '# substring 魚池'; SELECT rowid FROM t WHERE t MATCH '魚池';
SELECT '# adjacency: 南投魚池 must not match 南投縣魚池'; SELECT count(*) FROM t WHERE t MATCH '南投魚池';
SELECT '# two words ANDed'; SELECT rowid FROM t WHERE t MATCH '南投 魚池' ORDER BY rowid;
SELECT '# mixed latin + cjk, case folded'; SELECT rowid FROM t WHERE t MATCH 'sqlite 檢索';
SELECT '# diacritics folded by unicode61'; SELECT rowid FROM t WHERE t MATCH 'resume';
SELECT '# japanese'; SELECT rowid FROM t WHERE t MATCH '渋谷';
SELECT '# korean'; SELECT rowid FROM t WHERE t MATCH '강남';
SELECT '# lone char'; SELECT rowid FROM t WHERE t MATCH '茶' ORDER BY rowid;
SELECT '# prefix'; SELECT rowid FROM t WHERE t MATCH '魚池*';
SELECT '# highlight spans whole phrase, offsets correct'; SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH '魚池鄉';
SELECT '# highlight mixed'; SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH 'go 語言';
SELECT '# snippet'; SELECT snippet(t, 0, '[', ']', '…', 6) FROM t WHERE t MATCH '產於' ORDER BY rowid;
SELECT '# bm25 ranks shorter doc first'; SELECT rowid FROM t WHERE t MATCH '產於' ORDER BY bm25(t);
SELECT '# parent options pass through'; CREATE VIRTUAL TABLE t2 USING fts5(body, tokenize='cjk remove_diacritics 0');
INSERT INTO t2(body) VALUES('Résumé 履歷'); SELECT count(*) FROM t2 WHERE t2 MATCH 'resume'; SELECT count(*) FROM t2 WHERE t2 MATCH 'résumé 履歷';

-- Single-character search is opt-in; query bigrams and phrase adjacency stay intact.
CREATE VIRTUAL TABLE u USING fts5(body, tokenize='cjk unigram 1');
INSERT INTO u(rowid, body) VALUES (1, '日月潭紅茶產於南投縣魚池鄉');
SELECT '# F1 unigram 茶'; SELECT count(*) FROM u WHERE u MATCH '茶';
SELECT '# F2 phrase 魚池鄉'; SELECT count(*) FROM u WHERE u MATCH '魚池鄉';
SELECT '# F2 nonadjacent 南投魚池'; SELECT count(*) FROM u WHERE u MATCH '南投魚池';
SELECT '# F3 unigram highlight'; SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH '茶';
SELECT '# F4 unigram prefix'; SELECT count(*) FROM u WHERE u MATCH '茶*';
SELECT '# F5 default 茶'; SELECT count(*) FROM t WHERE rowid=1 AND t MATCH '茶';
SELECT '# F6 臺→台'; INSERT INTO u(rowid, body) VALUES (2, '臺灣大學'), (3, '台灣大學');
SELECT rowid FROM u WHERE u MATCH '台灣' AND rowid=2;
SELECT rowid FROM u WHERE u MATCH '臺灣' AND rowid=3;
SELECT '# F7 half-width kana'; INSERT INTO u(rowid, body) VALUES (4, 'ｶﾀｶﾅ半角');
SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH 'カタカナ';
SELECT '# F8 dakuten and handakuten'; INSERT INTO u(rowid, body) VALUES (5, 'ｶﾞｲﾄﾞ'), (6, 'ﾊﾟﾝ');
SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH 'ガイド';
SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH 'パン';
SELECT '# F9 full-width ASCII'; INSERT INTO u(rowid, body) VALUES (7, 'ＳＱＬｉｔｅ１２３');
SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH 'sqlite123';
SELECT '# F10 Ext H and F11 Compatibility Supplement';
INSERT INTO u(rowid, body) VALUES (8, char(0x31350)||'字'), (9, char(0x2F800)||'字');
CREATE VIRTUAL TABLE uv USING fts5vocab(u, 'row');
SELECT count(*) FROM uv WHERE term=char(0x31350)||'字';
SELECT count(*) FROM uv WHERE term=char(0x2F800)||'字';
SELECT '# F13 unicode61 options after unigram';
CREATE VIRTUAL TABLE u0 USING fts5(body, tokenize='cjk unigram 1 remove_diacritics 0');
INSERT INTO u0(body) VALUES ('Résumé 履歷');
SELECT count(*) FROM u0 WHERE u0 MATCH 'resume';
SELECT count(*) FROM u0 WHERE u0 MATCH 'résumé 履歷';
SELECT '# F14 著 must not fold to 着';
INSERT INTO u(rowid, body) VALUES (10, '到着'), (11, '著者');
SELECT rowid FROM u WHERE u MATCH '著者';
SELECT count(*) FROM u WHERE rowid=10 AND u MATCH '著';
SELECT '# F15 々 〆 〇 are CJK';
CREATE VIRTUAL TABLE j USING fts5(body, tokenize='cjk');
INSERT INTO j(rowid, body) VALUES (1, '多くの人々が集まった'), (2, '佐々木さん'), (3, '〆切は金曜'), (4, '西暦二〇二四年');
SELECT rowid FROM j WHERE j MATCH '人々';
SELECT rowid FROM j WHERE j MATCH '佐々木';
SELECT rowid FROM j WHERE j MATCH '〆切';
SELECT rowid FROM j WHERE j MATCH '二〇二四';
SELECT '# B1 single characters at CJK run boundaries';
CREATE VIRTUAL TABLE b USING fts5(body, tokenize='cjk');
INSERT INTO b(rowid, body) VALUES (1, '依第38條規定辦理'), (2, '我們在5月舉行'), (3, '使用iPhone手機很方便'),
  (4, '血型A型的人'), (5, '阿里山高山茶'), (6, '5AB'), (7, '南投縣，魚池鄉');
SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH '第38條';
SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH '5月';
SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH '用iphone';
SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH 'a型';
SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH '茶*';
SELECT '# B2 5b must not match 5AB'; SELECT count(*) FROM b WHERE b MATCH '5b';
SELECT '# B3 two runs in one query string'; SELECT highlight(b, 0, '[', ']') FROM b WHERE b MATCH '南投縣，魚池鄉';
SELECT '# B4 unigram 1 mixed query'; INSERT INTO u(rowid, body) VALUES (12, '新款iPhone手機');
SELECT highlight(u, 0, '[', ']') FROM u WHERE u MATCH 'iphone手機';
SELECT '# N1 NFD kana and Hangul jamo compose';
CREATE VIRTUAL TABLE n USING fts5(body, tokenize='cjk');
INSERT INTO n(rowid, body) VALUES (1, char(0x304B,0x3099,0x3063,0x3053,0x3046)),
  (2, char(0x1112,0x1161,0x11AB,0x1100,0x1173,0x11AF)), (3, char(0x306F,0x309A,0x3093));
SELECT rowid, highlight(n, 0, '[', ']') FROM n WHERE n MATCH 'がっこう';
SELECT rowid, highlight(n, 0, '[', ']') FROM n WHERE n MATCH '한글';
SELECT rowid, highlight(n, 0, '[', ']') FROM n WHERE n MATCH 'ぱん';
SELECT '# Q1 cjk_query quotes every word';
SELECT cjk_query('C++'), cjk_query('勞工-退休'), cjk_query('"'), cjk_query(''), cjk_query(' 茶 '), cjk_query('南投　魚池');
SELECT count(*) FROM t WHERE t MATCH cjk_query('C++');
SELECT count(*) FROM t WHERE t MATCH cjk_query('勞工-退休');
SELECT count(*) FROM t WHERE t MATCH cjk_query('"');
SELECT count(*) FROM t WHERE t MATCH cjk_query('');
SELECT rowid FROM t WHERE t MATCH cjk_query('茶') ORDER BY rowid;
SELECT '# Q2 cjk_query with a table splits words that have no hits';
SELECT cjk_query('南投魚池', 't'), cjk_query('日月潭南投魚池', 't'), cjk_query('sqlite 火星文字 茶', 't');
SELECT rowid FROM t WHERE t MATCH cjk_query('南投魚池', 't');
SELECT '# Q4 splitting is bounded: words over 32 characters, and past 256 probes per call';
CREATE VIRTUAL TABLE w USING fts5(body, tokenize='cjk');
INSERT INTO w(body) SELECT replace(hex(zeroblob(15)), '0', '茶') || '茶 南投';
SELECT cjk_query(x || '南投', 'w') = '"' || x || '" "南投"', cjk_query(x || '茶南投', 'w') = '"' || x || '茶南投"'
  FROM (SELECT replace(hex(zeroblob(15)), '0', '茶') AS x);
SELECT cjk_query(x, 't') = '"' || x || '"' FROM (SELECT replace(hex(zeroblob(20)), '0', '火') AS x);
SELECT cjk_query('日月潭紅茶產南投縣魚池鄉', 't');
SELECT cjk_query(x || ' ' || x || ' ' || x || ' 日月潭紅茶產南投縣魚池鄉', 't') LIKE '%" "日月潭紅茶產南投縣魚池鄉"'
  FROM (SELECT replace(hex(zeroblob(6)), '0', '火') AS x);
SELECT '# Q5 cjk_query composes NFD kana and Hangul jamo';
SELECT cjk_query(char(0x1112, 0x1161, 0x11AB)), cjk_query(char(0x304B, 0x3099)), cjk_query('ｶﾞ');
SELECT rowid FROM t WHERE t MATCH cjk_query(char(0x1110, 0x1173, 0x11A8));
INSERT INTO n(rowid, body) VALUES (4, 'ひらがな');
SELECT rowid FROM n WHERE n MATCH cjk_query(char(0x304B, 0x3099)) ORDER BY rowid;
SELECT cjk_query(char(0x1109,0x1165,0x110B,0x116E,0x11AF,0x1109,0x1175,0x1100,0x1161,0x11BC,0x1102,0x1161,0x11B7), 't');
SELECT rowid FROM t WHERE t MATCH cjk_query(char(0x1109,0x1165,0x110B,0x116E,0x11AF,0x1109,0x1175,0x1100,0x1161,0x11BC,0x1102,0x1161,0x11B7), 't');
SELECT '# R1 cjk_rank_query: bigrams plus each run''s first character, or both edges';
SELECT cjk_rank_query('東京大学の研究'); SELECT cjk_rank_query('東京大学の研究', 'edge');
SELECT cjk_rank_query('東京大学の研究', 'first') = cjk_rank_query('東京大学の研究');
SELECT '# R2 empty, NULL and whitespace-only input';
SELECT quote(cjk_rank_query(NULL)), quote(cjk_rank_query(NULL, 'edge')), cjk_rank_query(''), cjk_rank_query(' 　' || char(9, 10, 13));
SELECT count(*) FROM t WHERE t MATCH cjk_rank_query('');
SELECT '# R3 runs of one and two characters';
SELECT cjk_rank_query('茶'), cjk_rank_query('茶', 'edge'), cjk_rank_query('東京'), cjk_rank_query('東京', 'edge');
SELECT cjk_rank_query('紅茶 茶'), cjk_rank_query('東京 京都'), cjk_rank_query('東東', 'edge');
SELECT '# R4 mixed Latin and CJK';
SELECT cjk_rank_query('iPhone手機殼'), cjk_rank_query('手機殼iPhone', 'edge'), cjk_rank_query('第38條規定');
SELECT '# R5 punctuation: full-width separators split runs; ・ and ー stay inside a run, as in the index';
SELECT cjk_rank_query('南投縣，魚池鄉。'), cjk_rank_query('「東京」！'), cjk_rank_query('トム・クルーズ');
SELECT '# R6 operators and quotes in the input stay inside quoted terms';
SELECT cjk_rank_query('a" OR b'), cjk_rank_query('NEAR(東京 大学) -x +y ^z * col:東京'), cjk_rank_query('"'), cjk_rank_query('AND NOT 茶');
SELECT count(*) FROM t WHERE t MATCH cjk_rank_query('a" OR b');
SELECT count(*) FROM t WHERE t MATCH cjk_rank_query('NEAR(東京 大学) -x +y ^z * col:東京');
SELECT rowid FROM t WHERE t MATCH cjk_rank_query('AND NOT 茶') ORDER BY rowid;
SELECT '# R7 supplementary-plane Han';
SELECT cjk_rank_query(char(0x20000, 0x20001) || '字') = '"' || char(0x20000) || '" OR "' || char(0x20000, 0x20001) || '" OR "' || char(0x20001) || '字"';
SELECT rowid FROM u WHERE u MATCH cjk_rank_query(char(0x31350) || '字');
SELECT '# R8 Hangul syllables, NFD jamo and compatibility jamo';
SELECT cjk_rank_query('한국어 검색'), cjk_rank_query(char(0x1112, 0x1161, 0x11AB, 0x1100, 0x1173, 0x11AF)), cjk_rank_query('ㅋㅋㅋ');
SELECT '# R9 half-width katakana and full-width alphanumerics are folded like the index';
SELECT cjk_rank_query('ｶﾀｶﾅ'), cjk_rank_query('ｶﾞｲﾄﾞ'), cjk_rank_query('ＳＱＬｉｔｅ１２３'), cjk_rank_query('臺灣');
SELECT '# R10 々 〆 〇 are part of a run';
SELECT cjk_rank_query('佐々木'), cjk_rank_query('〆切'), cjk_rank_query('二〇二四');
SELECT rowid FROM j WHERE j MATCH cjk_rank_query('佐々木さん') ORDER BY rank;
SELECT '# R11 ranking on a default table: first adds the run-opening 日, edge also the run-final 茶';
SELECT rowid FROM t WHERE t MATCH cjk_rank_query('日月潭紅茶') ORDER BY rank;
SELECT rowid FROM t WHERE t MATCH cjk_rank_query('日月潭紅茶', 'edge') ORDER BY rank;
SELECT rowid FROM t WHERE t MATCH '"日月潭紅茶"';
SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH cjk_rank_query('魚池鄉在哪裡');
SELECT '# R12 at most 128 distinct terms, in order of first occurrence; repeats count once';
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 300)
SELECT length(q) - length(replace(q, ' OR ', '')) = 4 * 127, q LIKE '"w1" OR "w2" OR %', q LIKE '% OR "w128"'
  FROM (SELECT cjk_rank_query(group_concat('w' || i, ' ')) AS q FROM n);
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 5000)
SELECT cjk_rank_query(group_concat('茶 a', ' ') || ' 紅茶') FROM n;
SELECT length(cjk_rank_query(replace(hex(zeroblob(50000)), '0', '日月潭紅茶產於南投縣魚池鄉')));
SELECT '# R13 invalid UTF-8 never produces an invalid MATCH string';
SELECT count(*) >= 0 FROM t WHERE t MATCH cjk_rank_query(CAST(x'e88cb6e8' AS TEXT));
SELECT count(*) >= 0 FROM t WHERE t MATCH cjk_rank_query(CAST(x'ff22e88c' AS TEXT), 'edge');
SELECT count(*) >= 0 FROM t WHERE t MATCH cjk_rank_query(CAST(x'eda080f0a080e3' AS TEXT));
SELECT '# R14 unigram 1 tables: a single character term matches every occurrence';
SELECT cjk_rank_query('紅豆'), count(*) FROM t WHERE rowid = 1 AND t MATCH cjk_rank_query('紅豆');
SELECT count(*) FROM u WHERE rowid = 1 AND u MATCH cjk_rank_query('紅豆');
SELECT '# R15 deterministic and innocuous: usable in a view';
CREATE VIEW rv AS SELECT cjk_rank_query('南投魚池') AS q;
SELECT q FROM rv;
SELECT '# R17 the cap at its boundary: 128 distinct terms are kept, the 129th is not, in both modes';
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 129),
  w AS (SELECT (SELECT group_concat('w' || i, ' ') FROM n WHERE i <= 128) AS x128,
               (SELECT group_concat('w' || i, ' ') FROM n) AS x129,
               (SELECT group_concat('w' || i, ' ') FROM n WHERE i <= 126) AS x126),
  q AS (SELECT m, cjk_rank_query(x128, m) AS q128, cjk_rank_query(x129, m) AS q129,
               cjk_rank_query(x128 || ' w1 w2 w128', m) AS qrep, cjk_rank_query(x126 || ' 東京', m) AS qcjk
          FROM w, (SELECT 'first' AS m UNION ALL SELECT 'edge'))
SELECT m, (length(q128) - length(replace(q128, ' OR ', ''))) / 4 + 1, q128 LIKE '"w1" OR %' AND q128 LIKE '% OR "w128"',
       (length(q129) - length(replace(q129, ' OR ', ''))) / 4 + 1, q129 = q128, instr(q129, '"w129"') = 0, qrep = q128,
       (length(qcjk) - length(replace(qcjk, ' OR ', ''))) / 4 + 1, substr(qcjk, instr(qcjk, '"w126"') + 6)
  FROM q;
SELECT '# Q3 unknown table is an error';
.bail off
SELECT cjk_query('南投魚池', 'no_such_table');
SELECT '# R16 invalid mode is an error, not a default';
SELECT cjk_rank_query('茶', 'FIRST');
SELECT cjk_rank_query('茶', '');
SELECT cjk_rank_query('茶', NULL);
SELECT cjk_rank_query(NULL, 'edges');
SELECT '# F12 invalid unigram rejected';
CREATE VIRTUAL TABLE invalid_unigram USING fts5(body, tokenize='cjk unigram 2');
.bail on
