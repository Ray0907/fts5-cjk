# fts5-cjk

SQLite FTS5용 한국어·중국어·일본어 토크나이저. C 파일 하나, 사전 없음, 의존성 없음.

[English](README.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md)

## 문제

FTS5에 들어 있는 토크나이저 세 개는 한국어에 잘 맞지 않습니다.

- `unicode61`(기본값)은 공백으로만 나눕니다. 그래서 `MATCH '한강'`은 "한강은", "한강을", "한강의"를 찾지 못합니다. 조사가 붙은 단어는 전부 다른 토큰입니다.
- `trigram`은 세 글자 단위라서 "회사", "보험" 같은 두 글자 단어를 찾지 못합니다.
- `porter`는 영어용입니다.

## `cjk`가 하는 일

한글·한자·가나가 이어진 구간을 앞뒤가 겹치는 두 글자 토큰(bigram)으로 나눕니다. Lucene의 `CJKAnalyzer`, Elasticsearch의 `cjk` 분석기와 같은 방식입니다. 그 밖의 문자는 그대로 `unicode61`에 넘기므로 영어의 대소문자 접기, 악센트 처리, `unicode61`의 옵션이 모두 그대로 동작합니다.

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

## 빌드

```sh
make        # macOS는 cjk.dylib, 그 외는 cjk.so
make test   # test.sql을 sqlite3로 실행하고 test.expected와 diff
```

FTS5가 포함된 SQLite의 `sqlite3ext.h`가 필요합니다. macOS에서는 Homebrew의 것을 쓰세요(`brew install sqlite`). 시스템 `sqlite3`는 확장을 로드하지 못합니다. Debian / Ubuntu는 `libsqlite3-dev`. 헤더가 다른 곳에 있으면 `SQLITE_INC`로 지정합니다.

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

대만 법령 데이터베이스 47,257개 조문. `tokenize=`만 다른 테이블 세 개. 숫자는 찾은 조문 수.

| 검색어 | `unicode61` | `trigram` | `cjk` |
|---|---|---|---|
| 勞工(노동자) | 10 | 0 | 958 |
| 公司(회사) | 44 | 0 | 2050 |
| 保險(보험) | 41 | 0 | 1789 |
| 特別休假(연차휴가) | 0 | 7 | 7 |
| 勞工 退休 | 0 | 0 | 157 |

## 알아둘 것

- 공백 없는 한 단어는 하나의 구입니다. `MATCH '특별시'`는 부분 문자열 "특별시"를 뜻합니다. 두 단어를 AND 하려면 공백으로 나누세요.
- 한 글자 검색어는 그 글자가 홀로 있는 문서에만 걸립니다. 접두 검색을 쓰세요: `MATCH '강*'`.
- 한글, 한자, 히라가나, 가타카나를 CJK로 봅니다. CJK 문장부호는 구분자입니다.
- 인덱스 크기는 `trigram`과 비슷합니다.

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
| `cjk`(이 프로젝트) | 글자 bigram, 나머지는 unicode61 | 없음, C 155줄 |
| [wangfenjin/simple](https://github.com/wangfenjin/simple) | 글자 단위, 선택적으로 jieba·병음 | C++, jieba 사전 |
| [streetwriters/sqlite-better-trigram](https://github.com/streetwriters/sqlite-better-trigram) | 짧은 단어 폴백이 있는 trigram | TypeScript |
| [cwt/fts5-icu-tokenizer](https://github.com/cwt/fts5-icu-tokenizer) | ICU word break iterator | ICU 데이터 |
| 내장 `trigram` | 글자 trigram | 없음 |

## 왜 사전 대신 bigram인가

사전 기반 토크나이저의 품질은 곧 사전의 품질입니다. bigram은 처음 보는 단어를 놓치지 않습니다. 못 하는 것은 "서울한강"처럼 공백 없는 검색어를 두 단어로 나누는 일입니다. FTS5는 토크나이저를 부르기 전에 구 구조를 정하므로, 이 분리는 FTS5 위쪽, 예를 들어 검색어를 고쳐 쓰는 SQL 함수에서 해야 합니다. 그것이 다음 단계입니다.

## 라이선스

MIT
