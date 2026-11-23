#pragma once

#include "..\..\..\..\..\platform\QuadooForm\ScriptAdapter\ApplicationRuntime.h"

class CAQVMHost;

interface __declspec(uuid("72A087FB-0723-449A-BA91-985FF25AA901")) IPluginModuleLoader : IUnknown
{
	STDMETHOD(LoadModule)(RSTRING path, IQuadooObject** object) = 0;
	STDMETHOD(Disconnect)(VOID) = 0;
};

HRESULT CreatePluginFactory(CAQVMHost* host, IPluginModuleLoader** loader, IQuadooComponentFactory** factory);
