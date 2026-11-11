@echo off
rem Starts QuakeHole with your own Quake pak files.
rem Extra arguments are passed to the game, e.g.  "Play QuakeHole.bat" -window -width 1280 -height 720
rem
rem Where the pak files are looked for, in order:
rem   1. the folder named in quakepath.txt (next to this file), if that file exists
rem   2. the "Quake paks" folder next to this file
rem   3. the default Steam install
rem A folder may hold pak0.pak and pak1.pak directly, or in an id1 subfolder.
setlocal DisableDelayedExpansion

set "ROOT=%~dp0"
set "EXE=%ROOT%QuakeHole.exe"
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

if not exist "%PAKS%\pak0.pak" goto :noquake
if not exist "%PAKS%\pak1.pak" echo Warning: pak1.pak not found in "%PAKS%". The full game is needed; some weapon models will be missing.

rem An empty config.cfg here stops the game picking up the one in your Quake folder.
if not exist "%ROOT%game\arch\config.cfg" echo // QuakeHole settings>"%ROOT%game\arch\config.cfg"

start "" "%EXE%" +menu_main %* -basedir "%QUAKE%" -archdir "%ROOT%game\arch"
exit /b 0

:noquake
echo QuakeHole needs the data files from your own copy of Quake.
echo.
echo Could not find pak0.pak in:
echo   "%QUAKE%"
echo.
echo Copy pak0.pak and pak1.pak from your Quake folder (its id1 subfolder) into
echo the "Quake paks" folder next to this file, then run this file again.
echo Or create quakepath.txt next to this file containing the path to your Quake folder.
echo.
pause
exit /b 1
