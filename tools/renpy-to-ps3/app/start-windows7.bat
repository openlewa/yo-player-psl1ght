@echo off
setlocal
cd /d "%~dp0go"

where go >nul 2>&1
if errorlevel 1 (
  echo Install Go 1.20.14 ^(Windows 7 needs this exact series^):
  echo   https://go.dev/dl/go1.20.14.windows-amd64.msi
  exit /b 1
)

echo Building renpy-to-ps3.exe ...
go build -o renpy-to-ps3.exe .\cmd\renpy-to-ps3
if errorlevel 1 exit /b 1

if exist ..\..\ffmpeg.exe copy /Y ..\..\ffmpeg.exe .\ffmpeg.exe >nul

echo.
echo Starting local UI at http://127.0.0.1:8765/
echo Close this window to quit.
echo.
renpy-to-ps3.exe ui
