# Style language v1

Selectors: tag, `.class`, `#id`, compounds such as `Button.primary:hover`, `*`,
and comma lists. States: hover, focus, active, disabled, selected. No descendant
selectors, combinators, media queries, calc, URL access or executable content.
Specificity is ID 100, class/state 10, tag 1; later rules win ties. Imports
expand in place. Theme override sheets append after the screen sheet. Inline
declarations win. Use specificity to override more specific rules.

Properties:

- background/background-color, color, border-color: #RGB, #RRGGBB, #RRGGBBAA,
  transparent, white, black, red.
- border-width, border-radius, padding, margin, gap: a single nonnegative dp length.
- width, height, min-width, min-height, max-width, max-height: dp, %, auto.
- opacity: 0–1; inherited as a simplified effective opacity.
- align-items: start, center, end, stretch. justify-content: start, center, end,
  stretch (stretch currently has start behavior on the main axis).
- font-family: namespaced font resource ID. font-size: dp. font-weight: numeric weight; values ≥600 use FreeType synthetic bold.
- text-align: left, center, right. vertical-align: top, center, bottom.
- image-fit: contain, cover, stretch. overflow: visible, hidden, scroll.
- visibility: visible, hidden. Hidden elements consume no layout space.
- columns: positive integer; Grid's XML attribute takes precedence.
- anchor: top-left, top-right, bottom-left, bottom-right, center.
- transition: duration in seconds, e.g. `0.15s` or `0.15`; interpolates colors
  and opacity after style state changes, without relayout each animation frame.

Custom properties (`--accent`) inherit. `var(--accent)` must occupy the entire
value. Missing/circular variables are errors. No fallback var argument yet.
`@import "example:assets/base.css";` resolves through Resources with bounded
recursion, byte budgets and cycle detection. CSS comments are supported.
Unknown properties and invalid values produce errors, never native execution.

Length rules: bare numeric lengths mean dp. Percent resolves against the
parent's available content size. Auto uses intrinsic content size; row/column
stretch and grid cells may enlarge auto dimensions. Min wins when max < min.
Percent children inside auto parents use the externally available size rather
than an iterative fixed point. There is no flex grow/shrink solver.
