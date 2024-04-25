#pragma once

// GetCrashDir() is user-defined.
extern BOOL GetCrashDir (PTSTR ptzCrashDir, INT cchMaxDir, __out INT* pcchCrashDir);

VOID SetGlobalCrashHandler (VOID);
