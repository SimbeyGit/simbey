# Designer component commands

Both the standalone designer and Quadoo Studio use the shared editor command
layer for Send to Back, Send to Front, Cut, Copy, Paste, Delete, Enabled and Tab
Order. Context menus are available on the canvas, component tree and nonvisual
tray. Mouse invocation selects the clicked component; keyboard invocation uses
the current selection. The root cannot be cut, copied, deleted or reordered.
Tab Order is enabled only for a selected container. The existing tab-order
editor remains unchanged.

Creation order is the sibling order in `children` or `components`. Reordering
updates serialized definitions, the designer outline and design HWND stacking,
without editing TabOrder. Studio sorts existing component tree items by outline
position so source-file handles, expansion and selection remain stable. Its tree
popup uses MenuUtil::EnableMenuItems through the existing IOleCommandTarget.

Copy writes a complete subtree definition as CF_UNICODETEXT JSON, including
custom properties, nested children, embedded resources and event method names.
Cut writes the clipboard successfully before attempting deletion. Paste accepts
a single object or a nonempty array, including mixed visual and nonvisual items.
Visual items target the selected compatible container or its nearest compatible
ancestor; ordinary controls target a selected TabControl's active page. Nonvisual
components belong to the document's component collection.

Pasted components receive fresh GUIDs. Names are retained when available, or get
a free numeric suffix within the form/frame namespace. Descendant identities
are treated the same way. ObjectReference descriptor values referring to copied
names/GUIDs are remapped recursively; unrelated properties and event names are
preserved. Duplicate source names/IDs in one payload are rejected because they
would make reference remapping ambiguous. Factory metadata and design-placement
policies are consulted through published interfaces. Missing or rejecting
factories cause the paste to fail without changing the document.

Paste and reorder construct a candidate document before replacing the current
one. Failure preserves identity, dirty state, selection, tree and serialization.
The current design tab page and persisted active-page property are preserved
separately, by GUID. Successful operations replace design instances: editor
hosts must reacquire component interfaces by ID after these operations. No
QuadooForm.dll public ABI changes are needed.

Ctrl+X/C/V and Delete operate on the design selection when the canvas, outline
or nonvisual tray has focus. Inline property editors retain native text-editing
shortcuts. Palette keyboard input continues to operate on the palette. Both
hosts expose the commands in their regular menus and update enabled/checked
states when opening those menus.

The --commands-only designer suite exercises real clipboard and popup paths,
subtree/array/name/identity/event round trips, current and persisted tab pages,
nonvisual components, external factories, locked-clipboard Cut, and candidate
construction rollback. Tests preserve materialized clipboard formats and restore
them afterwards. Studio integration covers command routing, shared-tree sibling
order, shortcuts, popup states and designer-inactive command availability.
