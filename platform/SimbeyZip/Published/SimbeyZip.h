#pragma once

#include "..\..\..\Shared\Library\Core\ISeekableStream.h"

namespace SimbeyZip
{
	enum Append
	{
		CreateNew,
		CreateAfter,
		AddInZip
	};
}

/* ****************************************** */
/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_pos_s
{
	ULONG pos_in_zip_directory;   /* offset in zip file directory */
	ULONG num_of_file;            /* # of file */
} unz_file_pos;

typedef struct unz64_file_pos_s
{
	ULONGLONG pos_in_zip_directory;   /* offset in zip file directory */
	ULONGLONG num_of_file;            /* # of file */
} unz64_file_pos;

/* tm_unz contain date/time info */
typedef struct tm_unz_s
{
    UINT tm_sec;            /* seconds after the minute - [0,59] */
    UINT tm_min;            /* minutes after the hour - [0,59] */
    UINT tm_hour;           /* hours since midnight - [0,23] */
    UINT tm_mday;           /* day of the month - [1,31] */
    UINT tm_mon;            /* months since January - [0,11] */
    UINT tm_year;           /* years - [1980..2044] */
} tm_unz;

/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_info64_s
{
	ULONG version;              /* version made by                 2 bytes */
	ULONG version_needed;       /* version needed to extract       2 bytes */
	ULONG flag;                 /* general purpose bit flag        2 bytes */
	ULONG compression_method;   /* compression method              2 bytes */
	ULONG dosDate;              /* last mod file date in Dos fmt   4 bytes */
	ULONG crc;                  /* crc-32                          4 bytes */
	ULONGLONG compressed_size;   /* compressed size                 8 bytes */
	ULONGLONG uncompressed_size; /* uncompressed size               8 bytes */
	ULONG size_filename;        /* filename length                 2 bytes */
	ULONG size_file_extra;      /* extra field length              2 bytes */
	ULONG size_file_comment;    /* file comment length             2 bytes */

	ULONG disk_num_start;       /* disk number start               2 bytes */
	ULONG internal_fa;          /* internal file attributes        2 bytes */
	ULONG external_fa;          /* external file attributes        4 bytes */

	tm_unz tmu_date;
} unz_file_info64;

typedef struct unz_file_info_s
{
    ULONG version;              /* version made by                 2 bytes */
    ULONG version_needed;       /* version needed to extract       2 bytes */
    ULONG flag;                 /* general purpose bit flag        2 bytes */
    ULONG compression_method;   /* compression method              2 bytes */
    ULONG dosDate;              /* last mod file date in Dos fmt   4 bytes */
    ULONG crc;                  /* crc-32                          4 bytes */
    ULONG compressed_size;      /* compressed size                 4 bytes */
    ULONG uncompressed_size;    /* uncompressed size               4 bytes */
    ULONG size_filename;        /* filename length                 2 bytes */
    ULONG size_file_extra;      /* extra field length              2 bytes */
    ULONG size_file_comment;    /* file comment length             2 bytes */

    ULONG disk_num_start;       /* disk number start               2 bytes */
    ULONG internal_fa;          /* internal file attributes        2 bytes */
    ULONG external_fa;          /* external file attributes        4 bytes */

    tm_unz tmu_date;
} unz_file_info;

typedef BOOL (WINAPI* PFNUNZIPENUM)(PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam);

interface __declspec(uuid("F785E054-68DD-4d86-8096-F018668F9A48")) IZipFile : ISeekableStream
{
	virtual HRESULT Size (__out ULARGE_INTEGER* puliCompressed, __out ULARGE_INTEGER* puliUncompressed) = 0;
};

interface __declspec(uuid("CAF0AED1-D9A1-44b0-BCE2-7117C21FDD9C")) IUnzipEnum : IUnknown
{
	virtual HRESULT GoToFirstFile (VOID) = 0;
	virtual HRESULT GoToNextFile (VOID) = 0;
	virtual HRESULT LocateFile (PCWSTR pcwzFileName, INT iCaseSensitivity) = 0;
	virtual HRESULT EnumFiles (BOOL fListFromStart, PFNUNZIPENUM pfnEnum, PVOID pvParam) = 0;
	virtual HRESULT GetFilePos64 (unz64_file_pos* file_pos) = 0;
	virtual HRESULT GetFilePos (unz_file_pos* file_pos) = 0;
	virtual HRESULT GoToFilePos64 (const unz64_file_pos* file_pos) = 0;
	virtual HRESULT GoToFilePos (const unz_file_pos* file_pos) = 0;
	virtual HRESULT GetCurrentFileInfo64 (unz_file_info64* pfile_info,
		PWSTR pwzFileName,
		ULONG fileNameBufferSize,
		__out_opt INT* pcchFileName,
		PVOID extraField,
		ULONG extraFieldBufferSize,
		PSTR szComment,
		ULONG commentBufferSize) = 0;
	virtual HRESULT GetCurrentFileInfo (unz_file_info* pfile_info,
		PWSTR pwzFileName,
		ULONG fileNameBufferSize,
		__out_opt INT* pcchFileName,
		PVOID extraField,
		ULONG extraFieldBufferSize,
		PSTR szComment,
		ULONG commentBufferSize) = 0;
	virtual HRESULT OpenCurrentFile (BOOL fRaw, BOOL fSeekSupport, __in_opt PCSTR pcszPassword, __deref_out IZipFile** ppFile) = 0;
	virtual HRESULT GetUnzipObject (__deref_out interface ISimbeyUnzip** ppUnzip) = 0;
};

interface __declspec(uuid("00FD6151-EE34-4b18-88EB-DBB788F5D9B2")) IZipFS : IUnknown
{
	virtual HRESULT GetEnumerator (__deref_out IUnzipEnum** ppEnum) = 0;
	virtual HRESULT OpenReadOnly (PCWSTR pcwzFile, INT iCaseSensitivity, BOOL fRaw, BOOL fSeekSupport, __in_opt PCSTR pcszPassword, __deref_out IZipFile** ppFile) = 0;
	virtual HRESULT OpenWritable (PCWSTR pcwzFile, DWORD dwCreationDisposition, __deref_out IZipFile** ppFile) = 0;
};

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
typedef struct unz_global_info64_s
{
    ULONGLONG number_entry;      /* total number of entries in
                                     the central dir on this disk */
    ULONG size_comment;         /* size of the global comment of the zipfile */
} unz_global_info64;

typedef struct unz_global_info_s
{
    ULONG number_entry;         /* total number of entries in
                                     the central dir on this disk */
    ULONG size_comment;         /* size of the global comment of the zipfile */
} unz_global_info;

interface __declspec(uuid("80BE2F62-FCDF-43a0-90C7-8A2685AC0CF9")) ISimbeyUnzip : IZipFS
{
	virtual HRESULT unzGetGlobalInfo (unz_global_info* pglobal_info) = 0;
	virtual HRESULT unzGetGlobalInfo64 (unz_global_info64* pglobal_info) = 0;

	virtual HRESULT unzGetGlobalComment (char* szComment, ULONG uSizeBuf, __out INT* pcchComment) = 0;

	virtual HRESULT unzGoToFirstFile (VOID) = 0;
	virtual HRESULT unzGoToNextFile (VOID) = 0;

	virtual HRESULT unzLocateFile (PCWSTR pcwzFileName, int iCaseSensitivity) = 0;
	virtual HRESULT unzEnumFiles (BOOL fListFromStart, PFNUNZIPENUM pfnEnum, PVOID pvParam) = 0;

	virtual HRESULT unzGetFilePos64 (unz64_file_pos* file_pos) = 0;
	virtual HRESULT unzGetFilePos (unz_file_pos* file_pos) = 0;

	virtual HRESULT unzGoToFilePos64 (const unz64_file_pos* file_pos) = 0;
	virtual HRESULT unzGoToFilePos (const unz_file_pos* file_pos) = 0;

	virtual HRESULT unzGetCurrentFileInfo64 (unz_file_info64 *pfile_info,
		PWSTR pwzFileName,
		ULONG fileNameBufferSize,
		__out_opt INT* pcchFileName,
		void *extraField,
		ULONG extraFieldBufferSize,
		char *szComment,
		ULONG commentBufferSize) = 0;
	virtual HRESULT unzGetCurrentFileInfo (unz_file_info *pfile_info,
		PWSTR pwzFileName,
		ULONG fileNameBufferSize,
		__out_opt INT* pcchFileName,
		void *extraField,
		ULONG extraFieldBufferSize,
		char *szComment,
		ULONG commentBufferSize) = 0;
	virtual ULONGLONG unzGetCurrentFileZStreamPos64 (VOID) = 0;

	virtual HRESULT unzOpenCurrentFile (VOID) = 0;
	virtual HRESULT unzOpenCurrentFilePassword (const char* password) = 0;
	virtual HRESULT unzOpenCurrentFile2 (int* method, int* level, int raw) = 0;
	virtual HRESULT unzOpenCurrentFile3 (int* method, int* level, int raw, const char* password) = 0;
	virtual HRESULT unzCloseCurrentFile (VOID) = 0;

	virtual HRESULT unzReadCurrentFile (PVOID buf, DWORD len, __out DWORD* pcbRead) = 0;

	virtual LONG unztell (VOID) = 0;
	virtual ULONGLONG unztell64 (VOID) = 0;
	virtual HRESULT unzeof (__out bool* pfEOF) = 0;

	virtual HRESULT unzGetLocalExtrafield (PVOID buf, DWORD len, __out DWORD* pcbRead) = 0;

	virtual ULONGLONG unzGetOffset64 (VOID) = 0;
	virtual ULONG unzGetOffset (VOID) = 0;
	virtual HRESULT unzSetOffset64 (ULONGLONG pos) = 0;
	virtual HRESULT unzSetOffset (ULONG pos) = 0;
};

interface __declspec(uuid("B89FA89A-D9DD-4962-98D7-7A886AF264A5")) ISimbeyZip : IUnknown
{
	virtual PCSTR zipGetGlobalComment (VOID) = 0;
	virtual HRESULT zipSetGlobalComment (PCSTR pcszComment, INT cchComment) = 0;

	virtual HRESULT zipOpenNewFile (PCWSTR pcwzFileName, const SYSTEMTIME* pcst,
		const VOID* extrafield_local, DWORD size_extrafield_local,
        const VOID* extrafield_global, DWORD size_extrafield_global,
        PCSTR pcszComment, INT method, INT level, INT raw) = 0;
	virtual HRESULT zipOpenNewFilePassword (PCWSTR pcwzFileName, const SYSTEMTIME* pcst,
		const VOID* extrafield_local, DWORD size_extrafield_local,
        const VOID* extrafield_global, DWORD size_extrafield_global,
        PCSTR pcszComment, INT method, INT level, INT raw,
		PCSTR pcszPassword, ULONG crcForCrypting) = 0;
	virtual HRESULT zipCloseFileInZip (VOID) = 0;
	virtual HRESULT zipCloseFileInZipRaw (DWORD uncompressed_size, DWORD crc32) = 0;
	virtual HRESULT zipWriteInFileInZip (const VOID* pcData, DWORD cb) = 0;
	virtual BOOL zipFileOpen (VOID) = 0;
};

HRESULT WINAPI szCreateStandardFS (__deref_out IZipFS** ppStandardFS);
HRESULT WINAPI szUnzipOpen (IZipFS* pFS, PCWSTR pcwzPath, __deref_out ISimbeyUnzip** ppUnzip);
HRESULT WINAPI szUnzipFile (IZipFile* pZipFile, __deref_out ISimbeyUnzip** ppUnzip);
HRESULT WINAPI szZipOpen (IZipFS* pFS, PCWSTR pcwzPath, SimbeyZip::Append eAppend, __deref_out ISimbeyZip** ppZip);
HRESULT WINAPI szCreateMemoryFile (HANDLE hHeap, PBYTE pbData, ULONG cbData, __deref_out IZipFile** ppFile);

HRESULT WINAPI szDeflateInitStream (__inout PVOID pvzStream, PVOID pvOpaque, INT nLevel, INT cWindowBits, INT nMemLevel, INT nStrategy);
HRESULT WINAPI szInflateInitStream (__inout PVOID pvzStream, PVOID pvOpaque, INT cWindowBits);

HRESULT WINAPI szAddStreamToZip (ISequentialStream* pstmData, ULONGLONG cbData, ISimbeyZip* pZip, PCWSTR pcwzTargetFile, __in const SYSTEMTIME* pcst, INT nLevel);
HRESULT WINAPI szAddFileToZip (PCWSTR pcwzSourceFile, ISimbeyZip* pZip, PCWSTR pcwzTargetFile, __in_opt const SYSTEMTIME* pcst, INT nLevel, __out_opt ULONGLONG* pcbData);

VOID WINAPI szSetErrorTracer (ASSERT_FAILURE_CALLBACK lpfnTracer);
