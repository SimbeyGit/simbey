# ImageButton example

Open ImageButtons.qform.json in the standalone designer, or add an ImageButton
to a form/frame in Studio. Picture and DisabledPicture use the normal image
property editor (PNG/ICO, with JPEG/BMP import).

- Save places its picture to the left of the caption.
- Picture above uses ImagePosition=Top and taller bounds.
- Automatic gray has no DisabledPicture; disabling generates grayscale at runtime.
- Custom disabled deliberately assigns a different icon to demonstrate precedence.

ImageWidth/ImageHeight define a bounding cell in pixels at 96 DPI. Spacing is
also stored at 96 DPI. Images retain their aspect ratio and ICO entry selection
follows the actual display size. Both orientations center the group of picture
and caption. Set neither picture for a caption-only button.

ImageButton inherits the usual Button events and properties, including Default,
Cancel, ModalResult, PopupMenu and TabOrder. Studio generates one member for the
button. For example, an OnClick handler can set `Save.Enabled = false`; assigning
`Save.DisabledPicture = null` restores the automatic grayscale fallback.

The example embeds complete image data and needs no external icon files.
