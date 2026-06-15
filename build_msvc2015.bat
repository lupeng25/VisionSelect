@echo off
setlocal EnableExtensions

if not defined QT_DIR (
    if exist "D:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin\qmake.exe" (
        set "QT_DIR=D:\Qt\Qt5.12.9\5.12.9\msvc2015_64"
    ) else if exist "C:\Qt\Qt5.12.9\5.12.9\msvc2015_64\bin\qmake.exe" (
        set "QT_DIR=C:\Qt\Qt5.12.9\5.12.9\msvc2015_64"
    )
)

if not defined JOM (
    if exist "D:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom.exe" (
        set "JOM=D:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom.exe"
    ) else if exist "D:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe" (
        set "JOM=D:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe"
    ) else if exist "C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom.exe" (
        set "JOM=C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom.exe"
    ) else if exist "C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe" (
        set "JOM=C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe"
    )
)

if not defined VCVARS (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" (
        set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
    ) else if exist "D:\Microsoft Visual Studio\2022\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VCVARS=D:\Microsoft Visual Studio\2022\VC\Auxiliary\Build\vcvarsall.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    ) else if exist "D:\Microsoft Visual Studio\18\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VCVARS=D:\Microsoft Visual Studio\18\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

if not exist "%QT_DIR%\bin\qmake.exe" (
    echo ERROR: qmake.exe not found. Set QT_DIR to a Qt 5.12.9 msvc2015_64 directory.
    exit /b 1
)

if not exist "%JOM%" (
    echo ERROR: jom.exe not found. Set JOM to the Qt Creator jom executable.
    exit /b 1
)

if not exist "%VCVARS%" (
    echo ERROR: vcvarsall.bat not found. Set VCVARS to a Visual Studio VC environment script.
    exit /b 1
)

echo Using QT_DIR=%QT_DIR%
echo Using JOM=%JOM%
echo Using VCVARS=%VCVARS%

call "%VCVARS%" amd64
if errorlevel 1 exit /b 1

set "PATH=%QT_DIR%\bin;%PATH%"

if not defined SDK_ROOT set "SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
if not defined SDK_VER (
    if exist "%SDK_ROOT%\include\10.0.18362.0\ucrt" if exist "%SDK_ROOT%\Lib\10.0.18362.0\ucrt\x64" set "SDK_VER=10.0.18362.0"
)
if not defined SDK_VER (
    for /f "delims=" %%S in ('dir /b /ad /o-n "%SDK_ROOT%\include\10.*" 2^>nul') do (
        if not defined SDK_VER if exist "%SDK_ROOT%\Lib\%%S\ucrt\x64" if exist "%SDK_ROOT%\Lib\%%S\um\x64" set "SDK_VER=%%S"
    )
)

if defined SDK_VER (
    echo Using Windows SDK=%SDK_VER%
    set "PATH=%SDK_ROOT%\bin\%SDK_VER%\x64;%PATH%"
    set "INCLUDE=%SDK_ROOT%\include\%SDK_VER%\ucrt;%SDK_ROOT%\include\%SDK_VER%\shared;%SDK_ROOT%\include\%SDK_VER%\um;%SDK_ROOT%\include\%SDK_VER%\winrt;%INCLUDE%"
    set "LIB=%SDK_ROOT%\Lib\%SDK_VER%\ucrt\x64;%SDK_ROOT%\Lib\%SDK_VER%\um\x64;%LIB%"
)
if not defined SDK_VER (
    echo WARNING: Windows SDK not found under "%SDK_ROOT%"; using Visual Studio defaults.
)

"%QT_DIR%\bin\qmake.exe" -r VisionSelect.pro -spec win32-msvc
if errorlevel 1 exit /b 1

"%JOM%" clean
if errorlevel 1 exit /b 1

"%JOM%"
if errorlevel 1 exit /b 1

call :deploy_runtime
exit /b %ERRORLEVEL%

:deploy_runtime
set "BIN_DIR=%CD%\bin"
if not exist "%BIN_DIR%" exit /b 0

for %%D in (
    Qt5Core.dll
    Qt5Gui.dll
    Qt5Widgets.dll
    Qt5PrintSupport.dll
    Qt5Test.dll
    Qt5Svg.dll
    libEGL.dll
    libGLESV2.dll
    opengl32sw.dll
    D3Dcompiler_47.dll
) do (
    if exist "%QT_DIR%\bin\%%D" (
        copy /Y "%QT_DIR%\bin\%%D" "%BIN_DIR%\" >nul
        if errorlevel 1 exit /b 1
    )
)

for %%P in (
    platforms
    imageformats
    iconengines
    styles
    printsupport
) do (
    if exist "%QT_DIR%\plugins\%%P\*.dll" (
        if not exist "%BIN_DIR%\%%P" (
            mkdir "%BIN_DIR%\%%P"
            if errorlevel 1 exit /b 1
        )
        copy /Y "%QT_DIR%\plugins\%%P\*.dll" "%BIN_DIR%\%%P\" >nul
        if errorlevel 1 exit /b 1
    )
)

exit /b 0
