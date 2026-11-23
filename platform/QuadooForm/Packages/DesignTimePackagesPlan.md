# Design-time placeholder control packages

Status: implemented, 2026-10-18. The original plan below records the design
contract. See [Readme.md](Readme.md) for usage, implementation details and the
validation record for this implementation pass.

## Scope and host ownership

Add JSON-defined placeholder types for controls whose real implementations are
available only to an application at runtime. One package describes one control
type, either visual or nonvisual. A visual type can optionally be a container.
Both Quadoo Studio and the standalone Designer use the same package format,
installed directory, loader, placeholder implementation and Packages dialog.

Use a standard modal Win32 dialog containing a report-mode list view. Its image
list shows each package's 20x20 icon alongside its name; additional columns show
the control type, category (Visual, Container or Nonvisual), and installation
status. Provide Install..., Remove and Close buttons, plus a details/status area
for the selected package. This gives native keyboard navigation, selection and
scrolling without creating a separate form/frame collection for the list.

Compile shared C++ files directly into both EXEs and include a shared .rc fragment
from each host's resource script. All new shared code and resources live under
`platform/QuadooForm/Packages`. Do not add these resources to QuadooForm.dll or
introduce a resource DLL. Studio continues to use its existing status bar.

Add Form > Packages... as a host-level command in both programs. It is always
enabled, including Studio with no project, a non-Application project, or Code
view active. Route it in CQuadooStudio rather than requiring a CQuadooProject or
active editor. Keep its command ID outside editor-only command gating/ranges;
Studio's IOleCommandTarget query returns supported/enabled unconditionally.
Use the existing MenuUtil::EnableMenuItems path for Studio menu updates.

## Package format, version 1

The installable document is UTF-8 JSON, normally `<TypeName>.json`. For example:

```json
{
  "schemaVersion": 1,
  "packageId": "{A14F5FB9-D835-4812-ABEF-8A265E49B247}",
  "name": "QuadooPlugin",
  "description": "Design-time placeholder for an application-provided plugin component.",
  "control": {
    "type": "QuadooPlugin",
    "kind": "nonvisual",
    "container": false,
    "properties": [
      {
        "name": "Module",
        "type": "String",
        "default": ""
      }
    ]
  }
}
```

`packageId` is a stable GUID identifying the installation, independent of the
source filename and the GUIDs of control instances. `name` is the package display
name; `control.type` is the exact, case-sensitive factory/definition type name
that the real runtime factory must also register. Version 1 installs one type per
package, keeping conflict detection and removal atomic. A nonvisual package must
have `container: false`. A visual leaf has `kind: "visual", container: false`;
a visual container has `kind: "visual", container: true`.

Custom property entries have `name`, `type`, and optional `default`,
`displayName`, `category`, `description`, and `readOnly`. Package authors do not
provide raw native flag integers. Custom values are serializable; readOnly adds
PropertyReadOnly. Do not mark their metadata PropertyDesignOnly: these values
belong in the application's form definition. Optional `choices` entries with
`name` and integral `value` describe Enum/Flags properties.

Accept the published QuadooProperty type names, not numeric enum ordinals:

| Type names | JSON representation and omitted default |
| --- | --- |
| String, Memo | String; empty string. String is inline-edited; Memo uses the existing multiline editor. |
| Boolean | Boolean; false. |
| Int, Long, Float | Correctly ranged integer or finite number; zero. Preserve Long precision through JSON's integer API. |
| Color | Signed 32-bit Quadoo Color; zero. Preserve system-color tags through storage and serialization. |
| Enum, Flags | Existing numeric/name choice semantics; Enum requires choices and uses its first choice if no default; Flags defaults to zero and rejects unknown bits. |
| List | Array of strings; empty array. Reuse IRStringArray snapshot semantics when exposing a string collection. |
| Rect, Point, Size | Object with Left/Top/Width/Height, X/Y, or Width/Height respectively; omitted numeric fields default to zero. Reserved Padding/Margins retain their existing Left/Top/Right/Bottom shape. |
| Font | Existing Name/Size/Bold/Italic object; default Segoe UI, 9, false, false. |
| Icon, Picture | Null or the existing embedded image value format; null. Reuse image validation/import code. |
| ObjectReference | Component name or GUID string; null or empty string means unassigned. Edit through the inline component drop-down, storing new assignments as GUID strings. Preserve reference remapping during copy/paste. |
| Collection | JSON array/object; empty array. The current JSON editor can edit it; no custom element editor or runtime-specific validation is inferred. |

Do not silently accept an unknown type or coerce an incompatible default. Pin
these non-scalar shapes in the parser tests and document them as the package
contract. This adds package codecs over published interfaces; it does not assume
the private built-in property normalizer handles arbitrary custom property names.
Validate both defaults and every edited value through the same codec. For
Collection only structural JSON validity can be established without the real
control; its factory remains responsible for additional runtime rules.

Support optional `control.events` using the existing event descriptor model:
entries contain `name`, optional `category`, `flags` (named EventFlags), and an
`arguments` array of `{name, type, writable}` using named EventValueType values.
Create the matching Event property automatically, with an empty handler default;
require properties of type Event to be declared through this event list rather
than accepting a property without its matching event descriptor.
Reuse the common OnCreate/OnDestroy definitions with empty handlers. The designer
stores method names without resolving or invoking application handlers. Reject
duplicate event names and incompatible redefinitions of the common events.

The package's icon can be embedded as:

```json
"icon": { "format": "png", "encoding": "base64", "data": "..." }
```

Otherwise, resolve a PNG next to the selected source JSON with exactly the same
stem (`QuadooPlugin.json` -> `QuadooPlugin.png`). An explicit embedded icon takes
precedence; an invalid embedded icon is an error, not a reason to fall back to
the sidecar. A missing icon is allowed and uses the shared generic category
glyph. Validate decoded image bytes before installation. Author at 20x20; accept
other bounded PNG sizes and resample for 20px and DPI-scaled icon requests.

Only the selected JSON and its same-stem PNG are inputs. There is no arbitrary
icon path, script, DLL, URL, or executable installation hook in the package.
The Module property is inert text at design time; entering a DLL name never
loads it. Preserve recognized metadata and reject unknown schema fields in
version 1 so misspelled declarations are diagnosed rather than ignored.

## Framework properties and placeholder implementation

Generate immutable, per-type descriptor collections from the package. Use a
shared package implementation of the published descriptor and property
interfaces with descriptor-driven, deep-copied instance values. Keep every
metadata index range-checked in Release as well as Debug. Descriptors and
instances retain the immutable package definition that owns their strings and
defaults; installation/removal cannot leave dangling metadata pointers.

Compose the framework base descriptors before the custom entries:

- Nonvisual: Name:String with the required writable/serializable flags, plus
  lifecycle event descriptors/properties. No geometry, HWND or TabOrder.
- Visual: the mandatory Name, Left, Top, Width, Height, Visible and Enabled
  descriptors with the published flags; include standard layout, Font,
  ForeColor and BackColor definitions. Design windows also have TabOrder.
- Visual container: add standard Padding and expose the actual container/child
  window host/layout capabilities. Children are ordinary registered controls.

Treat every reserved framework property name as reserved even when it is absent
from a category. Reject custom duplicates instead of allowing a package to
replace Name, geometry, TabOrder, colors, Font, or another framework property
with a different type/flag/default contract. Obtain common definitions through
shared package metadata helpers using the published contract and verify them
with the registry's existing descriptor validation. Do not include private
Runtime.h or derive from CFactory/CControl/CComponentObject.

Implement separate package component, visual-leaf and visual-container adapters
over a shared package component base, using CBaseUnknown and BEGIN_UNK_MAP.
Factories implement IQuadooComponentFactory or IQuadooControlFactory as
appropriate, plus IQuadooFactoryLifetime and IQuadooPaletteIcon; visual factories
provide IQuadooDesignFactory placement checks. Containers expose
IQuadooControlContainer, IQuadooChildWindowHost and IQuadooLayoutControl, while
leaf controls do not claim container interfaces. Implement public construction,
property notification, ownership, rollback and destruction contracts, including
canonical IUnknown identity across all inheritance paths.

Register these factories through the current public registry interfaces. They
construct objects only when ContextMode is Design. Preview/Runtime requests
fail with a clear design-only diagnostic and no side effects; they never
silently create a substitute for a real application control. Native Preview of
a definition using a package therefore reports that its runtime factory is
unavailable, leaving the design document intact.

Paint visual placeholders as a generic labeled surface using their package
icon, name and type; containers have a real design child-host area with normal
layout, nesting, hit testing, selection, resize, tab-order editing and z-order.
Nonvisual placeholders appear in the tray with their package icon. Do not use
the existing unresolved fallback as the installed package implementation:
DesignPlaceholders.cpp currently treats all visual representations as
containers and conservatively marks descendants unavailable. Installed types
have known capabilities and should permit real design layout of their children.
Keep the existing missing-factory fallback for an actually missing package.

Installed placeholders must show their package artwork rather than the missing
factory warning glyph. Their origin can be shown in inspector/package details
through the shared package catalog; do not claim IQuadooDesignPlaceholder merely
to label an intentional installed placeholder as an unresolved control.

Serialization keeps the original control type, GUID/name, properties, events,
children and root components using the current schema. Never serialize the
package ID, installation path, icon, generic placeholder implementation type, or
other designer bookkeeping into application definitions. Use
IQuadooDefinitionSource to preserve existing unknown/vendor fields, matching the
current lossless behavior. A runtime factory registered under the same type
therefore consumes exactly the same JSON without conversion.

## Shared storage, installation and startup

Use the current user's roaming application-data directory shared by both hosts:
`SHGetFolderPathW(..., CSIDL_APPDATA, ...)` plus `QuadooForms\DesignTime`.
This means `%APPDATA%\QuadooForms\DesignTime`, not machine-wide ProgramData;
it matches the requested user-owned shared installation and needs no elevation.
Use XP-compatible Shell/Win32 APIs. Resolve the directory once through a shared
helper, independent of either EXE's directory or the current working directory.

Install the normalized JSON as `<packageId-without-braces>.json`, preventing
untrusted source/display names from becoming destination paths. If using a PNG
sidecar, read it, validate/decode it, and insert its original PNG bytes as the
embedded Base64 icon before writing the installed copy. Use SimbeyCore's
ScEncodeBase64A/ScDecodeBase64A and CMemoryStream, and existing shared lodepng
image helpers. Do not rebuild SimbeyCore or add another Base64 implementation.
The installed file is self-contained; never retain a dependency on the source
JSON/PNG after installation. Do not modify those source files.

Installation stages parsing, semantic validation, icon decoding, descriptor
construction, collision checks and refreshed palette/dialog data before commit.
Write a temporary file in the destination directory, flush, then atomically
rename it into place. Remove temporary files on failure. No partially installed
package, registered type, or palette row may remain after a failed operation.
Bound the parser/image workload (proposed: 1 MiB JSON, 256 KiB decoded PNG,
256x256 source image, 256 properties and 64 events); finalize constants in tests.

Reject package IDs or type names already installed, and type conflicts with
built-ins or independently registered native design factories. Apply the
registry's exact case-sensitive type semantics, and additionally reject
case-only package/type duplicates to avoid confusing Windows file/UI identities.
Version 1 has no in-place replacement/upgrade command: removal and reinstall
are explicit, and removal must pass usage checks. No old-extension migration
layer is needed.

Create a host-lifetime CPackageCatalog and design-registry facade before opening
definitions. Studio owns it at the main-window level, so Packages works before
there is a project; projects borrow its registry instead of lazily creating an
isolated m_formRegistry. The standalone host explicitly supplies that registry
to Create/CreateHosted rather than letting each editor create its own.

Keep the facade's IUnknown identity stable for existing construction contexts.
Prepare immutable registry generations using public factory enumeration and
registration, retaining independently registered native factories as well as
built-ins. Prepare palette snapshots for all live editors. Commit by swapping
the facade's generation and the prepared palette data without fallible work in
the commit section. Preserve selection, palette scrolling, active tool, current
tab page, document contents/dirty state and focus. Installation adds palette
entries immediately without recreating existing control instances. A newly
installed type resolves previously unresolved instances on explicit document
reopen/reload, not by silently replacing an unsaved document.
When removing the currently armed tool, select Pointer without scrolling the
palette. Retired factory references cannot create new instances after removal;
their retained immutable descriptors remain readable until released.

On startup, enumerate only installed package JSON files, validate each and
register accepted factories before creating any designer document or palette.
Use deterministic ordering. Invalid or conflicting packages remain visible in
Packages with their validation error and can be removed; do not register half
of a package or prevent unrelated valid packages/project loading. Startup
diagnostics should be summarized once rather than showing one modal error per
file. Deleted/missing packages continue to use the existing lossless fallback.

## Usage checks and safe removal

Removal is blocked whenever a current form/frame definition references the
package's control.type, including descendants and root nonvisual components.
Do not rely on factory live counts alone: unopened Studio form definitions can
use a package without having created an object.

The host supplies a small shared usage-query interface/callback to the dialog:

1. Studio scans every associated Form and Frame in the current project, not
   just the active designer tab. For a loaded editor, serialize its live
   in-memory definition (including unsaved edits); otherwise parse the saved
   associated definition directly without instantiating controls.
2. The standalone host scans its open form/frame and any future open sessions,
   including unsaved/new documents. Code view and hidden/inactive editors do
   not remove a document from the usage set.
3. Walk children/components recursively, compare exact type names and return
   the source/definition path plus matching component names. An instance still
   represented by the missing-factory fallback also counts as a use.
4. Commit any pending inline property edit before the scan. If a project
   definition is missing, malformed, unreadable or cannot be serialized, fail
   closed for removal and report the file that prevented verification.
5. Also require zero live package instances across retained generations and
   outstanding destroyed objects. Live counts protect retained interfaces,
   while the definition scan protects uninstantiated uses.

Disable Remove when a selected package is in use; show the first use and a
count/details list explaining where. Recheck immediately before committing
removal, under the package mutation guard. Do not offer force removal or
automatically delete/rewrite controls in user documents. Delete/cut/undo,
document reopen, project close and view changes refresh the usage display.
Clipboard contents alone do not count as an open-document use; pasting after
uninstall follows the existing unresolved-control behavior.

Serialize installation/removal and catalog refresh with a per-user named mutex.
Support both hosts running concurrently: maintain usage-file leases for custom
types referenced by current documents, updated under that mutex before adopting
a definition or adding a component. Use persistent markers in a Usage subfolder,
keyed by a stable hash of the exact type name (also store/verify the name).
Readers open markers without FILE_SHARE_DELETE; removal must first acquire a
DELETE-access handle to the marker and hold it through commit. A conflicting
lease fails with a sharing violation instead of removing an in-use package;
crashes release handles. Merely appearing in the palette is not a usage lease.

Lease by type, independently of the installed JSON file, so an unresolved use
already protects a package subsequently installed in another process. The same
mutex covers generation refresh and lease acquisition before new uses, avoiding
a check/delete race. If usage cannot be verified, retain conservative leases
until verification is possible; an unscannable project also needs a shared
unknown-usage guard that blocks removal in another host. Close usage handles
when their project/document closes or stops referencing the type. Empty marker
files do not count as use: only a live incompatible handle does.

Store mutations notify the other running host instances to refresh their
catalogs; also check store generation when opening Packages or activating a
host, so missed notifications do not leave it permanently stale. Active uses
retain their immutable metadata and prevent external removal. Recheck the
actual file state/leases under the mutex even if a dialog row appears removable.

For successful removal, stage the replacement registry/palettes and hide the
package file with a same-directory temporary/tombstone rename, then perform
the prepared in-memory commit and delete the tombstone. A failed preparation
keeps disk/catalog/UI unchanged; a final cleanup failure is reported and the
non-.json tombstone is excluded from startup enumeration. Distinguish an already
committed removal with pending cleanup from a failed removal. Installation uses
the corresponding staged commit. No callbacks/new document mutations run inside
either commit section. Under the mutex, startup deletes abandoned `.installing`
files (never published) and completes deletion of `.removing` tombstones
(committed removal intent). Test failure at every disk/registration/staging
boundary, including restart with either kind of leftover file.

## Application runtime and future Studio integration

Installed packages are design-time metadata only. Application hosts, compiled
applications and debug/run processes never enumerate DesignTime, link the
package loader, register package factories, or require installed package files.

Before creating the Default main form, an application registers every required
real visual/nonvisual control factory under the exact type names used by its
definitions. Only then does the private generated startup code load/create the
Default form. Frames and subsequently created forms use that same real-control
registry. Registration conflicts or missing real factories produce a startup
error before a partly created UI is shown; there is no runtime placeholder
fallback. Custom values/events must match the package's design contract.

AQVMT now registers a real QuadooPlugin component and loads its Module string
directly, then with a `Modules\` prefix on failure, using Host.LoadQuadoo. Its
returned object replaces the named script property before OnCreate. This behavior
belongs to AQVMT's real component; design packages remain metadata only. Future
Run/Debug/Compile work must provide general runtime factory registration and
dependency staging. Deployed applications must
work on a machine with no designer and an empty/missing DesignTime directory.

## Planned source integration

All entries below are proposed files; only this plan and the example assets
are created in the current pass.

| Location | Responsibility |
| --- | --- |
| Packages/Packages.h, PackageSchema.cpp | Shared contracts, strict parsing/normalization, immutable package definitions and diagnostics. |
| Packages/PackageMetadata.cpp, PackageProperties.cpp | Published descriptor interfaces, base definitions, value validation and instance storage. |
| Packages/PlaceholderComponent.cpp, PlaceholderControl.cpp, PlaceholderContainer.cpp | Separate public-interface adapters, lifecycle, notifications and visual container behavior. |
| Packages/PackageFactories.cpp, PackageIcons.cpp | Category factories, lifetime counts, Design-only guard, PNG/HICON provider. |
| Packages/PackageStore.cpp, PackageCatalog.cpp | User directory, normalized atomic files, usage leases, registry generations and host notifications. |
| Packages/PackagesDialog.cpp, PackagesDialog.rc, PackagesResource.h | Shared Win32 dialog procedure/template/IDs and 20px image list. |
| Designer/Editor.cpp, Palette.cpp, Designer.h | Prepare/commit palette refresh while retaining scroll/selection/focus and document state. |
| DesignerHost/Program.cpp and a host .rc include | Host catalog, always-enabled Packages command, usage callback and shared resources. |
| Studio main-window/project designer code and QuadooStudio.rc | Main-window package ownership/command, all-project usage scan and shared resources. |
| DesignerExamples/QuadooPlugin.json, QuadooPlugin.png | Production-oriented nonvisual test package with Module:String. |

Allocate shared dialog/control/resource IDs in a dedicated checked range; check
both hosts for collisions. Pass the host HINSTANCE to DialogBoxParamW so each
EXE loads its own compiled copy. The dialog owns its list-view image list and
temporary icon handles; it destroys them on every close/failure path. Reuse the
same IQuadooPaletteIcon provider for dialog, palette and nonvisual tray artwork.

Preserve VS2008, Win32/x64 and Windows XP, repository containers/strings,
HRESULT/Check cleanup conventions and no STL/exceptions. Shared package code
must not depend on QuadooScript or private runtime classes. No published
QuadooForm IID/vtable/export or form-definition schema change is currently
needed; proposed host/catalog interfaces are shared C++ implementation details.
Report any concrete ABI need before implementing it.

## Implementation stages and validation

1. **Schema and storage.** Implement parser, codecs, PNG normalization and
   self-contained install store. Validate QuadooPlugin from a sidecar and an
   embedded-icon variant; copy/move/delete its original source assets after
   installation and verify the installed package remains usable.
2. **Design factories.** Implement the three capability adapters and registry
   facade. Exercise Module editing/defaults/serialization, visual leaf versus
   container QueryInterface/layout, child nesting, copy/paste, tab order,
   destruction, observer errors and retained metadata/factory lifetimes.
3. **Both hosts and Packages UI.** Compile shared files/resources into both
   EXEs, add always-enabled commands, load before documents, refresh all
   palettes/trays, implement usage scans/leases and transactional install/remove.
4. **Hardening.** Run focused package tests, full existing Studio/designer and
   native suites in Win32/x64 Debug/Release. Build only affected projects; reuse
   existing SimbeyCore binaries. Record strict resource results and actual XP
   testing separately from VC9 compilation.

Acceptance coverage must include:

- QuadooPlugin listed with its 20px transparent icon, placed only in the tray,
  Module edited inline, persisted under `components` and restored on reopen.
- Startup discovery in either host after installing through the other; Packages
  usable in Studio with no project and every legacy project type/Code view.
- Removal blocked by active/inactive/unsaved/new forms, frames, unopened project
  units, nested custom controls, unresolved uses, retained instances and uses in
  the other host process. The dialog identifies uses and unreadable definitions.
- Removal succeeds after every referencing document/project is closed or edited
  to remove the last use, and palette-only factory references do not block it.
- Invalid JSON, duplicate keys/names/IDs/types, reserved property collisions,
  invalid defaults/choices/event arguments, invalid Base64/PNG, over-limit data,
  denied access, disk failure and every allocation/registration/staging failure
  leave the prior observable disk/catalog/editor state intact.
- Concurrent install/remove, use/remove races, external changes, missed refresh
  notifications, host crashes and leftover staging/tombstone files.
- COM identity/AddRef/Release across every interface, descriptor survival after
  removal, no leaks of HWNDs, GDI/USER objects, handles or image lists, DPI-scaled
  icons and preserved palette scroll position after both kinds of insertion.
- Preview rejects placeholder-backed types without changing the document.
  A native test registers an independent real QuadooPlugin factory first and
  loads the saved form in Runtime successfully; without that factory it fails.
  This test never loads the installed package catalog into its runtime process.

The example is a contract fixture and production package source. Preserve its
packageId and control type when using it in production; changing either would
create a different installation/type. No real plugin DLL is implemented here.

## Validation of this planning pass

The example JSON parses without duplicate keys and matches the example in this
plan. Its stable GUID, nonvisual category, container=false and Module:String
empty default were checked. QuadooPlugin.png decodes as a 20x20 RGBA PNG with
both transparent and opaque pixels; a simulated embedded-Base64 JSON round trip
preserves the PNG bytes. The integration-plan link resolves to this document.
No native binaries were changed or rebuilt; actual install/remove, factory and
host tests belong to the implementation stages above.
