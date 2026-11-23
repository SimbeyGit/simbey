#pragma once
#include "RuntimeObjects.h"

namespace AQVM
{
	HRESULT CanvasInvoke(IQuadooCanvas* canvas, RSTRING method, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result);
}
