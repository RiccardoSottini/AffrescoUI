;;; look.lisp --- Tempo's looks: its colours, dark and light, the assistant's and the shared kit's, and the styles its
;;; controls share

;;; Colours

(defpart colours "Every colour, dark and light, by name: @name in a view."
  :colors {:background {:dark "#10131c" :light "#f6f3ec"}
           :sidebar {:dark "#0b0d14" :light "#ece7dc"}
           :surface {:dark "#161a26" :light "#fffdf8"}
           :rowHover {:dark "#1e2332" :light "#f0eadd"}
           :playingRow {:dark "#2b2619" :light "#fbecc8"}
           :bar {:dark "#1a1f2d" :light "#fffdf8"}
           :text {:dark "#eef1f7" :light "#1d1a14"}
           :muted {:dark "#8d95a8" :light "#6d675a"}
           :faint {:dark "#323a4d" :light "#d8d0bf"}
           :border {:dark "#232a3a" :light "#e3dccd"}
           :accent {:dark "#f2b544" :light "#b45309"}
           :accentHover {:dark "#ffcb6b" :light "#cc6a14"}
           :accentSoft {:dark "#352d1a" :light "#fbe7c2"}
           :onAccent {:dark "#1c1405" :light "#ffffff"}
           :button {:dark "#232a3a" :light "#ece7dc"}
           :buttonHover {:dark "#2d3548" :light "#e2dbcb"}
           :menu {:dark "#1a1f2d" :light "#fffdf8"}
           :field {:dark "#10131c" :light "#f6f3ec"}})

(deftokens tokens "The assistant's and the shared kit's colours, from Tempo's."
  {:assistant.accent @accent
   :assistant.accentHover @accentHover
   :assistant.onAccent @onAccent
   :assistant.surface @menu
   :assistant.raised @button
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted
   :assistant.dot @faint
   :assistant.warningBackground @accentSoft
   :kit.background @background
   :kit.surface @menu
   :kit.bar @bar
   :kit.barHover @buttonHover
   :kit.field @field
   :kit.text @text
   :kit.dim @muted
   :kit.body @muted
   :kit.selection @accentSoft
   :kit.border @border
   :kit.primary @accent
   :kit.onPrimary @onAccent
   :kit.secondary @button
   :kit.onSecondary @text
   :kit.accent @accent
   :kit.focus @accent
   :kit.radius 10
   :kit.cardRadius 16})

;;; Styles

(defstyle caption "A small, muted caption."
  (label :font small :textColor @muted :fitText true))

(defstyle pageTitle "A page's title."
  (label :font large :textColor @text :anchors {:left 32 :top 24} :fitText true))

(defstyle control "A round icon button."
  (label :text ""
    :height 32
    :radius 16
    :align center
    :color @bar
    :hover {:color @buttonHover}
    :focused {:color @buttonHover}
    :textColor @text
    :cursor hand
    :focusable true
    :role button))

(defstyle chip "A choice among several, as a chip."
  (label :font small
    :padding 9
    :radius 15
    :align center
    :fitText true
    :color @button
    :hover {:color @buttonHover}
    :focused {:borderColor @accent}
    :borderWidth 1
    :borderColor transparent
    :textColor @text
    :cursor hand
    :focusable true
    :role button))

(defstyle playButton "The large Play button."
  (label :icon play
    :text "  Play"
    :font heading
    :width 112
    :height 36
    :radius 18
    :align center
    :color @accent
    :hover {:color @accentHover}
    :focused {:color @accentHover}
    :textColor @onAccent
    :cursor hand
    :focusable true
    :role button))

