#pragma once

// Input flags for GetDispID
#define fdexNameCaseSensitive       0x00000001L
#define fdexNameEnsure              0x00000002L
#define fdexNameImplicit            0x00000004L
#define fdexNameCaseInsensitive     0x00000008L
#define fdexNameInternal            0x00000010L
#define fdexNameNoDynamicProperties 0x00000020L

// Output flags for GetMemberProperties
#define fdexPropCanGet              0x00000001L
#define fdexPropCannotGet           0x00000002L
#define fdexPropCanPut              0x00000004L
#define fdexPropCannotPut           0x00000008L
#define fdexPropCanPutRef           0x00000010L
#define fdexPropCannotPutRef        0x00000020L
#define fdexPropNoSideEffects       0x00000040L
#define fdexPropDynamicType         0x00000080L
#define fdexPropCanCall             0x00000100L
#define fdexPropCannotCall          0x00000200L
#define fdexPropCanConstruct        0x00000400L
#define fdexPropCannotConstruct     0x00000800L
#define fdexPropCanSourceEvents     0x00001000L
#define fdexPropCannotSourceEvents  0x00002000L

#define grfdexPropCanAll \
	(fdexPropCanGet | fdexPropCanPut | fdexPropCanPutRef | \
	fdexPropCanCall | fdexPropCanConstruct | fdexPropCanSourceEvents)
#define grfdexPropCannotAll \
	(fdexPropCannotGet | fdexPropCannotPut | fdexPropCannotPutRef | \
	fdexPropCannotCall | fdexPropCannotConstruct | fdexPropCannotSourceEvents)
#define grfdexPropExtraAll \
	(fdexPropNoSideEffects | fdexPropDynamicType)
#define grfdexPropAll \
	(grfdexPropCanAll | grfdexPropCannotAll | grfdexPropExtraAll)

// Input flags for GetNextDispID
#define fdexEnumDefault             0x00000001L
#define fdexEnumAll                 0x00000002L

// Additional flags for Invoke - when object member is
// used as a constructor.
#define DISPATCH_CONSTRUCT 0x4000

// Standard DISPIDs
#define DISPID_THIS (-613)
#define DISPID_STARTENUM DISPID_UNKNOWN

interface __declspec(uuid("A6EF9860-C720-11d0-9337-00A0C90DCAA9")) IDispatchEx : IDispatch
{
	virtual HRESULT STDMETHODCALLTYPE GetDispID ( 
		/* [in] */ __RPC__in BSTR bstrName,
		/* [in] */ DWORD grfdex,
		/* [out] */ __RPC__out DISPID* pid) = 0;

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE InvokeEx ( 
		/* [annotation][in] */ 
		_In_  DISPID id,
		/* [annotation][in] */ 
		_In_  LCID lcid,
		/* [annotation][in] */ 
		_In_  WORD wFlags,
		/* [annotation][in] */ 
		_In_  DISPPARAMS* pdp,
		/* [annotation][out] */ 
		_Out_opt_  VARIANT *pvarRes,
		/* [annotation][out] */ 
		_Out_opt_  EXCEPINFO* pei,
		/* [annotation][unique][in] */ 
		_In_opt_  IServiceProvider *pspCaller) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteMemberByName ( 
		/* [in] */ __RPC__in BSTR bstrName,
		/* [in] */ DWORD grfdex) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteMemberByDispID ( 
		/* [in] */ DISPID id) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetMemberProperties ( 
		/* [in] */ DISPID id,
		/* [in] */ DWORD grfdexFetch,
		/* [out] */ __RPC__out DWORD* pgrfdex) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetMemberName ( 
		/* [in] */ DISPID id,
		/* [out] */ __RPC__deref_out_opt BSTR* pbstrName) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetNextDispID ( 
		/* [in] */ DWORD grfdex,
		/* [in] */ DISPID id,
		/* [out] */ __RPC__out DISPID* pid) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetNameSpaceParent ( 
		/* [out] */ __RPC__deref_out_opt IUnknown** ppunk) = 0;
};
