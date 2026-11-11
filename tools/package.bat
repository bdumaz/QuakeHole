@echo off
rem Builds the engine and packages a release zip:  tools\package.bat 0.1.0
rem The result is build\release\QuakeHole-<version>-win64.zip. It contains no Quake game data.
setlocal
set "VER=%~1"
if "%VER%"=="" set "VER=0.1.0"
set "ROOT=%~dp0.."
set "BIN=%ROOT%\engine\Windows\VisualStudio\Build-quakespasm-sdl2\x64\Release"
set "NAME=QuakeHole-%VER%-win64"
set "OUT=%ROOT%\build\release\%NAME%"

call "%~dp0build.bat" || exit /b 1

if exist "%OUT%" rmdir /s /q "%OUT%"
if exist "%OUT%.zip" del /q "%OUT%.zip"
mkdir "%OUT%\game\arch" "%OUT%\Quake paks" || exit /b 1

copy /y "%BIN%\quakespasm-sdl2.exe" "%OUT%\QuakeHole.exe" >nul || exit /b 1
copy /y "%BIN%\*.dll" "%OUT%\" >nul || exit /b 1
copy /y "%ROOT%\game\arch\default.cfg" "%OUT%\game\arch\" >nul || exit /b 1
copy /y "%~dp0release\Play QuakeHole.bat" "%OUT%\" >nul || exit /b 1
copy /y "%~dp0release\README.txt" "%OUT%\" >nul || exit /b 1
copy /y "%~dp0release\THIRD-PARTY.txt" "%OUT%\" >nul || exit /b 1
copy /y "%~dp0release\paks-readme.txt" "%OUT%\Quake paks\PUT YOUR PAK FILES HERE.txt" >nul || exit /b 1
copy /y "%ROOT%\LICENSE.txt" "%OUT%\" >nul || exit /b 1
copy /y "%ROOT%\engine\Windows\SDL2\LICENSE.txt" "%OUT%\SDL2-LICENSE.txt" >nul || exit /b 1

powershell -NoProfile -Command "Compress-Archive -Path '%OUT%' -DestinationPath '%OUT%.zip' -Force" || exit /b 1
echo Packaged %OUT%.zip
exit /b 0
