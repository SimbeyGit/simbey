@echo off
setlocal
set "TEST_DESKTOP=%~dp0results\bin\QuadooListViewTestDesktop.exe"
if exist "%TEST_DESKTOP%" goto Run
if not exist "%~dp0results\bin" mkdir "%~dp0results\bin"
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /MT /O2 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0500 /Fe"%TEST_DESKTOP%" /Fo"%~dp0results\bin\QuadooListViewTestDesktop.obj" "%~dp0platform\QuadooForm\ListViewCompatibility\TestDesktop.cpp" /link user32.lib /SUBSYSTEM:CONSOLE,5.00
if errorlevel 1 exit /b 1
:Run
"%TEST_DESKTOP%" "%~f1" %2 %3
exit /b %errorlevel%
