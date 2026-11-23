# QuadooForm palette artwork

Each ICO contains one transparent 20x20, 32-bit uncompressed DIB image with an AND
mask. Built-in component files are standard resources in QuadooForm.dll.

GenerateIcons.py authors the images and creates Designer/DesignerIconData.h for
the editor-only Pointer/fallback glyphs. It also writes PalettePreview.png for
actual-size inspection on white, selection blue and dark backgrounds. Regenerate
manually with Python and Pillow; C++ builds need neither dependency.

ExternalMeter and ExternalService are resources in the independent test DLL.
Provider/ownership details and validation are in ../PaletteIconsImplementation.md.

ColorDialog.ico is the new nonvisual picker component icon (20x20).
ColorPicker.ico is the dialog form icon (16/32px DIB), authored by
QuadooForm/Dialogs/GenerateAssets.py and embedded into its JSON definition.
