@echo off
setlocal
set "TEST_VC=C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC"
call "%TEST_VC%\bin\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
"%TEST_VC%\vcpackages\vcbuild.exe" /nologo "%~dp0..\%~1" "%~2|%~3"
exit /b %errorlevel%
