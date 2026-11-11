# fts5-cjk

SQLite FTS5용 한국어·중국어·일본어 토크나이저. C 파일 하나, 사전 없음, 의존성 없음.

[English](README.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md)

## 문제

FTS5에 들어 있는 토크나이저 세 개는 한국어에 잘 맞지 않습니다.

- `unicode61`(기본값)은 공백으로만 나눕니다. 그래서 `MATCH '한강'`은 "한강은", "한강을", "한강의"를 찾지 못합니다. 조사가 붙은 단어는 전부 다른 토큰입니다.
- `trigram`은 세 글자 단위라서 "회사", "보험" 같은 두 글자 단어를 찾지 못합니다.
- `porter`는 영어용입니다.

## `cjk`가 하는 일

한글·한자·가나가 이어진 구간을 앞뒤가 겹치는 두 글자 토큰(bigram)으로 나눕니다. Lucene의 `CJKAnalyzer`, Elasticsearch의 `cjk` 분석기와 같은 방식입니다. 그 밖의 문자는 그대로 `unicode61`에 넘기므로 영어의 대소문자 접기, 악센트 처리, `unicode61`의 옵션이 모두 그대로 동작합니다. 각 구간의 첫 글자와 마지막 글자는 따로도 색인하므로, `5월`이나 `第38條`처럼 영문이나 숫자 옆에 붙은 글자도 찾을 수 있습니다.

```sql
.load ./cjk
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
INSERT INTO t VALUES ('한강은 서울특별시를 가로질러 흐른다');

SELECT * FROM t WHERE t MATCH '한강';        -- 찾음: 조사가 붙어도 부분 일치
SELECT * FROM t WHERE t MATCH '서울 한강';   -- 찾음: 두 단어 AND
SELECT * FROM t WHERE t MATCH '서울한강';    -- 못 찾음: 공백 없는 한 단어는 하나의 구(phrase)이므로
                                            -- 서울 과 한강 이 붙어 있어야 함
SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH '특별시';
-- 한강은 서울[특별시]를 가로질러 흐른다
```

이름 뒤의 인자는 `unicode61`에 전달됩니다.

```sql
CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk remove_diacritics 2');
```

## 검색창 입력: `cjk_query()`

검색창에 입력한 문자열을 그대로 `MATCH`에 넘기면 안전하지 않습니다. `C++`는 구문 오류이고, `勞工-退休`는 열 필터로 읽히며, `"` 하나만 있으면 닫히지 않은 문자열입니다. 이 확장은 `cjk_query()`도 등록하며, 이런 입력을 `MATCH` 문자열로 바꿉니다.

```sql
SELECT cjk_query('C++ 勞工-退休');   -- "C++" "勞工-退休"
SELECT cjk_query('강');              -- "강"*
SELECT * FROM t WHERE t MATCH cjk_query('서울한강', 't');
                                    -- 찾음: "서울" "한강"으로 바꿈
```

- `cjk_query(text)`는 ASCII 공백과 전각 공백 U+3000으로 단어를 나누고(U+00A0 같은 다른 유니코드 공백은 구분자가 아닙니다), 각 단어를 큰따옴표로 감싸며, CJK 한 글자인 단어에는 `*`를 붙입니다. 단어가 없는 입력은 `""`가 되어 아무것도 찾지 않습니다.
- `cjk_query(text, 'table')`은 테이블도 확인합니다. 하나의 구로는 결과가 없는 CJK 단어를 결과가 가장 많은 위치에서 나눕니다. 먼저 두 조각으로 나눠 보고, 12글자 이하의 단어는 세 조각도 시도합니다. 어떻게 나눠도 결과가 없으면 원래 단어를 그대로 둡니다. 시도할 때마다 쿼리를 한 번 실행하므로 32글자 이하의 단어만 나누고, 한 번 호출에 최대 256번까지 시도합니다. 남은 횟수로 부족한 단어는 나누지 않습니다. 나누는 결과는 테이블 내용에 따라 달라서, 같은 입력도 다른 데이터베이스에서는 다르게 바뀔 수 있습니다.
- 인자 두 개 형태는 지정한 테이블을 읽기 때문에 SQLite가 트리거와 뷰 안에서는 허용하지 않습니다. 이름은 식별자 하나로 따옴표 처리되므로 `schema.table`이 아니라 테이블 이름을 넘기세요.

## 빌드

```sh
make        # macOS는 cjk.dylib, 그 외는 cjk.so
make test   # test.sql을 sqlite3로 실행하고 test.expected와 diff
```

빌드에는 C 컴파일러만 있으면 됩니다. SQLite 헤더는 `include/`에 있습니다. 로드와 테스트에는 FTS5와 확장 로드를 지원하는 `sqlite3`가 필요합니다. macOS에서는 Homebrew의 것을 쓰세요(`brew install sqlite`). 시스템 `sqlite3`는 확장을 로드하지 못합니다.

`make asan-test`는 AddressSanitizer와 UBSan으로 확장을 빌드하고 잘못된 UTF-8을 포함한 무작위 바이트를 입력합니다. `libsqlite3`를 링크합니다(macOS는 Homebrew, Debian / Ubuntu는 `libsqlite3-dev`).

### 정적 링크

확장을 실행 중에 로드하지 않고 SQLite를 프로그램에 함께 컴파일하는 경우, `cjk.c`를 `-DSQLITE_CORE`로 컴파일하고 연결을 열기 전에 등록합니다.

```c
int sqlite3_cjk_init(sqlite3*, char**, const sqlite3_api_routines*);
sqlite3_auto_extension((void(*)(void))sqlite3_cjk_init);
```

```sh
cc -DSQLITE_CORE -DSQLITE_ENABLE_FTS5 app.c cjk.c sqlite3.c -lm
```

amalgamation(`sqlite3.c`)에는 `-DSQLITE_ENABLE_FTS5`가 필요합니다. 없으면 로드할 때 `cjk: fts5 not available`이 납니다. `make static-test`는 이 방식으로 `test/static.c`를 빌드해 `libsqlite3`와 링크합니다.

## 결과

### 한국어

한국어 위키백과 40개 문서, 2,095개 문단. `tokenize=`만 다른 테이블 세 개. 숫자는 검색이 찾은 문단 수. `cjk` 열은 전체를 `LIKE '%서울%'`로 훑은 결과와 정확히 같습니다. 즉 실제로 그 단어가 들어 있는 문단 수이고, 다른 두 열은 내장 토크나이저가 그중 몇 개를 찾았는지입니다.

| 검색어 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 회사 | 4 | 0 | 24 |
| 보험 | 12 | 0 | 41 |
| 서울 | 64 | 0 | 338 |
| 학교 | 19 | 0 | 194 |
| 결혼 | 13 | 0 | 32 |
| 계약 | 6 | 0 | 19 |
| 대통령 | 22 | 62 | 62 |
| 인천공항 | 4 | 17 | 17 |
| 서울 대학 | 1 | 0 | 108 |

`unicode61`은 공백으로만 나누므로 "서울"은 찾지만 "서울은", "서울의", "서울에서"는 전부 놓칩니다. 실제 문장에서는 그쪽이 대부분입니다. `trigram`은 세 글자 이상 단어에서만 같은 결과를 내고, 두 글자 단어는 0입니다. 테이블 세 개를 만드는 데 1초가 안 걸렸습니다.

### 일본어

일본어 위키백과 40개 문서, 4,011개 문단.

| 검색어 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 会社(회사) | 1 | 0 | 243 |
| 保険(보험) | 0 | 0 | 217 |
| 東京(도쿄) | 16 | 0 | 560 |
| 新幹線(신칸센) | 367 | 379 | 379 |
| 労働基準法(노동기준법) | 79 | 80 | 80 |
| 東京 大学 | 0 | 0 | 262 |

일본어는 띄어쓰기가 없어서 `unicode61`은 절 전체를 토큰 하나로 봅니다. 두 글자 단어는 거의 못 찾습니다.

### 중국어(번체)

대만 법령 데이터베이스 47,281개 조문. `tokenize=`만 다른 테이블 세 개. 숫자는 찾은 조문 수. `bench/law.sh`로 재현할 수 있습니다.

| 검색어 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工(노동자) | 11 | 0 | 711 |
| 公司(회사) | 44 | 0 | 1809 |
| 保險(보험) | 41 | 0 | 1579 |
| 特別休假(연차휴가) | 0 | 7 | 7 |
| 勞工 退休 | 0 | 0 | 119 |

## 알아둘 것

- 공백 없는 한 단어는 하나의 구입니다. `MATCH '특별시'`는 부분 문자열 "특별시"를 뜻합니다. 두 단어를 AND 하려면 공백으로 나누세요.
- `*` 없는 한 글자는 그 글자가 홀로 있거나 CJK 구간의 처음 또는 끝에 있을 때만 걸립니다(`5월`의 "월" 등). 구간 중간의 글자는 bigram의 일부로만 있으므로, 모든 위치를 찾으려면 `MATCH '강*'`을 쓰세요. `cjk_query`는 `*`를 자동으로 붙입니다. `tokenize='cjk unigram 1'`로 모든 글자를 색인할 수도 있습니다(법령 4.7만 조문 실측 시 26.3 MB, 기본값은 18.3 MB).
- 아래의 통합(臺→台 같은 이체자, 전각과 반각, NFD 결합)으로 생기는 차이를 빼면, 두 글자 이상의 CJK 단어와 `강*`처럼 쓴 한 글자에서는 `cjk`의 결과가 `LIKE` 전체 검색과 같습니다. 영문과 숫자는 여전히 `unicode61`의 단어 단위로 일치하므로 `5월`로 `15월`은 찾지 못합니다.
- `+`로 이은 문자열은 따로 토큰화되므로 `iphone + 手機`는 `iPhone手機`를 찾지 못합니다. 인덱스에서는 둘 사이에 "手"가 들어 있기 때문입니다. `iphone手機`처럼 한 단어로 쓰세요.
- `detail`은 기본값 `'full'`로 두세요. `detail='column'`이나 `detail='none'`에서는 FTS5가 구 검색을 지원하지 않는데, 여기서는 세 글자 이상의 CJK 검색어와 CJK가 영문·숫자에 붙은 검색어가 모두 구이므로 `phrase queries are not supported` 오류가 납니다.
- 전각 ASCII와 반각 가타카나(탁점·반탁점 포함), 그리고 臺→台, 裡→裏, 羣→群, 峯→峰, 綫→線, 衞→衛, 爲→為, 眞→真, 敎→教, 册→冊, 祕→秘, 啓→啟, 淸→清, 靑→青을 색인과 검색 시 정규화합니다. 그 외의 번체·간체 변환은 하지 않습니다. NFD 텍스트(결합용 탁점이 붙은 가나, 자모로 풀어 쓴 한글)는 합성하므로 NFC 검색어로 찾을 수 있습니다.
- 버전마다 색인하는 내용이 달라지므로 업그레이드 후 기존 인덱스를 다시 빌드하세요: `INSERT INTO t(t) VALUES('rebuild')`.
- 한글, 한자, 히라가나, 가타카나, 그리고 々〆〇을 CJK로 봅니다. 그 밖의 CJK 문장부호는 구분자입니다.
- 법령 47,281 조문의 인덱스 크기: `unicode61` 11.7 MB, `cjk` 18.3 MB, `cjk unigram 1` 26.3 MB, `trigram` 27.7 MB. `cjk`는 `trigram`보다 약 3분의 1 작습니다.

## 지원 문자

| 문자 | 상태 |
|---|---|
| 한국어 | 테스트함. 띄어쓰기가 있으므로 주로 명사에 붙은 조사 처리에 효과 |
| 중국어(번체·간체) | 테스트함 |
| 일본어 | 테스트함. 한자·가나 혼용, 사전 불필요 |
| 라틴, 키릴, 그리스, 아랍, 히브리, 베트남어 등 | 기존대로 `unicode61` |
| 태국, 라오, 크메르, 버마, 티베트 문자 | 미지원. 역시 띄어쓰기가 없지만 모음과 성조 기호가 별도 코드포인트라서 자소 클러스터 단위 bigram이 필요 |

## 대안

| | 방식 | 의존성 |
|---|---|---|
| `cjk`(이 프로젝트) | 글자 bigram, 나머지는 unicode61 | 없음, C 432줄 |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | 글자 단위, 선택적으로 jieba·병음 | C++, jieba 사전 |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | 짧은 단어 폴백이 있는 trigram | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU 데이터 |
| 내장 `trigram` | 글자 trigram | 없음 |

## 왜 사전 대신 bigram인가

사전 기반 토크나이저의 품질은 곧 사전의 품질입니다. bigram은 처음 보는 단어를 놓치지 않습니다. 못 하는 것은 "서울한강"처럼 공백 없는 검색어를 두 단어로 나누는 일입니다. FTS5는 토크나이저를 부르기 전에 구 구조를 정하므로, 이 분리는 FTS5 위쪽에서 해야 합니다. `cjk_query(text, table)`이 그 자리에서 인덱스 자체를 사전으로 써서 나눕니다.

## 라이선스

MIT
