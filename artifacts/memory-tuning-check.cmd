@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /W4 /DNOMINMAX artifacts\memory-tuning-check.cpp /Foartifacts\memory-tuning-check.obj /Feartifacts\memory-tuning-check.exe
if errorlevel 1 exit /b 1
artifacts\memory-tuning-check.exe
