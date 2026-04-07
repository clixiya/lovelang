@echo off
setlocal

set "FILE=%~1"
if "%FILE%"=="" set "FILE=examples/01-romantic-hello.love"

if exist ".\lovelang.exe" (
  set "BIN=.\lovelang.exe"
) else (
  set "BIN=lovelang"
)

if "%~1"=="" (
  "%BIN%" "%FILE%"
) else (
  shift
  "%BIN%" "%FILE%" %*
)

exit /b %ERRORLEVEL%
