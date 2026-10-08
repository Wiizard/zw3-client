@echo off
echo Updating submodules...
call git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\prepare-x64.ps1"
exit /b %errorlevel%
