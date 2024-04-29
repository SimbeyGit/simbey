#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "SampleHostedSvc.h"

INT wmain (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	SvcInitGlobalCrashHandler(CSampleHostedSvc::_GetServiceRegKey);

	return (INT)SvcRunHostedService(CSampleHostedSvc::CreateService, cArgs, ppwzArgs);
}
