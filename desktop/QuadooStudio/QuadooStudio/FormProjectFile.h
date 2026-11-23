#pragma once
#include "..\..\..\platform\QuadooForm\Designer\Designer.h"

const UINT WM_STUDIO_DESIGNER = WM_APP + 0x441;
interface ITextDocument;
class CProjectTab;
class CProjectFile;
class CQuadooProject;
struct CProjectTreeNode
{
	CProjectTab* file;
	HTREEITEM item;
	GUID id;
	BOOL source, seen;
	UINT creationOrder;
	CProjectTreeNode(CProjectTab* owner, BOOL isSource) : file(owner), item(NULL), source(isSource), seen(FALSE), creationOrder(0) { ZeroMemory(&id,sizeof(id)); }
};
class CStudioFormUnit : public CBaseUnknown, public QFD::ISourceSite
{
private:
	CRString m_savedSource, m_savedDefinition;
	BOOL m_sourceSnapshot, m_definitionSnapshot;
public:
	CQuadooProject* owner;
	CProjectFile* sourceFile;
	BOOL metadataDirty;
	CRString sourceDiagnostic;
	HRESULT sourceError;
	CRString sourceRecovery;
	BOOL recoveryModified;
	IJSONObject* metadata;
	TStackRef<IJSONObject> savedMetadata;
	QFD::CDesignerEditor* editor;
	CRString definition, name;
	GUID id;
	UINT_PTR session;
	UINT outlineRevision;
	BOOL form, design, expandRoot;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IUnknown)
	END_UNK_MAP
	CStudioFormUnit();
	~CStudioFormUnit();
	HRESULT Initialize(IJSONObject* data, RSTRING directory, RSTRING source);
	HRESULT EnsureEditor(HWND parent, HWND paletteParent, IQuadooControlRegistry* registry, UINT dpi);
	BOOL Dirty() const { return metadataDirty || RStrLen(sourceRecovery) || (editor && editor->Document().Dirty()); }
	virtual HRESULT PrepareDefinition(IJSONObject* proposed, QFD::ISourceEdit** edit);
	virtual HRESULT OpenHandler(REFGUID component, RSTRING eventName);
	VOID Hide();
	VOID Close();
	HRESULT Save(ITextDocument* source, PCWSTR sourcePath);
	static HRESULT RecoverProject(PCWSTR projectPath);
	static HRESULT SourceName(RSTRING source, RSTRING* name);
};
