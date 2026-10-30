# Data binding

Value is a tagged union: empty, bool, finite host double, UTF-8 string, object,
array. Use `ui.data().set("settings.volume", .5)` and `{settings.volume}`.
Paths contain alphanumeric/underscore segments separated by periods, maximum
256 bytes. No operators, function calls, array expressions or script evaluation.
Host-computed values are ordinary observed properties; no separate expression engine.

DataModel tracks revisions and changed paths. Relevant ancestor/descendant paths
invalidate the computed tree; unrelated data changes do not run layout. Authored
XML stays parsed. A relevant update currently re-expands/resolves/layouts the
whole computed tree, with keyed state reconciliation; partial subtree layout is
still pending. Unchanged frames reuse layout and resource/text/vector caches.

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
