# fts5-cjk

An SQLite FTS5 tokenizer for Chinese, Japanese and Korean. One C file, no dictionary, no dependencies.

[繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

## The problem

FTS5 ships three tokenizers and none of them work for CJK text:

- `unicode61` (the default) has no idea where CJK words end, so a whole sentence becomes one token. `MATCH '魚池'` finds nothing inside `南投縣魚池鄉`.
- `trigram` needs three characters per token. Most Chinese words are two characters, so everyday queries return nothing.
- `porter` is for English.

## What `cjk` does

CJK runs are indexed as overlapping character bigrams, the same idea as Lucene's `CJKAnalyzer` and Elasticsearch's `cjk` analyzer. Text in any other script is passed to `unicode61` untouched, so Latin words keep case folding, diacritic handling and all of unicode61's options. The first and last character of each CJK run are also indexed on their own, so a character next to Latin text or digits, as in `5月` or `第38條`, can be found.

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

## Search-box input: `cjk_query()`

Text typed into a search box is not safe to pass to `MATCH`: `C++` is a syntax error, `勞工-退休` reads as a column filter, and a lone `"` is an unterminated string. The extension also registers `cjk_query()`, which turns that text into a `MATCH` string:

```sql
SELECT cjk_query('C++ 勞工-退休');   -- "C++" "勞工-退休"
SELECT cjk_query('茶');              -- "茶"*
SELECT * FROM t WHERE t MATCH cjk_query('南投魚池', 't');
                                    -- found: rewritten to "南投" "魚池"
```

- `cjk_query(text)` splits on ASCII whitespace and the ideographic space U+3000 (other Unicode spaces, such as U+00A0, stay inside a word), double-quotes each word and appends `*` to a word that is a single CJK character. Input with no words becomes `""`, which matches nothing.
- `cjk_query(text, 'table')` also consults the table. A CJK word that has no hits as one phrase is split where the pieces have the most hits: first into two pieces, then, for words of up to 12 characters, into three. If no split has hits, the word is kept. Every probe is a query, so only words of up to 32 characters are split, and one call runs at most 256 probes; a word that does not fit the remaining budget is kept whole. The split depends on what the table contains, so the same input can be rewritten differently in another database.
- The two-argument form reads the table it is given, so SQLite refuses it inside triggers and views. The name is quoted as one identifier: pass a table name, not `schema.table`.

## Build

```sh
make        # cjk.dylib on macOS, cjk.so elsewhere
make test   # runs test.sql through sqlite3 and diffs against test.expected
```

Building needs only a C compiler: the SQLite headers are in `vendor/sqlite/`. Loading and testing need a `sqlite3` with FTS5 and extension loading. On macOS use Homebrew's (`brew install sqlite`); the system `sqlite3` refuses to load extensions.

`make asan-test` builds the extension with AddressSanitizer and UBSan and feeds it random bytes, including invalid UTF-8. It links `libsqlite3` (Homebrew's on macOS, `libsqlite3-dev` on Debian and Ubuntu).

### Static linking

When SQLite is compiled into the program instead of loading extensions at run time, compile `cjk.c` with `-DSQLITE_CORE` and register it before opening a connection:

```c
int sqlite3_cjk_init(sqlite3*, char**, const sqlite3_api_routines*);
sqlite3_auto_extension((void(*)(void))sqlite3_cjk_init);
```

```sh
cc -DSQLITE_CORE -DSQLITE_ENABLE_FTS5 app.c cjk.c sqlite3.c -lm
```

The amalgamation (`sqlite3.c`) needs `-DSQLITE_ENABLE_FTS5`, or loading fails with `cjk: fts5 not available`. `make static-test` builds `test/static.c` this way against `libsqlite3`.

## Results

### Chinese

Same 47,281 articles from Taiwan's national law database, three tables that differ only in `tokenize=`. Numbers are how many articles the query returns. The `cjk` column is identical to a full-table `LIKE '%勞工%'` scan, i.e. every article that actually contains the word; the other two columns show how many of those the built-in tokenizers find.

| query | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工 | 11 | 0 | 711 |
| 退休 | 75 | 0 | 844 |
| 公司 | 44 | 0 | 1809 |
| 保險 | 41 | 0 | 1579 |
| 臺灣 | 0 | 0 | 501 |
| 特別休假 | 0 | 7 | 7 |
| 有期徒刑 | 8 | 1316 | 1316 |
| 勞工 退休 | 0 | 0 | 119 |

`unicode61` only hits when punctuation happens to isolate the word. `trigram` is fine for four-character terms and blind to two-character ones. The 臺灣 row also counts 台灣, which `cjk` folds to the same form. Reproduce with `bench/law.sh`: it downloads the data, rebuilds the tables and fails if any `cjk` count differs from the `LIKE` scan.

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
- A single character without `*` matches only where it stands alone or at the start or end of a CJK run, such as 月 in `5月`. Inside a run it exists only as part of a bigram, so use `MATCH '茶*'` to find every occurrence; `cjk_query` adds the `*` for you. Or create the table with `tokenize='cjk unigram 1'` to index every character (26.3 MB instead of 18.3 MB on 47k law articles).
- After the folding listed below (variants such as 臺→台, full-width and half-width forms, NFD composition), the `cjk` counts equal a `LIKE` scan for CJK words of two or more characters and for single characters written as `茶*`. Latin text and digits still match as whole `unicode61` words, so `5月` does not find `15月`.
- `+` joins strings that are tokenized separately, so `iphone + 手機` does not find `iPhone手機`: the index holds 手 between them. Write `iphone手機` as one bareword.
- Keep the default `detail='full'`. With `detail='column'` or `detail='none'`, FTS5 rejects phrase queries, and here every query of three or more CJK characters, or of CJK next to Latin text or digits, is a phrase: it fails with `phrase queries are not supported`.
- Full-width ASCII and half-width katakana (including voiced marks) are folded. NFD text, kana with combining voiced marks and Hangul as conjoining jamo, is composed, so it matches NFC queries. The variants 臺→台, 裡→裏, 羣→群, 峯→峰, 綫→線, 衞→衛, 爲→為, 眞→真, 敎→教, 册→冊, 祕→秘, 啓→啟, 淸→清, 靑→青 are folded at index and query time; other traditional/simplified mappings are not.
- Rebuild existing indexes after upgrading, since what gets indexed changes between versions: `INSERT INTO t(t) VALUES('rebuild')`.
- Han, hiragana, katakana, hangul and the marks 々〆〇 count as CJK. Other CJK punctuation is a separator.
- Index size on the 47,281 law articles: `unicode61` 11.7 MB, `cjk` 18.3 MB, `cjk unigram 1` 26.3 MB, `trigram` 27.7 MB. `cjk` is about a third smaller than `trigram`.

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
| `cjk` (this) | character bigrams, unicode61 for the rest | none, 432 lines of C |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | per character, optional jieba, pinyin | C++, jieba dictionary |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | trigram with a fallback for short words | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU data |
| built-in `trigram` | character trigrams | none |

## Why bigrams instead of a dictionary

A dictionary tokenizer is only as good as its dictionary, and Traditional Chinese dictionaries are thin. Bigrams never miss a word they have not seen. What they cannot do is split a no-space query such as 南投魚池 into two words. FTS5 decides phrase structure before it calls the tokenizer, so that split has to happen above FTS5. `cjk_query(text, table)` does it there, using the index as its dictionary.

## License

MIT
