#pragma once
#include "RuntimeObjects.h"

namespace AQVM
{
	HRESULT WrapStatusPanels(IQuadooStatusPanels* panels, QuadooVM::QVARIANT* result);
}
