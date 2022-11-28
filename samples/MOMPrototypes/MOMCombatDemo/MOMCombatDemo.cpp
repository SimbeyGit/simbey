#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\TextHelpers.h"
#include "Published\JSON.h"
#include "Published\QuadooParser.h"
#include "IntroScreen.h"
#include "MOMCombatDemo.h"

#define	GAME_TICK_MS		33

const WCHAR c_wzSIFClass[] = L"MOMCombatDemoCls";
const WCHAR c_wzSIFTitle[] = L"MOM Combat Demo";

const WCHAR c_wzCombatScriptPath[] = L"Assets\\scripts\\combat.quadoo";

class CParserStatus : public IQuadooCompilerStatus
{
public:
	CMemoryStream m_stmErrors;

public:
	CParserStatus ()
	{
	}

	~CParserStatus ()
	{
	}

	// IQuadooCompilerStatus
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError)
	{
		Stream::TPrintF(&m_stmErrors, L"%ls at %d of %ls, Code:0x%.8X\r\n", pcwzError, nLine, pcwzFile, hrCode);
	}
	virtual STDMETHODIMP OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath)
	{
		return E_NOTIMPL;
	}
};

CMOMCombatDemo::CMOMCombatDemo (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pPlacements(NULL),
	m_pSurface(NULL),
	m_pPackage(NULL),
	m_fActive(FALSE),
	m_pSystem(NULL),
	m_pYellowFont(NULL),
	m_pSmallYellowFont(NULL),
	m_pVM(NULL),
	m_pScreen(NULL)
{
	ZeroMemory(m_fKeys, sizeof(m_fKeys));
}

CMOMCombatDemo::~CMOMCombatDemo ()
{
	Assert(NULL == m_pScreen);
	Assert(NULL == m_pVM);

	SafeRelease(m_pSmallYellowFont);
	SafeRelease(m_pYellowFont);

	for(sysint i = 0; i < m_mapSounds.Length(); i++)
		(*m_mapSounds.GetValuePtr(i))->release();
	for(sysint i = 0; i < m_mapIntro.Length(); i++)
		(*m_mapIntro.GetValuePtr(i))->release();
	if(m_pSystem)
		m_pSystem->release();

	SafeRelease(m_pPackage);
	SafeRelease(m_pSurface);
	SafeRelease(m_pPlacements);
}

HRESULT CMOMCombatDemo::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzSIFClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CMOMCombatDemo::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzSIFClass, hInstance);
}

HRESULT CMOMCombatDemo::Initialize (INT nWidth, INT nHeight, PCWSTR pcwzCmdLine, INT nCmdShow)
{
	HRESULT hr;
	RECT rect = { 0, 0, nWidth, nHeight };
	ISimbeyInterchangeFile* pSIF = NULL;
	CIntroScreen* pScreen = NULL;

	m_pSurface = __new CInteractiveSurface(512, 320);
	CheckAlloc(m_pSurface);
	m_pSurface->EnableClear(RGB(255, 255, 255));

	Check(LoadPlacements(pcwzCmdLine));
	Check(LoadPackage());

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzSIFClass, c_wzSIFTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

	Check(m_pPackage->OpenSIF(L"graphics\\YellowFont.sif", &pSIF));
	Check(sifCreateFontFromSIF(pSIF, TRUE, &m_pYellowFont));
	SafeRelease(pSIF);

	Check(m_pPackage->OpenSIF(L"graphics\\SmallYellowFont.sif", &pSIF));
	Check(sifCreateFontFromSIF(pSIF, TRUE, &m_pSmallYellowFont));
	SafeRelease(pSIF);

	Check(LoadSounds());
	Check(m_player.Initialize());

	srand(GetTickCount());

	Check(LoadScript());

	pScreen = __new CIntroScreen(this, m_pSurface, m_pPackage, m_pYellowFont, m_pSmallYellowFont);
	CheckAlloc(pScreen);
	Check(pScreen->Initialize());
	m_pScreen = pScreen;
	pScreen = NULL;

Cleanup:
	if(FAILED(hr) && m_hwnd)
		Destroy();

	SafeRelease(pScreen);
	SafeRelease(pSIF);
	return hr;
}

VOID CMOMCombatDemo::Run (VOID)
{
	MSG msg;
	DWORD dwTimer = 0;

	for(;;)
	{
		while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
				return;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if(m_fActive)
		{
			DWORD dwNow = timeGetTime();
			DWORD dwFrame = dwNow - dwTimer;
			if(dwFrame >= GAME_TICK_MS || WAIT_TIMEOUT == MsgWaitForMultipleObjects(0,NULL,FALSE,GAME_TICK_MS - dwFrame,QS_ALLINPUT))
			{
				dwTimer = dwNow;

				m_pSurface->Tick();
				m_pScreen->OnUpdateFrame();
				m_pSystem->update();
				m_pSurface->Redraw(m_hwnd, NULL);
			}
		}
		else
			WaitMessage();
	}
}

// IQuadooObject

HRESULT STDMETHODCALLTYPE CMOMCombatDemo::Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(rstrMethod);

	if(0 == TStrCmpAssert(pcwzMethod, L"Display"))
	{
		RSTRING rstrValue;
		CheckIf(1 != pqvParams->cArgs, DISP_E_BADPARAMCOUNT);
		Check(QVMVariantToString(pqvParams->pqvArgs, &rstrValue));
		MessageBox(m_hwnd, RStrToWide(rstrValue), L"Debug Display", MB_OK);
		RStrRelease(rstrValue);
	}
	else
		hr = DISP_E_UNKNOWNNAME;

Cleanup:
	return hr;
}

// IScreenHost

bool CMOMCombatDemo::IsPressed (BYTE nKey)
{
	return m_fKeys[nKey];
}

HRESULT CMOMCombatDemo::LoadMIDI (ISequentialStream* pstmMIDI, const ULARGE_INTEGER* puliSize, MIDI::CFile* pmidiFile)
{
	return MIDI::LoadMIDI(pstmMIDI, puliSize, pmidiFile);
}

HRESULT CMOMCombatDemo::PlayMIDI (MIDI::CFile* pmidiFile, __in_opt MIDI::MIDI_RESUME* pcmResume)
{
	return m_player.StartFromFile(this, pmidiFile, pcmResume);
}

HRESULT CMOMCombatDemo::StopMIDI (__out_opt MIDI::MIDI_RESUME* pmResume)
{
	return m_player.Stop(pmResume);
}

HRESULT CMOMCombatDemo::FindSound (RSTRING rstrValue, FMOD::Sound** ppSound)
{
	return m_mapSounds.Find(rstrValue, ppSound);
}

HRESULT CMOMCombatDemo::FindIntroSound (RSTRING rstrValue, FMOD::Sound** ppSound)
{
	return m_mapIntro.Find(rstrValue, ppSound);
}

VOID CMOMCombatDemo::PlaySound (FMOD_CHANNELINDEX idxChannel, FMOD::Sound* pSound, bool fPaused, FMOD::Channel** ppChannel)
{
	m_pSystem->playSound(idxChannel, pSound, fPaused, ppChannel);
}

VOID CMOMCombatDemo::SwitchToScreen (IScreen* pScreen)
{
	if(m_pScreen)
		m_pScreen->OnDestroy();
	ReplaceInterface(m_pScreen, pScreen);
}

HRESULT CMOMCombatDemo::GetData (PCWSTR pcwzName, __deref_out IJSONObject** ppData)
{
	HRESULT hr;

	if(0 == TStrCmpAssert(pcwzName, L"placements"))
	{
		SetInterface(*ppData, m_pPlacements);
		hr = S_OK;
	}
	else
		hr = DISP_E_UNKNOWNNAME;

	return hr;
}

HINSTANCE CMOMCombatDemo::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CMOMCombatDemo::OnFinalDestroy (HWND hwnd)
{
	if(m_pScreen)
	{
		m_pScreen->OnDestroy();
		SafeRelease(m_pScreen);
	}

	if(m_pVM)
	{
		m_pVM->Unload();
		SafeRelease(m_pVM);
	}

	m_pSurface->Destroy();
	m_player.Stop();

	PostQuitMessage(0);
}

HRESULT CMOMCombatDemo::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CMOMCombatDemo::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CMOMCombatDemo::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	const RECT* prcUnpainted;
	INT cUnpainted;
	m_pSurface->GetUnpaintedRects(&prcUnpainted, &cUnpainted);

	for(INT i = 0; i < cUnpainted; i++)
		FillRect(hdc, prcUnpainted + i, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

	m_pSurface->Redraw(m_hwnd, hdc);
	EndPaint(m_hwnd, &ps);
	lResult = TRUE;
	return TRUE;
}

BOOL CMOMCombatDemo::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CMOMCombatDemo::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_pSurface->Position(LOWORD(lParam), HIWORD(lParam));
	return FALSE;
}

BOOL CMOMCombatDemo::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CMOMCombatDemo::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	if(m_pScreen)
		m_pScreen->OnChangeActive(m_fActive);
	return FALSE;
}

BOOL CMOMCombatDemo::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = true;
	return m_pSurface->ProcessLayerInput(uMsg, wParam, lParam, lResult);
}

BOOL CMOMCombatDemo::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = false;
	return m_pSurface->ProcessLayerInput(uMsg, wParam, lParam, lResult);
}

BOOL CMOMCombatDemo::OnSetCursor (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(HTCLIENT == LOWORD(lParam))
	{
		const RECT* prcUnpainted;
		INT cUnpainted;

		m_pSurface->GetUnpaintedRects(&prcUnpainted, &cUnpainted);
		if(0 < cUnpainted)
		{
			POINT pt;

			if(GetCursorPos(&pt))
			{
				ScreenToClient(m_hwnd, &pt);
				for(INT i = 0; i < cUnpainted; i++)
				{
					if(PtInRect(prcUnpainted + i, pt))
					{
						SetCursor(LoadCursor(NULL, IDC_ARROW));
						return FALSE;
					}
				}
			}
		}

		SetCursor(NULL);
		lResult = TRUE;
		return TRUE;
	}
	return FALSE;
}

// INotifyFinished

VOID CMOMCombatDemo::OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted)
{
	if(m_pScreen)
		m_pScreen->OnNotifyFinished(fCompleted);
}

HRESULT CMOMCombatDemo::LoadPlacements (PCWSTR pcwzCmdLine)
{
	HRESULT hr;
	WCHAR wzCustomPlacements[MAX_PATH];
	PWSTR pwzJSON = NULL;
	INT cchJSON;
	TStackRef<IJSONValue> srv;

	// The user can drop a "Placements.json" file onto the executable to load that file instead of the default.
	if(L'\0' != *pcwzCmdLine)
	{
		if(L'"' == *pcwzCmdLine)
		{
			PCWSTR pcwzEnd = TStrChr(++pcwzCmdLine, L'"');
			CheckIf(NULL == pcwzEnd, E_INVALIDARG);
			Check(TStrCchCpyLen(wzCustomPlacements, ARRAYSIZE(wzCustomPlacements), pcwzCmdLine, static_cast<INT>(pcwzEnd - pcwzCmdLine), NULL));
		}
		else
			Check(TStrCchCpy(wzCustomPlacements, ARRAYSIZE(wzCustomPlacements), pcwzCmdLine));
	}

	if(L'\0' != *pcwzCmdLine && INVALID_FILE_ATTRIBUTES != GetFileAttributes(wzCustomPlacements))
		Check(Text::LoadFromFile(wzCustomPlacements, &pwzJSON, &cchJSON));
	else
	{
		// Look for Placements.json first in the "Assets" folder and then in the current folder
		if(FAILED(Text::LoadFromFile(L"Assets\\Placements.json", &pwzJSON, &cchJSON)))
			Check(Text::LoadFromFile(L"Placements.json", &pwzJSON, &cchJSON));
	}
	Check(JSONParse(NULL, pwzJSON, cchJSON, &srv));
	Check(srv->GetObject(&m_pPlacements));

Cleanup:
	SafeDeleteArray(pwzJSON);
	return hr;
}

HRESULT CMOMCombatDemo::LoadPackage (VOID)
{
	HRESULT hr;
	bool fMissingPackage = false;

	m_pPackage = __new CSIFPackage;
	CheckAlloc(m_pPackage);

	// This file must be built by SIFPackage.exe
	if(GetFileAttributes(L"Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"Assets.pkg"));
	else if(GetFileAttributes(L"..\\Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"..\\Assets.pkg"));
	else if(GetFileAttributes(L"Assets\\Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"Assets\\Assets.pkg"));
	else
	{
		fMissingPackage = true;
		MessageBox(GetDesktopWindow(), L"Could not find Assets.pkg!", L"Missing Data Package", MB_ICONERROR | MB_OK);
		Check(HRESULT_FROM_WIN32(ERROR_MISSING_SYSTEMFILE));
	}

Cleanup:
	if(FAILED(hr) && !fMissingPackage)
	{
		WCHAR wzError[100];
		if(SUCCEEDED(Formatting::TPrintF(wzError, ARRAYSIZE(wzError), NULL, L"Could not load Assets.pkg due to error: 0x%.8X!", hr)))
			MessageBox(GetDesktopWindow(), wzError, L"Data Package Error", MB_ICONERROR | MB_OK);
	}
	return hr;
}

HRESULT CMOMCombatDemo::LoadSounds (VOID)
{
	HRESULT hr;
	INT cDrivers;

	CheckIf(FMOD_OK != FMOD::System_Create(&m_pSystem), E_FAIL);
	CheckIf(FMOD_OK != m_pSystem->getNumDrivers(&cDrivers), E_FAIL);
	CheckIf(0 == cDrivers, E_FAIL);
	CheckIf(FMOD_OK != m_pSystem->init(8, FMOD_INIT_NORMAL, NULL), E_FAIL);

	Check(LoadSoundFiles(SLP(L"sounds"), m_mapSounds));
	Check(LoadSoundFiles(SLP(L"intro\\voices"), m_mapIntro));

Cleanup:
	return hr;
}

HRESULT CMOMCombatDemo::LoadSoundFiles (PCWSTR pcwzSounds, INT cchSounds, TRStrMap<FMOD::Sound*>& mapSounds)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srSubPackage;
	TStackRef<IJSONArray> srArray;
	TStackRef<IJSONValue> srv;
	sysint cItems;
	RSTRING rstrType = NULL, rstrName = NULL, rstrFile = NULL;
	CMemoryStream stmData;
	FMOD_CREATESOUNDEXINFO exInfo;

	ZeroMemory(&exInfo, sizeof(exInfo));
	exInfo.cbsize = sizeof(exInfo);

	Check(m_pPackage->OpenDirectory(pcwzSounds, cchSounds, &srSubPackage));
	Check(srSubPackage->GetDirectory(&srArray));

	cItems = srArray->Count();
	for(sysint i = 0; i < cItems; i++)
	{
		TStackRef<IJSONObject> srSound;

		Check(srArray->GetObject(i, &srSound));
		if(SUCCEEDED(srSound->FindNonNullValueW(L"type", &srv)))
		{
			Check(srv->GetString(&rstrType));
			srv.Release();

			if(0 == TStrCmpAssert(RStrToWide(rstrType), L"sound"))
			{
				FMOD::Sound* pSound;
				FMOD_RESULT eResult;

				Check(srSound->FindNonNullValueW(L"name", &srv));
				Check(srv->GetString(&rstrName));
				srv.Release();

				PCWSTR pcwzName = RStrToWide(rstrName);

				stmData.Reset();
				Check(srSubPackage->ReadFile(RStrToWide(rstrName), RStrLen(rstrName), &stmData));
				exInfo.length = stmData.DataRemaining();

				eResult = m_pSystem->createSound(stmData.TGetReadPtr<CHAR>(), FMOD_HARDWARE | FMOD_OPENMEMORY, &exInfo, &pSound);
				CheckIf(FMOD_OK != eResult, E_FAIL);

				pSound->setMode(FMOD_LOOP_OFF);
				hr = mapSounds.Add(rstrName, pSound);
				if(FAILED(hr))
				{
					pSound->release();
					Check(hr);
				}

				RStrRelease(rstrFile); rstrFile = NULL;
				RStrRelease(rstrName); rstrName = NULL;
			}

			RStrRelease(rstrType); rstrType = NULL;
		}
	}

Cleanup:
	RStrRelease(rstrFile);
	RStrRelease(rstrName);
	RStrRelease(rstrType);
	return hr;
}

HRESULT CMOMCombatDemo::LoadScript (VOID)
{
	HRESULT hr;
	TStackRef<IQuadooInstanceLoader> srLoader;
	TStackRef<IQuadooObject> srException;
	HRESULT hrRegistered;
	RSTRING rstrCombat = NULL, rstrDbg = NULL;

	Check(RStrCreateW(LSP(L"Combat"), &rstrCombat));
	Check(RStrCreateW(LSP(L"Dbg"), &rstrDbg));
	Check(QVMCreateLoader(NULL, &srLoader));

	if(INVALID_FILE_ATTRIBUTES == GetFileAttributes(c_wzCombatScriptPath))
	{
		TStackRef<IPersistedFile> srFile;
		ULARGE_INTEGER uliSize;

		Check(m_pPackage->OpenFile(SLP(L"scripts\\combat.qbc"), &srFile, &uliSize));
		Check(srLoader->AddInstance(rstrCombat, NULL, srFile, uliSize.LowPart, NULL));
	}
	else
	{
		CMemoryStream stmScript;
		CParserStatus status;
		HRESULT hrParse = QuadooParseToStream(c_wzCombatScriptPath, QUADOO_COMPILE_LINE_NUMBER_MAP, &stmScript, NULL, &status);
		if(FAILED(hrParse))
		{
			WCHAR wchNil = L'\0';
			ULONG cb;

			Check(status.m_stmErrors.TWrite(&wchNil, 1, &cb));
			MessageBox(m_hwnd, status.m_stmErrors.TGetReadPtr<WCHAR>(), L"Script Parser Error!", MB_OK);
			Check(hrParse);
		}
		Check(srLoader->AddInstance(rstrCombat, NULL, &stmScript, stmScript.DataRemaining(), NULL));
	}

	Check(srLoader->LoadVM(rstrCombat, &hrRegistered, &m_pVM));
	Check(m_pVM->AddGlobal(rstrDbg, this));
	Check(m_pVM->RunConstructor(&srException));

Cleanup:
	RStrRelease(rstrDbg);
	RStrRelease(rstrCombat);
	return hr;
}
