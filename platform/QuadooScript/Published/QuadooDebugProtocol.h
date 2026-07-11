#pragma once

#include <windows.h>

enum QUADOO_DEBUG_MESSAGE
{
	QDM_HELLO = 1,
	QDM_SCRIPT_PATH,
	QDM_SCRIPT_ARGUMENTS,
	QDM_SCRIPT_BYTECODE,
	QDM_DEBUG_DATA,
	QDM_BREAKPOINTS,
	QDM_START,
	QDM_STEP_INTO,
	QDM_STEP_OVER,
	QDM_STOP,
	QDM_BREAKPOINT_ADD,
	QDM_BREAKPOINT_REMOVE,
	QDM_CONTINUE,
	QDM_BREAKPOINT_HIT,
	QDM_EXCEPTION,
	QDM_STACK_FRAMES,
	QDM_VARIABLE_QUERY,
	QDM_VARIABLE_VALUE
};

// Current protocol version: 2
// Version 1: Run, code stepping, and call stacks
// Version 2: Variable inspection
#define QUADOO_DEBUG_PROTOCOL_VERSION 2

enum QUADOO_DEBUG_VARIABLE_CLASS
{
	QDVC_LOCAL = 1,
	QDVC_GLOBAL,
	QDVC_MEMBER
};

// Every message is an 8-byte header followed by cbPayload bytes. String
// payloads are UTF-16 without a trailing null. QDM_BREAKPOINTS contains a
// DWORD count followed by QUADOO_DEBUG_BREAKPOINT and file-name pairs. The
// breakpoint add/remove and stopped-location messages contain one breakpoint
// and file-name pair.
struct QUADOO_DEBUG_MESSAGE_HEADER
{
	DWORD nMessage;
	DWORD cbPayload;
};

struct QUADOO_DEBUG_BREAKPOINT
{
	DWORD cchFile;
	DWORD nLine;
};

struct QUADOO_DEBUG_STACK_FRAME
{
	DWORD cchName;
	DWORD ip;
	DWORD sp;
	DWORD fp;
	DWORD nExecutionDepth;
};

struct QUADOO_DEBUG_VARIABLE_QUERY
{
	DWORD requestId;
	DWORD nKind;
	LONG nOffset;
	LONG nBaseOffset;
};

struct QUADOO_DEBUG_VARIABLE_VALUE
{
	DWORD requestId;
	HRESULT hr;
	DWORD cchValue;
};
