#pragma once

#include "..\..\..\..\platform\QuadooForm\Published\QuadooForm.h"

// An independent extension: this header and ExternalControl.cpp use only the
// published QuadooForm ABI and shared library, never the runtime's classes.
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B522")) IExternalMeter : IUnknown
{
	STDMETHOD(Pulse)(INT value, INT* accepted) = 0;
	STDMETHOD(GetArrangeCount)(UINT* count, UINT* dpi) = 0;
};

struct ExternalCounts
{
	UINT objects, destroyed, factories, paints, messages;
};

// Test-only extension contracts; none are private QuadooForm interfaces.
enum ExternalFailure
{
	ExternalNoFailure, ExternalIdentity, ExternalParent, ExternalState, ExternalName, ExternalID,
	ExternalProperties, ExternalDescriptors, ExternalWindowQI, ExternalFrameQI, ExternalNotificationsQI,
	ExternalObserverBefore, ExternalObserverAfter, ExternalObserverReentrant, ExternalAfterSubscribe,
	ExternalObserverRollbackFailure, ExternalNullProperties, ExternalNullDescriptors,
	ExternalNullWindowQI, ExternalFalseWindowQI, ExternalInvalidWindowQI, ExternalDefinitionQI, ExternalComponentHostQI
};
enum ExternalSchema
{
	ExternalValidSchema, ExternalMissingName, ExternalDuplicateProperty, ExternalWrongWidthType,
	ExternalWrongWidthFlags, ExternalWrongDefault, ExternalNullDescriptor, ExternalHugeCount,
	ExternalEventCollision, ExternalDuplicateEvent, ExternalReorderedProperties, ExternalBadEnumCount, ExternalBadEnumValue,
	ExternalMemoSchema
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B523")) IExternalTestControl : IUnknown
{
	STDMETHOD(SetFailure)(ExternalFailure failure, HRESULT error) = 0;
	STDMETHOD(GetTestState)(UINT* observers, ULONG* references, BOOL* isolated) = 0;
	STDMETHOD(SendObserverNotification)(VOID) = 0;
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B524")) IExternalTestFactory : IUnknown
{
	// Produce an unpublished candidate to test the public RegisterControl call.
	STDMETHOD(CreateCandidate)(IQuadooConstructionContext* context, IQuadooControlContainer* parent,
		IJSONObject* definition, IQuadooControl** control) = 0;
	STDMETHOD(SetSchema)(ExternalSchema schema) = 0;
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B525")) IExternalDesignPolicy : IUnknown
{
	STDMETHOD(SetDesignResult)(HRESULT result) = 0;
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B526")) IExternalService : IUnknown
{
	STDMETHOD(Pulse)(INT value, INT* accepted) = 0;
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B527")) IExternalComponentTestFactory : IUnknown
{
	STDMETHOD(CreateCandidate)(IQuadooConstructionContext* context, IQuadooComponentCollection* owner,
		IJSONObject* definition, IQuadooComponent** component) = 0;
};
interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B528")) IExternalComponentTest : IUnknown
{
	STDMETHOD(SetFailure)(ExternalFailure failure, HRESULT error) = 0;
};
typedef HRESULT (WINAPI* CREATE_EXTERNAL_FACTORY)(IQuadooControlFactory**);
typedef HRESULT (WINAPI* GET_EXTERNAL_COUNTS)(ExternalCounts*);
typedef HRESULT (WINAPI* EXTERNAL_CAN_UNLOAD)(VOID);
typedef HRESULT (WINAPI* CREATE_EXTERNAL_COMPONENT_FACTORY)(IQuadooComponentFactory**);
typedef HRESULT (WINAPI* CONFIGURE_EXTERNAL_KEY_CLAIMS)(IQuadooWinControl*, BOOL);

interface __declspec(uuid("9470AE52-A281-44DB-AB47-58B06C78B529")) IExternalPaletteTest : IUnknown
{
	// S_OK loads artwork, S_FALSE omits it, 2 returns malformed success.
	STDMETHOD(SetIconResult)(HRESULT result) = 0;
	STDMETHOD(GetIconRequests)(UINT* count) = 0;
};

// Host-side loader helpers, implemented separately from the extension DLL.
#ifndef QUADOOFORM_TEST_EXTENSION
HRESULT CreateExternalFactory(IQuadooControlFactory** factory);
HRESULT CreateExternalScriptFactory(IQuadooControlFactory** factory);
HRESULT CreateExternalComponentFactory(IQuadooComponentFactory** factory);
ExternalCounts ExternalStatistics();
HRESULT UnloadExternalExtension();
HRESULT ExternalCanUnload();
HRESULT ConfigureExternalKeyClaims(IQuadooWinControl* control, BOOL enable);

#endif
