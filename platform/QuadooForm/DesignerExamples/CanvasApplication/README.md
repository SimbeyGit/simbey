# Canvas Application

Open CanvasApplication.qsproj in Quadoo Studio. CanvasForm is the Default form.
Run without the debugger to see a Canvas scene containing measured text, a
rectangle, a line, and an indexed ImageList image. Resize the form to redraw the
scene. All image data is embedded in the form definition.

Click **Export PNG** to put an independent PNG binary object in the script's
LastPng property and display its byte count. That binary can be passed to the
existing file/HTTP APIs; the sample does not write a file automatically.

The designer and modal Preview suppress script callbacks, so their Canvas is
an empty drawing surface. The actual Application runtime runs the OnCreate and
OnPaint handlers. The designer shows a Canvas hint that is not part of exports.

Control Width/Height are stored at 96 DPI. Drawing coordinates, PixelWidth,
PixelHeight and MeasureText's Width/Height are physical pixels. Font Size is in
points. ImageList drawing already uses the current DPI, so do not scale it twice.

AutoRedraw is enabled here. With AutoRedraw=False, draw only in OnPaint;
GetPixel and ExportPng are unavailable, while MeasureText remains usable.

The example also creates three runtime sprites: one from Sprite.png, one from
an embedded PNG binary, and one from a top-down RGB24 binary buffer. It draws a
partially clipped sprite using default invocation as well as a named Draw call.
Sprites are script objects and never appear in the designer's Objects tree.

Studio starts this project in its project directory (startDir is .\), where
Sprite.png is supplied. If launching the generated executable manually, use that
same working directory or adjust the file path. The other two sprites require no
external files. Drawing uses intrinsic physical pixels, without DPI rescaling.

For callable sprite references stored as properties/globals, a local alias is
convenient: var sprite = FileSprite; sprite(20, 20). Named FileSprite.Draw(...)
can be used directly. The current compiler resolves bare global call names as
functions; this iteration does not alter that language behavior.
