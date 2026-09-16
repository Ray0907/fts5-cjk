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
