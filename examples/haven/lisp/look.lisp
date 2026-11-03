;;; look.lisp --- Haven's colours, light and dark, and the named styles its nodes share
;;;
;;; A colour is named once with its light and dark values (@card, @accent...); a style is a node's look that many
;;; nodes name (:style card).

;;; Colours

(defpart colours "The palette, light and dark: the surfaces, the text, the accent, and the climate's and energy's own."
  :colors {:background {:dark "#131318" :light "#f3f2ee"}
           :sidebar {:dark "#1a1a20" :light "#e9e7e1"}
           :header {:dark "#131318" :light "#f3f2ee"}
           :onHeader {:dark "#e6e5ee" :light "#26241f"}
           :onHeaderDim {:dark "#9a99a8" :light "#6f6b62"}
           :headerHover {:dark "#ffffff1a" :light "#26241f14"}
           :card {:dark "#1d1d24" :light "#ffffff"}
           :cardHover {:dark "#26262e" :light "#f6f5f1"}
           :raised {:dark "#2a2a33" :light "#ecebe6"}
           :raisedHover {:dark "#34343e" :light "#e2e0d9"}
           :text {:dark "#e6e5ee" :light "#26241f"}
           :muted {:dark "#9a99a8" :light "#6f6b62"}
           :border {:dark "#2e2e38" :light "#dedbd3"}
           :divider {:dark "#2e2e38" :light "#dedbd3"}
           :primary {:dark "#9ea3ff" :light "#4a50b8"}
           :accent {:dark "#9ea3ff" :light "#4048b0"}
           :onAccent {:dark "#15172e" :light "#ffffff"}
           :accentSoft {:dark "#252846" :light "#e6e7f7"}
           :accentFill {:dark "#9ea3ff40" :light "#4a50b833"}
           :navSelected {:dark "#9ea3ff" :light "#4048b0"}
           :onNavSelected {:dark "#15172e" :light "#ffffff"}
           :warm {:dark "#f2b544" :light "#e39b1b"}
           :warmSoft {:dark "#3a2e14" :light "#fbf0d9"}
           :alert {:dark "#ef6074" :light "#c9374a"}
           :alertSoft {:dark "#3e1a20" :light "#f9e3e6"}
           :ok {:dark "#58c08f" :light "#2e8b63"}
           :okSoft {:dark "#173126" :light "#e0f1e8"}
           :off {:dark "#4a4945" :light "#c4c1b8"}
           :heat {:dark "#f07a52" :light "#d9573a"}
           :cool {:dark "#5fb3e8" :light "#2f7fc0"}
           :autoMode {:dark "#58c08f" :light "#2e8b63"}
           :heatSoft {:dark "#f07a5233" :light "#d9573a33"}
           :coolSoft {:dark "#5fb3e833" :light "#2f7fc033"}
           :autoModeSoft {:dark "#58c08f33" :light "#2e8b6333"}
           :editHeader {:dark "#262947" :light "#dfe0f5"}
           :editLine {:dark "#6c70a8" :light "#8c90c8"}
           :editVeil {:dark "#9ea3ff14" :light "#4048b00f"}
           :accentTrack {:dark "#9ea3ff80" :light "#4048b080"}
           :snack {:dark "#2a2833" :light "#2a2833"}
           :snackAction {:dark "#b8bcff" :light "#b8bcff"}
           :thumbOff {:dark "#c4c1b8" :light "#f3f2ee"}
           :selected {:dark "#252846" :light "#e6e7f7"}
           :field {:dark "#26262d" :light "#ecebe6"}
           :knob {:dark "#ffffff" :light "#ffffff"}
           :plan {:dark "#17171c" :light "#ffffff"}
           :cover {:dark "#4fb0a8" :light "#3f8f8a"}
           :coverSoft {:dark "#4fb0a833" :light "#3f8f8a33"}
           :grid {:dark "#8a6ad8" :light "#8a6ad8"}
           :gridOut {:dark "#d0679a" :light "#d0679a"}
           :solar {:dark "#e8b931" :light "#e8b931"}
           :media {:dark "#9ea3ff" :light "#4a50b8"}
           :kit.surface {:dark "#1d1d24" :light "#ffffff"}
           :kit.text {:dark "#e6e5ee" :light "#26241f"}
           :kit.dim {:dark "#9a99a8" :light "#6f6b62"}
           :kit.selection {:dark "#2a2a33" :light "#ecebe6"}
           :kit.border {:dark "#2e2e38" :light "#dedbd3"}
           :kit.primary {:dark "#9ea3ff" :light "#4048b0"}
           :kit.onPrimary {:dark "#15172e" :light "#ffffff"}
           :kit.secondary {:dark "#2a2a33" :light "#ecebe6"}
           :kit.onSecondary {:dark "#e6e5ee" :light "#26241f"}
           :kit.accent {:dark "#9ea3ff" :light "#4048b0"}
           :kit.field {:dark "#26262d" :light "#ecebe6"}
           :kit.focus {:dark "#9ea3ff" :light "#4048b0"}
           :kit.backdrop {:dark "#00000052" :light "#00000052"}})

(deftokens tokens "The cards' radius, and the colours the shared assistant add-on draws with."
  {:kit.cardRadius 16
   :assistant.accent @accent
   :assistant.accentHover @primary
   :assistant.onAccent @kit.onPrimary
   :assistant.surface @card
   :assistant.raised @raised
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted
   :assistant.dot @off
   :assistant.success @ok
   :assistant.danger @alert
   :assistant.warning @warmSoft})

;;; Styles

(defstyle card "A card: a rounded surface with a soft shadow."
  {:color @card :radius 18 :borderWidth 0 :borderColor @border :shadow {:y 1 :blur 6 :color "#0000001a"}})

(defstyle chip "A chip of the home page's summary row: an icon and a short text in an outlined pill."
  (label :font small
    :textColor @text
    :color @card
    :borderWidth 1
    :borderColor @border
    :radius 8
    :padding 12
    :anchors {:top 0 :bottom 0}
    :fitText true
    :align {:horizontal start :vertical center}))

(defstyle cardTitle "A card's title, at its top start."
  (label :font heading :textColor @text :anchors {:left 24 :top 20} :fitText true))

(defstyle caption "A small, muted line of text."
  (label :font small :textColor @muted :fitText true))

(defstyle button "A button: a label on a raised surface, a hand over it."
  (label :align center :color @raised :radius 8 :hover {:color @raisedHover} :textColor @text :cursor hand
    :focusable true))

(defstyle round "A round button with an icon."
  {:style button :font heading :width 44 :height 44 :radius 22})

(defstyle choice "One of several choices, not the chosen one."
  {:style button})

(defstyle choiceOn "The chosen one of several choices."
  {:style button :color @accentSoft :hover {:color @accentSoft} :textColor @accent})

(defstyle pageTitle "A page's title, at its top start."
  (label :font title :textColor @text :anchors {:left 32 :top 28} :fitText true))

(defstyle stat "A card of a row of figures, as wide as the others."
  {:style card :anchors {:top 0 :bottom 0} :grow 1})

(defstyle statFigure "The big figure of a stat card."
  (label :font title :textColor @text :anchors {:left 20 :top 36} :fitText true))
