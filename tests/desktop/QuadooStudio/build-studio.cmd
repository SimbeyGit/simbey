@echo off
setlocal
set "QSI_CONFIGURATION=%~1"
if "%QSI_CONFIGURATION%"=="" set "QSI_CONFIGURATION=Debug"
set "QSI_PLATFORM=%~2"
if "%QSI_PLATFORM%"=="" set "QSI_PLATFORM=Win32"
if /I not "%QSI_CONFIGURATION%"=="Debug" if /I not "%QSI_CONFIGURATION%"=="Release" goto Usage
if /I not "%QSI_PLATFORM%"=="Win32" if /I not "%QSI_PLATFORM%"=="x64" goto Usage
set "QSI_ROOT=%~dp0..\..\.."
set "QSI_VCBUILD=C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
rem Build the mirrored sources against the published libraries.
for %%P in (desktop\QuadooStudio\CodeEdit\CodeEdit platform\QuadooForm\QuadooForm\QuadooForm platform\QuadooForm\Designer\QuadooFormDesignerCore platform\QuadooForm\DesignerHost\QuadooFormDesigner tests\platform\QuadooForm\Harness\QuadooFormHarness tests\platform\QuadooForm\TestExtension\QuadooFormTestExtension tests\platform\QuadooForm\TestInternals\QuadooFormNativeTests tests\platform\QuadooForm\DesignerTests\QuadooFormDesignerTests) do (
  "%QSI_VCBUILD%" /nologo "%QSI_ROOT%\%%P.vcproj" "%QSI_CONFIGURATION%|%QSI_PLATFORM%"
  if errorlevel 1 exit /b 1
)
"%QSI_VCBUILD%" /nologo "%~dp0ApplicationTests\QuadooApplicationTests.vcproj" "%QSI_CONFIGURATION%|%QSI_PLATFORM%"
if errorlevel 1 exit /b 1
"%QSI_VCBUILD%" /nologo "%QSI_ROOT%\desktop\QuadooStudio\QuadooStudio\QuadooStudio.vcproj" "%QSI_CONFIGURATION%|%QSI_PLATFORM%"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"%QSI_VCBUILD%" /nologo "%~dp0IntegrationTests\QuadooStudioIntegrationTests.vcproj" "%QSI_CONFIGURATION%|%QSI_PLATFORM%"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
set "QSI_OUTPUT=%QSI_ROOT%\tests\results\bin\%QSI_CONFIGURATION%"
if /I "%QSI_PLATFORM%"=="x64" set "QSI_OUTPUT=%QSI_OUTPUT%\x64"
call "%QSI_ROOT%\tests\StageRuntime.cmd" "%QSI_CONFIGURATION%" "%QSI_PLATFORM%" "%QSI_OUTPUT%" Quadoo
if errorlevel 1 exit /b 1
if /I not "%~3"=="--test" exit /b 0
for %%T in (QuadooFormHarness QuadooFormNativeTests QuadooFormDesignerTests QuadooStudioIntegrationTests) do (
  call "%QSI_ROOT%\tests\RunOnTestDesktop.cmd" "%QSI_OUTPUT%\%%T.exe"
  if errorlevel 1 exit /b 1
)
call "%QSI_ROOT%\tests\RunOnTestDesktop.cmd" "%QSI_OUTPUT%\QuadooApplicationTests.exe" "%~dp0ApplicationTests\Runtime.quadoo"
if errorlevel 1 exit /b 1
exit /b 0
:Usage
echo Usage: build-studio.cmd [Debug^|Release] [Win32^|x64] [--test]
exit /b 2
