# XML v1

Root: `<Screen version="1" id="menu" stylesheet="example:assets/theme.css">`.
Standard tags: Screen, Panel, Row, Column, Grid, Stack, ScrollView, Text, Image,
Vector, Button, Toggle, Checkbox, Slider, TextInput, ProgressBar, Repeat,
Template, Component, Modal, Tooltip, Select. Registered native widget tags are
accepted without changing the parser. Images and vectors require `source`.
Text, Image, Vector, TextInput, Slider and ProgressBar cannot contain children.
Screen cannot be nested. Unknown attributes are errors except Button action
parameters, Component template parameters, custom widget attributes and `data-*`.

Common attributes: id, class (space separated), style, disabled, selected,
visible, anchor, tooltip, draggable. Controls use value, checked, writable,
min, max, step, placeholder, action. Select uses `options="Low|Medium|High"`;
activation cycles choices. This is a compact selector, not a popup dropdown.
Tooltip elements are overlay containers; `tooltip="Text"` adds a hover popup.
Modal restricts focus and pointer targeting to its subtree. Put overlays last
in author order for visual stacking.

Text can use `value="Hello"` or direct text content; mixed rich text is not
supported. Property bindings occupy an entire attribute: `{player.name}`.
There is no expression evaluation or automatic `@` localization; supply
localized strings through the data model.

```xml
<Screen version="1">
 <Template name="Card"><Button id="card"><Text value="{props.title}"/></Button></Template>
 <Column>
  <Repeat id="cards" items="{items}" key="id">
   <Component template="Card" title="{item.name}"/>
  </Repeat>
 </Column>
</Screen>
```

Templates have a unique name and one root. Component passes named parameters
as `props`. Repeat has one child and requires nonempty unique scalar keys.
Stable expanded IDs contain the repeat ID, a hex encoded key and the child ID.
Explicit IDs survive safe reorder/reload. Auto IDs now use authored ordinals rather than generated traversal counts; they
survive list changes within one document. Continue to use explicit IDs for state
that must survive authored-document edits.

DTD/entity declarations, network resources and traversal IDs are forbidden.
Default limits: 4 MiB XML, 10,000 authored/generated nodes, depth 64,
64 attributes per element, 256-byte authored IDs and 16 MiB expanded payload. Parser diagnostics retain source/line; XML syntax
errors also report libxml2's column. Semantic columns are currently 1.

Template reference validation also has a total visit budget (10,000 by default),
including acyclic fanout. Depth and cycle checks alone do not bound traversal work.
