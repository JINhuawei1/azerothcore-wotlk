@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /MT hermes_auto_injector.c /Fe:build\hermes_auto_injector.exe /link user32.lib kernel32.lib
exit /b %errorlevel%
