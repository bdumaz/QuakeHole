@echo off
rem Launches QuakeArch using your own Quake installation.
rem Extra arguments are passed to the engine, e.g.  run.bat -window -width 1280 -height 720
setlocal DisableDelayedExpansion

set "ROOT=%~dp0"
set "EXE=%ROOT%engine\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Release\quakespasm-sdl2.exe"
set "QUAKE=C:\Program Files (x86)\Steam\steamapps\common\Quake"
if exist "%ROOT%quakepath.txt" set /p QUAKE=<"%ROOT%quakepath.txt"

if not exist "%EXE%" goto :noexe
if not exist "%QUAKE%\id1\pak0.pak" goto :noquake
if not exist "%QUAKE%\id1\pak1.pak" echo Warning: "%QUAKE%\id1\pak1.pak" not found. The full game is needed; some weapon models will be missing.

rem An empty config.cfg here stops the engine falling back to the one in Quake's id1 folder.
if not exist "%ROOT%game\arch\config.cfg" echo // QuakeArch settings>"%ROOT%game\arch\config.cfg"

rem Commands come first: the engine only keeps the first 256 characters for + commands.
"%EXE%" +map arch %* -basedir "%QUAKE%" -archdir "%ROOT%game\arch"
exit /b %errorlevel%

:noexe
echo The engine has not been built yet. Run tools\build.bat first.
exit /b 1

:noquake
echo Could not find Quake's id1\pak0.pak in:
echo   "%QUAKE%"
echo Create quakepath.txt containing the path to your Quake folder.
exit /b 1
