@echo off
setlocal
set "LIST_CONFIGURATION=%~1"
if "%LIST_CONFIGURATION%"=="" set "LIST_CONFIGURATION=Debug"
if /I "%LIST_CONFIGURATION%"=="Debug" goto Build
if /I "%LIST_CONFIGURATION%"=="Release" goto Build
echo Usage: build-listview-stage1.cmd [Debug^|Release]
exit /b 2

:Build
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0Harness\QuadooFormHarness.vcproj" "%LIST_CONFIGURATION%|Win32"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0..\..\desktop\QuadooStudio\ApplicationTests\QuadooApplicationTests.vcproj" "%LIST_CONFIGURATION%|Win32"
exit /b %ERRORLEVEL%
