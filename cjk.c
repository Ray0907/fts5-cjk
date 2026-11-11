/*
** cjk: an SQLite FTS5 tokenizer for Chinese / Japanese / Korean text.
**
** CJK runs become overlapping character bigrams (the Lucene CJKAnalyzer /
** Elasticsearch "cjk" approach); everything else is handed to unicode61, so
** Latin text keeps case folding, diacritic removal and the usual options.
** CJK orthographic variants and half-width kana are folded; full-width ASCII
** is folded too. Existing indexes must be rebuilt after upgrading.
**
**   CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
**   CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk unigram 1');
**   CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk unigram 1 remove_diacritics 2');
** unigram 1 additionally indexes every single character, without changing phrase
** adjacency. unigram 0 (the default) indexes bigrams plus the first and last
** character of each CJK run, so 第38條 and 茶* work without the larger index.
**
** A query bareword that tokenizes into several bigrams is matched by FTS5 as a
** phrase, so  MATCH '魚池鄉'  requires 魚池 and 池鄉 to be adjacent, i.e. it is a
** substring match. Space-separated words are ANDed as usual.
**
** The extension also registers cjk_query(text [, table]), which turns raw
** search-box text into a MATCH string; see cjkQueryFunc.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <string.h>
#include <stdlib.h>

typedef struct CjkTokenizer {
  fts5_tokenizer parent;   /* unicode61 */
  Fts5Tokenizer *pParent;
  int unigram;
} CjkTokenizer;

typedef int (*xTokenFn)(void*, int, const char*, int, int, int);

typedef struct Ctx {
  void *pCtx;
  xTokenFn xToken;
  int base;                /* byte offset of the segment passed to the parent */
  int seen;                /* a token has been emitted in this call */
  char tail[4];            /* query side: last character of the previous CJK run, */
  int nTail, tailStart, tailEnd;  /* emitted only if another token follows */
} Ctx;

static int emit(Ctx *ctx, int tflags, const char *z, int n, int iStart, int iEnd){
  if( ctx->nTail ){
    int nTail = ctx->nTail;
    ctx->nTail = 0;
    int rc = ctx->xToken(ctx->pCtx, 0, ctx->tail, nTail, ctx->tailStart, ctx->tailEnd);
    if( rc != SQLITE_OK ) return rc;
  }
  ctx->seen = 1;
  return ctx->xToken(ctx->pCtx, tflags, z, n, iStart, iEnd);
}

/* Han, Hiragana, Katakana, Hangul, and the ideographic marks 々〆〇 that sit
** inside words. The rest of CJK punctuation (U+3000-303F) is a separator. */
static int isCJK(unsigned int c){
  return (c >= 0x3005 && c <= 0x3007) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF)
      || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x20000 && c <= 0x3FFFD)
      || (c >= 0x3040 && c <= 0x30FF) || (c >= 0x31F0 && c <= 0x31FF)
      || (c >= 0xFF66 && c <= 0xFF9F)
      || (c >= 0x1100 && c <= 0x11FF) || (c >= 0x3130 && c <= 0x318F)
      || (c >= 0xAC00 && c <= 0xD7AF);
}

/* Decode one UTF-8 code point; returns byte length (1 for invalid bytes). */
static int utf8Decode(const unsigned char *z, int n, unsigned int *pc){
  unsigned char b = z[0];
  int len = b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
  if( len > n ) len = 1;
  unsigned int c = len == 1 ? b : len == 2 ? (b & 0x1F) : len == 3 ? (b & 0x0F) : (b & 0x07);
  for(int i = 1; i < len; i++){
    if( (z[i] & 0xC0) != 0x80 ){ *pc = b; return 1; }
    c = (c << 6) | (z[i] & 0x3F);
  }
  *pc = c;
  return len;
}

static int parentCb(void *p, int tflags, const char *pTok, int nTok, int iStart, int iEnd){
  Ctx *ctx = (Ctx*)p;
  int i;
  for(i = 0; i + 2 < nTok; i++){
    if( (unsigned char)pTok[i] == 0xEF && ((unsigned char)pTok[i+1] == 0xBC || (unsigned char)pTok[i+1] == 0xBD)
        && (unsigned char)pTok[i+2] >= 0x80 ) break;
  }
  if( i + 2 >= nTok )
    return emit(ctx, tflags, pTok, nTok, ctx->base + iStart, ctx->base + iEnd);
  char *z = sqlite3_malloc(nTok);
  if( !z ) return SQLITE_NOMEM;
  int n = 0;
  for(i = 0; i < nTok; ){
    unsigned int cp;
    int len = utf8Decode((const unsigned char*)pTok + i, nTok - i, &cp);
    if( cp >= 0xFF01 && cp <= 0xFF5E ){
      z[n++] = cp - 0xFEE0;
    }else{
      memcpy(z + n, pTok + i, len);
      n += len;
    }
    i += len;
  }
  int rc = emit(ctx, tflags, z, n, ctx->base + iStart, ctx->base + iEnd);
  sqlite3_free(z);
  return rc;
}

static int cjkCreate(void *pUser, const char **azArg, int nArg, Fts5Tokenizer **ppOut){
  fts5_api *pApi = (fts5_api*)pUser;
  CjkTokenizer *p = sqlite3_malloc(sizeof(*p));
  if( !p ) return SQLITE_NOMEM;
  memset(p, 0, sizeof(*p));
  const char **parentArgs = sqlite3_malloc64(((sqlite3_uint64)nArg + 1) * sizeof(*parentArgs));
  if( !parentArgs ){ sqlite3_free(p); return SQLITE_NOMEM; }
  int nParent = 0, seen = 0, rc = SQLITE_OK;
  for(int i = 0; i < nArg && rc == SQLITE_OK; i++){
    if( strcmp(azArg[i], "unigram") == 0 ){
      if( seen || ++i == nArg || (strcmp(azArg[i], "0") && strcmp(azArg[i], "1")) ){
        rc = SQLITE_ERROR;
      }else{
        p->unigram = azArg[i][0] == '1';
        seen = 1;
      }
    }else{
      parentArgs[nParent++] = azArg[i];
    }
  }
  void *pParentUser = 0;
  if( rc == SQLITE_OK ) rc = pApi->xFindTokenizer(pApi, "unicode61", &pParentUser, &p->parent);
  if( rc == SQLITE_OK ) rc = p->parent.xCreate(pParentUser, parentArgs, nParent, &p->pParent);
  sqlite3_free(parentArgs);
  if( rc != SQLITE_OK ){ sqlite3_free(p); return rc; }
  *ppOut = (Fts5Tokenizer*)p;
  return SQLITE_OK;
}

static void cjkDelete(Fts5Tokenizer *pTok){
  CjkTokenizer *p = (CjkTokenizer*)pTok;
  if( p->pParent ) p->parent.xDelete(p->pParent);
  sqlite3_free(p);
}

typedef struct Rune {
  unsigned int cp;
  int start, end;           /* offsets in the original input */
} Rune;

typedef struct Variant { unsigned int from, to; } Variant;
static const Variant variants[] = {
  {0x518C, 0x518A}, {0x5553, 0x555F}, {0x5CEF, 0x5CF0},
  {0x654E, 0x6559}, {0x6DF8, 0x6E05}, {0x7232, 0x70BA},
  {0x771E, 0x771F}, {0x7955, 0x79D8}, {0x7DAB, 0x7DDA},
  {0x7FA3, 0x7FA4}, {0x81FA, 0x53F0}, {0x885E, 0x885B},
  {0x88E1, 0x88CF}, {0x9751, 0x9752}
};

static int variantCmp(const void *key, const void *entry){
  unsigned int a = *(const unsigned int*)key, b = ((const Variant*)entry)->from;
  return (a > b) - (a < b);
}

static unsigned int fold(unsigned int cp){
  /* U+FF66..FF9F, including the two standalone voiced marks. */
  static const unsigned short kana[] = {
    0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5,
    0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA,
    0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9,
    0x30BB, 0x30BD, 0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA,
    0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8,
    0x30DB, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6,
    0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3,
    0x309B, 0x309C
  };
  if( cp >= 0xFF66 && cp <= 0xFF9F ) return kana[cp - 0xFF66];
  const Variant *v = bsearch(&cp, variants, sizeof(variants)/sizeof(*variants), sizeof(*variants), variantCmp);
  return v ? v->to : cp;
}

/* The precomposed form of prev followed by c, or 0. Covers voiced-sound marks
** (half-width or combining) after kana, and Hangul jamo L+V then LV+T, so NFD
** text indexes like NFC. */
static unsigned int compose(unsigned int prev, unsigned int c){
  if( c >= 0x1161 && c <= 0x1175 && prev >= 0x1100 && prev <= 0x1112 )
    return 0xAC00 + ((prev - 0x1100)*21 + c - 0x1161)*28;
  if( c >= 0x11A8 && c <= 0x11C2 && prev >= 0xAC00 && prev <= 0xD7A3 && (prev - 0xAC00)%28 == 0 )
    return prev + c - 0x11A7;
  int handakuten = c == 0xFF9F || c == 0x309A;
  if( !handakuten && c != 0xFF9E && c != 0x3099 ) return 0;
  if( prev >= 0x3041 && prev <= 0x3096 ){
    /* Hiragana mirrors katakana 0x60 lower, but only up to ゔ. */
    unsigned int k = compose(prev + 0x60, c);
    return k && k - 0x60 <= 0x3094 ? k - 0x60 : 0;
  }
  if( handakuten ) return prev >= 0x30CF && prev <= 0x30DB && (prev - 0x30CF)%3 == 0 ? prev + 2 : 0;
  if( prev == 0x30A6 ) return 0x30F4;  /* ウ → ヴ */
  if( (prev >= 0x30AB && prev <= 0x30C1 && (prev - 0x30AB)%2 == 0)
      || prev == 0x30C4 || prev == 0x30C6 || prev == 0x30C8
      || prev == 0x309D || prev == 0x30FD ) return prev + 1;
  if( prev >= 0x30CF && prev <= 0x30DB && (prev - 0x30CF)%3 == 0 ) return prev + 1;
  if( prev >= 0x30EF && prev <= 0x30F2 ) return prev + 8;  /* ワヰヱヲ → ヷヸヹヺ */
  return 0;
}

static int utf8Encode(unsigned int cp, char *z){
  if( cp < 0x80 ){ z[0] = cp; return 1; }
  if( cp < 0x800 ){ z[0] = 0xC0 | (cp >> 6); z[1] = 0x80 | (cp & 63); return 2; }
  if( cp < 0x10000 ){
    z[0] = 0xE0 | (cp >> 12); z[1] = 0x80 | ((cp >> 6) & 63); z[2] = 0x80 | (cp & 63); return 3;
  }
  z[0] = 0xF0 | (cp >> 18); z[1] = 0x80 | ((cp >> 12) & 63);
  z[2] = 0x80 | ((cp >> 6) & 63); z[3] = 0x80 | (cp & 63);
  return 4;
}

/* Emit folded bigrams, keeping offsets in the original CJK run. Without unigram 1,
** the first and last character of a run also get positions of their own, so a
** character next to Latin text or digits (第38條, 5月) is a token, and 茶* finds a
** run-final 茶. A query only needs those edges where another token adjoins the run;
** a pure CJK query stays a chain of bigrams. */
static int emitBigrams(Ctx *ctx, const char *text, int iStart, int iEnd, int unigram, int query){
  Rune *a = sqlite3_malloc64((sqlite3_uint64)(iEnd - iStart) * sizeof(*a));
  if( !a ) return SQLITE_NOMEM;
  int n = 0, rc = SQLITE_OK;
  for(int i = iStart; i < iEnd; ){
    unsigned int cp;
    int len = utf8Decode((const unsigned char*)text + i, iEnd - i, &cp);
    if( n ){
      unsigned int combined = compose(a[n-1].cp, cp);
      if( combined ){
        a[n-1].cp = combined;
        a[n-1].end = i + len;
        i += len;
        continue;
      }
    }
    a[n++] = (Rune){ fold(cp), i, i + len };
    i += len;
  }
  for(int j = 0; j < n && rc == SQLITE_OK; j++){
    char z[8];
    int size = utf8Encode(a[j].cp, z);
    if( j + 1 < n ){
      if( unigram ? !query : j == 0 && (!query || ctx->seen) ){
        rc = emit(ctx, 0, z, size, a[j].start, a[j].end);
        if( rc != SQLITE_OK ) break;
      }
      size += utf8Encode(a[j+1].cp, z + size);
      rc = emit(ctx, unigram && !query ? FTS5_TOKEN_COLOCATED : 0, z, size, a[j].start, a[j+1].end);
    }else if( query && !unigram && n > 1 ){
      memcpy(ctx->tail, z, size);
      ctx->nTail = size;
      ctx->tailStart = a[j].start;
      ctx->tailEnd = a[j].end;
    }else{
      /* A trailing query unigram spans the final character for highlight(). */
      rc = emit(ctx, 0, z, size, a[j].start, a[j].end);
    }
  }
  sqlite3_free(a);
  return rc;
}

static int cjkTokenize(Fts5Tokenizer *pTok, void *pCtx, int flags, const char *pText, int nText, xTokenFn xToken){
  CjkTokenizer *p = (CjkTokenizer*)pTok;
  Ctx ctx = { .pCtx = pCtx, .xToken = xToken };
  int i = 0, segStart = 0, rc = SQLITE_OK;
  int inCJK = 0;
  unsigned int c;
  while( i <= nText && rc == SQLITE_OK ){
    int len = 0, cjk = 0;
    if( i < nText ){
      len = utf8Decode((const unsigned char*)pText + i, nText - i, &c);
      cjk = isCJK(c);
    }
    if( i == nText || cjk != inCJK ){
      if( i > segStart ){
        if( inCJK ){
          rc = emitBigrams(&ctx, pText, segStart, i, p->unigram, flags & FTS5_TOKENIZE_QUERY);
        }else{
          ctx.base = segStart;
          rc = p->parent.xTokenize(p->pParent, &ctx, flags, pText + segStart, i - segStart, parentCb);
        }
      }
      segStart = i;
      inCJK = cjk;
    }
    if( i == nText ) break;
    i += len;
  }
  return rc;
}

/* NUL counts as a space so the result never hides text behind a terminator. */
static int spaceLen(const char *z, int n){
  if( *z == 0 || *z == ' ' || (*z >= '\t' && *z <= '\r') ) return 1;
  return n >= 3 && !memcmp(z, "\xE3\x80\x80", 3) ? 3 : 0;
}

/* A lone CJK character becomes a prefix query, which also matches it inside runs. */
static void appendWord(sqlite3_str *s, const char *z, int n){
  unsigned int c;
  int len = utf8Decode((const unsigned char*)z, n, &c);
  sqlite3_str_appendchar(s, 1, '"');
  for(int i = 0; i < n; i++){
    if( z[i] == '"' ) sqlite3_str_appendchar(s, 1, '"');
    sqlite3_str_appendchar(s, 1, z[i]);
  }
  sqlite3_str_appendall(s, len == n && isCJK(c) ? "\"*" : "\"");
}

/* Appends the word whose nc characters start at z + off[k], cut before
** characters i and j (i <= j <= nc; a cut at nc is no cut). */
static void appendPieces(sqlite3_str *s, const char *z, const int *off, int nc, int i, int j){
  int cut[] = { 0, i, j, nc };
  for(int k = 0; k < 3; k++){
    if( cut[k] == cut[k+1] ) continue;
    if( k ) sqlite3_str_appendchar(s, 1, ' ');
    appendWord(s, z + off[cut[k]], off[cut[k+1]] - off[cut[k]]);
  }
}

static sqlite3_int64 hits(sqlite3_stmt *pStmt, const char *z, const int *off, int nc, int i, int j, int *pRc){
  sqlite3_str *s = sqlite3_str_new(0);
  appendPieces(s, z, off, nc, i, j);
  char *q = sqlite3_str_finish(s);
  if( !q ){ *pRc = SQLITE_NOMEM; return 0; }
  sqlite3_bind_text(pStmt, 1, q, -1, sqlite3_free);
  sqlite3_int64 n = sqlite3_step(pStmt) == SQLITE_ROW ? sqlite3_column_int64(pStmt, 0) : 0;
  *pRc = sqlite3_reset(pStmt);
  return n;
}

/* cjk_query(text [, table]): turns search-box text into a MATCH string that
** cannot be a syntax error. With a table, a CJK word of up to 32 characters
** that has no hits as one phrase is split where the split has the most hits
** (two pieces, then three for up to 12 characters), so 南投魚池 becomes
** "南投" "魚池" when only the separate words occur. */
static void cjkQueryFunc(sqlite3_context *pCtx, int argc, sqlite3_value **argv){
  const char *z = (const char*)sqlite3_value_text(argv[0]);
  int n = sqlite3_value_bytes(argv[0]), rc = SQLITE_OK;
  if( !z ) return;
  sqlite3 *db = sqlite3_context_db_handle(pCtx);
  sqlite3_stmt *pStmt = 0;
  if( argc == 2 ){
    const char *zTab = (const char*)sqlite3_value_text(argv[1]);
    char *zSql = sqlite3_mprintf("SELECT count(*) FROM \"%w\" WHERE \"%w\" MATCH ?1", zTab, zTab);
    rc = zSql ? sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0) : SQLITE_NOMEM;
    sqlite3_free(zSql);
  }
  int *off = sqlite3_malloc64(((sqlite3_uint64)n + 1) * sizeof(*off));
  if( !off && rc == SQLITE_OK ) rc = SQLITE_NOMEM;
  sqlite3_str *out = sqlite3_str_new(db);
  int budget = 256;
  for(int i = 0; i < n && rc == SQLITE_OK; ){
    if( spaceLen(z + i, n - i) ){ i += spaceLen(z + i, n - i); continue; }
    int nc = 0, cjk = 1;
    while( i < n && !spaceLen(z + i, n - i) ){
      unsigned int c;
      off[nc++] = i;
      i += utf8Decode((const unsigned char*)z + i, n - i, &c);
      cjk &= isCJK(c);
    }
    off[nc] = i;
    int bi = nc, bj = nc;
    /* Every probe is a MATCH query, so a pasted wall of text must not fan out:
    ** a word is searched only if its worst case fits the call's probe budget. */
    int cost = 1 + (nc - 1) + (nc <= 12 ? (nc - 1)*(nc - 2)/2 : 0);
    int search = pStmt && cjk && nc > 1 && nc <= 32 && cost <= budget;
    if( search ) budget -= cost;
    if( search && hits(pStmt, z, off, nc, nc, nc, &rc) == 0 ){
      sqlite3_int64 best = 0, h;
      for(int a = 1; a < nc && rc == SQLITE_OK; a++){
        if( (h = hits(pStmt, z, off, nc, a, nc, &rc)) > best ){ best = h; bi = a; }
      }
      for(int a = 1; !best && nc <= 12 && a < nc; a++){
        for(int b = a + 1; b < nc && rc == SQLITE_OK; b++){
          if( (h = hits(pStmt, z, off, nc, a, b, &rc)) > best ){ best = h; bi = a; bj = b; }
        }
      }
    }
    if( sqlite3_str_length(out) ) sqlite3_str_appendchar(out, 1, ' ');
    appendPieces(out, z, off, nc, bi, bj);
  }
  /* MATCH '' is a syntax error; the empty phrase matches nothing. */
  if( !sqlite3_str_length(out) ) sqlite3_str_appendall(out, "\"\"");
  if( rc != SQLITE_OK ){
    char *zErr = sqlite3_mprintf("cjk_query: %s", rc == SQLITE_NOMEM ? sqlite3_errstr(rc) : sqlite3_errmsg(db));
    sqlite3_result_error(pCtx, zErr, -1);
    sqlite3_free(zErr);
  }else if( sqlite3_str_errcode(out) ){
    sqlite3_result_error_nomem(pCtx);
  }else{
    sqlite3_result_text(pCtx, sqlite3_str_value(out), sqlite3_str_length(out), SQLITE_TRANSIENT);
  }
  sqlite3_free(sqlite3_str_finish(out));
  sqlite3_finalize(pStmt);
  sqlite3_free(off);
}

static fts5_api *fts5ApiFromDb(sqlite3 *db){
  fts5_api *pApi = 0;
  sqlite3_stmt *pStmt = 0;
  if( sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &pStmt, 0) == SQLITE_OK ){
    sqlite3_bind_pointer(pStmt, 1, (void*)&pApi, "fts5_api_ptr", 0);
    sqlite3_step(pStmt);
  }
  sqlite3_finalize(pStmt);
  return pApi;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_cjk_init(sqlite3 *db, char **pzErr, const sqlite3_api_routines *pApiRoutines){
  SQLITE_EXTENSION_INIT2(pApiRoutines);
#ifdef CJK_BUSY_TIMEOUT_MS
  /* Static builds only: a driver that sets its lock policy after auto-extensions
  ** run would otherwise see a concurrent writer as "fts5 not available". */
  sqlite3_busy_timeout(db, CJK_BUSY_TIMEOUT_MS);
#endif
  fts5_api *pApi = fts5ApiFromDb(db);
  if( !pApi ){
    *pzErr = sqlite3_mprintf("cjk: fts5 not available");
    return SQLITE_ERROR;
  }
  static fts5_tokenizer tok = { cjkCreate, cjkDelete, cjkTokenize };
  int rc = pApi->xCreateTokenizer(pApi, "cjk", (void*)pApi, &tok, 0);
  if( rc == SQLITE_OK ) rc = sqlite3_create_function(db, "cjk_query", 1,
      SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS, 0, cjkQueryFunc, 0, 0);
  /* The two-argument form reads a table named by its caller, so keep it out of
  ** schema-defined triggers and views. */
  if( rc == SQLITE_OK ) rc = sqlite3_create_function(db, "cjk_query", 2,
      SQLITE_UTF8 | SQLITE_DIRECTONLY, 0, cjkQueryFunc, 0, 0);
  return rc;
}
