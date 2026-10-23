# controls: the form controls

Node types for input that holds one value: `toggle` and `checkbox` (`ToggleType.cpp`), `slider`, `calendar`,
`codeInput` and `carousel`. Each file has one `register<Name>Type()`, called from `types/Types.cpp`'s
`registerBuiltInTypes()`.

A control's value goes both ways, through the value hooks its type registered: the document's value is applied to
the widget, and the widget's change is written back to the state place the node binds.

Adding a control follows `types/README.md`, "Adding a node type".

Tests: `tests/unit/form_inputs.cpp`, `form_carousel.cpp`, `date_calendar.cpp`, `types.cpp`, `registries.cpp`.
