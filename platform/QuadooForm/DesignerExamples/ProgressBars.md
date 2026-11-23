# ProgressBar

Choose **ProgressBar** from the tools palette and place it on a form or container.
The native Windows progress bar displays `Position` in the range `Min` to `Max`.
Defaults are `Min = 0`, `Max = 100`, `Position = 0`, and `Step = 10`.

```javascript
ProgressBar1.Max = 500;
ProgressBar1.Position = 0;
ProgressBar1.Step = 1;
ProgressBar1.StepIt();
```

Position assignments clamp to the range. Changing either bound also clamps the
current position. A reversed range is rejected without changing the control.
To move the range above the current maximum, set `Max` before `Min`; when moving
it below the current minimum, set `Min` before `Max`.

`StepIt()` advances by `Step`. Like the native `PBM_STEPIT`, it wraps past the
range limits (0..100 at 100 with Step 10 becomes 10). A negative step moves
backward; a zero step leaves the position unchanged. Equal limits are allowed
and keep the position at that value.

All four values serialize as ordinary integer properties. They are logical
values and do not change with DPI; only the control's geometry scales.
