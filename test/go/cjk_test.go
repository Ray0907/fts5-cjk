package fts5cjktest

import "testing"

func TestAutoExtension(t *testing.T) {
	setup := `
		CREATE VIRTUAL TABLE t USING fts5(body, tokenize='cjk');
		INSERT INTO t(rowid, body) VALUES (1, '日月潭紅茶產於南投縣魚池鄉');
	`

	gotHighlight, err := queryOne(setup, `SELECT highlight(t, 0, '[', ']') FROM t WHERE t MATCH cjk_query('南投魚池', 't')`)
	if err != nil {
		t.Fatalf("highlight query failed: %v", err)
	}
	wantHighlight := "日月潭紅茶產於[南投]縣[魚池]鄉"
	if gotHighlight != wantHighlight {
		t.Fatalf("highlight = %q, want %q", gotHighlight, wantHighlight)
	}

	gotRankExpr, err := queryOne("", `SELECT cjk_rank_query('魚池鄉的紅茶')`)
	if err != nil {
		t.Fatalf("cjk_rank_query failed: %v", err)
	}
	wantRankExpr := `"魚" OR "魚池" OR "池鄉" OR "鄉的" OR "的紅" OR "紅茶"`
	if gotRankExpr != wantRankExpr {
		t.Fatalf("cjk_rank_query = %q, want %q", gotRankExpr, wantRankExpr)
	}

	gotRowID, err := queryOne(setup, `SELECT rowid FROM t WHERE t MATCH cjk_rank_query('魚池鄉的紅茶') ORDER BY rank`)
	if err != nil {
		t.Fatalf("rank MATCH failed: %v", err)
	}
	if gotRowID != "1" {
		t.Fatalf("rowid = %q, want %q", gotRowID, "1")
	}
}
