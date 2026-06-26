@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /TP /EHsc /MT /LD hermes_bridge_dll.c /Fe.\build\HermesBridge_0755_breakthroughwrite.dll /link user32.lib kernel32.lib
dir .\build\HermesBridge_0755_breakthroughwrite.*
