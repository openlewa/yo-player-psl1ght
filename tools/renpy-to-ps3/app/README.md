# Ren'Py to PS3 (Tauri app)

Desktop UI for `tools/renpy-to-ps3` that does **not** need Visual Studio or MSBuild.
The existing C# / WPF project is unchanged; this folder is a standalone Go converter
plus a thin TypeScript/Tauri shell around it.

## What you get

- **Go CLI** (`app/go`) — same commands as the original tool (`pack`, `info`, `compile`, `list`, `extract`, `rpk`, …)
- **Tauri window** (`app/ui`) — same tasks, paths, pack options, and log pane as the WPF app

`ffmpeg` is required for converting assets (`pack`). Put `ffmpeg` / `ffmpeg.exe` next to the binary, or on `PATH`. The original Windows `ffmpeg.exe` in `tools/renpy-to-ps3/` is used automatically if you run from a nearby folder.

## Prerequisites

- [Go](https://go.dev/) 1.22+
- [Rust](https://rustup.rs/) (for Tauri)
- Node.js 18+
- ffmpeg
- Linux also needs WebKitGTK (see [Tauri Linux prerequisites](https://tauri.app/start/prerequisites/))

## Run the desktop app

```bash
cd tools/renpy-to-ps3/app/ui
npm install
npm run tauri dev
```

Release build:

```bash
cd tools/renpy-to-ps3/app/ui
npm install
npm run tauri build
```

The Go sidecar is built automatically (`scripts/build-sidecar.mjs`) before `tauri dev` / `tauri build`.

## CLI only (no UI)

```bash
cd tools/renpy-to-ps3/app/go
go test ./renpy/
go run ./cmd/renpy-to-ps3 pack /path/to/game /tmp/out.rpk
```

Commands match the original tool:

```
list <rpa-file>
extract <rpa-file> <output>
info <game-dir>
compile <rpyc|game-dir> [out.rbc]
pack <game-dir> <out.rpk> [--max <px>] [--ascii-text] [--ffmpeg <path>] [--no-cache] [--clear-cache]
rpk <file>
script <rpyc>
ast <rpyc> [label]
atldump <rpyc>
```
