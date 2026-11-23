#pragma once

HRESULT HrShellExecute (HWND hwnd, PCWSTR pcwzOperation, PCWSTR pcwzFile, PCWSTR pcwzParameters, PCWSTR pcwzDirectory, INT nShowCmd, __out LONG_PTR& nResult);
HRESULT HrShellExecuteEx (LPSHELLEXECUTEINFOW psei);
HRESULT HrCreateLink (PCWSTR pcwzPathObj, __in_opt PCWSTR pcwzWorkingDir, __in_opt PCWSTR pcwzArgs, PCWSTR pcwzPathLink, __in_opt PCWSTR pcwzIconFile, INT idxIcon, INT nCmdShow, __in_opt PCWSTR pcwzDesc);
