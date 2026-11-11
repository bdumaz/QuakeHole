@echo off
rem Sets up the MSVC x64 build environment for the calling script.
if defined ARCH_ENV_READY exit /b 0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto :novs
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :novs
set ARCH_ENV_READY=1
exit /b 0
:novs
echo Could not find Visual Studio with the C++ x64 build tools.
exit /b 1
