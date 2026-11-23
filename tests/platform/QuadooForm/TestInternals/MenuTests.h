#include "..\Harness\MenuFixture.h"

static BOOL s_menuLayoutFailure;
class CMenuLayoutProbe : public QF::CButton
{
public:
	CMenuLayoutProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CButton(factory,context,parent)
	{
	}
	STDMETHOD(Arrange)(const QuadooForm::Rect* bounds, UINT dpi)
	{
		return s_menuLayoutFailure ? E_ABORT : CButton::Arrange(bounds,dpi);
	}
};
class CUniquePanelProbe : public QF::CPanel, public IQuadooUniqueComponent
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooUniqueComponent)
		UNK_CHAIN_TO(QF::CPanel)
	END_UNK_MAP
	CUniquePanelProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CPanel(factory,context,parent)
	{
	}
	STDMETHOD(GetUniquenessKey)(GUID* key)
	{
		if(key)
		{
			// An arbitrary capability key, unrelated to menus or built-in types.
			static const GUID unique = {0xf2200000,0,0,{0,0,0,0,0,0,0,1}};
			*key = unique;
		}
		return key ? S_OK : E_POINTER;
	}
};
static HRESULT MenuRollbackTests()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> probe;
	TStackRef<IQuadooControl> first;
	TStackRef<IQuadooControl> duplicate;
	TStackRef<IQuadooFrame> frame;
	TStackRef<IQuadooControl> frameUnique;
	TStackRef<IQuadooControlContainer> form;
	TStackRef<IQuadooFormMenu> menus;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<QF::CFactory> probeFactory;
	TStackRef<QF::CFactory> uniqueFactory;
	QuadooForm::ContextOptions options = {0};
	HWND hwnd;
	HMENU original;
	RECT client;
	UINT count;
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.Mode = QuadooForm::Preview;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(MenuTestDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	definition.Release();
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&form)));
	Check(root->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&menus)));
	Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window)));
	Check(window->GetHandle(&hwnd));
	probeFactory.Attach(__new QF::TFactory<CMenuLayoutProbe>);
	CheckAlloc(probeFactory);
	Check(probeFactory->Initialize());
	Check(NativeDefinition(L"{\"type\":\"Button\",\"name\":\"LayoutProbe\"}",&definition));
	Check(probeFactory->CreateControl(context,form,definition,&probe));
	definition.Release();
	Check(NativeActivate(probe,context));
	Check(form->UpdateLayout());
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&before));
	original = GetMenu(hwnd);
	s_menuLayoutFailure = TRUE;
	VERIFY(menus->SetMainMenu(NULL) == E_ABORT);
	s_menuLayoutFailure = FALSE;
	VERIFY(GetMenu(hwnd) == original && IsMenu(original));
	VERIFY(GetClientRect(hwnd,&client) && client.right == 380 && client.bottom == 180);
	Check(serializer->Serialize(root,&after));
	VERIFY(JSONCompareObjects(before,after) == 0);
	Check(form->UpdateLayout());
	Check(menus->SetMainMenu(NULL));
	VERIFY(!IsMenu(original) && !GetMenu(hwnd));

	uniqueFactory.Attach(__new QF::TFactory<CUniquePanelProbe>);
	CheckAlloc(uniqueFactory);
	Check(uniqueFactory->Initialize());
	Check(NativeDefinition(L"{\"type\":\"Panel\",\"name\":\"UniqueFirst\"}",&definition));
	Check(uniqueFactory->CreateControl(context,form,definition,&first));
	definition.Release();
	Check(NativeActivate(first,context));
	Check(form->GetChildCount(&count));
	Check(NativeDefinition(L"{\"type\":\"Panel\",\"name\":\"UniqueSecond\"}",&definition));
	VERIFY(uniqueFactory->CreateControl(context,form,definition,&duplicate) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	VERIFY(!duplicate);
	UINT remaining;
	Check(form->GetChildCount(&remaining));
	VERIFY(remaining == count);
	Check(first->Destroy());
	first.Release();
	Check(uniqueFactory->CreateControl(context,form,definition,&duplicate));
	Check(NativeActivate(duplicate,context));
	definition.Release();
	Check(NativeDefinition(L"{\"type\":\"Frame\",\"name\":\"UniqueFrame\"}",&definition));
	Check(QuadooCreateFrame(NULL,form,definition,&frame));
	definition.Release();
	Check(NativeDefinition(L"{\"type\":\"Panel\",\"name\":\"FrameUnique\"}",&definition));
	Check(uniqueFactory->CreateControl(context,frame,definition,&frameUnique));
	Check(NativeActivate(frameUnique,context));
	VERIFY(SUCCEEDED(QF::ValidateUniqueComponent(form,frame)));
	Check(root->Destroy());
	VERIFY(!IsWindow(hwnd));
Cleanup:
	s_menuLayoutFailure = FALSE;
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
