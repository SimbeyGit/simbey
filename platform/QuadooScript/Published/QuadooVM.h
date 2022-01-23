#pragma once

#include "..\..\..\Shared\Library\Util\RString.h"

interface IQuadooVM;
interface IQuadooArray;
interface IQuadooMap;
interface IQuadooObject;
interface IJSONArray;
interface IJSONObject;
interface IJSONValue;
interface ILockableStream;
interface IRStringArray;

#define	QVM_MIN_STACK_SIZE			32

namespace QuadooVM
{
	enum Instruction
	{
		NOP = 0x00,
		SETVAR,
		STORE_LOCAL,
		STORE_GLOBAL,
		STORE_MEMBER,
		STORE_LOCAL_INDEXED,
		STORE_GLOBAL_INDEXED,
		STORE_MEMBER_INDEXED,
		PUSH_SIGNED32,
		PUSH_SIGNED64,
		PUSH_STRING,
		PUSH_FLOAT,
		PUSH_DOUBLE,
		PUSH_CURRENCY,
		PUSH_FALSE,
		PUSH_TRUE,
		PUSH_NULL,
		ADD,
		SUBTRACT,
		MULTIPLY,
		DIVIDE,
		MOD,
		EXPONENT,
		LSHIFT,
		RSHIFT,
		BITWISE_AND,
		BITWISE_OR,
		XOR,
		NEGATE,
		COMPLEMENT,
		NOT,
		EQ,
		NEQ,
		LT,
		LTEQ,
		GT,
		GTEQ,
		PUSH_GLOBAL_OBJECT,
		PUSH_LOCAL,
		PUSH_GLOBAL,
		PUSH_MEMBER,
		PUSH_INDEXED,
		CALL,
		INVOKE_METHOD,
		INVOKE_INDIRECT,
		GET_PROPERTY,
		SET_PROPERTY,
		GET_PROPERTY_INDEXED,
		SET_PROPERTY_INDEXED,
		ALLOCATE_STACK,
		CLEAR_VARIABLES,
		PUSH_GLOBAL_FUNCTION_REF,
		DUP,
		POP,
		RETURN,
		NRETURN,
		JUMP,
		JUMP_NZ,
		JUMP_ZERO,
		JUMP_NZ_NO_POP,
		JUMP_Z_NO_POP,
		JUMP_CLEAR,
		JUMP_CASE,
		JUMP_NULLSAFE,
		INC_PRE_LOCALVAR,
		DEC_PRE_LOCALVAR,
		INC_PRE_STATICVAR,
		DEC_PRE_STATICVAR,
		INC_PRE_MEMBERVAR,
		DEC_PRE_MEMBERVAR,
		INC_POST_LOCALVAR,
		DEC_POST_LOCALVAR,
		INC_POST_STATICVAR,
		DEC_POST_STATICVAR,
		INC_POST_MEMBERVAR,
		DEC_POST_MEMBERVAR,
		THROW,
		THROW_CODE,
		BEGIN_TRY,
		END_TRY,
		POP_TRY,
		NEW_ARRAY,
		NEW_MAP_STRING,
		NEW_MAP_I4,
		NEW_MAP_I8,
		NEW_OBJECT,
		NEW_LAMBDA,
		PUSH_LOCAL_REF,
		PUSH_GLOBAL_REF,
		PUSH_MEMBER_REF,
		CAST_SIGNED32,
		CAST_SIGNED64,
		CAST_STRING,
		CAST_FLOAT,
		CAST_DOUBLE,
		CAST_CURRENCY,
		CAST_BOOL,
		GET_PROPERTY_INDIRECT,
		SET_PROPERTY_INDIRECT,
		GET_PROPERTY_INDEXED_INDIRECT,
		SET_PROPERTY_INDEXED_INDIRECT,
		ARRAY_SET,
		NEW_FIBER,
		TAIL_CALL,
		ROTL,
		ROTR,
		PUSH_METHOD_DELEGATE,
		INVOKE_DYNAMIC,
		GET_PROPERTY_DYNAMIC,
		SET_PROPERTY_DYNAMIC,
		INTERFACE_CALL,
		SHIFT_STACK,
		SYSCALL_STATIC = 0xF8,
		SYSCALL_DYNAMIC = 0xF9,
		JSON = 0xFA,
		INTRINSIC = 0xFB,
		EXIT = 0xFC,
		DBG_BREAK = 0xFD,
		DBG_COND = 0xFE,
		DBG_STOP = 0xFF
	};

	enum Intrinsic
	{
		INT_LEN,			// len(value) - Return the length of a string, array, JSON array, or JSON object
		INT_SQRT,			// sqrt(value) - Return the square root of value
		INT_LOG,			// log(value) - Return the natural logarithm of value
		INT_LOG10,			// log10(value) - Return the common logarithm of value
		INT_EXP,			// exp(value) - Return the natural exponential of value
		INT_ASC,			// asc(text) - Return the ASCII value of the first character of a string
		INT_CHR,			// chr(ascii) - Return a new string containing the ascii value as a character
		INT_TRIM,			// trim(text) - Return a new string with leading and trailing space characters removed
		INT_SUBSTRING,		// substring(text, start, count) - Return the substring of "text"
		INT_STRCHR,			// strchr(text, char) - Return the first position of "char" in "text"
		INT_STRCHR_START,	// strchr(start, text, char) - Return the position of "char" in "text" beginning with "start"
		INT_STRRCHR,		// strrchr(text, char) - Return the last position of "char" in "text"
		INT_HEX,			// hex(value) - Return value as a hexadecimal string
		INT_ABS,			// abs(value) - Return the absolute value
		INT_LCASE,			// lcase(text) - Return "text" in lower case
		INT_UCASE,			// ucase(text) - Return "text" in upper case
		INT_INSTR,			// instr(text, find) - Return the first position of "find" in "text"
		INT_INSTR_START,	// instr(start, text, find) - Return the position of "find" in "text" beginning with "start"
		INT_INSTRI,			// instri(text, find) - Return the first position of "find" in "text" (case insensitive)
		INT_INSTRI_START,	// instri(start, text, find) - Return the position of "find" in "text" beginning with "start" (case insensitive)
		INT_INSTRREV,		// instrrev(text, find) - Return the last position of "find" in "text"
		INT_INSTRREVI,		// instrrevi(text, find) - Return the last position of "find" in "text" (case insensitive)
		INT_NOW,			// now() - Create a date/time object with the current local time
		INT_NOWUTC,			// nowutc() - Create a date/time object with the current system time
		INT_STRCMPI,		// strcmpi(text1, text2) - Case insensitive string comparison of "text1" and "text2"
		INT_REPLACE,		// replace(text, find, replace) - Replace "find" text with "replace" text
		INT_TIMER,			// timer() - Return the the tick count from the OS
		INT_RAND,			// rand() - Generate a random number
		INT_SRAND,			// srand(seed) - Randomize using the specified seed value
		INT_YIELD,			// yield() - Yield to the caller
		INT_YIELD_ARG,		// yield(arg) - Yield to the caller with the specific value
		INT_SPLIT,			// split(text, separator) - Split a string into an array using the "separator" text
		INT_SPACE,			// space(count) - Create a string of space characters
		INT_SIN,			// sin(value) - Sine of value
		INT_COS,			// cos(value) - Cosine of value
		INT_TAN,			// tan(value) - Tangent of value
		INT_HYP,			// hyp(x, y) - Hypotenuse of x and y
		INT_LEFT,			// left(text, count) - Returns the left side of a string
		INT_RIGHT,			// right(text, count) - Returns the right side of a string
		INT_STRINGBUILDER,	// stringbuilder() - Create a string builder object
		INT_GC,				// gc() - Garbage Collection
		INT_MUTEX,			// mutex(name, initial_owner) - Creates an OS mutex object
		INT_EXTRACT,		// extract(text, start, end) - Returns the text that is between "start" and "end"
		INT_EXTRACTI,		// extract(text, start, end) - Returns the text that is between "start" and "end" (case insensitive)
		INT_EVENTSOURCE,	// eventsource() - Create a new script event source object
		INT_MODF,			// modf(value, ref whole) - Return fractional and whole parts of a floating point number
		INT_SIGMOID,		// sigmoid(value) - Sigmoid of value
		INT_SINH,			// sinh(value) - Hyperbolic Sine of value
		INT_COSH,			// cosh(value) - Hyperbolic Cosine of value
		INT_TANH,			// tanh(value) - Hyperbolic Tangent of value
		INT_RAD,			// rad(degrees) - Convert degrees to radians
		INT_DEG,			// deg(radians) - Convert radians to degrees
		INT_EVENT,			// event(name, manual_reset) - Creates an OS event object
		INT_WAIT,			// wait(array, timeout) - Wait for any handle to become signaled
		INT_WAITALL,		// waitall(array, timeout) - Wait for all handles in the array to become signaled
		INT_ARCSIN,			// arcsin(value) - Arc Sine of value
		INT_ARCCOS,			// arccos(value) - Arc Cosine of value
		INT_ARCTAN,			// arctan(value) - Arc Tangent of value
		INT_STRCMP,			// strcmp(text1, text2) - String comparison of "text1" and "text2"
		INT_SCAN,			// scan(text, start, scan_characters) - Scan text for first matching character
		INT_SCAN_OUT,		// scan(text, start, scan_characters, ref matched_char_value) - Scan text for first matching character
		INT_SLEEP,			// sleep(ms) - Sleep for the specified number of milliseconds
		INT_PRINT,			// print(text) - Print the text to the print target (IQuadooPrintTarget)
		INT_PRINTLN,		// println(text) - Print the text plus a <CRLF> sequence to the print target (IQuadooPrintTarget)
		INT_UTOA,			// utoa(value, base) - Unsigned To Ascii
		INT_REPLACEI,		// replacei(text, find, replace) - Replace "find" text with "replace" text (case insensitive)
		INT_ISEMPTY,		// isempty(v) - Is the value null or empty?
		INT_NSN,			// nsn(value, accuracy, format) - Normalized Scientific Notation using the specified format ('e', 'E', 'f', 'g', or 'G')
		INT_INF,			// inf(value) - Return true if the value is not a finite value
		INT_RAMP,			// ramp(value) - Return max(0.0, value)
		INT_NEWASYNC,		// newasync() - Create a new asynchronous handler
		INT_ASYNC,			// async(callback, event, timeout) - Schedule a callback to the current asynchronous handler
		INT_AWAIT,			// await(event, timeout) - Suspend the current fiber until the event or timeout is triggered, using the current asynchronous handler
		INT_PROPBAG,		// propbag() - Create a new property bag
		INT_PROPBAG_COPY,	// propbag(value) - Create a new property bag and copy from either a JSON object or string map
		INT_STRCMPN,		// strcmpn(string, fragment) - Perform a bounded string comparison
		INT_STRCMPN_START,	// strcmpn(string, fragment, start) - Perform a bounded string comparison with a start position for the first string
		INT_STRCMPNI,		// strcmpni(string, fragment) - Perform a bounded, case-insensitive string comparison
		INT_STRCMPNI_START,	// strcmpni(string, fragment, start) - Perform a bounded, case-insensitive string comparison with a start position for the first string
		INT_BASE64,			// base64(value) - Encode the value to a Base64 string
		INT_BASE64_CP,		// base64(string, code_page) - Encode the string to a Base64 string using the specified code page
		INT_BASE64URL,		// base64url(value) - Encode the value to a Base64Url string
		INT_BASE64URL_CP,	// base64url(string, code_page) - Encode the string to a Base64Url string using the specified code page
		INT_STRINS,			// strins(string, fragment, insert) - Return a string with "fragment" inserted into "string" at position "insert"
		INT_STRTOK,			// strtok(string, delimiters, ref start) - Return a string token and update the "start" position, white space is ignored, and quotes are considered
		INT_REDUCE,			// reduce(function, array/string) - Return a value based on the reduction of the data using the provided function
		INT_REDUCE_INITIAL,	// reduce(function, array/string, initial) - Return a value based on the reduction of the data using the provided function and the initial value
		INT_DICE,			// dice(sides) - Return a random value between 1 and sides, inclusive
		INT_STRINGLIST,		// stringlist() - Create a new string list
		INT_STRINGLIST_COPY,// stringlist(value) - Create a new string list and copy from a JSON array, an array of strings, or another string list
		INT_JOIN,			// join(array, separator) - Join the string values of an array together using the separator string
		INT_ROUND,			// round(value) - Round the value to the nearest whole number
		INT_CEIL,			// ceil(value) - Round up the value to the nearest whole number
		INT_FLOOR,			// floor(value) - Round down the value to the nearest whole number
		INT_SUM,			// sum(array) - Adds together all the items from the array
		INT_SUM_FIRST,		// sum(array, first) - Adds together all the items from the array, starting with the provided value first
		INT_SPLITLINES,		// splitlines(text) - Splits text delimited by CR or CRLF pairs into an array of strings
		INT_SPLITLINES_TO,	// splitlines(text, array) - Splits text by line into the provided array
		INT_MINVALUE,		// min(value, value) - Returns the smaller value
		INT_MAXVALUE,		// max(value, value) - Returns the larger value
		INT_WRITER,			// writer() - Creates a new binary writer object
		INT_WRITER_ALLOC,	// writer(alloc) - Creates a new binary writer object and reserves the specified amount of memory
		INT_READER,			// reader(binary) - Creates a new binary reader object on the provided binary object
		INT_ATOU,			// atou(string, base) - Ascii To Unsigned
		INT_TYPEOF = 0xFF	// typeof(value) - Return the type of the value using the Type enumeration below
	};

	enum JSONFunction
	{
		JSON_PARSE,
		JSON_PARSEWITHDICTIONARY,
		JSON_PARSEWITHDICTIONARY_OBJECT,
		JSON_PARSE_BASE64,					// JSONParseBase64(base64) - Parse Base64 encoded JSON
		JSON_PARSE_BASE64_DICTIONARY,		// JSONParseBase64(base64, dictionary) - Parse Base64 encoded JSON
		JSON_PARSE_BASE64URL,				// JSONParseBase64Url(base64url) - Parse Base64Url encoded JSON
		JSON_PARSE_BASE64URL_DICTIONARY,	// JSONParseBase64Url(base64url, dictionary) - Parse Base64Url encoded JSON
		JSON_CREATEDICTIONARY,
		JSON_GETOBJECT,
		JSON_GETVALUE,
		JSON_SETVALUE,
		JSON_REMOVEVALUE,
		JSON_CREATEOBJECT,
		JSON_CREATEARRAY,
		JSON_MERGEOBJECT,
		JSON_ADDFROMOBJECT,
		JSON_ADDFROMOBJECT_OPTION,
		JSON_FINDARRAYSTRING,
		JSON_CLONE,
		JSON_FORMAT
	};

	enum Type
	{
		Null,
		Bool,
		I4,
		I8,
		Float,
		Double,
		Currency,
		String,
		Array,
		Map,
		Object,
		JSONArray,
		JSONObject,
		Ref
	};

	enum State
	{
		Uninitialized,
		Initialized,
		Active,
		Running,
		Suspended,
		Yield
	};

	struct QVARIANT
	{
		Type eType;
		DWORD dwReserved;	// Padding
		union
		{
			bool fVal;
			LONG lVal;
			LONGLONG llVal;
			FLOAT fltVal;
			DOUBLE dblVal;
			RSTRING rstrVal;
			IQuadooArray* pArray;
			IQuadooMap* pMap;
			IQuadooObject* pObject;
			IJSONArray* pJSONArray;
			IJSONObject* pJSONObject;
			QVARIANT* pRef;
		};
	};

	struct EXCEPTION_FRAME
	{
		struct EXCEPTION_FRAME* pPrev;
		ULONG ip;
		ULONG sp;
		ULONG fp;
		ULONG spOffset;
	};

	struct QFRAME
	{
		QVARIANT* pqvStack;
		ULONG cbMaxStack;
		ULONG ip;
		ULONG sp;
		ULONG fp;
		EXCEPTION_FRAME* pexFrame;
		State eState;
		BOOL fStackAllocated;
		PVOID pvFiber;
	};

	struct VM
	{
		PBYTE pbCode;
		ULONG cbCode;
		ULONG cGlobals;
		QVARIANT* pqvGlobals;
		QFRAME frame;
	};

	struct QVPARAMS
	{
		QVARIANT* pqvArgs;
		BYTE cArgs;
	};

	struct PROPERTY
	{
		DWORD idxName;
		DWORD idxGet;
		DWORD idxIndexedGet;
		DWORD idxSet;
		DWORD idxIndexedSet;
	};

	struct METHOD
	{
		DWORD idxName;
		DWORD idxMethod;
		DWORD cParams;
	};

	struct INTERFACE
	{
		METHOD* pMethods;
		ULONG cMethods;
	};

	struct CLASS_DEF
	{
		ULONG idxDestructor;
		ULONG cMembers;
		ULONG cProps;
		ULONG cMethods;
		USHORT nDefaultMethod;
		USHORT nDefaultProperty;
	};
}

interface __declspec(uuid("B67EA814-F7EC-4613-A74B-909C0BAF7289")) IExternalScriptSite : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE ResolveExternalItem (RSTRING rstrName, __out QuadooVM::QVARIANT* pqvItem) = 0;
};

interface __declspec(uuid("7AFC6A0E-B836-4bc5-8210-4F34FB8300A9")) IQuadooFunction : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Invoke (IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) = 0;
};

interface __declspec(uuid("B98F4BA6-F109-48ce-8A2E-9B0EEC99A469")) IQuadooSysCallTarget : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Invoke (IQuadooVM* pVM, DWORD dwSysCall, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) = 0;
};

interface __declspec(uuid("C2E41AB2-E468-4083-AB48-28C87851C4D4")) IQuadooNativeHandle : IUnknown
{
	virtual HANDLE STDMETHODCALLTYPE GetNativeHandle (VOID) = 0;
};

interface __declspec(uuid("487A453D-60C7-4e8f-A762-D0C1F5B1466F")) IQuadooPrintTarget : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Print (RSTRING rstrText) = 0;
	virtual HRESULT STDMETHODCALLTYPE PrintLn (RSTRING rstrText) = 0;
};

interface __declspec(uuid("243449C6-8B58-47d4-B488-A6F9C0CF0F23")) IQuadooWaitForEvents : IUnknown
{
	virtual DWORD STDMETHODCALLTYPE WaitForEvents (DWORD cEvents, __in_ecount(cEvents) const HANDLE* prghEvents, DWORD msTimeout) = 0;
};

interface __declspec(uuid("CA591902-FAC9-4b7e-80F1-7661794364F7")) IQuadooAsyncHandler : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Async (IQuadooObject* pCallback, __in_opt IQuadooNativeHandle* pHandle, DWORD msTimeout) = 0;
	virtual HRESULT STDMETHODCALLTYPE Await (PVOID pvFiber, IQuadooNativeHandle* pHandle, DWORD msTimeout) = 0;
	virtual HRESULT STDMETHODCALLTYPE Configure (IQuadooWaitForEvents* pWait) = 0;
};

interface __declspec(uuid("35FE6D03-4D05-4b49-A7A3-CD9DC2C944C1")) IQuadooVM : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE AddGlobal (RSTRING rstrName, IQuadooObject* pObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindGlobal (RSTRING rstrName, __deref_out IQuadooObject** ppObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveGlobal (RSTRING rstrName, __deref_opt_out IQuadooObject** ppObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE RunConstructor (__deref_out IQuadooObject** ppException) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterDestructor (IQuadooObject* pObject, DWORD idxDestructor) = 0;
	virtual HRESULT STDMETHODCALLTYPE PushValue (QuadooVM::QVARIANT* pqvValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindFunction (PCWSTR pcwzFunction, __out ULONG* pidxFunction, __out DWORD* pcParams) = 0;
	virtual HRESULT STDMETHODCALLTYPE RunFunction (ULONG idxFunction, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE Throw (QuadooVM::QVARIANT* pqvValue, __in_opt QuadooVM::QVARIANT* pqvCode = NULL) = 0;
	virtual HRESULT STDMETHODCALLTYPE Resume (__in_opt QuadooVM::QVARIANT* pqvValue, __out_opt QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unload (VOID) = 0;
	virtual QuadooVM::State STDMETHODCALLTYPE GetState (VOID) = 0;
	virtual VOID STDMETHODCALLTYPE SetExternalScriptSite (__in_opt IExternalScriptSite* pSite) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddGlobalFunction (RSTRING rstrMethod, IQuadooFunction* pFunction) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveGlobalFunction (RSTRING rstrMethod) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetSysCallTarget (__in_opt IQuadooSysCallTarget* pTarget) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPrintTarget (__in_opt IQuadooPrintTarget* pTarget) = 0;
	virtual HRESULT STDMETHODCALLTYPE End (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE ThrowAndResume (QuadooVM::QVARIANT* pqvValue, __in_opt QuadooVM::QVARIANT* pqvCode, __out_opt QuadooVM::QVARIANT* pqvResult) = 0;
	virtual VOID STDMETHODCALLTYPE EnableCodeStepping (bool fStepping) = 0;
};

interface __declspec(uuid("23A1B25B-587A-4a4e-9506-396BE62A50CE")) IQuadooContainer : IUnknown
{
	virtual sysint STDMETHODCALLTYPE Length (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetItem (sysint nItem, const QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetItem (sysint nItem, __out QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveItem (sysint nItem, __out_opt QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID) = 0;
	virtual VOID STDMETHODCALLTYPE Clear (VOID) = 0;
};

interface __declspec(uuid("E8B88B7C-3FE7-4d6f-AB84-9827882E465E")) IQuadooArray : IQuadooContainer
{
	virtual HRESULT STDMETHODCALLTYPE InsertAt (const QuadooVM::QVARIANT* pqv, sysint nInsert) = 0;
	virtual HRESULT STDMETHODCALLTYPE Append (const QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Splice (sysint nInsertAt, sysint cRemove, const QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Slice (sysint nBegin, sysint nEnd, __out QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Swap (sysint nItemA, sysint nItemB) = 0;
};

interface __declspec(uuid("48A8AD92-4BA4-476f-9BF1-E58F8295FDE5")) IQuadooMap : IQuadooContainer
{
	virtual HRESULT STDMETHODCALLTYPE GetKey (sysint nItem, __out QuadooVM::QVARIANT* pqvKey) = 0;
	virtual HRESULT STDMETHODCALLTYPE Find (QuadooVM::QVARIANT* pqvItem, __out QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Add (QuadooVM::QVARIANT* pqvItem, const QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE MultiAdd (QuadooVM::QVARIANT* pqvItem, const QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE Delete (QuadooVM::QVARIANT* pqvItem, __out_opt QuadooVM::QVARIANT* pqv) = 0;
	virtual bool STDMETHODCALLTYPE HasItem (QuadooVM::QVARIANT* pqvItem) = 0;
};

interface __declspec(uuid("1DC75125-4C80-4525-9728-F8FCD97E1370")) IQuadooInterface : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Invoke (ULONG idxOrdinal, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE ResolveMethod (RSTRING rstrMethod, __out ULONG* pidxOrdinal) = 0;
};

interface __declspec(uuid("C91E38D9-86BF-4d71-8571-0251840CD542")) IQuadooObject : IServiceProvider
{
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, QuadooVM::QVARIANT* pqv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetMemberVariable (DWORD idxMember, __deref_out QuadooVM::QVARIANT** ppqvMember) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDefaultValue (__out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInterface (RSTRING rstrInterface, __deref_out IQuadooInterface** ppInterface) = 0;
	virtual HRESULT STDMETHODCALLTYPE Destruct (VOID) = 0;
};

class CQuadooObjectImpl : public IQuadooObject
{
public:
	// IServiceProvider
	virtual HRESULT STDMETHODCALLTYPE QueryService (REFGUID guidService, REFIID riid, PVOID* ppvObject) { return E_NOINTERFACE; }

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, __out QuadooVM::QVARIANT* pqvResult) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE SetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, QuadooVM::QVARIANT* pqv) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetMemberVariable (DWORD idxMember, __deref_out QuadooVM::QVARIANT** ppqvMember) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetDefaultValue (__out QuadooVM::QVARIANT* pqvResult) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetInterface (RSTRING rstrInterface, __deref_out IQuadooInterface** ppInterface) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Destruct (VOID) { return S_FALSE; }
};

interface __declspec(uuid("67DF4FAC-B9C2-44a6-82AB-ADBC6FC9BB2E")) IQuadooDebugger : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE RegisterByteCode (RSTRING rstrProgramName, PBYTE pbCode, ULONG cbCode, ULONG cGlobals, __in_opt ISequentialStream* pstmDebug) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnregisterByteCode (RSTRING rstrProgramName) = 0;

	// AttachVM() is called when a new VM has been instantiated.  The IQuadooVM instance
	// is provided, but no reference must be taken.  The pointer must be discarded once
	// DetachVM() is called, and no methods should be called during DetachVM().
	virtual HRESULT STDMETHODCALLTYPE AttachVM (RSTRING rstrProgramName, IQuadooVM* pVM) = 0;
	virtual HRESULT STDMETHODCALLTYPE DetachVM (RSTRING rstrProgramName) = 0;

	// InvokeBreakpoint() and UnhandledException() receive the IQuadooVM instance, the
	// IP that triggered the call, and the VM data, which includes the next IP position.
	// fConditional is true if a DBG_COND instruction was encountered.
	virtual VOID STDMETHODCALLTYPE InvokeBreakpoint (IQuadooVM* pVM, ULONG nIP, QuadooVM::VM* pQVM, bool fConditional) = 0;
	virtual VOID STDMETHODCALLTYPE UnhandledException (IQuadooVM* pVM, ULONG nIP, QuadooVM::VM* pQVM) = 0;
};

interface __declspec(uuid("8C32C545-0802-4e32-A830-83EA42BA2870")) IQuadooInstanceLoader : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE FindInstance (RSTRING rstrProgramName, __deref_out IUnknown** ppunkCustomData) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddInstance (RSTRING rstrProgramName, IUnknown* punkCustomData, ISequentialStream* pstmProgram, DWORD cbProgram, __in_opt ISequentialStream* pstmDebug) = 0;
	virtual HRESULT STDMETHODCALLTYPE LoadVM (RSTRING rstrProgramName, __out HRESULT* phrRegistered, __deref_out IQuadooVM** ppVM) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveInstance (RSTRING rstrProgramName) = 0;
	virtual bool STDMETHODCALLTYPE IsUsingDebugger (VOID) = 0;
};

HRESULT WINAPI QVMCreateLoader (__in_opt IQuadooDebugger* pDebugger, __deref_out IQuadooInstanceLoader** ppLoader);
VOID WINAPI QVMAddRef (QuadooVM::QVARIANT* pqvDest);
VOID WINAPI QVMSetVariant (__out QuadooVM::QVARIANT* pqvDest, const QuadooVM::QVARIANT* pqvSrc);
VOID WINAPI QVMSetDWord (__out QuadooVM::QVARIANT* pqvDest, DWORD dwValue);
VOID WINAPI QVMClearVariant (QuadooVM::QVARIANT* pqv);
VOID WINAPI QVMReplaceVariant (QuadooVM::QVARIANT* pqvDest, const QuadooVM::QVARIANT* pqvSrc);
VOID WINAPI QVMReferenceVariant (QuadooVM::QVARIANT* pqvDest, QuadooVM::QVARIANT* pqvSrc);
HRESULT WINAPI QVMLen (QuadooVM::QVARIANT* pqv, __out LONGLONG* pllLen);
VOID WINAPI QVMReleaseObject (IQuadooObject* pObject);
HRESULT WINAPI QVMCreateNewArray (QuadooVM::QVARIANT* pqvSize, __out QuadooVM::QVARIANT* pqvArray);
HRESULT WINAPI QVMCreateStringMap (__out QuadooVM::QVARIANT* pqvMap);
HRESULT WINAPI QVMCreateI4Map (__out QuadooVM::QVARIANT* pqvMap);
HRESULT WINAPI QVMCreateI8Map (__out QuadooVM::QVARIANT* pqvMap);
HRESULT WINAPI QVMCreateDateObject (const FILETIME& ftDate, __out QuadooVM::QVARIANT* pqvDate);
HRESULT WINAPI QVMCreateStringBuilder (__out QuadooVM::QVARIANT* pqvStringBuilder);
HRESULT WINAPI QVMCreateBinaryFile (RSTRING rstrContentType, RSTRING rstrFileName, ILockableStream* pData, INT nStart, INT nEnd, __out QuadooVM::QVARIANT* pqvBinary);
HRESULT WINAPI QVMCreateEventSource (__in_opt RSTRING rstrMethod, __deref_out IQuadooObject** ppEventSource);
HRESULT WINAPI QVMCreateEvent (RSTRING rstrEvent, bool fManualReset, __deref_out IQuadooObject** ppEvent);
HRESULT WINAPI QVMCreateEventFromHandle (HANDLE hEvent, __deref_out IQuadooObject** ppEvent);
HRESULT WINAPI QVMCreateStringList (__in_opt QuadooVM::QVARIANT* pqvCopy, __out QuadooVM::QVARIANT* pqvStringList);
HRESULT WINAPI QVMWrapStringArray (IRStringArray* pStringArray, __out QuadooVM::QVARIANT* pqvStringList);
HRESULT WINAPI QVMLockBuffer (ILockableStream* pData, __out QuadooVM::QVARIANT* pqvResult);
HRESULT WINAPI QVMCreateWriter (DWORD cbReserve, __out QuadooVM::QVARIANT* pqvWriter);
HRESULT WINAPI QVMCreateReader (ILockableStream* pStream, DWORD nStart, DWORD cbData, __out QuadooVM::QVARIANT* pqvReader);

HRESULT WINAPI QVMAdd (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMSubtract (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMMultiply (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMDivide (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMMod (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMPow (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMLShift (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMRShift (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMAnd (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMOr (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMXor (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMLessThan (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMLessThanEqual (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMEqual (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMNotEqual (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMNegate (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMComplement (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMNot (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMRotateLeft (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);
HRESULT WINAPI QVMRotateRight (QuadooVM::QVARIANT* pLeft, QuadooVM::QVARIANT* pRight, QuadooVM::QVARIANT* pResult);

HRESULT WINAPI QVMSqrt (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMLog (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMLog10 (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMExp (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMAsc (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMChr (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMTrim (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSubString (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrChr (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrChrStart (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrRChr (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMHex (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMAbs (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMLCase (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMUCase (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrCmpI (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMReplaceSubString (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMReplaceSubStringI (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSplit (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSplitLines (QuadooVM::QVARIANT* pqvText, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSplitLinesInto (QuadooVM::QVARIANT* pqvText, IQuadooArray* pArray);
HRESULT WINAPI QVMSpace (QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMLeft (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMRight (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMExtractString (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMExtractStringI (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMModF (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSigmoid (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMWait (QuadooVM::QVARIANT* pqvArray, QuadooVM::QVARIANT* pqvTimeout, BOOL fWaitAll, __out QuadooVM::QVARIANT* pqvResult);
HRESULT WINAPI QVMStrCmp (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMScan (QuadooVM::QVARIANT* pqvArgs, __out_opt QuadooVM::QVARIANT* pqvChar, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMUToA (QuadooVM::QVARIANT* pqvNumber, QuadooVM::QVARIANT* pqvBase, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMNSN (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrCmpN (INT (*pfnStrCmpN)(const WCHAR*, const WCHAR*, INT), QuadooVM::QVARIANT* pqvArgs, __in_opt QuadooVM::QVARIANT* pqvStart, __out QuadooVM::QVARIANT* pqvResult);
HRESULT WINAPI QVMStrInsert (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMStrTok (QuadooVM::QVARIANT* pqvArgs, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMDecodeAndParseJSON (QuadooVM::QVARIANT* pqvEncoded, __in_opt QuadooVM::QVARIANT* pqvDictionary, HRESULT (WINAPI* pfnDecode)(PCSTR, INT, ISequentialStream*, __out_opt DWORD*), __out QuadooVM::QVARIANT* pqvResult);
HRESULT WINAPI QVMReduce (QuadooVM::QVARIANT* pqvFunction, QuadooVM::QVARIANT* pqvData, __in_opt QuadooVM::QVARIANT* pqvInitial, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMJoin (QuadooVM::QVARIANT* pqvArray, QuadooVM::QVARIANT* pqvSeparator, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMAppendJoin (QuadooVM::QVARIANT* pqvArray, QuadooVM::QVARIANT* pqvSeparator, __out ISequentialStream* pstmJoin);
HRESULT WINAPI QVMRound (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMCeil (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMFloor (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMSum (__inout QuadooVM::QVARIANT* pqv, __in_opt QuadooVM::QVARIANT* pqvFirst);
HRESULT WINAPI QVMMin (QuadooVM::QVARIANT* pqvA, QuadooVM::QVARIANT* pqvB, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMMax (QuadooVM::QVARIANT* pqvA, QuadooVM::QVARIANT* pqvB, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMAToU (QuadooVM::QVARIANT* pqvString, QuadooVM::QVARIANT* pqvBase, __out QuadooVM::QVARIANT* pqv);

HRESULT WINAPI QVMVariantToDouble (const QuadooVM::QVARIANT* pqv, __out DOUBLE* pdbl);
HRESULT WINAPI QVMVariantToString (const QuadooVM::QVARIANT* pqv, __out RSTRING* prstr);
HRESULT WINAPI QVMVariantToDWord (const QuadooVM::QVARIANT* pqv, __out DWORD* pdw);
HRESULT WINAPI QVMMultiAdd (__inout QuadooVM::QVARIANT* pqvSlot, const QuadooVM::QVARIANT* pqv);

HRESULT WINAPI QVMConvertToSigned32 (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToSigned64 (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToString (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToFloat (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToDouble (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToCurrency (__inout QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToBool (__inout QuadooVM::QVARIANT* pqv);

HRESULT WINAPI QVMConvertFromJSON (__in_opt IJSONValue* pv, __out QuadooVM::QVARIANT* pqv);
HRESULT WINAPI QVMConvertToJSON (QuadooVM::QVARIANT* pqv, __deref_out IJSONValue** ppv);
HRESULT WINAPI QVMGetProperty (__in_opt IQuadooVM* pVM, QuadooVM::QVARIANT* pqvObject, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvDest);
HRESULT WINAPI QVMSetProperty (__in_opt IQuadooVM* pVM, QuadooVM::QVARIANT* pqvObject, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvSrc);
HRESULT WINAPI QVMGetPropertyIndexed (__in_opt IQuadooVM* pVM, QuadooVM::QVARIANT* pqvObject, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, __out QuadooVM::QVARIANT* pqvDest);
HRESULT WINAPI QVMSetPropertyIndexed (__in_opt IQuadooVM* pVM, QuadooVM::QVARIANT* pqvObject, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, QuadooVM::QVARIANT* pqvSrc);
HRESULT WINAPI QVMInvokeManagedMethod (QuadooVM::QVARIANT* pqvObject, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
HRESULT WINAPI QVMFindJSONArrayObject (IJSONArray* pJSONArray, QuadooVM::QVARIANT* pqvField, QuadooVM::QVARIANT* pqvValue, __deref_out IJSONObject** ppObject, __out_opt sysint* pidxItem);
HRESULT WINAPI QVMOptFind (QuadooVM::QVARIANT* pqvObject, QuadooVM::QVARIANT* pqvField, QuadooVM::QVARIANT* pqvDefault, __out QuadooVM::QVARIANT* pqvResult);
