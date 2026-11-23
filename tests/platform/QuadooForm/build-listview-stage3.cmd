@echo off
call "%~dp0build-listview-stage2.cmd" %1 Full
if errorlevel 1 exit /b %ERRORLEVEL%
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat" >nul
if errorlevel 1 exit /b %ERRORLEVEL%
cl /nologo /W4 /MT /O2 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0500 /Fe"%~dp0..\..\results\bin\QuadooListViewTestDesktop.exe" /Fo"%~dp0..\..\results\bin\QuadooListViewTestDesktop.obj" "%~dp0ListViewCompatibility\TestDesktop.cpp" /link user32.lib /SUBSYSTEM:CONSOLE,5.00
exit /b %ERRORLEVEL%
