#pragma once

HRESULT HrShellExecute (HWND hwnd, PCWSTR pcwzOperation, PCWSTR pcwzFile, PCWSTR pcwzParameters, PCWSTR pcwzDirectory, INT nShowCmd, __out LONG_PTR& nResult);
HRESULT HrShellExecuteEx (LPSHELLEXECUTEINFOW psei);
HRESULT HrCreateLink (PCWSTR pcwzPathObj, PCWSTR pcwzPathLink, PCWSTR pcwzDesc);
