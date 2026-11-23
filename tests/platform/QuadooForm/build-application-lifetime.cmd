@echo off
setlocal
set "LIFETIME_CONFIGURATION=%~1"
set "LIFETIME_PLATFORM=%~2"
if "%LIFETIME_CONFIGURATION%"=="" set "LIFETIME_CONFIGURATION=Debug"
if "%LIFETIME_PLATFORM%"=="" set "LIFETIME_PLATFORM=Win32"
if /I "%LIFETIME_CONFIGURATION%"=="Debug" goto Build
if /I "%LIFETIME_CONFIGURATION%"=="Release" goto Build
exit /b 2

:Build
if /I "%LIFETIME_PLATFORM%"=="x64" goto Build64
if /I not "%LIFETIME_PLATFORM%"=="Win32" exit /b 2
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
goto Projects

:Build64
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\amd64\vcvarsamd64.bat" >nul

:Projects
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
rem Individual projects only; reuse SimbeyCore, JSON and compiler/VM DLLs.
for %%P in ("..\..\..\desktop\QuadooStudio\CodeEdit\CodeEdit.vcproj" "..\..\..\platform\QuadooForm\QuadooForm\QuadooForm.vcproj" "TestInternals\QuadooFormNativeTests.vcproj" "TestExtension\QuadooFormTestExtension.vcproj" "Harness\QuadooFormHarness.vcproj" "..\..\..\platform\QuadooForm\Designer\QuadooFormDesignerCore.vcproj" "..\..\..\platform\QuadooForm\DesignerHost\QuadooFormDesigner.vcproj" "DesignerTests\QuadooFormDesignerTests.vcproj" "..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooStudio.vcproj" "..\..\desktop\QuadooStudio\IntegrationTests\QuadooStudioIntegrationTests.vcproj" "..\..\desktop\QuadooStudio\ApplicationTests\QuadooApplicationTests.vcproj") do (
	"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0%%~P" "%LIFETIME_CONFIGURATION%|%LIFETIME_PLATFORM%"
	if errorlevel 1 exit /b 1
)
exit /b 0
