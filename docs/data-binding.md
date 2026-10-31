# Data binding

Value is a tagged union: empty, bool, finite host double, UTF-8 string, object,
array. Use `ui.data().set("settings.volume", .5)` and `{settings.volume}`.
Paths contain alphanumeric/underscore segments separated by periods, maximum
256 bytes. No operators, function calls, array expressions or script evaluation.
Host-computed values are ordinary observed properties; no separate expression engine.

DataModel tracks revisions and changed paths. Persistent dependency metadata maps
model paths through Component props and Repeat item scopes to computed node IDs.
Direct values and same-order keyed item properties patch existing nodes. Unrelated
bindings are skipped. Class and pseudo-state updates resolve affected styles;
measurements invalidate the changed node and ancestors and reuse clean subtrees.
Insertion/removal/reorder and resource changes retain validated full candidate
expansion, preserving compatible keyed state. See architecture.md for propagation
and remaining conservative placement work. Unchanged frames reuse computed nodes,
layout and immutable resource/text/vector caches.

A rejected type/action/style/budget update retains the previous UI and pending
changed paths until a later model revision can validate them. Runtime owns change
acknowledgement; hosts must not call DataModel::acknowledge while it is attached
to a runtime. Repeated writes to the same pending path are coalesced.

Repeat requires an array and scalar unique keys. Missing paths/arrays yield
warnings and safe empty content. Binding Slider/ProgressBar to nonnumbers,
Toggle/Checkbox to nonbooleans, or TextInput to nonstrings yields an error.
Action bindings preserve their Value type through dispatch.

`writable="true"` enables TextInput, Slider, Toggle, Checkbox and Select writes
back to a direct model path. Without it, edits are local. Item/props scope writes
are intentionally unsupported; send a host action with the item key instead.
Scroll, selection, text, focus and capture use stable IDs. Removed nodes release
state. A different tag at the same ID resets incompatible state.

The model/runtime are single threaded. Call set from the UI thread; marshal host
async completion onto that thread. DataModel::changes is acknowledged by update.
