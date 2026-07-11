#pragma once

interface __declspec(uuid("CBC68AFC-AEFB-4a74-BF3A-D22D6C4E0137")) IUpdateTitle : IUnknown
{
	virtual HRESULT UpdateTitle (PCWSTR pcwzProject, __in_opt PCWSTR pcwzStatus) = 0;
};
