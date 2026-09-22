package renpy

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func writeRpyc(t *testing.T, path, who, what string) {
	t.Helper()
	py := fmt.Sprintf(`
import pickle, zlib, types, sys
renpy = types.ModuleType("renpy")
ast = types.ModuleType("renpy.ast")
renpy.ast = ast
sys.modules["renpy"] = renpy
sys.modules["renpy.ast"] = ast
def make(name):
    cls = type(name, (), {})
    cls.__module__ = "renpy.ast"
    setattr(ast, name, cls)
    return cls
Say, Label = make("Say"), make("Label")
class N:
    def __init__(self, cls, state):
        self.cls = cls; self.state = state
    def __reduce__(self):
        return (self.cls, (), self.state)
script = N(Label, (None, {"name": "start", "block": [N(Say, (None, {"who": %q, "what": %q}))]}))
raw = pickle.dumps((1, "k", [script]), protocol=2)
open(%q, "wb").write(zlib.compress(raw))
`, who, what, path)
	out, err := exec.Command("python3", "-c", py).CombinedOutput()
	if err != nil {
		t.Fatalf("python rpyc: %v\n%s", err, out)
	}
}

func TestPickleIntAndList(t *testing.T) {
	// PROTO 2, BININT1 42, STOP
	v, err := LoadPickle([]byte{0x80, 0x02, 'K', 42, '.'})
	if err != nil {
		t.Fatal(err)
	}
	if n, ok := v.(int); !ok || n != 42 {
		t.Fatalf("got %#v", v)
	}

	// empty list: PROTO2 EMPTY_LIST STOP
	v, err = LoadPickle([]byte{0x80, 0x02, ']', '.'})
	if err != nil {
		t.Fatal(err)
	}
	if l, ok := v.(List); !ok || len(l) != 0 {
		t.Fatalf("empty list: %#v", v)
	}
}

func TestPicklePythonRoundtrip(t *testing.T) {
	py := `import pickle, sys
sys.stdout.buffer.write(pickle.dumps({"hello": [1, 2, "x"], "t": (True, False, None)}, protocol=2))
`
	out, err := exec.Command("python3", "-c", py).Output()
	if err != nil {
		t.Skip("python3 pickle:", err)
	}
	v, err := LoadPickle(out)
	if err != nil {
		t.Fatal(err)
	}
	d := asDict(v)
	if d == nil {
		t.Fatalf("want dict, got %T", v)
	}
	hello, ok := d.Get("hello")
	if !ok {
		t.Fatal("missing hello")
	}
	seq := asSeq(hello)
	if len(seq) != 3 {
		t.Fatalf("hello list %#v", hello)
	}
}

func TestPickleRenpyAst(t *testing.T) {
	py := `
import pickle, sys, types
renpy = types.ModuleType("renpy")
ast = types.ModuleType("renpy.ast")
renpy.ast = ast
sys.modules["renpy"] = renpy
sys.modules["renpy.ast"] = ast
Say = type("Say", (), {})
Say.__module__ = "renpy.ast"
ast.Say = Say
class Inst:
    def __reduce__(self):
        return (Say, (), (None, {"who": "Eileen", "what": "Hello"}))
sys.stdout.buffer.write(pickle.dumps((1, "key", [Inst()]), protocol=2))
`
	out, err := exec.Command("python3", "-c", py).Output()
	if err != nil {
		t.Skip(err)
	}
	v, err := LoadPickle(out)
	if err != nil {
		t.Fatal(err)
	}
	stmts := FindStatementList(v)
	if stmts == nil {
		t.Fatalf("no statements in %#v", v)
	}
	nodes := ToNodes(stmts)
	if len(nodes) != 1 || nodes[0].Short() != "Say" {
		t.Fatalf("nodes=%v", nodes)
	}
	if nodes[0].Text("who") != "Eileen" || nodes[0].Text("what") != "Hello" {
		t.Fatalf("say fields %#v %#v", nodes[0].Raw("who"), nodes[0].Raw("what"))
	}
}

func TestExprCompiler(t *testing.T) {
	p := NewIrProgram()
	ep := CompileExpr("a + 1 and True", p)
	if ep == nil {
		t.Fatal("compile failed")
	}
	if CompileExpr("foo(bar)", p) != nil {
		t.Fatal("calls should be unsupported")
	}
	if CompileExpr("x ** 2", p) != nil {
		t.Fatal("** should be unsupported")
	}
	if CompileExpr("max(1, 2, 3)", p) == nil {
		t.Fatal("max() should compile")
	}
}

func TestRpkRoundtrip(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "t.rpk")
	entries := []RpkEntry{
		{Name: "manifest", Data: []byte("hello")},
		{Name: "a/b.png", Data: []byte{1, 2, 3}},
	}
	if err := WriteRpk(path, entries); err != nil {
		t.Fatal(err)
	}
	toc, err := ReadRpkToc(path)
	if err != nil {
		t.Fatal(err)
	}
	if len(toc) != 2 || toc[0].Name != "manifest" || toc[0].Length != 5 {
		t.Fatalf("%#v", toc)
	}
}

func TestBytecodeRoundtrip(t *testing.T) {
	p := NewIrProgram()
	p.SetLabel("start", 0)
	p.Emit(NewInstr(IrLabel, p.Intern("start")))
	p.Emit(NewInstr(IrSay, p.Intern("Eileen"), p.Intern("Hi")))
	p.Emit(NewInstr(IrEnd))
	b := WriteBytecode(p)
	d, err := ReadBytecode(b)
	if err != nil {
		t.Fatal(err)
	}
	if len(d.Code) != 3 || d.Strings[0] != "start" {
		t.Fatalf("%#v %#v", d.Code, d.Strings)
	}
	if d.Labels["start"] != 0 {
		t.Fatal(d.Labels)
	}
}

func TestCompileSayAndJump(t *testing.T) {
	node := func(cls string, attrs map[string]any) *PyObject {
		d := NewDict()
		for k, v := range attrs {
			d.Set(k, v)
		}
		return &PyObject{ClassName: "renpy.ast." + cls, State: Tuple{nil, d}}
	}
	start := node("Label", map[string]any{
		"name":  "start",
		"block": List{node("Say", map[string]any{"who": "e", "what": "hi"}), node("Jump", map[string]any{"target": "start"})},
	})
	prog := Compile([]any{start})
	if _, ok := prog.Labels["start"]; !ok {
		t.Fatal("missing start")
	}
	foundSay := false
	for _, ins := range prog.Code {
		if ins.Op == IrSay {
			foundSay = true
			if prog.Str(ins.B) != "hi" {
				t.Fatalf("say text %q", prog.Str(ins.B))
			}
		}
	}
	if !foundSay {
		t.Fatal("no say")
	}
}

func TestRunUsage(t *testing.T) {
	if Run(nil) != 0 {
		t.Fatal("empty args")
	}
	if Run([]string{"nope"}) != 1 {
		t.Fatal("unknown")
	}
}

func TestPackTinyGame(t *testing.T) {
	ff, err := NewFfmpeg("")
	if err != nil {
		t.Skip("ffmpeg:", err)
	}
	dir := t.TempDir()
	game := filepath.Join(dir, "MyGame", "game")
	if err := os.MkdirAll(game, 0755); err != nil {
		t.Fatal(err)
	}
	pngPath := filepath.Join(game, "bg.png")
	if out, err := exec.Command("ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=red:s=8x8", "-frames:v", "1", pngPath).CombinedOutput(); err != nil {
		t.Fatalf("ffmpeg png: %v\n%s", err, out)
	}
	writeRpyc(t, filepath.Join(game, "script.rpyc"), "Eileen", "Hello")
	outRpk := filepath.Join(dir, "out.rpk")
	rc := Pack(game, outRpk, ff, 1920, false, false, false)
	if rc != 0 {
		t.Fatalf("pack rc=%d", rc)
	}
	toc, err := ReadRpkToc(outRpk)
	if err != nil {
		t.Fatal(err)
	}
	if len(toc) < 3 {
		t.Fatalf("toc %#v", toc)
	}
}

func TestCLIInfoAndCompile(t *testing.T) {
	dir := t.TempDir()
	writeRpyc(t, filepath.Join(dir, "script.rpyc"), "Eileen", "Hi")
	if Run([]string{"info", dir}) != 0 {
		t.Fatal("info")
	}
	outRbc := filepath.Join(dir, "g.rbc")
	if Run([]string{"compile", dir, outRbc}) != 0 {
		t.Fatal("compile")
	}
	if _, err := os.Stat(outRbc); err != nil {
		t.Fatal(err)
	}
}

func TestPutAssetCaseFold(t *testing.T) {
	m := map[string][]byte{}
	putAsset(m, "Eileen.png", []byte{1})
	putAsset(m, "eileen.png", []byte{2})
	if len(m) != 1 {
		t.Fatalf("want 1 key, got %#v", m)
	}
	if got := m["Eileen.png"]; len(got) != 1 || got[0] != 2 {
		t.Fatalf("last write should win with first casing: %#v", m)
	}
}

func TestScanClassesNullMemoDoesNotShift(t *testing.T) {
	// PROTO2, "renpy", "ast", BININT1 0, MEMOIZE, BINGET 0, STACK_GLOBAL, STOP
	b := []byte{0x80, 0x02, 0x8c, 5}
	b = append(b, "renpy"...)
	b = append(b, 0x8c, 3)
	b = append(b, "ast"...)
	b = append(b, 'K', 0, 0x94, 'h', 0, 0x93, '.')
	classes := ScanClasses(b)
	if _, ok := classes["renpy.ast"]; !ok {
		t.Fatalf("missing renpy.ast in %v", classSetKeys(classes))
	}
	if _, ok := classes["ast."]; ok {
		t.Fatal("null memo shifted STACK_GLOBAL into ast.")
	}
	if _, ok := classes["ast.ast"]; ok {
		t.Fatal("null memo replayed ast as a second string")
	}
}

func TestImageArgsSingleFrame(t *testing.T) {
	args := imageArgs("doc/_static/ajax-loader.gif", "out.png", "scale=8:8")
	got := strings.Join(args, " ")
	if !strings.Contains(got, "-frames:v 1") || !strings.Contains(got, "-update 1") {
		t.Fatal(args)
	}
	if args[len(args)-1] != "out.png" {
		t.Fatal(args)
	}
}

func TestGifToPng(t *testing.T) {
	ff, err := NewFfmpeg("")
	if err != nil {
		t.Skip("ffmpeg:", err)
	}
	dir := t.TempDir()
	gifPath := filepath.Join(dir, "ajax-loader.gif")
	pngPath := filepath.Join(dir, "out.png")
	if out, err := exec.Command("ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=blue:s=8x8", "-frames:v", "4", gifPath).CombinedOutput(); err != nil {
		t.Fatalf("make gif: %v\n%s", err, out)
	}
	ok, errText := ff.Image(gifPath, pngPath, 1920, 1920)
	if !ok {
		t.Fatal(errText)
	}
	if st, err := os.Stat(pngPath); err != nil || st.Size() == 0 {
		t.Fatal(err, st)
	}
}

func TestScanClassesTruncated(t *testing.T) {
	ScanClasses([]byte{0x80})
	ScanClasses([]byte{0x8c, 50})
}
