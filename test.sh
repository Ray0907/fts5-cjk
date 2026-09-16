#!/bin/sh
# Runs test.sql through sqlite3 and diffs against test.expected.
set -e
cd "$(dirname "$0")"
SQLITE3=${SQLITE3:-$(ls /opt/homebrew/opt/sqlite/bin/sqlite3 2>/dev/null || command -v sqlite3)}
ext=$(ls cjk.dylib cjk.so 2>/dev/null | head -1)
"$SQLITE3" -bail -cmd ".load ./${ext%.*}" < test.sql > test.out 2>&1 || { cat test.out; exit 1; }
diff -u test.expected test.out && echo "ok"
