# Changelog

## [v0.4.0](https://github.com/Ray0907/fts5-cjk/compare/v0.3.0...v0.4.0) - 2026-11-26

No index rebuild needed when upgrading from `v0.3.0`.

### Added
- `cjk_rank_query(text [, mode])`: builds a quoted `OR` query of up to 128 distinct terms (`'first'` by default, or `'edge'`) for BM25 ranking on default `cjk` tables without changing the index.
- Isolated Go cgo test (`make go-test` in `test/go/`) and CI check exercising `import _ "github.com/Ray0907/fts5-cjk"`, while keeping the root `go.mod` dependency-free.

### Fixed
- `cjk_query()` now composes NFD voiced kana, half-width voiced katakana, and Hangul jamo before counting characters and splitting words, so single composed characters get `*` and multi-syllable NFD words split at syllable boundaries.

### Changed
- `cjk_query(text, table)` checks whole-word existence with `LIMIT 1` and prepares `count(*)` only when splitting is needed (~2.5× faster across the 400-query Taiwan law benchmark).
- `cjk_rank_query()` reuses a connection-scoped tokenizer with a stack context (~1.3× faster), `fold()` replaces `bsearch()` with a range-guarded `switch`, and hot-path buffers in `emitBigrams()`, `parentCb()` and `cjkQueryFunc()` use stack fast-paths.
- Updated all four READMEs (`en`, `zh-TW`, `ja`, `ko`) with `cjk_rank_query()` documentation, deployed index-size comparisons against application-level bigrams, and corrected `sqlite-better-trigram` details from [cjk-fts-bench](https://github.com/Ray0907/cjk-fts-bench).

## [v0.3.0](https://github.com/Ray0907/fts5-cjk/compare/v0.2.0...v0.3.0) - 2026-11-11

- Added `go.mod` and `cjk.go` (`import _ "github.com/Ray0907/fts5-cjk"`) to statically link `cjk` and `cjk_query()` into cgo SQLite drivers.
- Moved vendored SQLite headers from `vendor/sqlite/` to `include/` so Go module zips retain them.
- Added opt-in `CJK_BUSY_TIMEOUT_MS` for static builds whose driver configures busy timeouts after auto-extensions run.

## [v0.2.0](https://github.com/Ray0907/fts5-cjk/releases/tag/v0.2.0) - 2026-11-11

- Added `cjk_query(text [, table])` to turn raw search-box input into a safe `MATCH` string and split no-space CJK compounds using the index.
- Indexed the first and last character of each CJK run in default `unigram 0` tables so mixed Latin/digit tokens (`5月`, `第38條`) and `茶*` prefix queries match every occurrence.
- Composed NFD kana voiced marks and Hangul jamo at index and query time; treated `々〆〇` as CJK characters.
- Added static linking support (`-DSQLITE_CORE`), ASan/UBSan fuzzing (`make asan-test`), Windows builds, and GitHub Actions release binaries.
