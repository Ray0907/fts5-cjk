#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sqlite3.h>

static uint64_t rng = 0x9E3779B97F4A7C15ull;

static uint32_t rnd(void){
  rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
  return (uint32_t)(rng >> 32);
}

static const char *const frag[] = {
  "a", "Zebra", " ", "\t", "-", "\"", "'", "*", "(", ")", ":", "NEAR", "OR", "AND",
  "\xe8\x8c\xb6", "\xe9\xad\x9a\xe6\xb1\xa0\xe9\x84\x89", "\xe3\x81\x82\xe3\x81\x84",
  "\xe3\x82\xab\xe3\x83\x8a", "\xea\xb0\x95\xeb\x82\xa8", "\xef\xbd\xb1\xef\xbd\xb2",
  "\xef\xbc\xa1\xef\xbc\xa2", "\xe3\x80\x80", "\xf0\xa0\x80\x80", "\xe4\xb8\xad",
  "\xc3\xa9", "\xe2\x82\xac", "\xef\xbf\xbd", "\xef\xbf\xbf", "\xf4\x8f\xbf\xbf",
  "\xe8\x8c", "\xe8", "\xf0\xa0\x80", "\xf0", "\xf4\x90\x80\x80", "\xed\xa0\x80",
  "\xc0\x80", "\xc1\xbf", "\xe0\x80\x80", "\xf8\x88\x80\x80\x80", "\xfe", "\xff",
  "\x80", "\xbf", "\xe3\x80\x85", "\xe3\x82\x99", "\xe3\x82\x9a",
  "\xe1\x84\x92", "\xe1\x85\xa1", "\xe1\x86\xab", "\xea\xb0\x80",
};

static size_t gen(unsigned char *buf, size_t cap){
  size_t n = 0, parts = rnd() % 12;
  size_t i;
  for(i = 0; i < parts; i++){
    uint32_t kind = rnd() % 8;
    if( kind < 5 ){
      const char *s = frag[rnd() % (sizeof frag / sizeof *frag)];
      size_t l = strlen(s);
      if( n + l < cap ){ memcpy(buf + n, s, l); n += l; }
    }else if( kind == 5 ){
      if( n < cap ) buf[n++] = 0;
    }else{
      size_t l = rnd() % 16, j;
      for(j = 0; j < l && n < cap; j++) buf[n++] = (unsigned char)rnd();
    }
  }
  return n;
}

static void check(sqlite3 *db, int rc, const char *what){
  if( rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW ){
    fprintf(stderr, "%s: %s\n", what, sqlite3_errmsg(db));
    exit(1);
  }
}

static size_t quote(unsigned char *out, const unsigned char *in, size_t n){
  size_t o = 0, i;
  out[o++] = '"';
  for(i = 0; i < n; i++){
    if( in[i] == '"' ) out[o++] = '"';
    out[o++] = in[i];
  }
  out[o++] = '"';
  return o;
}

int main(int argc, char **argv){
  const char *ext = argc > 1 ? argv[1] : "./cjk-asan";
  int iters = argc > 2 ? atoi(argv[2]) : 20000;
  const char *const tok[] = { "cjk", "cjk unigram 1", "cjk unigram 1 remove_diacritics 2" };
  sqlite3 *db;
  char *err = 0;
  unsigned char buf[256], q[600];
  long matches = 0;
  size_t t;
  int i;

  check(0, sqlite3_open(":memory:", &db), "open");
  sqlite3_enable_load_extension(db, 1);
  if( sqlite3_load_extension(db, ext, "sqlite3_cjk_init", &err) != SQLITE_OK ){
    fprintf(stderr, "load %s: %s\n", ext, err);
    return 1;
  }
  for(t = 0; t < sizeof tok / sizeof *tok; t++){
    char sql[200];
    sqlite3_stmt *ins, *sel, *raw, *cq[2];
    snprintf(sql, sizeof sql, "CREATE VIRTUAL TABLE t%zu USING fts5(body, tokenize='%s')", t, tok[t]);
    check(db, sqlite3_exec(db, sql, 0, 0, 0), "create");
    snprintf(sql, sizeof sql, "INSERT INTO t%zu(body) VALUES(?1)", t);
    check(db, sqlite3_prepare_v2(db, sql, -1, &ins, 0), "prepare ins");
    snprintf(sql, sizeof sql, "SELECT rowid FROM t%zu WHERE t%zu MATCH ?1", t, t);
    check(db, sqlite3_prepare_v2(db, sql, -1, &sel, 0), "prepare sel");
    check(db, sqlite3_prepare_v2(db, sql, -1, &raw, 0), "prepare raw");
    snprintf(sql, sizeof sql, "SELECT rowid FROM t%zu WHERE t%zu MATCH cjk_query(?1)", t, t);
    check(db, sqlite3_prepare_v2(db, sql, -1, &cq[0], 0), "prepare cjk_query");
    snprintf(sql, sizeof sql, "SELECT rowid FROM t%zu WHERE t%zu MATCH cjk_query(?1, 't%zu')", t, t, t);
    check(db, sqlite3_prepare_v2(db, sql, -1, &cq[1], 0), "prepare cjk_query table");
    for(i = 0; i < iters; i++){
      size_t n = gen(buf, sizeof buf), m;
      sqlite3_bind_text(ins, 1, (const char*)buf, (int)n, SQLITE_TRANSIENT);
      check(db, sqlite3_step(ins), "insert");
      sqlite3_reset(ins);
      n = gen(buf, sizeof buf);
      m = quote(q, buf, n);
      sqlite3_bind_text(sel, 1, (const char*)q, (int)m, SQLITE_TRANSIENT);
      while( sqlite3_step(sel) == SQLITE_ROW ) matches++;
      sqlite3_reset(sel);
      sqlite3_bind_text(raw, 1, (const char*)buf, (int)n, SQLITE_TRANSIENT);
      while( sqlite3_step(raw) == SQLITE_ROW ) matches++;
      sqlite3_reset(raw);
      for(m = 0; m < 2; m++){
        int rc;
        sqlite3_bind_text(cq[m], 1, (const char*)buf, (int)n, SQLITE_TRANSIENT);
        while( (rc = sqlite3_step(cq[m])) == SQLITE_ROW ) matches++;
        check(db, rc, "cjk_query output must be a valid MATCH");
        sqlite3_reset(cq[m]);
      }
    }
    sqlite3_finalize(ins);
    sqlite3_finalize(sel);
    sqlite3_finalize(raw);
    sqlite3_finalize(cq[0]);
    sqlite3_finalize(cq[1]);
    snprintf(sql, sizeof sql, "INSERT INTO t%zu(t%zu) VALUES('integrity-check')", t, t);
    check(db, sqlite3_exec(db, sql, 0, 0, 0), "integrity-check");
  }
  sqlite3_close(db);
  printf("asan ok: %d docs per tokenizer, %ld matches\n", iters, matches);
  return 0;
}
