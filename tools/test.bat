@echo off
rem Builds and runs the unit tests for the engine-independent code.
setlocal
call "%~dp0env.bat" || exit /b 1
cd /d "%~dp0.."
if not exist build\tests mkdir build\tests
set Q=engine\Quake
set SRC=%Q%\arch_brush.c %Q%\arch_world.c %Q%\arch_map.c %Q%\arch_cursor.c %Q%\arch_gesture.c %Q%\arch_wheel.c %Q%\arch_pads.c %Q%\arch_dig.c
cl /nologo /W4 /WX /wd4127 /Zi /Od /D_CRT_SECURE_NO_WARNINGS /I%Q% /Fobuild\tests\ /Fdbuild\tests\ /Febuild\tests\arch_tests.exe tests\*.c %SRC% >build\tests\compile.log 2>&1
if errorlevel 1 (type build\tests\compile.log & exit /b 1)
build\tests\arch_tests.exe
exit /b %errorlevel%
