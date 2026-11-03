;;; look.lisp --- Tempera's colours and its named styles
;;;
;;; The tokens are the palette, light and dark, with what the shared kit and the assistant add-on take from it; a
;;; style is a node's look that many nodes name (:style card).

;;; Colours

(deftokens palette "Tempera's colours: warm ochre on sage greens, light and dark."
  {:background {:light "#aab4a6" :dark "#141815"}
   :panel {:light "#eef1ea" :dark "#242a25"}
   :header {:light "#dfe5da" :dark "#1c211d"}
   :canvasBack {:light "#c4ccc0" :dark "#1a1f1b"}
   :border {:light "#c3ccbf" :dark "#38423a"}
   :text {:light "#1c231d" :dark "#dfe5dc"}
   :muted {:light "#5e6a5d" :dark "#8e9a8c"}
   :accent {:light "#9a6a0e" :dark "#d39b2a"}
   :accentSoft {:light "#efdfb6" :dark "#4a3b19"}
   :selected {:light "#d8e1d2" :dark "#364238"}
   :hover {:light "#e2e8dd" :dark "#2e362f"}
   :input {:light "#fbfcf8" :dark "#191e1a"}
   :ruler {:light "#e4e9df" :dark "#1f2520"}
   :guide {:light "#9333ea" :dark "#c084fc"}
   :menuHover {:light "#efdfb6" :dark "#4a3b19"}})

(deftokens kit "The shared kit's colours and corners (dialogs, the command search), from the palette."
  {:kit.background @panel
   :kit.surface @panel
   :kit.bar @panel
   :kit.barHover @hover
   :kit.field @input
   :kit.text @text
   :kit.dim @muted
   :kit.body @muted
   :kit.selection @accent
   :kit.border @border
   :kit.primary @accent
   :kit.secondary @selected
   :kit.onSecondary @text
   :kit.focus @text
   :kit.radius 6
   :kit.cardRadius 10})

(deftokens assistant "The assistant add-on's colours, from the palette."
  {:assistant.accent @accent
   :assistant.accentHover @menuHover
   :assistant.onAccent "#1c1608"
   :assistant.surface @panel
   :assistant.raised @header
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted
   :assistant.warningBackground @selected})

;;; Styles

(defstyle bar "A bar across the window: the menu bar, the status bar." {:color @panel :alignItems center :width "100%"})

(defstyle card "A rounded panel: the options bar, the tools bar." {:color @panel :radius @kit.cardRadius})

(defstyle dim "Quiet text, as wide as it is." (label :textColor @muted :fitText true))

(defstyle sep "A thin upright line between groups of options." {:width 1 :height 20 :color @border})

(defstyle clear "Nothing to see: a spacer, or a frame for what is inside." {:color transparent})
