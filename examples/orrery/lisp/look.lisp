;;; look.lisp --- Orrery's colours and styles

;;; Colours

(deftokens tokens "Tokens."
  {:background {:light "#ebf0ee" :dark "#0d1414"}
   :panel {:light "#f7faf9" :dark "#131c1c"}
   :raised {:light "#dfe8e5" :dark "#1b2827"}
   :border {:light "#c3d0cc" :dark "#263634"}
   :text {:light "#172120" :dark "#e3ecea"}
   :muted {:light "#536662" :dark "#8fa39f"}
   :accent {:light "#0f766e" :dark "#2dd4bf"}
   :sky {:light "#0b1213" :dark "#070b0c"}
   :note {:light "#fcd9b8" :dark "#fcd9b8"}
   :mark {:light "#e11d74" :dark "#f472b6"}
   :onSky "#e3ecea"
   :onSkyMuted "#93a8a3"
   :hud "#0b1514e6"
   :hudBorder "#2dd4bf38"
   :assistant.accent @accent
   :assistant.accentHover {:light "#0d9488" :dark "#5eead4"}
   :assistant.onAccent {:light "#f2fffc" :dark "#04201c"}
   :assistant.surface @panel
   :assistant.raised @raised
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted})

;;; Styles

(defstyle button
  (label :height 30
    :radius 6
    :padding 8
    :color @raised
    :textColor @text
    :align center
    :cursor hand
    :focusable true
    :hover {:color @border}
    :focused {:borderWidth 2 :borderColor @accent}))

(defstyle heading (label :height 26 :padding 6 :textColor @muted :font small))
