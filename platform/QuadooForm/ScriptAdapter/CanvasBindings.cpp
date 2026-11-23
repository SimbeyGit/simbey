#include "CanvasBindings.h"
#include "SpriteObjects.h"
#include "Library\Core\LockableStream.h"

namespace AQVM
{
	static QuadooVM::QVARIANT* Arg(QuadooVM::QVPARAMS* args, UINT index)
	{
		return args->pqvArgs + args->cArgs - index - 1;
	}

	static HRESULT Number(const QuadooVM::QVARIANT* value, INT* result)
	{
		HRESULT hr;
		LONGLONG number;
		Check(VariantInteger(value,&number));
		CheckIf(number < INT_MIN || number > INT_MAX,DISP_E_OVERFLOW);
		*result = static_cast<INT>(number);
	Cleanup:
		return hr;
	}

	static HRESULT Color(const QuadooVM::QVARIANT* value, COLORREF* result)
	{
		HRESULT hr;
		LONGLONG number;
		Check(VariantInteger(value,&number));
		CheckIf(number < INT_MIN || number > UINT_MAX,DISP_E_OVERFLOW);
		*result = static_cast<COLORREF>(number);
	Cleanup:
		return hr;
	}

	static HRESULT Bounds(QuadooVM::QVARIANT* value, RECT* bounds)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		TStackRef<IJSONObject> object;
		INT fields[4];
		static PCWSTR const names[] = {L"Left",L"Top",L"Width",L"Height"};
		Check(QVMConvertToJSON(value,&json));
		CheckIf(!json,DISP_E_TYPEMISMATCH);
		Check(json->GetObject(&object));
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			TStackRef<IJSONValue> part;
			LONGLONG number;
			Check(object->FindNonNullValueW(names[n],&part));
			if(part->GetType() == JSON::Integer)
			{
				Check(part->GetInteger(&fields[n]));
			}
			else
			{
				Check(part->GetLongInteger(&number));
				CheckIf(number < INT_MIN || number > INT_MAX,DISP_E_OVERFLOW);
				fields[n] = static_cast<INT>(number);
			}
		}
		CheckIf(fields[2] < 0 || fields[3] < 0,E_INVALIDARG);
		LONGLONG right = static_cast<LONGLONG>(fields[0]) + fields[2];
		LONGLONG bottom = static_cast<LONGLONG>(fields[1]) + fields[3];
		CheckIf(right > INT_MAX || bottom > INT_MAX,DISP_E_OVERFLOW);
		bounds->left = fields[0];
		bounds->top = fields[1];
		bounds->right = static_cast<INT>(right);
		bounds->bottom = static_cast<INT>(bottom);
	Cleanup:
		return hr;
	}

	static HRESULT SizeObject(const SIZE& size, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		TStackRef<IJSONObject> object;
		TStackRef<IJSONValue> value;
		Check(JSONCreateObject(&object));
		Check(JSONCreateInteger(size.cx,&value));
		Check(object->AddValueW(L"Width",value));
		value.Release();
		Check(JSONCreateInteger(size.cy,&value));
		Check(object->AddValueW(L"Height",value));
		value.Release();
		Check(JSONWrapObject(object,&value));
		Check(QVMConvertFromJSON(value,result));
	Cleanup:
		return hr;
	}

	enum Operation
	{
		Clear, SetPixel, GetPixel, PixelSize, Line, Rectangle, Ellipse, Text,
		Measure, Font, ResetFont, ImageList, Export, Resolve, Invalidate, CreateSprite
	};

	struct Method
	{
		PCWSTR name;
		UINT length;
		Operation operation;
		UINT minimum;
		UINT maximum;
	};

	static const Method methods[] =
	{
		{L"Clear",StaticLength(L"Clear"),Clear,1,1},
		{L"SetPixel",StaticLength(L"SetPixel"),SetPixel,3,3},
		{L"GetPixel",StaticLength(L"GetPixel"),GetPixel,2,2},
		{L"GetPixelSize",StaticLength(L"GetPixelSize"),PixelSize,0,0},
		{L"DrawLine",StaticLength(L"DrawLine"),Line,5,5},
		{L"DrawRectangle",StaticLength(L"DrawRectangle"),Rectangle,3,4},
		{L"DrawEllipse",StaticLength(L"DrawEllipse"),Ellipse,3,4},
		{L"DrawText",StaticLength(L"DrawText"),Text,4,4},
		{L"MeasureText",StaticLength(L"MeasureText"),Measure,1,1},
		{L"SetFont",StaticLength(L"SetFont"),Font,4,4},
		{L"ResetFont",StaticLength(L"ResetFont"),ResetFont,0,0},
		{L"DrawImageList",StaticLength(L"DrawImageList"),ImageList,4,4},
		{L"ExportPng",StaticLength(L"ExportPng"),Export,0,0},
		{L"ResolveColor",StaticLength(L"ResolveColor"),Resolve,1,1},
		{L"Invalidate",StaticLength(L"Invalidate"),Invalidate,0,0},
		{L"CreateSprite",StaticLength(L"CreateSprite"),CreateSprite,0,0}
	};

	HRESULT CanvasInvoke(IQuadooCanvas* canvas, RSTRING name, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		const Method* method = NULL;
		INT coordinates[4];
		COLORREF color;
		SIZE size;
		CheckIf(!canvas || !args || !result,E_POINTER);
		result->eType = QuadooVM::Null;
		CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
		for(UINT n = 0; n < ARRAYSIZE(methods); n++)
		{
			if(RStrLen(name) == static_cast<INT>(methods[n].length) && !memcmp(RStrToWide(name),methods[n].name,methods[n].length * sizeof(WCHAR)))
			{
				method = methods + n;
				break;
			}
		}
		CheckIf(!method,DISP_E_MEMBERNOTFOUND);
		CheckIf(args->cArgs < method->minimum || args->cArgs > method->maximum,DISP_E_BADPARAMCOUNT);
		switch(method->operation)
		{
		case Clear:
		case Resolve:
			Check(Color(Arg(args,0),&color));
			if(method->operation == Clear)
			{
				Check(canvas->Clear(color));
			}
			else
			{
				Check(canvas->ResolveColor(color,&color));
				result->eType = QuadooVM::I4;
				result->lVal = color;
			}
			break;
		case SetPixel:
		case GetPixel:
		case Line:
			{
				UINT count = method->operation == Line ? 4 : 2;
				for(UINT n = 0; n < count; n++)
				{
					Check(Number(Arg(args,n),coordinates + n));
				}
				if(method->operation == GetPixel)
				{
					Check(canvas->GetPixel(coordinates[0],coordinates[1],&color));
					result->eType = QuadooVM::I4;
					result->lVal = color;
				}
				else
				{
					Check(Color(Arg(args,count),&color));
					if(method->operation == Line)
					{
						Check(canvas->DrawLine(coordinates[0],coordinates[1],coordinates[2],coordinates[3],color));
					}
					else
					{
						Check(canvas->SetPixel(coordinates[0],coordinates[1],color));
					}
				}
			}
			break;
		case PixelSize:
			Check(canvas->GetPixelSize(&size));
			Check(SizeObject(size,result));
			break;
		case Rectangle:
		case Ellipse:
			{
				RECT bounds;
				COLORREF fill;
				INT flags = QuadooForm::CanvasOutline | QuadooForm::CanvasFill;
				Check(Bounds(Arg(args,0),&bounds));
				Check(Color(Arg(args,1),&color));
				Check(Color(Arg(args,2),&fill));
				if(args->cArgs == 4)
				{
					Check(Number(Arg(args,3),&flags));
				}
				if(method->operation == Rectangle)
				{
					Check(canvas->DrawRectangle(&bounds,color,fill,flags));
				}
				else
				{
					Check(canvas->DrawEllipse(&bounds,color,fill,flags));
				}
			}
			break;
		case Text:
		case Measure:
		case Font:
			CheckIf(Arg(args,0)->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			if(method->operation == Measure)
			{
				Check(canvas->MeasureText(Arg(args,0)->rstrVal,&size));
				Check(SizeObject(size,result));
			}
			else if(method->operation == Text)
			{
				Check(Number(Arg(args,1),coordinates));
				Check(Number(Arg(args,2),coordinates + 1));
				Check(Color(Arg(args,3),&color));
				Check(canvas->DrawTextString(Arg(args,0)->rstrVal,coordinates[0],coordinates[1],color));
			}
			else
			{
				Check(Number(Arg(args,1),coordinates));
				CheckIf(Arg(args,2)->eType != QuadooVM::Bool || Arg(args,3)->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
				Check(canvas->SetFont(Arg(args,0)->rstrVal,coordinates[0],Arg(args,2)->fVal,Arg(args,3)->fVal));
			}
			break;
		case ResetFont:
			Check(canvas->ResetFont());
			break;
		case ImageList:
			{
				TStackRef<IComponentObject> wrapper;
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooImageList> images;
				LONGLONG index;
				CheckIf(Arg(args,0)->eType != QuadooVM::Object || !Arg(args,0)->pObject,DISP_E_TYPEMISMATCH);
				Check(Arg(args,0)->pObject->QueryInterface(&wrapper));
				Check(wrapper->GetComponent(&component));
				Check(component->QueryInterface(&images));
				Check(VariantInteger(Arg(args,1),&index));
				CheckIf(index < 0 || index > UINT_MAX,DISP_E_BADINDEX);
				Check(Number(Arg(args,2),coordinates));
				Check(Number(Arg(args,3),coordinates + 1));
				Check(canvas->DrawImageList(images,static_cast<UINT>(index),coordinates[0],coordinates[1]));
			}
			break;
		case Export:
			{
				TStackRef<ISequentialStream> stream;
				TStackRef<CLockableStream> locked;
				ULONG count;
				Check(canvas->ExportPng(&stream,&count));
				CheckIf(!stream || !count,E_UNEXPECTED);
				CheckIf(count > INT_MAX,DISP_E_OVERFLOW);
				locked.Attach(__new CLockableStream(stream,count));
				CheckAlloc(locked);
				Check(QVMCreateBinaryFile(RSTRING_CAST(L"image/png"),NULL,locked,0,count,result));
			}
			break;
		case Invalidate:
			Check(canvas->Invalidate());
			break;
		case CreateSprite:
			Check(CreateSpriteObject(canvas,&result->pObject));
			result->eType = QuadooVM::Object;
			break;
		}
		hr = S_OK;
	Cleanup:
		if(FAILED(hr) && result)
		{
			QVMClearVariant(result);
		}
		return hr;
	}
}
