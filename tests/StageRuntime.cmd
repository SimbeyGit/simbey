@echo off
setlocal
set "TEST_RUNTIME=%~dp0..\target\%~1"
if /i "%~2"=="x64" set "TEST_RUNTIME=%TEST_RUNTIME%\x64"
set "TEST_BIN=%~f3"
set "TEST_EXAMPLES=%~dp0..\platform\QuadooForm\DesignerExamples"
if not exist "%TEST_BIN%" mkdir "%TEST_BIN%"
if not exist "%TEST_BIN%" exit /b 1
for %%F in (SimbeyCore.dll JSON.dll QuadooForm.dll CodeEdit.dll QuadooVM.dll QuadooParser.dll AQVMT.bin AQVMDbg.exe QVMDbg.exe WQVMDbg.exe QuadooFormDesigner.exe QuadooStudio.exe SimbeyComm.dll SimbeyTransports.dll SimbeyPresence.dll MbedTLS.dll SPM.dll QuadooUI.dll SIF.dll fmodex.dll SServCtl.ocx) do (
	if exist "%TEST_RUNTIME%\%%F" (
		xcopy /I /Y /Q "%TEST_RUNTIME%\%%F" "%TEST_BIN%\" >nul
		if errorlevel 2 exit /b 1
	)
)
if /i not "%~4"=="Quadoo" exit /b 0
if exist "%TEST_EXAMPLES%\*.json" (
	xcopy /D /I /Y /Q "%TEST_EXAMPLES%\*.json" "%TEST_BIN%\DesignerExamples\" >nul
	if errorlevel 2 exit /b 1
)
if exist "%TEST_EXAMPLES%\*.png" (
	xcopy /D /I /Y /Q "%TEST_EXAMPLES%\*.png" "%TEST_BIN%\DesignerExamples\" >nul
	if errorlevel 2 exit /b 1
)
for %%F in (QuadooPlugin.json QuadooPlugin.png) do (
	xcopy /D /I /Y /Q "%~dp0..\desktop\QuadooStudio\DesignControls\%%F" "%TEST_BIN%\DesignerExamples\" >nul
	if errorlevel 1 exit /b 1
)
for %%F in (CanvasApplication.qsproj CanvasForm.quadoo CanvasForm.qform.json Sprite.png) do (
	xcopy /D /I /Y /Q "%TEST_EXAMPLES%\CanvasApplication\%%F" "%TEST_BIN%\DesignerExamples\CanvasApplication\" >nul
	if errorlevel 1 exit /b 1
)
exit /b 0
