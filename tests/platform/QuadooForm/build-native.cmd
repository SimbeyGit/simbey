@echo off
setlocal
set "QF_CONFIGURATION=%~1"
if "%QF_CONFIGURATION%"=="" set "QF_CONFIGURATION=Debug"
if /I "%QF_CONFIGURATION%"=="Debug" goto Build
if /I "%QF_CONFIGURATION%"=="Release" goto Build
echo Usage: build-native.cmd [Debug^|Release] [--test]
exit /b 2

:Build
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0..\..\..\platform\QuadooForm\QuadooForm\QuadooForm.vcproj" "%QF_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0Harness\QuadooFormHarness.vcproj" "%QF_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0TestExtension\QuadooFormTestExtension.vcproj" "%QF_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0TestInternals\QuadooFormNativeTests.vcproj" "%QF_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
if /I "%~2"=="--test" goto Test
exit /b 0

:Test
call "%~dp0..\..\RunOnTestDesktop.cmd" "%~dp0..\..\results\bin\%QF_CONFIGURATION%\QuadooFormHarness.exe"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
call "%~dp0..\..\RunOnTestDesktop.cmd" "%~dp0..\..\results\bin\%QF_CONFIGURATION%\QuadooFormNativeTests.exe"
exit /b %ERRORLEVEL%
