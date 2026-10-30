# Build a custom game menu without writing C++

The preview already registers mock host actions. Create a directory under examples,
then author main.xml:

```xml
<Screen version="1" stylesheet="example:my_menu/theme.css">
 <Column anchor="center" style="width:400dp;gap:16dp">
  <Vector source="example:my_menu/logo.svg" style="width:100dp;height:100dp"/>
  <Text value="My menu / Моё меню" style="font-size:30dp"/>
  <Button action="example:play">Explore</Button>
 </Column>
</Screen>
```

Create theme.css:

```css
@import "example:assets/theme.css";
Button { background: #27735f; transition: 0.15s; }
Button:hover { background: #399b7f; }
```

Create logo.svg:

```xml
<svg viewBox="0 0 100 100"><path d="M50 5 L95 85 H5 Z" fill="#6de0be"/></svg>
```

Run `build/voxelui-preview examples/my_menu/main.xml`. Edit XML/CSS/SVG while
it runs. F3 shows IDs/bounds; F5 reloads; +/- changes scale. Mouse, Tab/Enter and
text editing work through SDL3. For a screenshot use `--headless menu.png`.

The example:play action is registered by apps/common/host.hpp and prints its
owned action context. A real host registers app-specific actions once in C++;
screen authors then reuse those capabilities entirely from text documents.
Validate with `build/voxelui-validate examples/my_menu/main.xml --mock-schema`.
