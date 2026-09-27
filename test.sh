#!/bin/sh
# Runs test.sql through sqlite3 and diffs against test.expected.
set -e
cd "$(dirname "$0")"
SQLITE3=${SQLITE3:-$(ls /opt/homebrew/opt/sqlite/bin/sqlite3 2>/dev/null || command -v sqlite3)}
ext=$(ls cjk.dylib cjk.so 2>/dev/null | head -1)
# The final CREATE deliberately fails (unigram 2); compare its error too.
if "$SQLITE3" -bail -cmd ".load ./${ext%.*}" < test.sql > test.out 2>&1; then
  echo "expected invalid unigram to fail"; exit 1
else
  rc=$?
  if [ "$rc" -ne 1 ]; then cat test.out; exit "$rc"; fi
fi
# sqlite3 versions differ in the error prefix ("Error" vs "Runtime error").
sed 's/^.*rror near line [0-9]*: //' test.out > test.out.tmp && mv test.out.tmp test.out
diff -u test.expected test.out && echo "ok"
