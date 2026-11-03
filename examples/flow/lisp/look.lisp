;;; look.lisp --- Flow's colours, its named styles and the one component of the Display menu
;;;
;;; The tokens are the palette, light and dark; a style is a node's look that many nodes name (:style toolButton).

;;; Colours

(deftokens tokens "Tokens."
  {:radius.card 10
   :assistant.accent @color.accent
   :assistant.accentHover @color.accent
   :assistant.surface @color.menu
   :assistant.raised @color.column
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.muted
   :radius.chip 12
   :color.background {:light "#e9eee9" :dark "#17201d"}
   :color.topBar {:light "#ffffff" :dark "#101614"}
   :color.topText {:light "#2b2925" :dark "#f6f5f2"}
   :color.topMuted {:light "#73706a" :dark "#918d86"}
   :color.topLine {:light "#e4e9e4" :dark "#1e2824"}
   :color.surface {:light "#ffffff" :dark "#101614"}
   :color.column {:light "#f3f6f3" :dark "#141c19"}
   :color.text {:light "#2b2925" :dark "#f6f5f2"}
   :color.muted {:light "#73706a" :dark "#918d86"}
   :color.border {:light "#dde4de" :dark "#24302b"}
   :color.input {:light "#ffffff" :dark "#141c19"}
   :color.inputFocused {:light "#effaf5" :dark "#15201c"}
   :color.accent {:light "#0d7f63" :dark "#3ecf9e"}
   :color.accentSoft {:light "#def5ec" :dark "#13281f"}
   :color.chipHover {:light "#e9eee9" :dark "#1e2824"}
   :color.chipOn {:light "#def5ec" :dark "#173326"}
   :color.chipOnText {:light "#0b6a52" :dark "#c4f0de"}
   :color.menu {:light "#ffffff" :dark "#1b2420"}
   :color.field {:light "#f3f6f3" :dark "#141c19"}
   :color.backdrop {:light "#0000004d" :dark "#00000099"}
   :color.card {:light "#ffffff" :dark "#18211e"}
   :color.sidebar {:light "#e9eee9" :dark "#17201d"}
   :color.sidebarHover {:light "#dfe6e0" :dark "#1f2a26"}
   :color.sidebarOn {:light "#d2ddd5" :dark "#283530"}
   :color.urgent {:light "#eb5757" :dark "#f2787a"}
   :color.high {:light "#73706a" :dark "#918d86"}
   :bug {:light "#d93f3f" :dark "#f2787a"}
   :bugSoft {:light "#fbe9e9" :dark "#2e1617"}
   :feature {:light "#3d7ae0" :dark "#78a6f0"}
   :featureSoft {:light "#e7effc" :dark "#16213a"}
   :design {:light "#8e4fd9" :dark "#b48cf0"}
   :designSoft {:light "#f1e9fb" :dark "#24183a"}
   :chore {:light "#26a17a" :dark "#4cc79e"}
   :choreSoft {:light "#e2f5ef" :dark "#122a22"}
   :kit.surface {:light "#ffffff" :dark "#1b2420"}
   :kit.text {:light "#2b2925" :dark "#f6f5f2"}
   :kit.body {:light "#73706a" :dark "#918d86"}
   :kit.dim {:light "#73706a" :dark "#918d86"}
   :kit.border {:light "#dde4de" :dark "#2b3833"}
   :kit.primary {:light "#0d7f63" :dark "#3ecf9e"}
   :kit.secondary {:light "#ffffff" :dark "#1b2420"}
   :kit.onSecondary {:light "#2b2925" :dark "#f6f5f2"}
   :kit.focus {:light "#0d7f63" :dark "#3ecf9e"}
   :kit.selection {:light "#def5ec" :dark "#13281f"}
   :kit.bar {:light "#ffffff" :dark "#101614"}
   :kit.barHover {:light "#e9eee9" :dark "#1e2824"}})

;;; Styles

(defstyle displayPill
  (label :font small
    :height 24
    :fitText true
    :padding 8
    :radius @radius.chip
    :borderWidth 1
    :cursor hand
    :focusable true
    :color transparent
    :hover {:color @color.chipHover}
    :focused {:color @color.chipHover}))

(defstyle caption (label :font small :textColor @color.muted :fitText true))

(defstyle badge (label :font small :align center))

(defstyle cardChip
  (label :font small
    :fitText true
    :padding 6
    :height 22
    :radius @radius.chip
    :borderWidth 1
    :borderColor @color.border
    :color transparent
    :textColor @color.muted))

(defstyle chip
  {:style badge
   :width 84
   :height 32
   :radius @radius.chip
   :color @color.surface
   :hover {:color @color.chipHover}
   :textColor @color.text
   :cursor hand})

(defstyle chipOn {:style chip :color @color.chipOn :hover {:color @color.chipOn} :textColor @color.chipOnText})

(defstyle toolButton
  (label :font small
    :height 28
    :radius @radius.chip
    :padding 10
    :fitText true
    :borderWidth 1
    :borderColor @color.border
    :color @color.surface
    :hover {:color @color.chipHover}
    :textColor @color.text
    :cursor hand
    :focusable true
    :role button))

(defstyle toolButtonOn
  {:style toolButton
   :color @color.chipOn
   :borderColor @color.chipOn
   :hover {:color @color.chipOn}
   :textColor @color.chipOnText})

(defstyle menuRow
  (label :font small
    :height 30
    :padding 10
    :radius @radius.chip
    :anchors {:left 0 :right 0}
    :textColor @color.text
    :color transparent
    :hover {:color @color.chipHover}
    :focused {:color @color.chipHover}
    :cursor hand
    :focusable true
    :align {:horizontal start :vertical center}))

(defstyle topButton
  (label :font small
    :height 32
    :radius @radius.chip
    :padding 10
    :fitText true
    :color transparent
    :hover {:color @color.topLine}
    :textColor @color.topText
    :cursor hand
    :focusable true
    :role button))

(defstyle detailButton
  (label :font small
    :height 30
    :radius @radius.chip
    :padding 12
    :fitText true
    :borderWidth 1
    :borderColor @color.border
    :color @color.surface
    :hover {:color @color.chipHover}
    :textColor @color.text
    :cursor hand))

;;; Components

(defcomponent layoutChoice "One layout in the Display menu: its icon over its name, outlined while it is the one shown"
  {:icon kanban :text "" :on false}
  `(node#panel-text :width 58
     :height 52
     :radius @radius.chip
     :borderWidth 1
     :borderColor ~(if on "@color.accent" "@color.border")
     :color ~(if on "@color.chipOn" "transparent")
     :hover {:color ~(if on "@color.chipOn" "@color.chipHover")}
     :cursor hand
     :focusable true
     :role radio
     :selected ~on
     :label ~text
     :tooltip ~text
     (label#layout-choice-icon :icon ~icon
       :text ""
       :fitText false
       :anchors {:left 0 :right 0 :top 7}
       :height 20
       :align center
       :textColor ~(if on "@color.chipOnText" "@color.muted"))
     (label#layout-choice-text :font small
       :text ~text
       :fitText false
       :anchors {:left 0 :right 0 :bottom 6}
       :height 16
       :align center
       :textColor ~(if on "@color.chipOnText" "@color.text"))))
