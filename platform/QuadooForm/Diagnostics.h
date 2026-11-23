#pragma once

// Private cooperation between the native runtime and its script host. A
// recoverable display-provider failure must not stop an application pump.
// Third-party construction contexts/reporters can retain ReportError/OnError.
namespace QuadooPrivate
{
	interface __declspec(uuid("4AD07E49-BD5E-466B-ABD2-3DB3A442C2F9")) IDiagnosticReport : IUnknown
	{
		STDMETHOD(ReportDiagnostic)(HRESULT error, RSTRING path, RSTRING detail) = 0;
	};
}
