/* Links cjk.c into the program with -DSQLITE_CORE and registers it with
** sqlite3_auto_extension, as the README describes for static builds. */
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

int sqlite3_cjk_init(sqlite3*, char**, const sqlite3_api_routines*);

int main(void){
  sqlite3 *db;
  sqlite3_stmt *pStmt;
  const char *want = "日月潭紅茶產於[南投]縣[魚池]鄉";
  int ok = 0;
  sqlite3_auto_extension((void(*)(void))sqlite3_cjk_init);
  if( sqlite3_open(":memory:", &db) != SQLITE_OK
   || sqlite3_exec(db, "CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');"
                       "INSERT INTO t VALUES ('日月潭紅茶產於南投縣魚池鄉');", 0, 0, 0) != SQLITE_OK
   || sqlite3_prepare_v2(db, "SELECT highlight(t, 0, '[', ']') FROM t"
                             " WHERE t MATCH cjk_query('南投魚池', 't')", -1, &pStmt, 0) != SQLITE_OK ){
    fprintf(stderr, "static: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  if( sqlite3_step(pStmt) == SQLITE_ROW ){
    ok = strcmp((const char*)sqlite3_column_text(pStmt, 0), want) == 0;
    printf("static %s: %s\n", ok ? "ok" : "FAIL", sqlite3_column_text(pStmt, 0));
  }else{
    printf("static FAIL: no row (%s)\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return !ok;
}
