# Actions

```cpp
ui.actions().registerAction("app:open", {
    {{"worldId", voxelui::ValueType::string}}, "world.read",
    [](const voxelui::ActionContext& c) -> std::optional<std::string> {
        // Enqueue a host operation using a copy of c.parameters.
        return {}; // or an error message
    }
});
ui.actions().grant("world.read");
```

XML: `<Button action="app:open" worldId="{item.id}">Open</Button>`.
Required parameter names/types and optional capability are checked before the
callback. Literal attributes are strings; bindings retain model types. Extra
attributes are allowed as UI context. Context owns IDs and parameter values;
callbacks must copy retained data. Errors/exceptions become diagnostics.
Unregistering an action safely disables later invocation. A callback is copied
before invocation, so it can unregister itself. Register before loading a screen;
unknown authored actions are errors. Enqueue long running work in the host.

This is a host capability boundary, not a sandbox for trusted native plugins.
Documents cannot resolve native function names, addresses or arbitrary symbols.
