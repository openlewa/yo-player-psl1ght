# Ren'Py to PS3 — Go port (alternative)

Alternative to the original C# / WPF program in `tools/renpy-to-ps3`. That program is unchanged and still builds with Visual Studio or MSBuild. This folder is a separate Go port for when you do not want those tools, including on Windows 7.

It does not call the C# program. The UI is a local page at http://127.0.0.1:8765/ (`renpy-to-ps3 ui`), with the same tasks and log pane on Windows 7 and Windows 11.

Licensed under the Apache License, Version 2.0, January 2004. See `LICENSE`. Copyright openlewa.

## Why this does not call the C# app

`Program.Run` is an internal method. The project starts `RenpyToPs3.App`, which always opens `MainWindow.xaml`. There is no command-line entry point, and this repo does not ship a built `renpy-to-ps3.exe`.

This app is a separate Go program. Build it with the Go toolchain only.

## What you need

- [Go 1.20.14](https://go.dev/dl/go1.20.14.windows-amd64.msi) on Windows 7. Windows 11 can use 1.20.14 or any newer Go.
- A browser (Windows 7: Chrome 109 or Firefox 115 ESR).
- ffmpeg. `tools/renpy-to-ps3/ffmpeg.exe` is already in the repo; the start script copies it next to the Go binary.

No Node, Rust, WebView2, or Visual Studio.

## Start on Windows 7 and Windows 11

Double-click:

```bat
tools\renpy-to-ps3\app(go_port)\start-windows7.bat
```

Or:

```bat
cd /d "path\to\yo-player-psl1ght\tools\renpy-to-ps3\app(go_port)"
start-windows7.bat
```

The script builds `app(go_port)\go\renpy-to-ps3.exe`, copies `ffmpeg.exe` beside it, and opens the page. Close the console window to quit.

If Go is missing, the script downloads the Go 1.20.14 installer and starts it. Finish that installer, then run `start-windows7.bat` again. If the download fails, install Go from https://go.dev/dl/go1.20.14.windows-amd64.msi and run the script again.

Same thing by hand in **PowerShell** (including PowerShell 7):

```powershell
Set-Location "path\to\yo-player-psl1ght\tools\renpy-to-ps3\app(go_port)\go"
go build -o renpy-to-ps3.exe .\cmd\renpy-to-ps3
Copy-Item -Force ..\..\ffmpeg.exe .\ffmpeg.exe
.\renpy-to-ps3.exe ui
```

In **Command Prompt**:

```bat
cd /d "path\to\yo-player-psl1ght\tools\renpy-to-ps3\app(go_port)\go"
go build -o renpy-to-ps3.exe .\cmd\renpy-to-ps3
copy /Y ..\..\ffmpeg.exe .\ffmpeg.exe
.\renpy-to-ps3.exe ui
```

`cd /d` and `copy /Y` are Command Prompt only. PowerShell does not accept them. PowerShell also will not run `renpy-to-ps3.exe` from the current folder unless you write `.\renpy-to-ps3.exe`.

A binary built with Go 1.21 or newer will not start on Windows 7. Build with Go 1.20.14 if the exe has to run in that VM.

`game` in the UI is the Ren'Py **game** folder (the one with `.rpyc` / `.rpa` files).

## Start on Linux / macOS

```bash
cd "tools/renpy-to-ps3/app(go_port)/go"
go test ./renpy/
go run ./cmd/renpy-to-ps3 ui
```

## Commands

The page sends these to the same binary. You can also run them directly:

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
