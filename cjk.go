// Package cjk statically links the cjk FTS5 tokenizer and cjk_query() into
// every SQLite connection opened by a cgo driver such as mattn/go-sqlite3
// (build with -tags sqlite_fts5). Import it for its side effect:
//
//	import _ "github.com/Ray0907/fts5-cjk"
package cjk

/*
#cgo CFLAGS: -DSQLITE_CORE -DCJK_BUSY_TIMEOUT_MS=5000 -I${SRCDIR}/include
#include "sqlite3.h"
int sqlite3_cjk_init(sqlite3*, char**, const sqlite3_api_routines*);
static int reg(void){ return sqlite3_auto_extension((void(*)(void))sqlite3_cjk_init); }
*/
import "C"

func init() { C.reg() }
