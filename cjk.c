/*
** cjk: an SQLite FTS5 tokenizer for Chinese / Japanese / Korean text.
**
** CJK runs become overlapping character bigrams (the Lucene CJKAnalyzer /
** Elasticsearch "cjk" approach); everything else is handed to unicode61, so
** Latin text keeps case folding, diacritic removal and the usual options.
** No dictionary, no dependencies, ~200 lines.
**
**   CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
**   CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk remove_diacritics 2');
**
** A query bareword that tokenizes into several bigrams is matched by FTS5 as a
** phrase, so  MATCH '魚池鄉'  requires 魚池 and 池鄉 to be adjacent, i.e. it is a
** substring match. Space-separated words are ANDed as usual.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <string.h>
#include <stdlib.h>

typedef struct CjkTokenizer {
  fts5_tokenizer parent;   /* unicode61 */
  Fts5Tokenizer *pParent;
} CjkTokenizer;

typedef int (*xTokenFn)(void*, int, const char*, int, int, int);

typedef struct Ctx {
  void *pCtx;
  xTokenFn xToken;
  int base;                /* byte offset of the segment passed to the parent */
} Ctx;

/* Han, Hiragana, Katakana, Hangul. CJK punctuation (U+3000-303F) is not included
** so unicode61 treats it as a separator. */
static int isCJK(unsigned int c){
  return (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF)
      || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0x20000 && c <= 0x3134F)
      || (c >= 0x3040 && c <= 0x30FF) || (c >= 0x31F0 && c <= 0x31FF)
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
  return ctx->xToken(ctx->pCtx, tflags, pTok, nTok, ctx->base + iStart, ctx->base + iEnd);
}

static int cjkCreate(void *pUser, const char **azArg, int nArg, Fts5Tokenizer **ppOut){
  fts5_api *pApi = (fts5_api*)pUser;
  CjkTokenizer *p = sqlite3_malloc(sizeof(*p));
  if( !p ) return SQLITE_NOMEM;
  memset(p, 0, sizeof(*p));
  void *pParentUser = 0;
  int rc = pApi->xFindTokenizer(pApi, "unicode61", &pParentUser, &p->parent);
  if( rc == SQLITE_OK ) rc = p->parent.xCreate(pParentUser, azArg, nArg, &p->pParent);
  if( rc != SQLITE_OK ){ sqlite3_free(p); return rc; }
  *ppOut = (Fts5Tokenizer*)p;
  return SQLITE_OK;
}

static void cjkDelete(Fts5Tokenizer *pTok){
  CjkTokenizer *p = (CjkTokenizer*)pTok;
  if( p->pParent ) p->parent.xDelete(p->pParent);
  sqlite3_free(p);
}

/* Emit bigrams for the CJK run text[iStart..iEnd). */
static int emitBigrams(Ctx *ctx, const char *text, int iStart, int iEnd){
  int i = iStart, rc = SQLITE_OK, count = 0;
  unsigned int c;
  int prev = -1;             /* start of previous rune */
  while( i < iEnd && rc == SQLITE_OK ){
    int len = utf8Decode((const unsigned char*)text + i, iEnd - i, &c);
    if( prev >= 0 ){
      rc = ctx->xToken(ctx->pCtx, 0, text + prev, i + len - prev, prev, i + len);
      count++;
    }
    prev = i;
    i += len;
  }
  if( rc == SQLITE_OK && count == 0 && prev >= 0 ){   /* lone character */
    rc = ctx->xToken(ctx->pCtx, 0, text + prev, iEnd - prev, prev, iEnd);
  }
  return rc;
}

static int cjkTokenize(Fts5Tokenizer *pTok, void *pCtx, int flags, const char *pText, int nText, xTokenFn xToken){
  CjkTokenizer *p = (CjkTokenizer*)pTok;
  Ctx ctx = { pCtx, xToken, 0 };
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
          rc = emitBigrams(&ctx, pText, segStart, i);
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
  fts5_api *pApi = fts5ApiFromDb(db);
  if( !pApi ){
    *pzErr = sqlite3_mprintf("cjk: fts5 not available");
    return SQLITE_ERROR;
  }
  static fts5_tokenizer tok = { cjkCreate, cjkDelete, cjkTokenize };
  return pApi->xCreateTokenizer(pApi, "cjk", (void*)pApi, &tok, 0);
}
