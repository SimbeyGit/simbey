#pragma once

class IZDBSPFile
{
public:
	virtual void Close (void) = 0;
	virtual size_t Read (void* pvData, size_t cbMaxData) = 0;
	virtual size_t Write (const void* pcvData, size_t cbData) = 0;
	virtual int Seek (long nOffset, int nOrigin) = 0;
	virtual long Tell (void) = 0;
};

class IZDBSPFS
{
public:
	virtual bool CheckInOutNames (const char* pcszInName, const char* pcszOutName) = 0;
	virtual int Remove (const char* pcszName) = 0;
	virtual int Rename (const char* pcszOld, const char* pcszNew) = 0;
	virtual IZDBSPFile* OpenForReading (const char* pcszFile) = 0;
	virtual IZDBSPFile* OpenForWriting (const char* pcszFile) = 0;
};

class IZDBSPLogger
{
public:
	virtual int Log (const char* pcszFormat, ...) = 0;
	virtual int LogV (const char* pcszFormat, va_list vArgs) = 0;
};

struct ZDBSPArgs
{
	int argc;
	char** argv;

	IZDBSPFS* pFS;
	IZDBSPLogger* pLogger;
};

int __stdcall zdbsp_core (ZDBSPArgs* pArgs);
