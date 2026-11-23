#pragma once
#include "RuntimeObjects.h"

namespace AQVM
{
	HRESULT CreateSpriteObject(IQuadooCanvas* canvas, IQuadooObject** object);
	LONG SpriteLiveObjects();
}
