# components: the built-in components

Components the format ships, written in the format itself as JSON text: `menu`, `menuBar`, `contextMenu`, `dialog`,
`toast`, `hoverCard`, `settingsPage` and `quickPick` (`BuiltInComponents.cpp`), the desktop kit's `toolbar` and
`statusBar` (`KitBars.cpp`) and `popover` (`KitOverlays.cpp`), the form components `comboBox` and `passwordMeter`
(`BuiltInFormComponents.cpp`), and the chat components `chatBubble`, `typingIndicator`, `proposalCard`,
`chatComposer` and `chatLauncher` (`BuiltInChatComponents.cpp`). A document's `"use"` finds one with `builtInComponent()` when its
own `components` doesn't define that name, so a document can always replace a built-in one.

They use only what a document may write, with no service, connection or native call. Their pictures are in the
render check's golden images, so a change to one is a visual change.

Tests: `tests/unit/overlays.cpp`, `quickpick.cpp`, `form_inputs.cpp`, `kit.cpp`, `chat_components.cpp`.
