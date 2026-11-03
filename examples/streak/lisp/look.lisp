;;; look.lisp --- Streak's colours, light, dark and pure black, and its styles

;;; Colours and styles
(deftokens tokens "Tokens."
  {:c.bg {:light "#f2efec" :dark "#1f1d1c" :black "#000000"}
   :c.card {:light "#ffffff" :dark "#2d2a28" :black "#0f0f0f"}
   :c.bar {:light "#ffffff" :dark "#282523" :black "#000000"}
   :c.actionBar {:light "#5b4a42" :dark "#463a35" :black "#1c1c1c"}
   :c.text {:light "#212121" :dark "#e0e0e0" :black "#e0e0e0"}
   :c.muted {:light "#757575" :dark "#9e9e9e" :black "#9e9e9e"}
   :c.faint {:light "#cfcfcf" :dark "#5a5a5a" :black "#454545"}
   :c.track {:light "#ebe7e3" :dark "#424242" :black "#262626"}
   :c.divider {:light "#e3dfdb" :dark "#3d3d3d" :black "#222222"}
   :c.hover {:light "#0000000d" :dark "#ffffff12" :black "#ffffff12"}
   :c.selected {:light "#fdebe1" :dark "#4a3a33" :black "#2a1810"}
   :c.primary {:light "#c2410c" :dark "#ff9e6b" :black "#ff9e6b"}
   :c.onPrimary {:light "#ffffff" :dark "#3a1404" :black "#3a1404"}
   :c.danger {:light "#d32f2f" :dark "#ef9a9a" :black "#ef9a9a"}
   :c.backdrop {:light "#00000066" :dark "#000000a0" :black "#000000c0"}
   :c.raised {:light "#ffffff" :dark "#424242" :black "#1c1c1c"}
   :c.knob {:light "#fafafa" :dark "#bdbdbd" :black "#bdbdbd"}
   :c.switchTrack {:light "#c2410c80" :dark "#ff9e6b80" :black "#ff9e6b80"}
   :c.snack {:light "#323232" :dark "#e0e0e0" :black "#e0e0e0"}
   :c.snackText {:light "#ffffff" :dark "#212121" :black "#212121"}
   :c.snackAction {:light "#ffb38a" :dark "#b23a0a" :black "#b23a0a"}
   :assistant.accent @c.primary
   :assistant.accentHover @c.primary
   :assistant.onAccent @c.onPrimary
   :assistant.surface @c.card
   :assistant.raised @c.bg
   :assistant.border @c.divider
   :assistant.text @c.text
   :assistant.dim @c.muted
   :assistant.dot @c.faint
   :assistant.danger @c.danger
   :kit.background @c.bg
   :kit.surface @c.raised
   :kit.bar @c.bar
   :kit.barHover @c.hover
   :kit.field @c.card
   :kit.text @c.text
   :kit.dim @c.muted
   :kit.body @c.muted
   :kit.selection @c.selected
   :kit.border @c.divider
   :kit.primary @c.primary
   :kit.onPrimary @c.onPrimary
   :kit.secondary @c.track
   :kit.onSecondary @c.text
   :kit.accent @c.primary
   :kit.focus @c.primary
   :kit.backdrop @c.backdrop
   :kit.radius 4
   :kit.cardRadius 10})

(defstyle iconButton "A round icon button."
  (label :font icons
    :width 40
    :height 40
    :radius 20
    :align center
    :cursor hand
    :focusable true
    :role button
    :textColor @c.muted
    :hover {:color "#8080802e"}
    :focused {:color "#80808040"}))

(defstyle card "A card of the detail." {:color @c.card :radius 14 :shadow {:y 1 :blur 3 :color "#0000002e"}})

(defstyle cardTitle "A card's title." (label :font heading :anchors {:left 16 :top 14} :fitText true))

(defstyle small "Small text." (label :font small :textColor @c.muted))

(defstyle caption "A caption." {:style small :fitText true})

(defstyle tiny "The smallest text." (label :font tiny :textColor @c.muted))

(defstyle axis "A chart's axis label." {:style tiny :anchors {:left 0 :right 0 :top 2} :height 12 :align center})

(defstyle axis2 "A chart's second axis label." {:style axis :anchors {:left 0 :right 0 :top 16}})

(defstyle body "Body text." (label :textColor @c.text :fitText true))

(defstyle field "A text field."
  (textarea :height 40
    :padding 10
    :color @c.card
    :radius 4
    :borderWidth 1
    :borderColor @c.divider
    :textColor @c.text
    :placeholderColor @c.muted
    :focused {:borderColor ~computed.accent :borderWidth 2}))

(defstyle textButton "A text button."
  (label :height 36
    :padding 10
    :radius 4
    :textColor @c.primary
    :fitText true
    :focusable true
    :role button
    :cursor hand
    :hover {:color @c.hover}
    :focused {:color @c.hover}))

(defstyle chip "A choice, as a chip."
  (label :height 32
    :padding 10
    :radius 16
    :fitText true
    :font small
    :focusable true
    :role radio
    :cursor hand
    :borderWidth 1
    :borderColor @c.divider
    :textColor @c.text
    :hover {:color @c.hover}))

(defstyle chipOn "The chip chosen."
  (label :height 32
    :padding 10
    :radius 16
    :fitText true
    :font small
    :focusable true
    :role radio
    :cursor hand
    :borderWidth 1
    :borderColor ~computed.accent
    :color ~(str computed.accent "1f")
    :textColor ~computed.accent))

(defstyle sectionTitle "A section's title."
  (label :font small :textColor @c.primary :height 40 :fitText true :padding 16 :align {:horizontal start :vertical end}))

