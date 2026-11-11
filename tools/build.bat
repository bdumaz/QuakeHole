@echo off
rem Builds the engine (Release x64).
setlocal
call "%~dp0env.bat" || exit /b 1
set "SLN=%~dp0..\engine\Windows\VisualStudio\"
msbuild "%SLN%quakespasm-sdl2.vcxproj" /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=%SLN%\" /m /nologo /v:minimal
exit /b %errorlevel%
