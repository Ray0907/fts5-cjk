package fts5cjktest

/*
#cgo CFLAGS: -I../../include
#cgo darwin LDFLAGS: -L/opt/homebrew/opt/sqlite/lib
#cgo LDFLAGS: -lsqlite3
#include <stdlib.h>
#include "sqlite3.h"

static char *runQueryOne(const char *setupSql, const char *querySql, char **errOut) {
	sqlite3 *db = NULL;
	if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
		*errOut = sqlite3_mprintf("open: %s", db ? sqlite3_errmsg(db) : "nomem");
		if (db) sqlite3_close(db);
		return NULL;
	}
	char *errmsg = NULL;
	if (sqlite3_exec(db, setupSql, NULL, NULL, &errmsg) != SQLITE_OK) {
		*errOut = errmsg;
		sqlite3_close(db);
		return NULL;
	}
	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(db, querySql, -1, &stmt, NULL) != SQLITE_OK) {
		*errOut = sqlite3_mprintf("prepare: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}
	char *res = NULL;
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const unsigned char *txt = sqlite3_column_text(stmt, 0);
		if (txt) res = sqlite3_mprintf("%s", (const char *)txt);
	} else {
		*errOut = sqlite3_mprintf("step (%d): %s", rc, sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return res;
}
*/
import "C"

import (
	"errors"
	"unsafe"

	_ "github.com/Ray0907/fts5-cjk"
)

func queryOne(setupSQL, querySQL string) (string, error) {
	cSetup := C.CString(setupSQL)
	defer C.free(unsafe.Pointer(cSetup))
	cQuery := C.CString(querySQL)
	defer C.free(unsafe.Pointer(cQuery))

	var cErr *C.char
	cRes := C.runQueryOne(cSetup, cQuery, &cErr)
	if cErr != nil {
		defer C.sqlite3_free(unsafe.Pointer(cErr))
		return "", errors.New(C.GoString(cErr))
	}
	if cRes == nil {
		return "", nil
	}
	defer C.sqlite3_free(unsafe.Pointer(cRes))
	return C.GoString(cRes), nil
}
