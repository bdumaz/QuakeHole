@echo off
rem Runs one scripted in-engine check:  tools\selftest.bat boot
rem The mouse is not captured, so moving it cannot disturb a run.
rem The console output ends up in game\arch\qconsole.log, screenshots in game\arch.
setlocal DisableDelayedExpansion
set "ROOT=%~dp0.."
if not exist "%~dp0selftest\%~1.cfg" (echo No such selftest: %~1 & exit /b 1)
copy /y "%~dp0selftest\%~1.cfg" "%ROOT%\game\arch\selftest.cfg" >nul
del /q "%ROOT%\game\arch\qconsole.log" 2>nul
del /q "%ROOT%\game\arch\spasm*.png" 2>nul
call "%ROOT%\run.bat" -window -width 1280 -height 720 -nomouse -condebug +exec selftest.cfg
set RESULT=%errorlevel%
del /q "%ROOT%\game\arch\selftest.cfg" 2>nul
exit /b %RESULT%
