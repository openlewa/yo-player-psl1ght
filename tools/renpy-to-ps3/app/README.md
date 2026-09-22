# Ren'Py to PS3 (Tauri app)

Desktop UI for `tools/renpy-to-ps3` that does **not** need Visual Studio or MSBuild.
The existing C# / WPF project is unchanged; this folder is a standalone Go converter
plus a thin TypeScript/Tauri shell around it.

## What you get

- **Go CLI** (`app/go`) — same commands as the original tool (`pack`, `info`, `compile`, `list`, `extract`, `rpk`, …)
- **Tauri window** (`app/ui`) — same tasks, paths, pack options, and log pane as the WPF app

`ffmpeg` is required for converting assets (`pack`). Put `ffmpeg` / `ffmpeg.exe` next to the binary, or on `PATH`. The original Windows `ffmpeg.exe` in `tools/renpy-to-ps3/` is used automatically if you run from a nearby folder.

## Prerequisites

- [Go](https://go.dev/) 1.20+ (Windows 7: **1.20.14** only — see below)
- [Rust](https://rustup.rs/) (desktop app only)
- Node.js 18+ (desktop app only)
- ffmpeg
- Linux also needs WebKitGTK (see [Tauri Linux prerequisites](https://tauri.app/start/prerequisites/))

## Start on Windows 11

The desktop window is the supported path on Windows 11. Visual Studio and MSBuild are not required.

### One-time installs

1. [Go 1.22 or newer](https://go.dev/dl/) (Windows MSI, amd64)
2. [Node.js 18 or newer LTS](https://nodejs.org/)
3. [Rust](https://rustup.rs/) — run `rustup-init.exe`, then close and reopen the terminal
4. ffmpeg — either install it, or use the `ffmpeg.exe` already in `tools\renpy-to-ps3\`

WebView2 is already part of Windows 11, so you do not install it separately.

Open **Command Prompt** or **PowerShell**.

### Desktop app

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app\ui
npm install
npm run tauri dev
```

A window titled **Ren'Py to PS3** opens. Pick a task, browse or type paths, then **Run**. Logs stream in the pane at the bottom.

Release installer (NSIS / MSI under `src-tauri\target\release\bundle\`):

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app\ui
npm install
npm run tauri build
```

The Go sidecar is built automatically (`scripts\build-sidecar.mjs`) before `tauri dev` / `tauri build`.

### CLI only (no UI)

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app\go
go test .\renpy\
go build -o renpy-to-ps3.exe .\cmd\renpy-to-ps3
renpy-to-ps3.exe pack C:\Games\MyNovel\game C:\out.rpk
```

If `ffmpeg` is not on `PATH`, copy `tools\renpy-to-ps3\ffmpeg.exe` next to `renpy-to-ps3.exe`, or pass `--ffmpeg C:\path\to\ffmpeg.exe`.

## Start on Windows 7

Windows 7 SP1 is supported through the **Go CLI + local web UI**. The Tauri window is Windows 10/11 only (Node, Rust, and current WebView2 dropped Windows 7). Visual Studio and MSBuild are not required.

### One-time installs

1. [Go 1.20.14](https://go.dev/dl/go1.20.14.windows-amd64.msi) — last Go that still targets Windows 7. Do **not** install 1.21+.
2. A browser that still runs on Windows 7 (Chrome 109 or Firefox 115 ESR).
3. ffmpeg — `tools\renpy-to-ps3\ffmpeg.exe` is already in this repo.

You do **not** need Node, Rust, WebView2, or Visual Studio on Windows 7.

### Desktop-style UI (recommended)

Double-click `tools\renpy-to-ps3\app\start-windows7.bat`, or in Command Prompt:

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app
start-windows7.bat
```

That builds `renpy-to-ps3.exe` (if needed), copies `ffmpeg.exe` beside it, and opens **http://127.0.0.1:8765/** in your browser. The page is the same task list / log pane as the WPF and Tauri apps. Close the console window to quit.

Or:

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app\go
go build -o renpy-to-ps3.exe .\cmd\renpy-to-ps3
copy /Y ..\..\ffmpeg.exe .\ffmpeg.exe
renpy-to-ps3.exe ui
```

### CLI only

```bat
cd /d path\to\yo-player-psl1ght\tools\renpy-to-ps3\app\go
renpy-to-ps3.exe pack C:\Games\MyNovel\game C:\out.rpk
```

`game` must be the Ren'Py **game** folder (the one that contains `.rpyc` / `.rpa` files), not the project root.

A `.exe` built with Go 1.22+ will not start on Windows 7. If you build on Windows 11 for the VM, install Go 1.20.14 there and copy `renpy-to-ps3.exe` plus `ffmpeg.exe`.

## Start on Linux / macOS

Desktop app:

```bash
cd tools/renpy-to-ps3/app/ui
npm install
npm run tauri dev
```

CLI only:

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
ui [--port N]
```
