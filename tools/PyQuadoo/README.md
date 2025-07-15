# PyQuadoo

![PyQuadoo](images/PyQuadoo.png)

**PyQuadoo** is a CPython extension module that exposes the QuadooScript runtime (`QuadooVM`) and its core object model to Python. This integration allows Python scripts to instantiate and interact with QuadooScript types such as arrays, maps, JSON objects/arrays, and VM-executed script logic.

---

## Features

- Instantiate and run QuadooScript virtual machines (`QuadooVM`) directly from Python.
- Convert between QuadooScript and Python data types.
- Wrap and expose QuadooScript objects (`IQuadooObject`) to Python and vice versa.
- Full support for JSON objects and arrays (`IJSONObject` and `IJSONArray`), including indexing, attribute access, and serialization.
- Native `void*` handle (QuadooVM::Void) support via Python capsules.
- Supports dynamic property access, `delete`, `len()`, iteration, and conversion to strings.
- Python→Quadoo interop: wrap native Python lists/objects for use in QuadooScript.
- Quadoo→Python interop: unwrap QuadooScript arrays into Python-native equivalents.

---

## Installation

- Either build from the source code here, or...
- Install the pre-built package from http://www.quadooscript.com/

---

## Types and Classes

### `QuadooVM`
Represents a running QuadooScript virtual machine.

**Methods**:
- `FindFunction(name: str) -> (index: int, param_count: int)`
- `PushValue(value: Any)`: Push a Python value to the QuadooScript call stack.
- `RunFunction(index: int) -> Any`: Executes a function from the VM.
- `AddGlobal(name: str, obj: QuadooObject)`: Adds a global object reference.
- `RemoveGlobal(name: str)`: Removes a global reference.

---

### `QuadooObject`
Wraps a QuadooScript object (`IQuadooObject`) for Python access.

- Attribute access (`obj.name`) maps to `GetProperty`, `SetProperty`, or `Invoke`.
- Indexing (`obj["key"]`) and deletion (`del obj["key"]`) supported via `GetIndexedProperty`, `SetIndexedProperty`, and `DeleteProperty`.

---

### `QuadooAttribute`
Provides indirection to get/set/invoke members on `QuadooObject` attributes.

**Methods**:
- `Get()`: Retrieve value
- `Set(value)`: Assign value
- Callable to invoke (i.e., `obj.name(args)`)

---

### `QuadooJSONArray`
Wraps a `IJSONArray` for Python.

**Pythonic Access**:
- Supports `len()`, `obj[i]`, assignment, deletion.
- Iterable

**Methods**:
- `Add(value)`
- `Insert(index, value)`
- `Remove(index)`
- `Clear()`
- `FindObject(field, value) -> (JSONObject, index) | None`
- `FindString(value) -> index | None`

---

### `QuadooJSONObject`
Wraps a `IJSONObject` for Python.

**Pythonic Access**:
- Supports `obj["key"]`, `len(obj)`, deletion with `del`, and iteration via `obj.names`.

**Properties**:
- `names`: Iterable view of key names (like `.keys()`)

**Methods**:
- `Count` (as `len(obj)`)
- Attribute access maps to field access.

---

### `QuadooMap`
Exposes `IQuadooMap` (QuadooScript's native map type) to Python.

---

## Type Conversions

Python → Quadoo:
- `None` → `Null`
- `bool` → `Bool`
- `int` → `I4` or `I8`
- `float` → `Double`
- `str` → `String`
- `list` (if wrapped) → `Array`
- `dict` (not automatic) → must be converted manually
- Python `object` → `Object` (wrapped in `CPyObjectWrapper`)
- `capsule` (with name `"PyQuadoo.Void"`) → `Void`

Quadoo → Python:
- `Null` → `None`
- `Bool` → `bool`
- `I4`, `I8` → `int`
- `Float`, `Double` → `float`
- `Currency` → `Decimal`
- `String` → `str`
- `Array` → `list` (fully converted if not a Python wrapper)
- `Object` → `QuadooObject` or unwrapped Python object
- `Void` → `PyCapsule` with name `"PyQuadoo.Void"`
- `JSONObject`, `JSONArray` → `QuadooJSONObject`, `QuadooJSONArray`

---

## JSON Utilities

**Global Functions**:
- `JSONGetValue(json, path) -> Any`
- `JSONSetValue(json, path, value)`
- `JSONRemoveValue(json, path)`
- `JSONGetObject(json, path, ensure_exists=False) -> QuadooJSONObject | None`

Supports colon-path navigation (`"user:profile:name"`) and value manipulation.

---

## Serialization

- Calling `str(obj)` on `QuadooJSONObject` or `QuadooJSONArray` will serialize them to a JSON string via `JSONSerializeObject` or `JSONSerializeArray`.

---

## Future Enhancements

- Expand `QuadooMap` to support full dict-like behavior.
- Optional type hinting and Cython bindings for performance.
