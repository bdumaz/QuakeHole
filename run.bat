@echo off
rem Launches QuakeArch using your own Quake pak files.
rem Extra arguments are passed to the engine, e.g.  run.bat -window -width 1280 -height 720
rem
rem Where the paks are looked for, in order:
rem   1. the folder named in quakepath.txt (next to this file), if that file exists
rem   2. the "Quake paks" folder next to this file
rem   3. the default Steam install
rem A folder may hold pak0.pak and pak1.pak directly, or in an id1 subfolder.
setlocal DisableDelayedExpansion

set "ROOT=%~dp0"
set "EXE=%ROOT%engine\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Release\quakespasm-sdl2.exe"
set "STEAM=C:\Program Files (x86)\Steam\steamapps\common\Quake"

set "QUAKE=%STEAM%"
if exist "%ROOT%Quake paks\pak0.pak" set "QUAKE=%ROOT%Quake paks"
if exist "%ROOT%Quake paks\id1\pak0.pak" set "QUAKE=%ROOT%Quake paks"
if exist "%ROOT%quakepath.txt" set /p QUAKE=<"%ROOT%quakepath.txt"
rem tolerate quotes and a trailing backslash in quakepath.txt
set "QUAKE=%QUAKE:"=%"
if "%QUAKE:~-1%"=="\" set "QUAKE=%QUAKE:~0,-1%"

set "PAKS=%QUAKE%\id1"
if not exist "%PAKS%\pak0.pak" set "PAKS=%QUAKE%"

if not exist "%EXE%" goto :noexe
if not exist "%PAKS%\pak0.pak" goto :noquake
if not exist "%PAKS%\pak1.pak" echo Warning: pak1.pak not found in "%PAKS%". The full game is needed; some weapon models will be missing.

rem An empty config.cfg here stops the engine falling back to the one in Quake's id1 folder.
if not exist "%ROOT%game\arch\config.cfg" echo // QuakeArch settings>"%ROOT%game\arch\config.cfg"

rem The game opens on the main menu. Commands come first: the engine only keeps the first 256 characters for + commands.
"%EXE%" +menu_main %* -basedir "%QUAKE%" -archdir "%ROOT%game\arch"
exit /b %errorlevel%

:noexe
echo The engine has not been built yet. Run tools\build.bat first.
exit /b 1

:noquake
echo Could not find pak0.pak in:
echo   "%QUAKE%"
echo Put your Quake pak0.pak and pak1.pak in the "Quake paks" folder next to run.bat,
echo or create quakepath.txt containing the path to your Quake folder.
exit /b 1
