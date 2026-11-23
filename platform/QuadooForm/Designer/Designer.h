#pragma once
#include <windows.h>
#include <commctrl.h>
#include <cguid.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "..\Published\QuadooForm.h"
#include "Published\SimbeyCore.h"
#include "DesignerHost.h"
#include "..\Packages\PackageServices.h"

namespace QFD
{
	const UINT ChangedMessage = WM_APP + 0x121;
	const UINT RefreshMessage = WM_APP + 0x122;
	const UINT CommitInlineMessage = WM_APP + 0x123;
	const UINT FocusInlineMessage = WM_APP + 0x124;
	enum Command
	{
		NewForm = 41001, NewFrame, Open, Save, SaveAs, Exit, DeleteSelection,
		SelectRoot, SelectParent, Preview, Zoom50, Zoom75, Zoom100, Zoom150, Zoom200,
		Grid, Snap, ResetLayout, About, TabOrder, SendToBack, SendToFront, Cut, Copy, Paste, ToggleEnabled,
		CreateOpenHandler, MenuEditor, ImageListEditor, TreeNodesEditor, ListColumnsEditor,
		AddStatusPanel, MoveStatusPanelLeft, MoveStatusPanelRight,
		LastCommand = MoveStatusPanelRight
	};
	HRESULT Property(IQuadooComponent* control, PCWSTR name, IQuadooProperties** values, UINT* index);
	HRESULT ReadProperty(IQuadooComponent* control, PCWSTR name, IJSONValue** value);
	INT IntProperty(IQuadooComponent* control, PCWSTR name, INT fallback = 0);
	BOOL BoolProperty(IQuadooComponent* control, PCWSTR name, BOOL fallback = FALSE);
	HRESULT StringProperty(IQuadooComponent* control, PCWSTR name, RSTRING* text);
	HRESULT PutString(IJSONObject* object, PCWSTR name, RSTRING text);
	HRESULT PutInt(IJSONObject* object, PCWSTR name, INT value);
	HRESULT Parse(PCWSTR text, INT length, __deref_out IJSONObject** object);
	HMENU CreateControlMenu();
	BOOL OpenDesignerClipboard(HWND owner);
	HRESULT WriteDefinitionClipboard(HWND owner, IJSONObject* definition);
	HRESULT WriteComponentClipboard(HWND owner, IJSONValue* definition);
	HRESULT ReadDefinitionClipboard(HWND owner, IJSONValue** definition);
	BOOL SameObject(IUnknown* first, IUnknown* second);
	HRESULT FinishSourceEdit(ISourceEdit* edit, HRESULT result);
	HRESULT NextTabOrder(IQuadooControlContainer* parent, INT* order);
	HRESULT ImportIcon(const BYTE* bytes, UINT size, IJSONValue** value);
	HRESULT LoadIconFile(PCWSTR path, IJSONValue** value);

	// Document operations do not depend on standalone menus, dialogs or script code.
	class CDesignerDocument
	{
		IQuadooControlRegistry* m_registry;
		IQuadooConstructionContext* m_context;
		IQuadooControl* m_root;
		IQuadooDesignUsage* m_usage;
		ISourceSite* m_sourceSite; // Borrowed; the host detaches before destroying the unit.
		HWND m_host;
		CRString m_path;
		BOOL m_dirty, m_form;
		struct PageView { GUID tabs, active; };
		TArray<PageView> m_pages;
		HRESULT CapturePages(IQuadooControl* control);
		HRESULT RestorePages(IJSONObject* definition);
		HRESULT ApplyDefinition(IJSONObject* definition, const GUID* changedViews = NULL, UINT count = 0);
		HRESULT PrepareInsert(IQuadooComponent* parent, IJSONObject* item, BOOL component, ISourceEdit** edit);
		HRESULT PrepareProperty(REFGUID id, RSTRING name, BOOL event, IJSONValue* value, ISourceEdit** edit);
		HRESULT PrepareRemove(REFGUID id, ISourceEdit** edit);
	public:
		CDesignerDocument();
		~CDesignerDocument();
		HRESULT Initialize(IQuadooControlRegistry* registry);
		VOID SetSourceSite(ISourceSite* site) { m_sourceSite = site; }
		ISourceSite* SourceSite() const { return m_sourceSite; }
		VOID Clear();
		HRESULT Load(HWND workspace, IJSONObject* definition, RSTRING path = NULL);
		HRESULT New(HWND workspace, BOOL form);
		HRESULT Open(HWND workspace, PCWSTR path);
		HRESULT Save(PCWSTR path);
		HRESULT Serialize(IJSONObject** definition);
		HRESULT Find(REFGUID id, IQuadooControl** control);
		HRESULT FindComponent(REFGUID id, IQuadooComponent** component);
		HRESULT AddComponent(RSTRING type, IQuadooComponent** component);
		HRESULT AddMenuItem(IQuadooComponent* parent, IQuadooComponent** item);
		HRESULT RemoveMenu(IQuadooComponent* component);
		HRESULT MoveMenuItem(IQuadooComponent* component, BOOL down, BOOL end = FALSE);
		HRESULT ApplyMenuDefinition(IJSONObject* definition);
		HRESULT ApplyComponentProperties(REFGUID id, IJSONObject* properties);
		HRESULT SetMenuAssignment(IQuadooComponent* menu);
		HRESULT Add(IQuadooControlContainer* parent, RSTRING type, const QuadooForm::Rect* bounds, IQuadooControl** control);
		HRESULT SetProperty(IQuadooComponent* control, UINT index, IJSONValue* value);
		HRESULT SetBounds(IQuadooControl* control, const QuadooForm::Rect* bounds);
		HRESULT Remove(IQuadooComponent* control);
		HRESULT CopyDefinition(REFGUID id, IJSONObject** definition);
		HRESULT CopySelectionDefinition(REFGUID id, IJSONValue** definition);
		HRESULT PasteDefinition(REFGUID selected, IJSONValue* definition, GUID* first, TArray<GUID>* added = NULL);
		HRESULT CopyControls(const GUID* ids, UINT count, IJSONValue** definition);
		HRESULT RemoveControls(const GUID* ids, UINT count);
		HRESULT MoveControls(const GUID* ids, UINT count, INT dx, INT dy);
		HRESULT ResizeControls(const GUID* ids, UINT count, INT widthDelta, INT heightDelta);
		HRESULT SetControlProperties(const GUID* ids, UINT count, RSTRING name, IJSONValue* value);
		HRESULT Reorder(REFGUID id, BOOL front);
		HRESULT GetTabOrder(IQuadooControlContainer* parent, TArray<GUID>& order);
		HRESULT SetTabOrder(IQuadooControlContainer* parent, const GUID* order, UINT count);
		HRESULT RememberPage(IQuadooTabControl* tabs);
		IQuadooControl* Root() const { return m_root; }
		IQuadooControlRegistry* Registry() const { return m_registry; }
		HWND Host() const { return m_host; }
		RSTRING Path() const { return m_path; }
		BOOL IsForm() const { return m_form; }
		BOOL Dirty() const { return m_dirty; }
		VOID SetDirty(BOOL dirty) { m_dirty = dirty; }
	};

	// A descriptor intersection over stable identities. The inspector uses the same
	// source for one control and a group; property indices remain those of the primary.
	class CSelectionProperties
	{
		struct Entry
		{
			UINT index;
			DWORD flags;
			IQuadooPropertyDescriptor* descriptor;
		};
		CDesignerDocument* m_document;
		TArray<GUID> m_ids;
		TArray<Entry> m_entries;
		CSelectionProperties(const CSelectionProperties&);
		CSelectionProperties& operator=(const CSelectionProperties&);
	public:
		CSelectionProperties();
		~CSelectionProperties();
		HRESULT Initialize(CDesignerDocument* document, const GUID* ids, UINT count);
		UINT Count() const;
		HRESULT GetAt(UINT row, UINT* index, IQuadooPropertyDescriptor** descriptor, DWORD* flags);
		HRESULT Find(RSTRING name, UINT* index, DWORD* flags);
		HRESULT GetDescriptor(UINT index, IQuadooPropertyDescriptor** descriptor);
		HRESULT GetValue(UINT index, IJSONValue** value, BOOL* mixed);
		HRESULT SetValue(UINT index, IJSONValue* value);
	};

	struct MenuCell
	{
		GUID id;
		RECT bounds;
		RSTRING caption;
		BOOL enabled;
	};
	// TArray stores raw relocatable values; retain caption strings explicitly.
	class CMenuCells
	{
		TArray<MenuCell> m_cells;
		CMenuCells(const CMenuCells&);
		CMenuCells& operator=(const CMenuCells&);
	public:
		CMenuCells()
		{
		}

		~CMenuCells()
		{
			Clear();
		}
		VOID Clear()
		{
			for(sysint n = 0; n < m_cells.Length(); n++)
			{
				RStrRelease(m_cells[n].caption);
			}
			m_cells.Clear();
		}
		sysint Length() const
		{
			return m_cells.Length();
		}
		MenuCell& operator[](sysint index)
		{
			return m_cells[index];
		}
		HRESULT Append(const MenuCell& cell)
		{
			HRESULT hr = m_cells.Append(cell);
			if(SUCCEEDED(hr))
			{
				RStrAddRef(cell.caption);
			}
			return hr;
		}
	};
	HRESULT AssignedMainMenu(IQuadooControl* root, IQuadooComponent** menu);
	HRESULT LayoutMenuBar(IQuadooControl* root, UINT dpi, INT width, CMenuCells& cells, INT* height);
	VOID PaintMenuBar(HWND window, HDC dc, IQuadooControl* root, UINT dpi, const RECT* rect, REFGUID selected);
	class CDesignerWindowFrame
	{
	public:
		static VOID Insets(IQuadooControl* root, BOOL form, UINT dpi, RECT* insets);
		static VOID Paint(HWND window, HDC dc, IQuadooControl* root, BOOL form, UINT dpi, const RECT* client, REFGUID selected = GUID_NULL);
	};
	class CDesignerObserver;
	class CDesignerEditor : public CBaseUnknown
	{
		friend class CDesignerObserver;
		struct Node { IQuadooComponent* component; IQuadooControl* control; GUID id; HTREEITEM tree; BOOL expanded; };
		struct Tool { IUnknown* factory; RSTRING name; BOOL nonvisual; INT image; };
		struct Row
		{
			UINT index;
			QuadooProperty::Type type;
			DWORD flags;
			RSTRING name;
			INT color;
			BOOL dropdown;
			BOOL mixed;
		};
		struct DropdownChoice
		{
			GUID reference;
			LONGLONG number;
		};
		HWND m_window, m_canvas, m_overlay, m_tree, m_tools, m_list, m_tabs;
		HWND m_paletteWindow, m_notifyWindow;
		UINT_PTR m_session;
		UINT m_outlineRevision;
		BOOL m_embedded, m_paletteLaidOut;
		HWND m_inline;
		HWND m_tray;
		HIMAGELIST m_images;
		BOOL m_paletteMouse;
		GUID m_editControl;
		CRString m_editProperty, m_editInitial;
		INT m_editRow;
		BOOL m_editing, m_finishingEdit, m_inlineComposing, m_deferredRefresh;
		BOOL m_editDropdown;
		INT m_dropdownInitial;
		TArray<DropdownChoice> m_dropdownChoices;
		QuadooProperty::Type m_dropdownType;
		CDesignerDocument m_document;
		CDesignerObserver* m_observer;
		TArray<Node> m_nodes;
		TArray<Tool> m_palette;
		TArray<Row> m_rows;
		GUID m_selected;
		TArray<GUID> m_selection;
		GUID m_clickSelection;
		UINT m_changeDepth, m_zoom, m_dpi;
		INT m_leftWidth, m_rightWidth, m_treeHeight, m_splitter, m_tool;
		QuadooForm::Point m_scroll;
		BOOL m_refreshPosted, m_grid, m_snap, m_events, m_dragging, m_newControl, m_arranging, m_layingOut;
		BOOL m_readOnly;
		INT m_grip;
		POINT m_start;
		QuadooForm::Rect m_startBounds, m_pendingBounds;
		RECT m_ghost;
		CRString m_status;
		HRESULT AttachNodes(IQuadooControl* control, HTREEITEM parent, UINT depth);
		HRESULT AttachComponents();
		HRESULT AttachCollectionItems(IQuadooComponent* component, HTREEITEM parent, UINT depth);
		HRESULT AttachMenuItems(IQuadooComponent* component, HTREEITEM parent, UINT depth);
		VOID ClearNodes();
		VOID CloseSession();
		VOID ClearRows();
		HRESULT Palette();
		HRESULT RebuildPaletteImages(UINT dpi);
		VOID ClearPaletteImages();
		VOID ClearPalette();
		INT ComponentImage(IQuadooComponent* component);
		VOID ResetTool();
		VOID ChooseTool(INT row);
		static LRESULT CALLBACK PaletteProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
		static LRESULT CALLBACK PaletteHostProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
		HRESULT Inspector();
		LRESULT DrawInspector(NMLVCUSTOMDRAW* draw);
		HRESULT Refresh(BOOL tree);
		VOID Layout();
		VOID ArrangeSurface();
		VOID Notify();
		VOID Changed(BOOL property);
		HRESULT Selected(IQuadooControl** control);
		HRESULT SelectedComponent(IQuadooComponent** component);
		HRESULT SelectionProperties(CSelectionProperties& properties);
		HRESULT UpdateSelection(REFGUID id, BOOL add);
		HRESULT NormalizeSelection();
		HRESULT NudgeSelection(UINT key);
		HRESULT ResizeSelection(UINT key);
		HRESULT CopySelection(BOOL cut);
		HRESULT PasteSelection();
		HRESULT ReorderSelection(BOOL front);
		VOID ContextMenu(HWND source, POINT screen);
		HRESULT SelectContextID(REFGUID id);
		HRESULT Select(IQuadooControl* control, BOOL add = FALSE);
		HRESULT ControlRect(IQuadooControl* control, RECT* rect);
		HRESULT ParentAt(POINT point, RSTRING type, IQuadooControlContainer** parent);
		IQuadooControl* HitControl(POINT point);
		VOID GripRects(const RECT* bounds, RECT* grips);
		INT HitGrip(POINT point, DWORD* capabilities);
		VOID PaintBuffered(HDC dc, const RECT* update, UINT surface);
		VOID PaintCanvas(HDC dc);
		VOID PaintOverlay(HDC dc);
		VOID MouseDown(POINT point, BOOL add);
		VOID MouseMove(POINT point);
		VOID MouseUp(POINT point, BOOL cancel);
		VOID EditRow(UINT row, BOOL dialog = FALSE);
		HRESULT BeginInline(UINT row, RSTRING initial);
		HRESULT BeginReference(UINT row, IJSONValue* initial);
		HRESULT BeginChoices(UINT row, IQuadooPropertyDescriptor* descriptor, IJSONValue* initial);
		HRESULT BeginDropdown(UINT row, HWND* combo, INT selected, TArray<DropdownChoice>& choices);
		static HRESULT AddDropdownChoice(HWND combo, TArray<DropdownChoice>& choices, REFGUID reference, LONGLONG number, PCWSTR name);
		HRESULT DropdownValue(IJSONValue** value, BOOL* changed);
		HRESULT FinishInline(BOOL commit);
		VOID PositionInline();
		static LRESULT CALLBACK InlineProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
		static LRESULT CALLBACK InspectorProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
		VOID Error(HRESULT hr, PCWSTR operation);
		LRESULT Message(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT surface);
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
		static LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
		static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IUnknown)
		END_UNK_MAP
		CDesignerEditor();
		virtual ~CDesignerEditor();
		struct PaletteUpdate;
		HRESULT PreparePalette(IQuadooControlRegistry* registry, PaletteUpdate** update);
		VOID CommitPalette(PaletteUpdate* update);
		static VOID DiscardPalette(PaletteUpdate* update);
		HRESULT Create(HWND parent, IQuadooControlRegistry* registry = NULL);
		HRESULT CreateHosted(HWND parent, const EditorHostOptions* options, IQuadooControlRegistry* registry = NULL);
		UINT OutlineRevision() const { return m_outlineRevision; }
		UINT OutlineCount() const { return static_cast<UINT>(m_nodes.Length()); }
		HRESULT GetOutlineEntry(UINT index, OutlineEntry* entry);
		HRESULT GetSelectionInfo(RSTRING* name, QuadooForm::Rect* bounds, BOOL* visual);
		BOOL CanExecute(UINT command);
		VOID UpdateCommandMenu(HMENU menu);
		BOOL IsCommandChecked(UINT command);
		HRESULT ExecuteCommand(UINT command, HWND owner);
		VOID SetReadOnly(BOOL readOnly);
		HWND PaletteWindow() const { return m_paletteWindow; }
		HWND ToolsWindow() const { return m_tools; }
		HWND Window() const { return m_window; }
		CDesignerDocument& Document() { return m_document; }
		HRESULT NewDocument(BOOL form);
		HRESULT OpenDocument(PCWSTR path);
		HRESULT SaveDocument(PCWSTR path);
		HRESULT LoadDefinition(IJSONObject* definition);
		HRESULT SelectID(REFGUID id, BOOL add = FALSE);
		UINT SelectionCount() const;
		BOOL IsSelected(REFGUID id) const;
		HRESULT GetSelectionID(UINT index, GUID* id) const;
		HRESULT GetSelectedID(GUID* id);
		HRESULT Delete();
		HRESULT SetSelectedProperty(UINT index, IJSONValue* value);
		HRESULT AddControl(RSTRING type, IQuadooControlContainer* parent, const QuadooForm::Rect* bounds, GUID* id);
		HRESULT AddComponent(RSTRING type, GUID* id);
		HRESULT SetSelectionBounds(const QuadooForm::Rect* bounds);
		HRESULT PreviewForm(HWND owner, LRESULT* result);
		HRESULT CommitPropertyEdit() { return FinishInline(TRUE); }
		// Hosts may supply their shared component tree for selection shortcuts.
		BOOL TranslateEditorMessage(MSG* message, HWND outline = NULL);
		HRESULT GetTabOrderContainer(IQuadooControlContainer** parent);
		HRESULT EditTabOrder();
		HRESULT EditMenu();
		HRESULT EditImageList();
		HRESULT EditTreeNodes();
		HRESULT EditListColumns();
		HRESULT EditStatusPanels(UINT command);
		HRESULT ApplyComponentProperties(REFGUID id, IJSONObject* properties);
		HRESULT SelectMenuAt(POINT point);
		HRESULT MenuSelectionRect(RECT* rect);
		HRESULT OpenSelectedHandler();
		VOID SetZoom(UINT percent);
		VOID SetDPI(UINT dpi);
		VOID GetLayout(INT* left, INT* right, INT* tree) const { *left = m_leftWidth; *right = m_rightWidth; *tree = m_treeHeight; }
		VOID SetLayout(INT left, INT right, INT tree);
		VOID SetGrid(BOOL value) { m_grid = value; RedrawWindow(m_canvas,NULL,NULL,RDW_INVALIDATE | RDW_ALLCHILDREN); }
		VOID SetSnap(BOOL value) { m_snap = value; }
		VOID ResetPanes();
		UINT Zoom() const { return m_zoom; }
		BOOL GridVisible() const { return m_grid; }
		BOOL SnapEnabled() const { return m_snap; }
		RSTRING Status() const { return m_status; }
		HRESULT SelectionRect(RECT* rect);
		HRESULT SelectionCapabilities(DWORD* capabilities);
		HWND TreeWindow() const { return m_tree; }
		HWND CanvasWindow() const { return m_canvas; }
		HWND OverlayWindow() const { return m_overlay; }
		HWND TrayWindow() const { return m_tray; }
		VOID SelectAncestor(BOOL root);
	};
}
