@echo off
setlocal

set "QT_DIR=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64"
set "JOM=C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe"
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
set "SDK_VER=10.0.18362.0"
set "SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"

call "%VCVARS%" amd64

set "PATH=%SDK_ROOT%\bin\%SDK_VER%\x64;%QT_DIR%\bin;%PATH%"
set "INCLUDE=%SDK_ROOT%\include\%SDK_VER%\ucrt;%SDK_ROOT%\include\%SDK_VER%\shared;%SDK_ROOT%\include\%SDK_VER%\um;%SDK_ROOT%\include\%SDK_VER%\winrt;%INCLUDE%"
set "LIB=%SDK_ROOT%\Lib\%SDK_VER%\ucrt\x64;%SDK_ROOT%\Lib\%SDK_VER%\um\x64;%LIB%"

"%QT_DIR%\bin\qmake.exe" VisionSelect.pro -spec win32-msvc
if errorlevel 1 exit /b 1

"%JOM%" clean
"%JOM%"
exit /b %ERRORLEVEL%
