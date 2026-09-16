# fts5-cjk

SQLite FTS5 用の日本語・中国語・韓国語トークナイザ。C ファイル 1 つ、辞書なし、依存なし。

[English](README.md) · [繁體中文](README.zh-TW.md) · [한국어](README.ko.md)

## 問題

FTS5 に付属する 3 つのトークナイザは、どれも日本語に使えません。

- `unicode61`（デフォルト）は単語の区切りを知らないので、文全体が 1 トークンになります。`MATCH '琵琶湖'` は「滋賀県の琵琶湖は」の中を見つけられません。
- `trigram` は 3 文字単位です。「会社」「保険」のような 2 文字の単語は検索できません。
- `porter` は英語用です。

## `cjk` がすること

日本語・中国語・韓国語の連続した部分を、前後が重なる 2 文字のトークン（bigram）に分けます。Lucene の `CJKAnalyzer`、Elasticsearch の `cjk` アナライザ、Groonga のデフォルトと同じ方式です。それ以外の文字はそのまま `unicode61` に渡すので、英語の大文字小文字やアクセントの扱い、`unicode61` のオプションはすべてそのまま使えます。

```sql
.load ./cjk
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
INSERT INTO t VALUES ('琵琶湖は滋賀県にある日本最大の湖です');

SELECT * FROM t WHERE t MATCH '滋賀';        -- ヒット：部分一致
SELECT * FROM t WHERE t MATCH '滋賀 最大';   -- ヒット：2 語の AND
SELECT * FROM t WHERE t MATCH '滋賀最大';    -- ヒットしない：空白なしの 1 語は 1 つのフレーズなので、
                                            -- 滋賀 と 最大 が隣接している必要がある
SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH '滋賀県';
-- 琵琶湖は[滋賀県]にある日本最大の湖です
```

名前の後ろの引数は `unicode61` に渡されます。

```sql
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk remove_diacritics 2');
```

## ビルド

```sh
make        # macOS では cjk.dylib、それ以外は cjk.so
make test   # test.sql を sqlite3 で実行し、test.expected と diff
```

FTS5 入りの SQLite の `sqlite3ext.h` が必要です。macOS では Homebrew のもの（`brew install sqlite`）を使ってください。OS 付属の `sqlite3` は拡張を読み込めません。Debian / Ubuntu では `libsqlite3-dev`。ヘッダが別の場所にあるなら `SQLITE_INC` で指定します。

## 結果

### 日本語

日本語版 Wikipedia の 40 記事、4,011 段落。`tokenize=` だけが違う 3 つのテーブル。数字はクエリがヒットした段落数。`cjk` の列は全件 `LIKE '%会社%'` と完全に一致します。つまり実際にその語を含む段落の数で、他の 2 列は組み込みトークナイザがそのうち何件を見つけたかです。

| クエリ | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 会社 | 1 | 0 | 243 |
| 保険 | 0 | 0 | 217 |
| 労働 | 0 | 0 | 161 |
| 東京 | 16 | 0 | 560 |
| 契約 | 0 | 0 | 40 |
| 新幹線 | 367 | 379 | 379 |
| 労働基準法 | 79 | 80 | 80 |
| 東京 大学 | 0 | 0 | 262 |

`unicode61` は句読点で区切られた文節全体を 1 トークンにするので、2 文字の単語はほぼ見つかりません。`trigram` は 3 文字以上の単語なら同じ結果になりますが、2 文字の単語はゼロです。3 テーブルの構築は 1 秒未満でした。

### 中国語（繁体字）

台湾の法令データベース 47,257 条。`tokenize=` だけが違う 3 つのテーブル。数字はヒットした条文数。

| クエリ | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工（労働者） | 10 | 0 | 958 |
| 公司（会社） | 44 | 0 | 2050 |
| 保險（保険） | 41 | 0 | 1789 |
| 特別休假（年次有給休暇） | 0 | 7 | 7 |
| 勞工 退休 | 0 | 0 | 157 |

### 韓国語

韓国語版 Wikipedia の 40 記事、2,095 段落。

| クエリ | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 회사（会社） | 4 | 0 | 24 |
| 보험（保険） | 12 | 0 | 41 |
| 서울（ソウル） | 64 | 0 | 338 |
| 학교（学校） | 19 | 0 | 194 |
| 대통령（大統領） | 22 | 62 | 62 |
| 서울 대학 | 1 | 0 | 108 |

韓国語は分かち書きがあるので `unicode61` でも単独の語は見つかりますが、助詞が付いた形（서울은、서울의）はすべて取りこぼします。

## 知っておくこと

- 空白なしの 1 語は 1 つのフレーズです。`MATCH '滋賀県'` は部分文字列「滋賀県」を意味します。2 語を AND したいときは空白で区切ります。
- 1 文字だけのクエリは、その文字が単独で現れる文書にしかヒットしません。前方一致を使ってください：`MATCH '湖*'`。
- 漢字、ひらがな、カタカナ、ハングルを CJK として扱います。CJK の句読点は区切りです。
- インデックスのサイズは `trigram` と同程度です。

## 対応する文字

| 文字 | 状態 |
|---|---|
| 日本語 | テスト済み。漢字かな混じり文、辞書不要 |
| 中国語（繁体字・簡体字） | テスト済み |
| 韓国語 | テスト済み。韓国語は分かち書きがあるので、主に名詞に付く助詞の処理に効きます |
| ラテン、キリル、ギリシャ、アラビア、ヘブライ、ベトナム語など | 従来どおり `unicode61` |
| タイ、ラオ、クメール、ビルマ、チベット文字 | 未対応。同じく分かち書きがない文字ですが、母音や声調記号が独立したコードポイントなので、書記素クラスタ単位の bigram が必要です |

## 代替

| | 方式 | 依存 |
|---|---|---|
| `cjk`（本プロジェクト） | 文字 bigram、残りは unicode61 | なし、C 155 行 |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | 1 文字単位、任意で jieba と拼音 | C++、jieba 辞書 |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | 短い語にフォールバックする trigram | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU の word break iterator | ICU データ |
| 組み込み `trigram` | 文字 trigram | なし |

## なぜ辞書ではなく bigram か

辞書型トークナイザの品質は辞書の品質です。bigram は未知語を取りこぼしません。できないのは「滋賀最大」のような空白なしのクエリを 2 語に分けることです。FTS5 はトークナイザを呼ぶ前にフレーズ構造を決めてしまうので、この分割は FTS5 の上の層、たとえばクエリを書き換える SQL 関数で行う必要があります。それが次のステップです。

## ライセンス

MIT
