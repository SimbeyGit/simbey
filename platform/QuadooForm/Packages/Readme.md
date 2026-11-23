# Design-time control packages

Both Quadoo Studio and QuadooForm Designer provide Form > Packages..., including
when Studio has no project or is showing code. The shared Win32 dialog lists
installed package names, control types, categories, status and 20px icons.
Install selects a JSON package; Remove is unavailable while a current document
or project references its control type. The details area identifies uses or
files whose usage could not be verified.

To install the supplied example, select
platform/QuadooForm/DesignerExamples/QuadooPlugin.json. Keep the adjacent
QuadooPlugin.png beside it for this first installation. The installer embeds
that PNG into its installed JSON copy. QuadooPlugin then appears in the palette
and nonvisual tray, with a single-line Module property. Module is text only in
the designer; it never causes the designer to load a DLL.

Packages are shared by the two programs in the current user's
%APPDATA%\QuadooForms\DesignTime directory. Installed filenames use the package
GUID; display/type names do not become filesystem paths. Source files are not
modified, and installed packages have no subsequent dependency on their source
JSON or PNG. An embedded icon takes precedence over a same-stem PNG. A missing
icon uses a generic glyph; corrupt image data is rejected.

The package format and full implementation contract are described in
[DesignTimePackagesPlan.md](DesignTimePackagesPlan.md). Version 1 supports one
visual leaf, visual container or nonvisual type per package. Type, property,
event and argument names use ASCII identifiers, at most 128 characters, so the
designer can generate valid component names. Package display names can be
Unicode. Native QuadooForm framework property names are reserved. Package
metadata supplies custom properties/defaults/choices and optional events; Name,
visual layout properties and lifecycle events are composed automatically.

Installation accepts at most 1 MiB of UTF-8 JSON, 256 custom properties, 64 custom
events and 64 arguments per event. PNG icons are bounded to 256 KiB and 256x256
pixels; they are displayed at 20x20 at 96 DPI and scale with UI DPI. Base64 is
canonical padded data using SimbeyCore. Custom Collection values receive
structural JSON validation; validation specific to the real control belongs to
its runtime factory.

Boolean properties use an inline True/False drop-down. Enum properties use an
inline drop-down populated from their descriptor choices; the selected numeric
value is stored rather than its label or list position. Read-only properties
remain read-only, and Flags keep the existing checkbox editor.

ObjectReference properties use the inspector's inline component drop-down. New
connections are stored as component GUID strings; **(none)** clears a reference
to null. Empty-string defaults and existing name-based or unresolved references
are preserved until edited. Read-only descriptors cannot be changed.

Saving preserves ordinary form-definition JSON: the real type name, component
GUID/name, property values, event method names and children/components. It adds
no installation IDs, package paths, artwork or placeholder implementation names.
An actually missing package continues to use the designer's existing lossless
missing-factory representation. Installing a missing type adds it to the
palette immediately; explicitly reopen its document to resolve existing
unavailable instances. No unsaved document is silently replaced.

Installed factories construct only in Design mode. Native Preview reports a
missing real runtime implementation when the definition uses a placeholder.
Applications register their real factories before creating the Default form or
loading any other forms/frames. Runtime property types and defaults must match
the package contract, since ordinary form serialization omits default values.
Applications and QuadooForm.dll do not discover
this directory or load design-time package factories. There is no DLL-loading
or executable installation hook in this format.

Usage protection includes unsaved and inactive editors, unopened Studio form
and frame definitions, unknown/missing-factory references, retained instances,
and documents in another running host. Unreadable definitions fail closed.
Per-type file leases are acquired before document adoption and refreshed from
serialized definitions; file handles vanish automatically after a host crash.
Closing the last referencing project/document or removing the last instance
allows removal after outstanding control references are released. Clipboard
contents alone do not pin a package. An unreadable installed package can be
removed when no custom/unknown document uses prevent verifying its safety.

The catalog serializes mutations with a per-user/per-store named mutex and
rejects reentrant changes. It stages validated descriptors, registry generations
and replacement palettes before publishing files or UI state. Failed staging or
file writes keep the prior installation and document intact. Palette refresh
retains scrolling, selection and armed tools; removing an armed tool selects
Pointer without scrolling. Files ending in .installing were never published;
.json.removing files record committed removals. Startup cleans these staging
files and ignores them during package discovery.

Source ownership:

- Packages.h / PackageServices.h: shared host/catalog and private document-usage
  contracts, separate from the published runtime ABI.
- PackageSchema.cpp / PackageMetadata.cpp: strict JSON and metadata/value codecs,
  shared published-interface descriptor adapters and bounded PNG handling.
- PackageFactories.cpp: independent design-only component, leaf and container
  implementations, lifecycle, observers, layout and palette icon provider.
- PackageStore.cpp: registry facade, transactional storage, usage leases,
  catalog refresh and deterministic transaction fault injection.
- PackagesDialog.cpp / PackagesDialog.rc / PackagesResource.h: shared Win32 UI.

These C++ sources and dialog resources are compiled directly into each host EXE.
There is no new DLL, resource added to QuadooForm.dll, private runtime-class
dependency, STL, exception support or QuadooScript integration. Build with the
existing build-studio.cmd/build-designer.cmd scripts. Focused validation uses
QuadooFormDesignerTests --packages-only and
QuadooStudioIntegrationTests --packages-only; the normal suites also include
the new tests.

Results and the full-suite clipboard limitation are recorded in
[Validation.md](Validation.md).
