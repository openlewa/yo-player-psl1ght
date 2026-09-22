package renpy

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestUIIndex(t *testing.T) {
	ts := httptest.NewServer(newUIMux())
	defer ts.Close()
	res, err := http.Get(ts.URL + "/")
	if err != nil {
		t.Fatal(err)
	}
	defer res.Body.Close()
	b, _ := io.ReadAll(res.Body)
	if res.StatusCode != 200 || !strings.Contains(string(b), "Ren'Py to PS3") {
		t.Fatalf("status=%d body=%s", res.StatusCode, b)
	}
}

func TestUIRunInfoAndReject(t *testing.T) {
	ts := httptest.NewServer(newUIMux())
	defer ts.Close()

	bad, err := http.Post(ts.URL+"/api/run", "application/json", strings.NewReader(`{"args":["ui"]}`))
	if err != nil {
		t.Fatal(err)
	}
	bad.Body.Close()
	if bad.StatusCode != http.StatusBadRequest {
		t.Fatalf("ui recursion should be rejected, got %d", bad.StatusCode)
	}

	dir := t.TempDir()
	writeRpyc(t, filepath.Join(dir, "script.rpyc"), "Eileen", "Hi")
	body := `{"args":["info",` + jsonQuote(dir) + `]}`
	res, err := http.Post(ts.URL+"/api/run", "application/json", strings.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	defer res.Body.Close()
	out, _ := io.ReadAll(res.Body)
	s := string(out)
	if !strings.Contains(s, "renpy.ast.Label") || !strings.Contains(s, "__EXIT__ 0") {
		t.Fatalf("info via ui: %s", s)
	}
}

func TestUIFs(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "a.txt"), []byte("x"), 0644); err != nil {
		t.Fatal(err)
	}
	ts := httptest.NewServer(newUIMux())
	defer ts.Close()
	res, err := http.Get(ts.URL + "/api/fs?path=" + url.QueryEscape(dir))
	if err != nil {
		t.Fatal(err)
	}
	defer res.Body.Close()
	var listing fsListing
	if err := json.NewDecoder(res.Body).Decode(&listing); err != nil {
		t.Fatal(err)
	}
	found := false
	for _, e := range listing.Entries {
		if e.Name == "a.txt" && !e.IsDir {
			found = true
		}
	}
	if !found {
		t.Fatalf("listing %#v", listing)
	}
}

func jsonQuote(s string) string {
	b, _ := json.Marshal(s)
	return string(b)
}
