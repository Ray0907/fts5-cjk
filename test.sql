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
SELECT '# F12 invalid unigram rejected';
.bail off
CREATE VIRTUAL TABLE invalid_unigram USING fts5(body, tokenize='cjk unigram 2');
.bail on
