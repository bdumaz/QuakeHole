@echo off
rem Builds and runs the unit tests for the geometry core.
setlocal
call "%~dp0env.bat" || exit /b 1
cd /d "%~dp0.."
if not exist build\tests mkdir build\tests
set SRC=engine\Quake\arch_brush.c engine\Quake\arch_world.c engine\Quake\arch_map.c
cl /nologo /W4 /WX /wd4127 /Zi /Od /D_CRT_SECURE_NO_WARNINGS /Iengine\Quake /Fobuild\tests\ /Fdbuild\tests\ /Febuild\tests\arch_tests.exe tests\*.c %SRC% >build\tests\compile.log 2>&1
if errorlevel 1 (type build\tests\compile.log & exit /b 1)
build\tests\arch_tests.exe
exit /b %errorlevel%
