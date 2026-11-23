@echo off
setlocal
set "LIST_CONFIGURATION=%~1"
if "%LIST_CONFIGURATION%"=="" set "LIST_CONFIGURATION=Debug"
if /I "%LIST_CONFIGURATION%"=="Debug" goto Build
if /I "%LIST_CONFIGURATION%"=="Release" goto Build
echo Usage: build-listview-stage2.cmd [Debug^|Release] [Full]
exit /b 2

:Build
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
for %%P in ("..\..\..\desktop\QuadooStudio\CodeEdit\CodeEdit.vcproj" "..\..\..\platform\QuadooForm\QuadooForm\QuadooForm.vcproj" "TestInternals\QuadooFormNativeTests.vcproj" "TestExtension\QuadooFormTestExtension.vcproj" "Harness\QuadooFormHarness.vcproj") do (
	"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0%%~P" "%LIST_CONFIGURATION%|Win32"
	if errorlevel 1 exit /b 1
)
if /I not "%~2"=="Full" exit /b 0
for %%P in ("..\..\..\desktop\QuadooStudio\CodeEdit\CodeEdit.vcproj" "..\..\..\platform\QuadooForm\Designer\QuadooFormDesignerCore.vcproj" "..\..\..\platform\QuadooForm\DesignerHost\QuadooFormDesigner.vcproj" "DesignerTests\QuadooFormDesignerTests.vcproj" "..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooStudio.vcproj" "..\..\desktop\QuadooStudio\IntegrationTests\QuadooStudioIntegrationTests.vcproj" "..\..\desktop\QuadooStudio\ApplicationTests\QuadooApplicationTests.vcproj") do (
	"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0%%~P" "%LIST_CONFIGURATION%|Win32"
	if errorlevel 1 exit /b 1
)
exit /b 0
