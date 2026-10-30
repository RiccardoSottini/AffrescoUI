{:note "Quaderno's look, \"ink and paper\": a dark theme (cool charcoal) and a light one (warm paper), both with a violet ink accent; the syntax colours, the assistant's and the kit's, so menus and dialogs match. The theme is state.theme: dark, light, or system (the window's colour scheme)."
  :colors {:background {:dark "#23282a" :light "#fbf9f4"}
    :chrome {:dark "#1a1e20" :light "#efebe1"}
    :sidebar {:dark "#1e2325" :light "#f4f1e9"}
    :tabBar {:dark "#1a1e20" :light "#e9e4d8"}
    :tab {:dark "#2b3134" :light "#f1eee6"}
    :hover {:dark "#2f3639" :light "#e5e0d3"}
    :rowSelected {:dark "#363352" :light "#e3def8"}
    :panel {:dark "#1e2325" :light "#f4f1e9"}
    :text {:dark "#e3e6e4" :light "#24262b"}
    :muted {:dark "#98a19d" :light "#5c6066"}
    :faint {:dark "#5f6965" :light "#a29f96"}
    :border {:dark "#131618" :light "#d8d2c4"}
    :accent {:dark "#a08ff5" :light "#5b48c8"}
    :accentSoft {:dark "#3a3558" :light "#d9d3f5"}
    :onAccent {:dark "#17141f" :light "#ffffff"}
    :input {:dark "#1a1e20" :light "#ffffff"}
    :menu {:dark "#272c2f" :light "#fffdf8"}
    :backdrop {:dark "#00000088" :light "#00000040"}
    :line {:dark "#2c3235" :light "#efebe1"}
    :selection {:dark "#403a66" :light "#d6cff5"}
    :guide {:dark "#343b3e" :light "#dfd9cc"}
    :plain {:dark "#e3e6e4" :light "#24262b"}
    :comment {:dark "#7d8984" :light "#7b8079"}
    :string {:dark "#b5d68a" :light "#3f7d20"}
    :prompt {:dark "#6fc3df" :light "#1b7290"}
    :error {:dark "#ef6f7a" :light "#c4314b"}
    :warning {:dark "#e8c872" :light "#9a6400"}
    :badgeHeading {:dark "#463f6e" :light "#d9d3f5"}
    :badgeClass {:dark "#2f5148" :light "#cfe8df"}
    :badgeFunction {:dark "#2c4a5c" :light "#d3e6f1"}}
  :tokens {:assistant.accent @accent
    :assistant.accentHover {:dark "#b6a8f8" :light "#4a39b0"}
    :assistant.onAccent @onAccent
    :assistant.surface @sidebar
    :assistant.raised @tab
    :assistant.border @border
    :assistant.text @text
    :assistant.dim @muted
    :assistant.dot @faint
    :assistant.success @string
    :assistant.danger @error
    :assistant.warning {:dark "#3f3a26" :light "#f5ead0"}
    :syntax.comment {:dark "#78857f" :light "#848a83"}
    :syntax.keyword {:dark "#b39cf0" :light "#6a3fc8"}
    :syntax.string {:dark "#b5d68a" :light "#3f7d20"}
    :syntax.string.escape {:dark "#6fc3df" :light "#1b7290"}
    :syntax.number {:dark "#f0a6c0" :light "#b0306a"}
    :syntax.constant {:dark "#e8c872" :light "#9a6400"}
    :syntax.function {:dark "#6fc3df" :light "#1d6fa3"}
    :syntax.type {:dark "#5fd0b0" :light "#12806a"}
    :syntax.operator {:dark "#a5b0ac" :light "#5c6066"}
    :syntax.punctuation {:dark "#8e9995" :light "#6b6f69"}
    :syntax.property {:dark "#e3e6e4" :light "#24262b"}
    :syntax.variable {:dark "#e3e6e4" :light "#24262b"}
    :syntax.attribute {:dark "#b39cf0" :light "#6a3fc8"}
    :syntax.tag {:dark "#b39cf0" :light "#6a3fc8"}
    :syntax.label {:dark "#e8c872" :light "#9a6400"}
    :syntax.text.title {:dark "#6fc3df" :light "#1d6fa3"}
    :syntax.text.literal {:dark "#b5d68a" :light "#3f7d20"}
    :kit.background @background
    :kit.surface @menu
    :kit.bar @chrome
    :kit.barHover @hover
    :kit.field @input
    :kit.text @text
    :kit.dim @muted
    :kit.body @muted
    :kit.selection @rowSelected
    :kit.border @guide
    :kit.primary @accent
    :kit.onPrimary @onAccent
    :kit.secondary @tab
    :kit.onSecondary @text
    :kit.accent @accent
    :kit.focus @accent
    :kit.backdrop @backdrop
    :kit.info @prompt
    :kit.warning @warning
    :kit.error @error
    :kit.success @string}}
