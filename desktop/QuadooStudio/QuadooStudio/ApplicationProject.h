#pragma once
interface IJSONObject;
class CMemoryStream;
// Save an empty Application project. Forms are added through NewFormUnit.
HRESULT CreateApplicationProject(IJSONObject* project, PCWSTR path);
HRESULT SerializeStudioProject(IJSONObject* project, CMemoryStream& text);
HRESULT SaveStudioProject(IJSONObject* project, PCWSTR path);
