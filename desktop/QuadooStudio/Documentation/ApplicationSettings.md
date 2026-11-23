# Application project settings

Implemented 2026-10-22. Project > Settings is enabled only for Application
projects, in either Code or Design view. It uses a standard Win32 dialog with
an icon preview and Load/Clear actions above a two-column module list.

The dialog keeps independent working values. Cancel or closing the dialog
discards them. OK saves through the existing atomic project writer. A failed
save restores the live project's previous settings as well as preserving the
previous project file; a later Save cannot publish rejected settings.

## Project format

```json
{
  "target": "application",
  "icon": "AAABAA...",
  "modules": [
    ".\\Modules\\MyPlugin.qbc",
    "D:\\Shared Modules\\Helper.dll"
  ]
}
```

`icon` is the existing project field containing canonical Base64 ICO bytes,
using SimbeyCore's Base64 functions. Clear removes the field. Loading reuses
the form designer's validated ICO/PNG importer; PNG is converted to ICO in
memory. Complete 32-bit ICO images that omit the legacy AND mask are repaired
in memory from their alpha data; original files are never rewritten. The same
normalization applies when reading existing project icons, so executable
resources include the reconstructed masks. All-zero alpha without a mask is
treated as a legacy opaque bitmap. Truncated pixels, partial masks, overlapping
images and repaired data over the existing size limit remain invalid. The preview is a caller-owned HICON and is released on replacement,
clear, cancellation or dialog destruction.

`modules` is an array of path strings, preserving list order. Add stores a
project-relative path when possible and an absolute path otherwise. The first
column shows the file name; the second shows the saved path. Relative paths
resolve against the project directory, independent of Studio's current directory.
Missing or malformed dependencies cause a build error. Case-insensitive file
name collisions with other dependencies, runtime DLLs or the generated EXE are
rejected rather than renamed or overwritten.

## Executable preparation

The existing AQVMT staging transaction now includes the selected modules,
copied by file name directly beside the generated EXE. The icon is embedded
as a standard RT_GROUP_ICON resource and its RT_ICON images, with correct
14-byte group entries. Every build starts from AQVMT.bin, so clearing an icon
also removes it from the next generated executable.

Module files and the executable publish together. A copy, resource update,
locked-output or publication failure preserves the previous output. Unchanged
files need not be replaced. Removing a dependency stops preparing it and
removes its runtime catalog entry; the builder does not sweep unrelated or
previously copied files from the output directory.

The private application catalog records deployed file names, not development
source paths. AQVMT's one-argument Host.LoadQuadoo(name) resolves declared names
beside the running executable. QuadooPlugin preserves its original Module and
Modules-prefix attempts, then tries the declared executable-adjacent copy.
Other module paths retain their existing lookup rules. The configured working
directory is preserved. No design-time package loader runs in AQVMT.

This copies explicitly listed files; it does not discover transitive module
dependencies or register arbitrary native control factories. Those remain
separate deployment/bootstrap work.

## Validation

Visual Studio 2008 builds and the following suites pass in Debug/Release on
Win32 and x64:

| Suite | Assertions per configuration |
| --- | --- |
| Full Studio integration | 372 |
| Settings dialog, command state, Cancel, OK and failed-save rollback | 49 |
| Executable resources, module copies, collisions, rollback and launch | 84 |
| Script source synchronization | 106 |
| Studio package management | 19 |
| Application runtime | 103 |

The generated executable is run from the Windows directory after original
application source, form definitions and module files are hidden. Both copied
script/bytecode modules work, including direct Host.LoadQuadoo and QuadooPlugin.
Tests also load the embedded icon through Win32 and inject a failure after a
changed module is published, verifying that its old bytes and the old EXE survive.

Logs and the validation driver are under target/AppSettings-*. All four
SimbeyCore DLL hashes are unchanged. No QuadooForm or QuadooVM public ABI changed.
Actual Windows XP VM execution was not performed; VC9 and XP-compatible APIs
and project settings are retained.
