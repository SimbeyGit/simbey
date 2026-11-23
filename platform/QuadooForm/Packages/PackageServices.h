#pragma once
#include "..\Published\QuadooForm.h"

// Private design-host services. Neither interface is exported by QuadooForm.dll.
interface __declspec(uuid("048B2453-C216-42B4-9150-12DCD753A101")) IQuadooDesignUsage : IUnknown
{
	STDMETHOD(Update)(IJSONObject* definition) = 0;
	STDMETHOD(Include)(RSTRING type) = 0;
};
interface __declspec(uuid("048B2453-C216-42B4-9150-12DCD753A102")) IQuadooDesignUsageFactory : IUnknown
{
	STDMETHOD(CreateUsage)(IJSONObject* definition,IQuadooDesignUsage** usage) = 0;
};
