// Included by Harness.cpp. Resource tests use the published ABI and Win32 only.
struct GuiResourceCounts
{
	DWORD gdi, user;
	BOOL Same(const GuiResourceCounts& other) const { return gdi == other.gdi && user == other.user; }
	BOOL Within(const GuiResourceCounts& limit) const { return gdi <= limit.gdi && user <= limit.user; }
};
static HRESULT ReadGuiResourceCounts(GuiResourceCounts* counts)
{
	HRESULT hr = S_OK;
	SetLastError(0); counts->gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	CheckIf(!counts->gdi && GetLastError(), HrEnsureLastError());
	SetLastError(0); counts->user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
	CheckIf(!counts->user && GetLastError(), HrEnsureLastError());
Cleanup:
	return hr;
}
static HRESULT SettleGuiResources(const GuiResourceCounts* limit, GuiResourceCounts* counts, DWORD timeout = 2000)
{
	HRESULT hr;
	GuiResourceCounts previous;
	DWORD started = GetTickCount(), changed = started;
	Check(ReadGuiResourceCounts(&previous));
	for(;;)
	{
		CheckIf(PumpPendingMessages(), HRESULT_FROM_WIN32(ERROR_CANCELLED));
		GdiFlush(); Check(ReadGuiResourceCounts(counts));
		DWORD now = GetTickCount();
		if(!counts->Same(previous)) { previous = *counts; changed = now; }
		if(now - changed >= 100 && (!limit || counts->Within(*limit))) { hr = S_OK; break; }
		if(now - started >= timeout) { hr = limit ? S_FALSE : HRESULT_FROM_WIN32(ERROR_TIMEOUT); break; }
		// GdiFlush does not pump USER/input/theme messages or yield to their
		// producers. Keep the UI thread responsive while allowing cleanup time.
		DWORD wait = min(static_cast<DWORD>(10), timeout - (now - started));
		CheckIf(MsgWaitForMultipleObjects(0, NULL, FALSE, wait, QS_ALLINPUT) == WAIT_FAILED, HrEnsureLastError());
	}
Cleanup:
	return hr;
}
static BOOL CALLBACK LogResourceWindow(HWND window, LPARAM parameter)
{
	UINT* logged = reinterpret_cast<UINT*>(parameter);
	DWORD process; DWORD thread = GetWindowThreadProcessId(window, &process);
	if(process == GetCurrentProcessId() && *logged < 64)
	{
		WCHAR name[128] = { 0 }; CRString detail;
		GetClassNameW(window, name, ARRAYSIZE(name)); (*logged)++;
		if(SUCCEEDED(RStrFormatW(&detail, L"  HWND %p, thread %u, class %ls\r\n", window, thread, name))) Log(detail.c_wstr());
	}
	return TRUE;
}
static BOOL CALLBACK LogResourceWindowTree(HWND window, LPARAM parameter)
{
	DWORD process; GetWindowThreadProcessId(window, &process);
	if(process == GetCurrentProcessId()) { LogResourceWindow(window, parameter); EnumChildWindows(window, LogResourceWindow, parameter); }
	return TRUE;
}
class CGuiResourceMonitor
{
	PCWSTR m_name;
	BOOL m_report, m_ready;
	UINT m_warmups, m_stable, m_measured;
	GuiResourceCounts m_baseline, m_previous;
	GuiResourceCounts m_history[8];
	VOID Report(PCWSTR phase, const GuiResourceCounts& current)
	{
		if(m_report)
		{
			CRString detail;
			if(SUCCEEDED(RStrFormatW(&detail, L"\r\n%ls: %ls; warm-up %u, measured %u; baseline GDI=%u USER=%u, actual GDI=%u USER=%u\r\n",
				m_name, phase, m_warmups, m_measured, m_baseline.gdi, m_baseline.user, current.gdi, current.user))) Log(detail.c_wstr());
			for(UINT n = 0; n < m_warmups; n++)
			{
				detail = static_cast<RSTRING>(NULL);
				if(SUCCEEDED(RStrFormatW(&detail, L"  warm-up %u: GDI=%u USER=%u\r\n", n + 1, m_history[n].gdi, m_history[n].user))) Log(detail.c_wstr());
			}
			UINT logged = 0;
			EnumWindows(LogResourceWindowTree, reinterpret_cast<LPARAM>(&logged));
			HWND message = NULL;
			for(UINT n = 0; n < 256 && logged < 64; n++)
			{
				message = FindWindowExW(HWND_MESSAGE, message, NULL, NULL); if(!message) break;
				LogResourceWindow(message, reinterpret_cast<LPARAM>(&logged));
			}
		}
	}
public:
	enum { MaximumWarmups = 8, MeasuredCycles = 20 };
	CGuiResourceMonitor(PCWSTR name, BOOL report = TRUE) : m_name(name), m_report(report), m_ready(FALSE), m_warmups(0), m_stable(0), m_measured(0)
	{ ZeroMemory(&m_baseline, sizeof(m_baseline)); ZeroMemory(&m_previous, sizeof(m_previous)); }
	BOOL Complete() const { return m_ready && m_measured == MeasuredCycles; }
	BOOL Ready() const { return m_ready; }
	HRESULT AfterCycle(DWORD timeout = 2000)
	{
		HRESULT hr;
		GuiResourceCounts current = { 0, 0 };
		CheckIf(Complete(), E_UNEXPECTED);
		CheckIf(!m_ready && m_warmups >= MaximumWarmups, E_UNEXPECTED);
		hr = SettleGuiResources(m_ready ? &m_baseline : NULL, &current, timeout);
		if(FAILED(hr)) { Report(L"resource sampling failed", current); goto Cleanup; }
		if(!m_ready)
		{
			m_stable = m_warmups && current.Same(m_previous) ? m_stable + 1 : 1;
			m_previous = current; m_history[m_warmups++] = current;
			if(m_stable == 3) { m_baseline = current; m_ready = TRUE; }
			else if(m_warmups == MaximumWarmups) { Report(L"counts did not stabilize", current); hr = E_FAIL; }
		}
		else if(hr == S_FALSE) { Report(L"counts exceeded the baseline or did not settle", current); hr = E_FAIL; }
		else m_measured++;
Cleanup:
		return hr;
	}
};
static HRESULT CollectNativeWindows(IQuadooControl* control, TArray<HWND>& windows, UINT depth = 0)
{
	HRESULT hr;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooChildWindowHost> host;
	HWND handles[2] = { NULL, NULL };
	CheckIf(depth >= 256, E_UNEXPECTED);
	CheckIf(!control, E_INVALIDARG);
	hr = control->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&native));
	if(hr != E_NOINTERFACE) { Check(hr); CheckIf(!native, E_UNEXPECTED); Check(native->GetHandle(&handles[0])); }
	hr = control->QueryInterface(__uuidof(IQuadooChildWindowHost), reinterpret_cast<PVOID*>(&host));
	if(hr != E_NOINTERFACE) { Check(hr); CheckIf(!host, E_UNEXPECTED); Check(host->GetChildHostHandle(&handles[1])); }
	for(UINT n = 0; n < ARRAYSIZE(handles); n++)
	{
		if(handles[n])
		{
			sysint index; for(index = 0; index < windows.Length(); index++) if(windows[index] == handles[n]) break;
			if(index == windows.Length()) { Check(windows.Append(handles[n])); }
		}
	}
	hr = control->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container));
	if(hr != E_NOINTERFACE)
	{
		UINT count; Check(hr); CheckIf(!container, E_UNEXPECTED); Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> child; Check(container->GetChild(n, &child)); Check(CollectNativeWindows(child, windows, depth + 1)); }
	}
	hr = S_OK;
Cleanup:
	return hr;
}
static VOID CALLBACK ReleaseGuiTestWindow(HWND window, UINT, UINT_PTR timer, DWORD)
{ KillTimer(window, timer); DestroyWindow(window); }
static HRESULT GuiResourceMonitorTests()
{
	HRESULT hr;
	GuiResourceCounts baseline, current;
	CGuiResourceMonitor leaking(L"Injected USER leak", FALSE);
	CGuiResourceMonitor frozen(L"Injected post-warm-up USER leak", FALSE);
	HWND parent = NULL, retained = NULL;
	parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
	CheckIf(!parent, HrEnsureLastError()); Check(SettleGuiResources(NULL, &baseline));
	retained = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 10, 10, parent, NULL, GetModuleHandle(NULL), NULL);
	CheckIf(!retained, HrEnsureLastError());
	// A fixed baseline must reject a real retained USER object, even though the
	// count is stable. A timer then proves that waiting dispatches real cleanup.
	REQUIRE(SettleGuiResources(&baseline, &current, 150) == S_FALSE); REQUIRE(current.user > baseline.user && IsWindow(retained));
	CheckIf(!SetTimer(retained, 1, 30, ReleaseGuiTestWindow), HrEnsureLastError());
	Check(SettleGuiResources(&baseline, &current)); REQUIRE(hr == S_OK && !IsWindow(retained)); retained = NULL;
	for(UINT n = 0; n < CGuiResourceMonitor::MaximumWarmups && !frozen.Ready(); n++) { Check(frozen.AfterCycle()); }
	REQUIRE(frozen.Ready());
	retained = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 10, 10, parent, NULL, GetModuleHandle(NULL), NULL);
	CheckIf(!retained, HrEnsureLastError());
	// Once measured cycles start, even a stable extra handle must fail rather
	// than becoming a new baseline or restarting warm-up.
	REQUIRE(frozen.AfterCycle(150) == E_FAIL && !frozen.Complete() && IsWindow(retained));
	CheckIf(!DestroyWindow(retained), HrEnsureLastError()); retained = NULL;
	for(UINT n = 0; n < CGuiResourceMonitor::MaximumWarmups; n++)
	{
		HWND window = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 10, 10, parent, NULL, GetModuleHandle(NULL), NULL);
		CheckIf(!window, HrEnsureLastError());
		hr = leaking.AfterCycle();
		if(n + 1 < CGuiResourceMonitor::MaximumWarmups) { REQUIRE(hr == S_OK && !leaking.Complete()); }
		else { REQUIRE(hr == E_FAIL && !leaking.Complete()); }
	}
	hr = S_OK;
Cleanup:
	if(parent) DestroyWindow(parent);
	return hr;
}
