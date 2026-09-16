# fts5-cjk

SQLite FTS5 的中日韓 tokenizer。一個 C 檔，不用字典，沒有相依套件。

[English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

## 問題

FTS5 內建三個 tokenizer，對中文都不能用：

- `unicode61`（預設）不知道中文詞在哪裡斷，整句變成一個 token。`MATCH '魚池'` 在「南投縣魚池鄉」裡找不到。
- `trigram` 一個 token 要三個字。中文大部分的詞是兩個字，日常查詢幾乎全空。
- `porter` 是英文用的。

## `cjk` 做什麼

中日韓文字連續的一段，切成前後重疊的兩字 token（bigram），跟 Lucene 的 `CJKAnalyzer`、Elasticsearch 的 `cjk` analyzer 同一個做法。其他文字原樣交給 `unicode61`，所以英文照樣有大小寫摺疊、去變音符號，`unicode61` 的選項全部可用。

```sql
.load ./cjk
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
INSERT INTO t VALUES ('日月潭紅茶產於南投縣魚池鄉');

SELECT * FROM t WHERE t MATCH '魚池';       -- 有：子字串比對
SELECT * FROM t WHERE t MATCH '南投 紅茶';   -- 有：兩個詞，AND
SELECT * FROM t WHERE t MATCH '南投魚池';    -- 沒有：一個沒空白的查詢字串是一個 phrase，
                                            -- 南投 跟 魚池 必須相鄰
SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH '魚池鄉';
-- 日月潭紅茶產於南投縣[魚池鄉]
```

名稱後面的參數會傳給 `unicode61`：

```sql
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk remove_diacritics 2');
```

## 編譯

```sh
make        # macOS 產生 cjk.dylib，其他平台 cjk.so
make test   # 用 sqlite3 跑 test.sql，跟 test.expected 比對
```

需要有 FTS5 的 SQLite 標頭檔 `sqlite3ext.h`。macOS 用 Homebrew 的（`brew install sqlite`），系統內建的 `sqlite3` 不能載入 extension。Debian、Ubuntu 裝 `libsqlite3-dev`。標頭檔在別處就設 `SQLITE_INC`。

## 結果

### 中文

全國法規資料庫 47,257 條條文，三張表只差 `tokenize=`。數字是查詢命中的條文數。`cjk` 那欄和 `LIKE '%勞工%'` 全表掃描的結果完全相同，也就是真正含有這個詞的條文數；前兩欄是預設 tokenizer 從中找到幾條。

| 查詢 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工 | 10 | 0 | 958 |
| 退休 | 75 | 0 | 1057 |
| 公司 | 44 | 0 | 2050 |
| 保險 | 41 | 0 | 1789 |
| 臺灣 | 0 | 0 | 849 |
| 特別休假 | 0 | 7 | 7 |
| 有期徒刑 | 8 | 1316 | 1316 |
| 勞工 退休 | 0 | 0 | 157 |

`unicode61` 只有在標點剛好把詞隔開時才中。`trigram` 四個字的詞正常，兩個字的詞看不到。三張表建完 3.6 秒。

### 日文

日文維基 40 篇文章、4,011 個段落。查詢字串是日文原文，括號是繁中對照。

| 查詢 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 会社（公司） | 1 | 0 | 243 |
| 保険（保險） | 0 | 0 | 217 |
| 労働（勞動） | 0 | 0 | 161 |
| 東京 | 16 | 0 | 560 |
| 契約 | 0 | 0 | 40 |
| 新幹線 | 367 | 379 | 379 |
| 労働基準法（勞動基準法） | 79 | 80 | 80 |
| 東京 大学（東京 大學） | 0 | 0 | 262 |

日文沒有空格，`unicode61` 把整個子句當一個 token，兩字詞全部消失。三字以上的詞 `trigram` 才追得上。

### 韓文

韓文維基 40 篇文章、2,095 個段落。

| 查詢 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 회사（公司） | 4 | 0 | 24 |
| 보험（保險） | 12 | 0 | 41 |
| 서울（首爾） | 64 | 0 | 338 |
| 학교（學校） | 19 | 0 | 194 |
| 결혼（結婚） | 13 | 0 | 32 |
| 대통령（總統） | 22 | 62 | 62 |
| 인천공항（仁川機場） | 4 | 17 | 17 |
| 서울 대학 | 1 | 0 | 108 |

韓文有空格，所以 `unicode61` 找得到單獨出現的詞，但黏了助詞的形態（서울은、서울의、서울에서）全部漏掉，而那才是大多數。兩音節的詞 `trigram` 一樣看不到。

## 要知道的事

- 一個沒空白的查詢字串是一個 phrase。`MATCH '魚池鄉'` 的意思是子字串「魚池鄉」。要 AND 兩個詞，中間加空白。
- 單一個字的查詢只會中那個字單獨出現的文件。用前綴查詢：`MATCH '茶*'`。
- 漢字、平假名、片假名、諺文算 CJK。中日韓標點是分隔符。
- 索引大小跟 `trigram` 差不多。

## 支援的文字

| 文字 | 狀態 |
|---|---|
| 中文，繁體與簡體 | 測過 |
| 日文 | 測過；漢字假名混排，不用字典 |
| 韓文 | 測過；韓文本來有空格，bigram 主要處理黏在名詞後面的助詞 |
| 拉丁、西里爾、希臘、阿拉伯、希伯來、越南文等 | 照原本由 `unicode61` 處理 |
| 泰、寮、高棉、緬、藏文 | 沒處理：也是無空格文字，但母音和聲調符號是獨立 code point，bigram 要跑在 grapheme cluster 上 |

## 其他選擇

| | 做法 | 相依 |
|---|---|---|
| `cjk`（本專案） | 字元 bigram，其餘交給 unicode61 | 無，155 行 C |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | 逐字，可選 jieba、拼音 | C++、jieba 字典 |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | trigram 加短詞 fallback | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU 資料 |
| 內建 `trigram` | 字元 trigram | 無 |

## 為什麼用 bigram 不用字典

字典式 tokenizer 的品質就是字典的品質，繁體中文的字典很薄。bigram 不會漏掉沒見過的詞。它做不到的是把「南投魚池」這種沒空白的查詢切成兩個詞。FTS5 在呼叫 tokenizer 之前就決定了 phrase 結構，所以這件事要在 FTS5 上面做，例如用一個 SQL function 改寫查詢字串。這是下一步。

## 授權

MIT
