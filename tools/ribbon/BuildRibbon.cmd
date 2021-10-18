@Echo off

SET _ProjectDir=%CD%
SET _ThisDir=%~dp0
SET _AssembleSIF=%_ThisDir%\..\..\target\release\AssembleSIF.exe

IF NOT EXIST "%_AssembleSIF%" GOTO Missing_AssembleSIF

cscript "%_ThisDir%\RibbonDui.js" "%1.xml" /rcxml:"%_ProjectDir%" /mappings:"%_ProjectDir%"
"%_ThisDir%\UICC.exe" "%_ProjectDir%\%1.post.xml" "%_ProjectDir%\%1.bml" /header:"%_ProjectDir%\%1.h" /res:"%_ProjectDir%\%1.rc"

Echo Assembling SIF files...
"%_AssembleSIF%" "%_ProjectDir%\%1.rcxml" "%_ProjectDir%"

Echo Removing temporary %1.xml files

del "%_ProjectDir%\%1.new.xml"
del "%_ProjectDir%\%1.merge.xml"
del "%_ProjectDir%\%1.post.xml"
del "%_ProjectDir%\%1.rcxml"

SET _AssembleSIF=
SET _ThisDir=
SET _ProjectDir=

Exit

:Missing_AssembleSIF
Echo ERROR - Missing "%_AssembleSIF%"
Exit 1