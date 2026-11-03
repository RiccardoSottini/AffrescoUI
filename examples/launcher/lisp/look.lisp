;;; look.lisp --- the Launcher's colours, light and dark, and the named styles its nodes share

;;; Colours

(deftokens tokens "The palette, light and dark, and the colours of the kit and the intelligence add-on, from it."
  {:c.bg {:light "#f5f4fa" :dark "#101017"}
   :c.side {:light "#ffffff" :dark "#16161e"}
   :c.surface {:light "#ffffff" :dark "#1c1c26"}
   :c.raised {:light "#f0eff7" :dark "#252532"}
   :c.hover {:light "#e9e7f5" :dark "#2c2c3b"}
   :c.border {:light "#e3e1ee" :dark "#2b2b39"}
   :c.text {:light "#1b1a28" :dark "#ececf4"}
   :c.muted {:light "#5d5b72" :dark "#a4a2b9"}
   :c.dim {:light "#8d8ba2" :dark "#6e6c84"}
   :c.accent {:light "#5b4fe0" :dark "#8b80ff"}
   :c.accentHover {:light "#4a3fcc" :dark "#a097ff"}
   :c.accentSoft {:light "#ecebff" :dark "#28244d"}
   :c.onAccent "#ffffff"
   :c.danger {:light "#cf3446" :dark "#ff6b7a"}
   :c.dangerSoft {:light "#fdecee" :dark "#3a1d24"}
   :c.success {:light "#1f9d62" :dark "#4ad08f"}
   :c.shadow {:light "#2a1f7a1f" :dark "#00000066"}
   :kit.background @c.bg
   :kit.surface @c.surface
   :kit.bar @c.raised
   :kit.barHover @c.hover
   :kit.field @c.surface
   :kit.text @c.text
   :kit.dim @c.dim
   :kit.body @c.muted
   :kit.selection @c.accentSoft
   :kit.border @c.border
   :kit.primary @c.accent
   :kit.onPrimary "#ffffff"
   :kit.secondary @c.raised
   :kit.onSecondary @c.text
   :kit.accent @c.accent
   :kit.focus @c.accent
   :intelligence.accent @c.accent
   :intelligence.accentHover @c.accentHover
   :intelligence.onAccent @c.onAccent
   :intelligence.surface @c.surface
   :intelligence.raised @c.raised
   :intelligence.selected @c.accentSoft
   :intelligence.border @c.border
   :intelligence.text @c.text
   :intelligence.dim @c.muted
   :intelligence.success @c.success
   :intelligence.danger @c.danger
   :kit.radius 8
   :kit.cardRadius 14})

;;; Styles

(defstyle pageTitle "A page's title."
  (label :font title :textColor @c.text :anchors {:left 36 :top 30} :fitText true))

(defstyle pageSub "The line under a page's title."
  (label :textColor @c.muted :anchors {:left 36 :right 36 :top 72} :height 20))

(defstyle sectionTitle "A section's title in a form or a card."
  (label :font heading :textColor @c.text :height 24))

(defstyle hint "A small, muted hint."
  (label :font small :textColor @c.muted :height 18))

(defstyle primary "The main button: the accent, white text."
  (label :align center
    :radius 10
    :color @c.accent
    :textColor @c.onAccent
    :font strong
    :cursor hand
    :focusable true
    :role button
    :hover {:color @c.accentHover}
    :transition {:color 120}))

(defstyle secondary "A button beside the main one."
  (label :align center
    :radius 10
    :color @c.raised
    :textColor @c.text
    :cursor hand
    :focusable true
    :role button
    :hover {:color @c.hover}
    :transition {:color 120}))

(defstyle iconButton "A button that is an icon."
  (label :align center
    :radius 8
    :color transparent
    :textColor @c.muted
    :font bigIcons
    :width 34
    :height 34
    :cursor hand
    :focusable true
    :role button
    :hover {:color @c.hover :textColor @c.text}
    :focused {:color @c.hover :textColor @c.text}))

(defstyle card "A card: a surface with a border and a soft shadow."
  {:radius 16 :color @c.surface :borderWidth 1 :borderColor @c.border :shadow {:y 2 :blur 10 :color @c.shadow}})

(defstyle field "A text field."
  (textarea :radius 10
    :color @c.surface
    :borderWidth 1
    :borderColor @c.border
    :padding 10
    :textColor @c.text
    :placeholderColor @c.dim
    :focused {:borderColor @c.accent}))
