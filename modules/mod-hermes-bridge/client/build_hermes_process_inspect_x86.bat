@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /TC /MT hermes_process_inspect.c /Fe:build\hermes_process_inspect.exe /link kernel32.lib
exit /b %errorlevel%
