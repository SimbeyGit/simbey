# ListView example

Open ListViews.qform.json in the standalone Designer, select Results, then use
Form > ListView Columns Editor or double-click Columns in the inspector.
The file contains an ImageList and PopupMenu assignment and no initial rows.

The columns use widths 260, 110 and 170 at 96 DPI. At 144 DPI their native widths
are 390, 165 and 255 pixels. Changing the designer zoom or runtime DPI does not
multiply the widths stored in the definition.

Applications populate the existing collection from their form's script code:

```text
var item = Results.Items.Add("Readme.txt");
item.ImageIndex = 0;
item.SubItems.Add("2 KB");
item.SubItems.Add("2026-10-26");
item.Checked = true;
```

Name is logical column zero (Caption); Size and Modified use SubItems[0] and
SubItems[1]. Columns are collection objects. They never become script-class
members or component-tree nodes. Item data is not edited in the designer.
