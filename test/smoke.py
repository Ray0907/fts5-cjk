"""Loads a built cjk extension through Python's sqlite3 and runs one CJK query.

    python test/smoke.py ./cjk.dll
"""
import sqlite3
import sys

db = sqlite3.connect(":memory:")
print("sqlite", sqlite3.sqlite_version)
db.enable_load_extension(True)
db.load_extension(sys.argv[1])
db.execute("CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk')")
db.execute("INSERT INTO t VALUES (?)", ("日月潭紅茶產於南投縣魚池鄉",))
rows = db.execute("SELECT rowid FROM t WHERE t MATCH cjk_query(?, 't')", ("南投魚池",)).fetchall()
print("smoke", "ok" if rows == [(1,)] else "FAIL", rows)
sys.exit(rows != [(1,)])
