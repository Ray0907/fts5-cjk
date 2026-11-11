# fts5-cjk

SQLite FTS5 的中日韓 tokenizer。一個 C 檔，不用字典，沒有相依套件。

[English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

## 問題

FTS5 內建三個 tokenizer，對中文都不能用：

- `unicode61`（預設）不知道中文詞在哪裡斷，整句變成一個 token。`MATCH '魚池'` 在「南投縣魚池鄉」裡找不到。
- `trigram` 一個 token 要三個字。中文大部分的詞是兩個字，日常查詢幾乎全空。
- `porter` 是英文用的。

## `cjk` 做什麼

中日韓文字連續的一段，切成前後重疊的兩字 token（bigram），跟 Lucene 的 `CJKAnalyzer`、Elasticsearch 的 `cjk` analyzer 同一個做法。其他文字原樣交給 `unicode61`，所以英文照樣有大小寫摺疊、去變音符號，`unicode61` 的選項全部可用。每一段中日韓文字的第一個字和最後一個字也會單獨索引，所以緊貼英文或數字的字（例如 `5月`、`第38條`）也找得到。

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

## 搜尋框輸入：`cjk_query()`

使用者在搜尋框打的字不能直接丟進 `MATCH`：`C++` 是語法錯誤，`勞工-退休` 會被當成欄位篩選，單獨一個 `"` 是沒結束的字串。這個 extension 另外註冊了 `cjk_query()`，把這種輸入轉成 `MATCH` 字串：

```sql
SELECT cjk_query('C++ 勞工-退休');   -- "C++" "勞工-退休"
SELECT cjk_query('茶');              -- "茶"*
SELECT * FROM t WHERE t MATCH cjk_query('南投魚池', 't');
                                    -- 有：改寫成 "南投" "魚池"
```

- `cjk_query(text)` 以 ASCII 空白和全形空白 U+3000 切詞（U+00A0 等其他 Unicode 空白不算），每個詞加上雙引號，單一個中日韓字的詞再加 `*`。沒有任何詞的輸入會變成 `""`，什麼都不中。
- `cjk_query(text, 'table')` 還會查表。整個詞當 phrase 查不到的中日韓詞，會切在命中最多的位置：先試切成兩段，詞長 12 字以內再試三段。每種切法都沒命中就保留原詞。每次嘗試都是一次查詢，所以只切 32 字以內的詞，一次呼叫最多嘗試 256 次；剩餘次數不夠的詞保留原樣。切法取決於表裡的資料，同一個輸入在別的資料庫可能改寫成不同結果。
- 兩個參數的版本會讀取指定的表，所以 SQLite 不允許在 trigger 和 view 裡使用它。表名會被當成一個識別字加引號：請傳表名，不要傳 `schema.table`。

## 編譯

```sh
make        # macOS 產生 cjk.dylib，其他平台 cjk.so
make test   # 用 sqlite3 跑 test.sql，跟 test.expected 比對
```

編譯只需要 C compiler，SQLite 標頭檔已放在 `include/`。載入和測試需要支援 FTS5 與 extension 載入的 `sqlite3`。macOS 用 Homebrew 的（`brew install sqlite`），系統內建的 `sqlite3` 不能載入 extension。

`make asan-test` 用 AddressSanitizer 和 UBSan 編譯 extension，並餵入隨機位元組（含非法 UTF-8）。它會連結 `libsqlite3`（macOS 用 Homebrew 的，Debian、Ubuntu 裝 `libsqlite3-dev`）。

### 靜態連結

SQLite 直接編進程式、不在執行時載入 extension 時，用 `-DSQLITE_CORE` 編譯 `cjk.c`，並在開啟連線前註冊：

```c
int sqlite3_cjk_init(sqlite3*, char**, const sqlite3_api_routines*);
sqlite3_auto_extension((void(*)(void))sqlite3_cjk_init);
```

```sh
cc -DSQLITE_CORE -DSQLITE_ENABLE_FTS5 app.c cjk.c sqlite3.c -lm
```

amalgamation（`sqlite3.c`）要加 `-DSQLITE_ENABLE_FTS5`，否則載入時會出現 `cjk: fts5 not available`。`make static-test` 就是用這個方式編譯 `test/static.c` 並連結 `libsqlite3`。

## 結果

### 中文

全國法規資料庫 47,281 條條文，三張表只差 `tokenize=`。數字是查詢命中的條文數。`cjk` 那欄和 `LIKE '%勞工%'` 全表掃描的結果完全相同，也就是真正含有這個詞的條文數；前兩欄是預設 tokenizer 從中找到幾條。

| 查詢 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工 | 11 | 0 | 711 |
| 退休 | 75 | 0 | 844 |
| 公司 | 44 | 0 | 1809 |
| 保險 | 41 | 0 | 1579 |
| 臺灣 | 0 | 0 | 501 |
| 特別休假 | 0 | 7 | 7 |
| 有期徒刑 | 8 | 1316 | 1316 |
| 勞工 退休 | 0 | 0 | 119 |

`unicode61` 只有在標點剛好把詞隔開時才中。`trigram` 四個字的詞正常，兩個字的詞看不到。臺灣那列也包含寫成台灣的條文，`cjk` 把兩者視為同一個字。用 `bench/law.sh` 可重現：它會下載資料、重建各表，只要 `cjk` 和 `LIKE` 掃描的筆數不同就失敗。

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
- 不加 `*` 的單一個字，只會中那個字單獨出現、或位在一段中日韓文字開頭或結尾的地方，例如 `5月` 的「月」。在一段文字中間的字只存在於 bigram 裡，所以要找出所有出現的地方請用 `MATCH '茶*'`；`cjk_query` 會自動加上 `*`。也可以建表時設 `tokenize='cjk unigram 1'`，索引每一個字（4.7 萬條法規實測 26.3 MB，預設為 18.3 MB）。
- 除了下面列的折疊（臺→台等異體字、全形與半形、NFD 組合）造成的差異，兩個字以上的中日韓詞，以及寫成 `茶*` 的單字，`cjk` 的結果和 `LIKE` 掃描相同。英文和數字仍照 `unicode61` 以整個詞比對，所以 `5月` 找不到 `15月`。
- `+` 連接的字串會分開 tokenize，所以 `iphone + 手機` 找不到 `iPhone手機`：索引裡兩者之間還有一個「手」。請寫成一個詞 `iphone手機`。
- 請維持預設的 `detail='full'`。`detail='column'` 或 `detail='none'` 時 FTS5 不支援 phrase 查詢，而這裡三個字以上的中日韓查詢、或中日韓字緊貼英文或數字的查詢都是 phrase，會出現 `phrase queries are not supported` 錯誤。
- 全形 ASCII、半形片假名（含濁音）會摺疊。NFD 文字（帶組合濁點的假名、拆成字母的諺文）會先組合，所以能用 NFC 查詢找到。臺→台、裡→裏、羣→群、峯→峰、綫→線、衞→衛、爲→為、眞→真、敎→教、册→冊、祕→秘、啓→啟、淸→清、靑→青會在索引與查詢時摺疊；不做其他繁簡轉換。
- 不同版本索引的內容不同，升級後須重建舊索引：`INSERT INTO t(t) VALUES('rebuild')`。
- 漢字、平假名、片假名、諺文，以及 々〆〇 算 CJK。其他中日韓標點是分隔符。
- 47,281 條法規的索引大小：`unicode61` 11.7 MB、`cjk` 18.3 MB、`cjk unigram 1` 26.3 MB、`trigram` 27.7 MB。`cjk` 比 `trigram` 小約三分之一。

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
| `cjk`（本專案） | 字元 bigram，其餘交給 unicode61 | 無，432 行 C |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | 逐字，可選 jieba、拼音 | C++、jieba 字典 |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | trigram 加短詞 fallback | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU 資料 |
| 內建 `trigram` | 字元 trigram | 無 |

## 為什麼用 bigram 不用字典

字典式 tokenizer 的品質就是字典的品質，繁體中文的字典很薄。bigram 不會漏掉沒見過的詞。它做不到的是把「南投魚池」這種沒空白的查詢切成兩個詞。FTS5 在呼叫 tokenizer 之前就決定了 phrase 結構，所以這件事要在 FTS5 上面做。`cjk_query(text, table)` 就在那裡做這件事，拿索引本身當字典。

## 授權

MIT
