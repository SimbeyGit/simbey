#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\ActiveX\DLLServer.h"
#include "Library\ActiveX\ClassFactory.h"
#include "QSDemoModule.h"

// {22DCE754-C5E4-40b1-9EEE-4AEE49096F5A}
static const GUID CLSID_QSDemoModule = 
{ 0x22dce754, 0xc5e4, 0x40b1, { 0x9e, 0xee, 0x4a, 0xee, 0x49, 0x9, 0x6f, 0x5a } };

#include "Published\QuadooObject.inc"

class CQSDemoModule :
	public CDLLServer
{
public:
	CQSDemoModule ()
	{
	}

	~CQSDemoModule ()
	{
	}

	virtual const IID& GetStaticClassID (VOID)
	{
		return CLSID_QSDemoModule;
	}

	virtual LPCTSTR GetStaticProgID (VOID)
	{
		return L"Simbey.QSDemoModule";
	}

	virtual LPCTSTR GetStaticModuleDescription (VOID)
	{
		return L"QuadooScript Demonstration Module";
	}
};

BEGIN_GET_CLASS_OBJECT
	EXPORT_FACTORY(CLSID_QSDemoModule, CQSDemo)

	// QuadooScript plug-ins always support the CLSID_QuadooObject class.
	EXPORT_FACTORY(CLSID_QuadooObject, CQSDemo)
END_GET_CLASS_OBJECT

CQSDemoModule g_module;
