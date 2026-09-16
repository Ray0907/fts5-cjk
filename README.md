# fts5-cjk

An SQLite FTS5 tokenizer for Chinese, Japanese and Korean. One C file, no dictionary, no dependencies.

[繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

## The problem

FTS5 ships three tokenizers and none of them work for CJK text:

- `unicode61` (the default) has no idea where CJK words end, so a whole sentence becomes one token. `MATCH '魚池'` finds nothing inside `南投縣魚池鄉`.
- `trigram` needs three characters per token. Most Chinese words are two characters, so everyday queries return nothing.
- `porter` is for English.

## What `cjk` does

CJK runs are indexed as overlapping character bigrams, the same idea as Lucene's `CJKAnalyzer` and Elasticsearch's `cjk` analyzer. Text in any other script is passed to `unicode61` untouched, so Latin words keep case folding, diacritic handling and all of unicode61's options.

```sql
.load ./cjk
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
INSERT INTO t VALUES ('日月潭紅茶產於南投縣魚池鄉');

SELECT * FROM t WHERE t MATCH '魚池';       -- found: substring match
SELECT * FROM t WHERE t MATCH '南投 紅茶';   -- found: two words, ANDed
SELECT * FROM t WHERE t MATCH '南投魚池';    -- not found: one bareword is one phrase,
                                            -- so 南投 and 魚池 would have to be adjacent
SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH '魚池鄉';
-- 日月潭紅茶產於南投縣[魚池鄉]
```

Arguments after the name go to `unicode61`:

```sql
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk remove_diacritics 2');
```

## Build

```sh
make        # cjk.dylib on macOS, cjk.so elsewhere
make test   # runs test.sql through sqlite3 and diffs against test.expected
```

You need `sqlite3ext.h` from an SQLite built with FTS5. On macOS use Homebrew's (`brew install sqlite`); the system `sqlite3` refuses to load extensions. On Debian and Ubuntu install `libsqlite3-dev`. Point `SQLITE_INC` at the headers if they are somewhere else.

## Results

### Chinese

Same 47,257 articles from Taiwan's national law database, three tables that differ only in `tokenize=`. Numbers are how many articles the query returns. The `cjk` column is identical to a full-table `LIKE '%勞工%'` scan, i.e. every article that actually contains the word; the other two columns show how many of those the built-in tokenizers find.

| query | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工 | 10 | 0 | 958 |
| 退休 | 75 | 0 | 1057 |
| 公司 | 44 | 0 | 2050 |
| 保險 | 41 | 0 | 1789 |
| 臺灣 | 0 | 0 | 849 |
| 特別休假 | 0 | 7 | 7 |
| 有期徒刑 | 8 | 1316 | 1316 |
| 勞工 退休 | 0 | 0 | 157 |

`unicode61` only hits when punctuation happens to isolate the word. `trigram` is fine for four-character terms and blind to two-character ones. Building all three tables took 3.6 s.

### Japanese

4,011 paragraphs from 40 Japanese Wikipedia articles.

| query | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 会社 | 1 | 0 | 243 |
| 保険 | 0 | 0 | 217 |
| 労働 | 0 | 0 | 161 |
| 東京 | 16 | 0 | 560 |
| 契約 | 0 | 0 | 40 |
| 新幹線 | 367 | 379 | 379 |
| 労働基準法 | 79 | 80 | 80 |
| 東京 大学 | 0 | 0 | 262 |

Japanese has no spaces, so `unicode61` sees whole clauses as one token and two-character words vanish. Three-character words and longer are where `trigram` catches up.

### Korean

2,095 paragraphs from 40 Korean Wikipedia articles.

| query | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 회사 | 4 | 0 | 24 |
| 보험 | 12 | 0 | 41 |
| 서울 | 64 | 0 | 338 |
| 학교 | 19 | 0 | 194 |
| 결혼 | 13 | 0 | 32 |
| 대통령 | 22 | 62 | 62 |
| 인천공항 | 4 | 17 | 17 |
| 서울 대학 | 1 | 0 | 108 |

Korean is written with spaces, so `unicode61` does find the bare word. It misses every form with a particle attached (서울은, 서울의, 서울에서), which is most of them. Two-syllable words are again invisible to `trigram`.

## Things to know

- One bareword is one phrase. `MATCH '魚池鄉'` means the substring 魚池鄉. To AND two words, separate them with a space.
- A single-character query only matches documents where that character stands alone. Use a prefix query: `MATCH '茶*'`.
- Han, hiragana, katakana and hangul count as CJK. CJK punctuation is a separator.
- Index size is in the same range as `trigram`.

## Languages

| script | status |
|---|---|
| Chinese, Traditional and Simplified | tested |
| Japanese | tested; kanji and kana mixed, no dictionary |
| Korean | tested; Korean already has spaces, bigrams mainly help with particles attached to nouns |
| Latin, Cyrillic, Greek, Arabic, Hebrew, Vietnamese, ... | handled by `unicode61` as before |
| Thai, Lao, Khmer, Burmese, Tibetan | not handled: also written without spaces, but vowels and tone marks are separate code points, so bigrams would have to run over grapheme clusters |

## Alternatives

| | approach | dependencies |
|---|---|---|
| `cjk` (this) | character bigrams, unicode61 for the rest | none, 155 lines of C |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | per character, optional jieba, pinyin | C++, jieba dictionary |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | trigram with a fallback for short words | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU data |
| built-in `trigram` | character trigrams | none |

## Why bigrams instead of a dictionary

A dictionary tokenizer is only as good as its dictionary, and Traditional Chinese dictionaries are thin. Bigrams never miss a word they have not seen. What they cannot do is split a no-space query such as 南投魚池 into two words. FTS5 decides phrase structure before it calls the tokenizer, so that split has to happen above FTS5, for example in an SQL function that rewrites the query. That is the planned next step.

## License

MIT
