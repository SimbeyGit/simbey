@echo off
setlocal
set "IMAGE_CONFIGURATION=%~1"
if "%IMAGE_CONFIGURATION%"=="" set "IMAGE_CONFIGURATION=Debug"
if /I "%IMAGE_CONFIGURATION%"=="Debug" goto Build
if /I "%IMAGE_CONFIGURATION%"=="Release" goto Build
echo Usage: build-imagebutton.cmd [Debug^|Release]
exit /b 2

:Build
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
rem Build individual projects. Never build compiler/VM, SimbeyCore or JSON DLLs.
for %%P in ("..\..\..\desktop\QuadooStudio\CodeEdit\CodeEdit.vcproj" "..\..\..\platform\QuadooForm\QuadooForm\QuadooForm.vcproj" "TestInternals\QuadooFormNativeTests.vcproj" "TestExtension\QuadooFormTestExtension.vcproj" "Harness\QuadooFormHarness.vcproj" "..\..\..\platform\QuadooForm\Designer\QuadooFormDesignerCore.vcproj" "..\..\..\platform\QuadooForm\DesignerHost\QuadooFormDesigner.vcproj" "DesignerTests\QuadooFormDesignerTests.vcproj" "..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooStudio.vcproj" "..\..\desktop\QuadooStudio\IntegrationTests\QuadooStudioIntegrationTests.vcproj" "..\..\desktop\QuadooStudio\ApplicationTests\QuadooApplicationTests.vcproj") do (
	"C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcpackages\vcbuild.exe" /nologo "%~dp0%%~P" "%IMAGE_CONFIGURATION%|Win32"
	if errorlevel 1 exit /b 1
)
exit /b 0
