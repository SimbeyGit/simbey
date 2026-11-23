#include <mmsystem.h>
#include "Library\Core\ILockableStream.h"

// Two silent, unsigned 8-bit PCM samples. Tests never send these to a device.
static const BYTE c_rgSoundWave[] =
{
	'R','I','F','F',38,0,0,0,'W','A','V','E',
	'f','m','t',' ',16,0,0,0,1,0,1,0,64,31,0,0,64,31,0,0,1,0,8,0,
	'd','a','t','a',2,0,0,0,128,128
};

struct SoundProbe
{
	UINT calls;
	UINT stops;
	DWORD flags;
	BOOL fail;
	BOOL corrupt;
	BOOL filename;
	BOOL shutdownOnStop;
	const BYTE* playing;
	BYTE expected[sizeof(c_rgSoundWave)];
	SoundProbe()
	{
		ZeroMemory(this,sizeof(*this));
	}
};

class CQuietSoundHost : public CAQVMHost
{
	SoundProbe* m_probe;
protected:
	virtual BOOL CallPlaySound(PCWSTR sound, DWORD flags)
	{
		if(!sound)
		{
			m_probe->stops++;
			if(m_probe->shutdownOnStop)
			{
				m_probe->shutdownOnStop = FALSE;
				ShutdownApplication();
			}
			if(m_probe->playing && memcmp(m_probe->playing,m_probe->expected,sizeof(m_probe->expected)))
			{
				m_probe->corrupt = TRUE;
			}
			m_probe->playing = NULL;
			return TRUE;
		}
		m_probe->calls++;
		m_probe->flags = flags;
		if(m_probe->fail)
		{
			m_probe->fail = FALSE;
			return FALSE;
		}
		if(flags & SND_MEMORY)
		{
			CopyMemory(m_probe->expected,sound,sizeof(m_probe->expected));
			m_probe->playing = flags & SND_ASYNC ? reinterpret_cast<const BYTE*>(sound) : NULL;
		}
		else
		{
			m_probe->filename = !lstrcmpW(sound,L"quiet.wav");
			m_probe->playing = NULL;
		}
		return TRUE;
	}
public:
	CQuietSoundHost(SoundProbe* probe) : m_probe(probe)
	{
	}
	~CQuietSoundHost()
	{
		// Exercise the virtual backend before destruction reaches the base.
		ShutdownApplication();
	}
};

class CMutableSoundBinary : public CBaseUnknown, public CQuadooObjectImpl, public ILockableStream
{
	UINT* m_destroyed;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(ILockableStream)
	END_UNK_MAP
	BYTE bytes[sizeof(c_rgSoundWave)];
	DWORD count;
	UINT locks;
	UINT unlocks;
	HRESULT lockError;
	HRESULT nested;
	CAQVMHost* reenter;
	BOOL shutdown;
	CMutableSoundBinary(UINT* destroyed) : m_destroyed(destroyed), count(sizeof(bytes)), locks(0), unlocks(0),
		lockError(S_OK), nested(S_OK), reenter(NULL), shutdown(FALSE)
	{
		CopyMemory(bytes,c_rgSoundWave,sizeof(bytes));
	}
	~CMutableSoundBinary()
	{
		(*m_destroyed)++;
	}
	STDMETHOD(Read)(PVOID, ULONG, ULONG*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(Write)(const VOID*, ULONG, ULONG*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(Lock)(BYTE** data, DWORD* size)
	{
		HRESULT hr;
		locks++;
		*data = NULL;
		*size = 0;
		Check(lockError);
		if(reenter && !shutdown)
		{
			QuadooVM::QVARIANT argument = {QuadooVM::String};
			QuadooVM::QVARIANT result = {QuadooVM::Null};
			QuadooVM::QVPARAMS parameters = {&argument,1};
			argument.rstrVal = RSTRING_CAST(L"quiet.wav");
			nested = reenter->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result);
			QVMClearVariant(&result);
		}
		*data = bytes;
		*size = count;
		hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHOD_(VOID,Unlock)()
	{
		unlocks++;
		if(reenter && shutdown)
		{
			reenter->ShutdownApplication();
		}
	}
};

struct SoundThreadProbe
{
	CAQVMHost* host;
	HRESULT result;
};

static DWORD WINAPI SoundWrongThread(PVOID context)
{
	SoundThreadProbe* probe = static_cast<SoundThreadProbe*>(context);
	QuadooVM::QVARIANT argument = {QuadooVM::String};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {&argument,1};
	argument.rstrVal = RSTRING_CAST(L"quiet.wav");
	probe->result = probe->host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result);
	QVMClearVariant(&result);
	return 0;
}

static HRESULT SoundNativeTests()
{
	HRESULT hr;
	CRString embedded;
	SoundProbe probe;
	UINT destroyed = 0;
	TStackRef<CQuietSoundHost> host;
	TStackRef<CMutableSoundBinary> binary;
	QuadooVM::QVARIANT argument = {QuadooVM::String};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {&argument,1};
	HANDLE thread = NULL;
	host.Attach(__new CQuietSoundHost(&probe));
	CheckAlloc(host);
	argument.rstrVal = RSTRING_CAST(L"quiet.wav");
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result));
	REQUIRE(result.eType == QuadooVM::Bool && result.fVal && probe.filename && probe.flags == (SND_FILENAME | SND_SYNC | SND_NODEFAULT));
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	REQUIRE(result.eType == QuadooVM::Bool && result.fVal && probe.flags == (SND_FILENAME | SND_ASYNC | SND_NODEFAULT));
	parameters.cArgs = 0;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == DISP_E_BADPARAMCOUNT);
	parameters.cArgs = 2;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result) == DISP_E_BADPARAMCOUNT);
	parameters.cArgs = 1;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),NULL,&result) == E_POINTER);
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,NULL) == E_POINTER);
	parameters.pqvArgs = NULL;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_POINTER);
	parameters.pqvArgs = &argument;
	argument.eType = QuadooVM::I4;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == DISP_E_TYPEMISMATCH);
	argument.eType = QuadooVM::String;
	argument.rstrVal = RSTRING_CAST(L"");
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_INVALIDARG);
	Check(RStrCreateW(LSP(L"quiet\0wav"),&embedded));
	argument.rstrVal = embedded;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_INVALIDARG);
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result) == E_INVALIDARG);
	embedded.Clear();
	Check(RStrCreateA(LSP("quiet\0wav"),&embedded));
	argument.rstrVal = embedded;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_INVALIDARG);
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result) == E_INVALIDARG);
	argument.eType = QuadooVM::Object;
	argument.pObject = host;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_NOINTERFACE);
	REQUIRE(probe.calls == 2 && !probe.stops);
	binary.Attach(__new CMutableSoundBinary(&destroyed));
	CheckAlloc(binary);
	argument.pObject = binary;
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result));
	REQUIRE(result.fVal && probe.flags == (SND_MEMORY | SND_SYNC | SND_NODEFAULT) && probe.stops == 1);
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	REQUIRE(result.fVal && probe.flags == (SND_MEMORY | SND_ASYNC | SND_NODEFAULT));
	REQUIRE(binary->locks == 2 && binary->unlocks == 2 && probe.playing && probe.playing != binary->bytes);
	ZeroMemory(binary->bytes,sizeof(binary->bytes));
	REQUIRE(!memcmp(probe.playing,c_rgSoundWave,sizeof(c_rgSoundWave)));
	binary.Release();
	REQUIRE(destroyed == 1 && !memcmp(probe.playing,c_rgSoundWave,sizeof(c_rgSoundWave)));
	binary.Attach(__new CMutableSoundBinary(&destroyed));
	CheckAlloc(binary);
	argument.pObject = binary;
	for(DWORD size = 0; size < sizeof(c_rgSoundWave); size++)
	{
		binary->count = size;
		REQUIRE(FAILED(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result)));
	}
	binary->count = sizeof(c_rgSoundWave);
	binary->bytes[40] = 255;
	REQUIRE(FAILED(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result)));
	binary->bytes[40] = 2;
	binary->bytes[16] = 255;
	REQUIRE(FAILED(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result)));
	CopyMemory(binary->bytes,c_rgSoundWave,sizeof(binary->bytes));
	FillMemory(binary->bytes + 4,4,255);
	REQUIRE(FAILED(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result)));
	CopyMemory(binary->bytes,c_rgSoundWave,sizeof(binary->bytes));
	binary->lockError = E_ACCESSDENIED;
	UINT unlocks = binary->unlocks;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == E_ACCESSDENIED);
	REQUIRE(binary->unlocks == unlocks && probe.calls == 4 && probe.stops == 1);
	binary->lockError = S_OK;
	binary->reenter = host;
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	REQUIRE(binary->nested == QuadooForm::EInvalidState && probe.stops == 2 && !probe.corrupt);
	binary->reenter = NULL;
	probe.fail = TRUE;
	Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	REQUIRE(result.eType == QuadooVM::Bool && !result.fVal && probe.stops == 3 && !probe.playing && !probe.corrupt);
	for(UINT n = 0; n < 32; n++)
	{
		Check(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	}
	{
		SoundThreadProbe worker = {host,S_OK};
		thread = CreateThread(NULL,0,SoundWrongThread,&worker,0,NULL);
		CheckIf(!thread,HrEnsureLastError());
		REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0 && worker.result == QuadooForm::EWrongThread);
		CloseHandle(thread);
		thread = NULL;
	}
	binary->reenter = host;
	binary->shutdown = TRUE;
	UINT calls = probe.calls;
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result) == QuadooForm::EDestroyed);
	REQUIRE(probe.calls == calls && !probe.playing && !probe.corrupt);
	REQUIRE(host->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == QuadooForm::EDestroyed);
	host->ShutdownApplication();
	host.Release();
	REQUIRE(!probe.playing && !probe.corrupt);
	binary.Release();
	REQUIRE(destroyed == 2);
	hr = S_OK;
Cleanup:
	if(thread)
	{
		CloseHandle(thread);
	}
	QVMClearVariant(&result);
	return hr;
}

static HRESULT SoundScriptTests()
{
	HRESULT hr;
	SoundProbe probe;
	TStackRef<CQuietSoundHost> host;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	CMemoryStream code;
	CCompilerStatus compiler;
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	static const WCHAR script[] =
		L"function SoundChecks()\n{\n"
		L"\tvar wave = Host.FileFromHex(null, \"524946462600000057415645666d74201000000001000100401f0000401f00000100080064617461020000008080\");\n"
		L"\tif(!Host.PlaySound(\"quiet.wav\") || !Host.PlaySoundAsync(\"quiet.wav\"))\n\t{\n\t\treturn false;\n\t}\n"
		L"\tif(!Host.PlaySound(wave) || !Host.PlaySoundAsync(wave))\n\t{\n\t\treturn false;\n\t}\n"
		L"\twave.Data[44] = 0;\n"
		L"\tvar errors = 0;\n"
		L"\ttry\n\t{\n\t\tHost.PlaySound();\n\t}\n\tcatch(error)\n\t{\n\t\terrors++;\n\t}\n"
		L"\ttry\n\t{\n\t\tHost.PlaySoundAsync(42);\n\t}\n\tcatch(error)\n\t{\n\t\terrors++;\n\t}\n"
		L"\treturn errors == 2;\n}\n";
	host.Attach(__new CQuietSoundHost(&probe));
	CheckAlloc(host);
	Check(QuadooParseTextToStream(SLP(L"SoundTests.quadoo"),SLP(script),0,&code,NULL,&compiler));
	Check(QVMCreateLoader(NULL,&loader));
	Check(loader->AddInstance(RSTRING_CAST(L"SoundTests"),NULL,&code,code.DataRemaining(),NULL));
	HRESULT registered;
	Check(loader->LoadVM(RSTRING_CAST(L"SoundTests"),&registered,&vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Host"),host));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(Call(vm,L"SoundChecks",&result));
	REQUIRE(result.eType == QuadooVM::Bool && result.fVal && probe.calls == 4);
	REQUIRE(probe.playing && !memcmp(probe.playing,c_rgSoundWave,sizeof(c_rgSoundWave)));
	host->ShutdownApplication();
	REQUIRE(!probe.playing && !probe.corrupt && probe.stops == 2);
	hr = S_OK;
Cleanup:
	QVMClearVariant(&result);
	if(host)
	{
		host->ShutdownApplication();
	}
	if(vm)
	{
		vm->Unload();
	}
	return hr;
}

static HRESULT SoundTests()
{
	HRESULT hr;
	CRString filenameUtf8;
	TStackRef<CAQVMHost> native;
	QuadooVM::QVARIANT argument = {QuadooVM::String};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {&argument,1};
	Check(SoundNativeTests());
	Check(SoundScriptTests());
	// Real WinMM failure path only: NODEFAULT prevents audible fallback.
	native.Attach(__new CAQVMHost);
	CheckAlloc(native);
	argument.rstrVal = RSTRING_CAST(L"?:\\QuadooMissingSound.wav");
	Check(native->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result));
	REQUIRE(result.eType == QuadooVM::Bool && !result.fVal);
	Check(native->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result));
	// Some WinMM versions accept asynchronous work before discovering that
	// the file is unavailable. Only the immediate native result is exposed.
	REQUIRE(result.eType == QuadooVM::Bool);
	native->ShutdownApplication();
	{
		SoundProbe probe;
		TStackRef<CQuietSoundHost> owner;
		QuadooVM::QVARIANT wave = {QuadooVM::Null};
		Check(SpriteBinary(c_rgSoundWave,sizeof(c_rgSoundWave),&wave));
		parameters.pqvArgs = &wave;
		owner.Attach(__new CQuietSoundHost(&probe));
		if(owner)
		{
			hr = owner->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result);
		}
		else
		{
			hr = E_OUTOFMEMORY;
		}
		QVMClearVariant(&wave);
		Check(hr);
		argument.eType = QuadooVM::String;
		Check(RStrCreateA(LSP("quiet.wav"),&filenameUtf8));
		argument.rstrVal = filenameUtf8;
		parameters.pqvArgs = &argument;
		Check(owner->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result));
		REQUIRE(result.fVal && probe.filename);
		Check(SpriteBinary(c_rgSoundWave,sizeof(c_rgSoundWave),&wave));
		parameters.pqvArgs = &wave;
		hr = owner->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result);
		QVMClearVariant(&wave);
		Check(hr);
		owner.Release();
		REQUIRE(probe.stops == 2 && !probe.playing && !probe.corrupt);
	}
	{
		SoundProbe probe;
		TStackRef<CQuietSoundHost> owner;
		QuadooVM::QVARIANT wave = {QuadooVM::Null};
		Check(SpriteBinary(c_rgSoundWave,sizeof(c_rgSoundWave),&wave));
		parameters.pqvArgs = &wave;
		owner.Attach(__new CQuietSoundHost(&probe));
		if(owner)
		{
			hr = owner->Invoke(NULL,RSTRING_CAST(L"PlaySoundAsync"),&parameters,&result);
		}
		else
		{
			hr = E_OUTOFMEMORY;
		}
		QVMClearVariant(&wave);
		Check(hr);
		parameters.pqvArgs = &argument;
		probe.shutdownOnStop = TRUE;
		REQUIRE(owner->Invoke(NULL,RSTRING_CAST(L"PlaySound"),&parameters,&result) == QuadooForm::EDestroyed);
		REQUIRE(probe.calls == 1 && probe.stops == 1 && !probe.playing && !probe.corrupt);
	}
	hr = S_OK;
Cleanup:
	QVMClearVariant(&result);
	return hr;
}
