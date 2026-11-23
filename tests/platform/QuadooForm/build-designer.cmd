@echo off
setlocal
set "QFD_CONFIGURATION=%~1"
if "%QFD_CONFIGURATION%"=="" set "QFD_CONFIGURATION=Debug"
if /I "%QFD_CONFIGURATION%"=="Debug" goto Build
if /I "%QFD_CONFIGURATION%"=="Release" goto Build
echo Usage: build-designer.cmd [Debug^|Release] [--test]
exit /b 2
:Build
call "%~dp0build-native.cmd" %QFD_CONFIGURATION%
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0..\..\..\platform\QuadooForm\Designer\QuadooFormDesignerCore.vcproj" "%QFD_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0..\..\..\platform\QuadooForm\DesignerHost\QuadooFormDesigner.vcproj" "%QFD_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0DesignerTests\QuadooFormDesignerTests.vcproj" "%QFD_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
call "%~dp0..\..\StageRuntime.cmd" "%QFD_CONFIGURATION%" Win32 "%~dp0..\..\results\bin\%QFD_CONFIGURATION%" Quadoo
if errorlevel 1 exit /b %ERRORLEVEL%
if /I not "%~2"=="--test" exit /b 0
call "%~dp0..\..\RunOnTestDesktop.cmd" "%~dp0..\..\results\bin\%QFD_CONFIGURATION%\QuadooFormHarness.exe"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
call "%~dp0..\..\RunOnTestDesktop.cmd" "%~dp0..\..\results\bin\%QFD_CONFIGURATION%\QuadooFormNativeTests.exe"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
call "%~dp0..\..\RunOnTestDesktop.cmd" "%~dp0..\..\results\bin\%QFD_CONFIGURATION%\QuadooFormDesignerTests.exe"
exit /b %ERRORLEVEL%
