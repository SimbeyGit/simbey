# QuadooPlugin design-time package

`QuadooPlugin.json` and its same-stem `QuadooPlugin.png` are source assets for the
Packages feature. The PNG is a transparent 20x20 plug-in module glyph.
The package defines one nonvisual type, QuadooPlugin, with Module:String and an
empty-string default. Name and lifecycle events come from the shared component
base metadata. It has no children, window, layout bounds or tab order.

This is a package declaration, not a form definition. Choose this JSON from
Form > Packages > Install in either host. Installation embeds the adjacent PNG into
the installed JSON; the two source files can then be moved without affecting
the installation. Keep their stems identical when distributing this source pair.

A future saved form uses an ordinary component definition, for example:

```json
{
  "type": "QuadooPlugin",
  "name": "QuadooPlugin1",
  "properties": { "Module": "MyPlugin.dll" }
}
```

That instance goes in the form/frame's root `components` array. The designer
only edits Module as text and does not load MyPlugin.dll. AQVMT now registers a
real QuadooPlugin component before loading forms/frames. It tries Module first,
then `Modules\` plus Module, using the existing QuadooScript/native module loader.
The module's CreateObject(null) result replaces the named script property before
OnCreate. A failure of both attempts aborts construction. Runtime does not load
the design-time package. Other application hosts must supply an equivalent real
factory; module deployment remains the application's responsibility.

Preserve the packageId `{A14F5FB9-D835-4812-ABEF-8A265E49B247}` when distributing
updates of this same package. Version 1's UI requires removal before
reinstallation and blocks removal while a current document/project uses it.

The full design is in [DesignTimePackagesPlan.md](../Packages/DesignTimePackagesPlan.md).
Regenerate the artwork with GenerateQuadooPluginIcon.py and Pillow; normal
native builds and package installation do not need Python.
