{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Design tokens, for the rendering check: spacing, radius, a type size and colour roles used as \"@name\" in styles and nodes, with a light and a dark theme. The scenario restyles the whole screen with one small edit to the tokens."
  :window {:title "Design tokens" :width 420 :height 300}
  :state {:theme light}
  :theme "{{state.theme}}"
  :colors {:brand "#6366f1"}
  :tokens {:space.s 8
    :space.m 16
    :radius.card 10
    :type.body 16
    :color.surface {:light "#f8fafc" :dark "#0f172a"}
    :color.card {:light "#ffffff" :dark "#1e293b"}
    :color.text {:light "#0f172a" :dark "#e2e8f0"}
    :color.line {:light "#cbd5e1" :dark "#334155"}
    :color.accent @brand}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size @type.body}}
  :styles {:card {:type label
      :height 44
      :padding @space.m
      :radius @radius.card
      :color @color.card
      :textColor @color.text
      :borderWidth 1
      :borderColor @color.line}
    :primary {:type label
      :height 44
      :padding @space.m
      :radius @radius.card
      :color @color.accent
      :textColor "#ffffff"
      :align center}}
  :root (node #page :color @color.surface
    (column #list :x 20 :y 20 :width 380 :gap @space.s
      (node #first :style card :text "Spacing, radius and type from tokens")
      (node #second :style card :text "Colour roles switch with the theme")
      (node #action :style primary :text "One edit restyles the app")))}
