# VisionSelect

Qt Widgets industrial machine-vision selection assistant.

## VS Code build and run

Prerequisites on this Windows machine:

- Qt 5.12.9 MSVC2015 x64: `C:\Qt\Qt5.12.9\5.12.9\msvc2015_64`
- Qt Creator jom: `C:\Qt\Qt5.12.9\Tools\QtCreator\bin\jom\jom.exe`
- Visual Studio 2015 C++ tools: `C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat`
- VS Code C/C++ extension: `ms-vscode.cpptools`

Common actions:

- Build: `Ctrl+Shift+B`, then choose `Build VisionSelect` if prompted.
- Run app: open `Run and Debug`, choose `Run VisionSelect`, press `F5`.
- Run tests: `Terminal > Run Task... > Test VisionSelect`.

The VS Code tasks call `build_msvc2015.bat`, which sets the Visual Studio, Windows SDK, and Qt environment before running `qmake` and `jom`.
