;;; look.lisp --- Sign in's colours and its named styles
;;;
;;; The tokens are the palette, light and dark, and the assistant add-on's colours taken from it; a style is a node's
;;; look that many nodes name (:style primary).

;;; Colours

(deftokens tokens "The palette, light and dark: the page, the card, the fields, the accent and the avatars."
  {:radius.card 16
   :radius.field 10
   :color.background {:light "#eef2ff" :dark "#0b1020"}
   :color.glow {:light "#818cf855" :dark "#6366f130"}
   :color.glow2 {:light "#f0abfc44" :dark "#a855f724"}
   :color.surface {:light "#ffffff" :dark "#141b2e"}
   :color.text {:light "#0f172a" :dark "#e2e8f0"}
   :color.muted {:light "#64748b" :dark "#94a3b8"}
   :color.border {:light "#e2e8f0" :dark "#26304a"}
   :color.input {:light "#f8fafc" :dark "#0f1527"}
   :color.inputFocused {:light "#ffffff" :dark "#111a33"}
   :color.hover {:light "#f1f5f9" :dark "#1c2540"}
   :color.accent {:light "#6366f1" :dark "#818cf8"}
   :color.accentHover {:light "#7477f3" :dark "#939cfa"}
   :color.accentPressed {:light "#4f46e5" :dark "#6366f1"}
   :color.accentText {:light "#ffffff" :dark "#0b1020"}
   :color.accentSoft {:light "#e0e7ff" :dark "#1e2550"}
   :color.error {:light "#dc2626" :dark "#f87171"}
   :color.errorSoft {:light "#fef2f2" :dark "#2a1620"}
   :color.notice {:light "#fffbeb" :dark "#2a2414"}
   :color.noticeBorder {:light "#fcd34d" :dark "#a16207"}
   :color.warning {:light "#b45309" :dark "#fbbf24"}
   :color.weak {:light "#ef4444" :dark "#f87171"}
   :color.fair {:light "#f59e0b" :dark "#fbbf24"}
   :color.good {:light "#6366f1" :dark "#818cf8"}
   :color.strong {:light "#16a34a" :dark "#4ade80"}
   :color.shadow {:light "#0f172a1f" :dark "#00000066"}
   :color.backdrop {:light "#0f172a66" :dark "#000000a0"}
   :color.toast {:light "#0f172a" :dark "#e2e8f0"}
   :color.toastText {:light "#ffffff" :dark "#0f172a"}
   :color.qrBack {:light "#ffffff" :dark "#ffffff"}
   :color.qrInk {:light "#111111" :dark "#111111"}
   :color.avatar1 {:light "#0ea5e9" :dark "#38bdf8"}
   :color.avatar2 {:light "#10b981" :dark "#34d399"}
   :color.avatar3 {:light "#f59e0b" :dark "#fbbf24"}
   :color.avatar4 {:light "#ef4444" :dark "#f87171"}
   :color.avatar5 {:light "#8b5cf6" :dark "#a78bfa"}
   :color.avatar6 {:light "#ec4899" :dark "#f472b6"}
   :color.avatarText {:light "#ffffff" :dark "#0b1020"}
   :color.selected {:light "#eef2ff" :dark "#1a2244"}
   :color.toastAction {:light "#a5b4fc" :dark "#4f46e5"}
   :assistant.accent @color.accent
   :assistant.accentHover @color.accentHover
   :assistant.onAccent @color.accentText
   :assistant.surface @color.surface
   :assistant.raised {:light "#f8fafc" :dark "#1b2338"}
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.muted
   :assistant.success {:light "#16a34a" :dark "#4ade80"}
   :assistant.danger @color.error
   :assistant.warning {:light "#fef3c7" :dark "#3b2e0a"}})

(defpart colors "The accent's plain colours, for what takes no token."
  :colors {:accent "#6366f1" :accentHover "#7477f3" :fieldFocused "#ffffff"})

;;; Text

(defstyle caption "A line of muted text under a title."
  (label :height 20 :textColor @color.muted :anchors {:left 0 :right 10}))

(defstyle fieldLabel "A field's name, over it."
  (label :font small :height 18 :color transparent :textColor @color.text :anchors {:left 0 :right 10}))

(defstyle title "A screen's title, large."
  (label :font title :height 34 :textColor @color.text :anchors {:left 0 :right 10}))

(defstyle hint "A small muted line: a hint, a time, a count."
  (label :font small :height 18 :textColor @color.muted :anchors {:left 0 :right 10}))

(defstyle heading "A section's heading on the account page."
  (label :font heading :height 28 :textColor @color.text :role heading :anchors {:left 0 :right 10}))

;;; Fields

(defstyle fieldBox "The box around a checked field."
  {:anchors {:left 0 :right 0 :top 0}
   :height 44
   :radius @radius.field
   :color @color.input
   :borderWidth 1
   :borderColor @color.border
   :hover {:borderColor @color.muted}})

(defstyle fieldBoxOn "The checked field's box while it has the focus: the accent, and a glow."
  {:style fieldBox
   :color @color.inputFocused
   :borderWidth 2
   :borderColor @color.accent
   :hover {:borderColor @color.accent}
   :shadow {:y 0 :blur 8 :color @color.glow}})

(defstyle fieldBoxError "The checked field's box with an error."
  {:style fieldBox :borderColor @color.error :hover {:borderColor @color.error}})

(defstyle input "The text inside a checked field's box."
  (textarea :padding 8 :color transparent :textColor @color.text :placeholderColor @color.muted))

(defstyle field "A plain text field, with its own border: the dialogs' fields."
  (textarea :padding 10
    :radius @radius.field
    :borderWidth 1
    :borderColor @color.border
    :color @color.input
    :focused {:borderColor @color.accent}
    :textColor @color.text
    :placeholderColor @color.muted))

(defstyle code "Six cells for a one-time code."
  (codeInput :height 52
    :anchors {:left 0 :right 10}
    :cellColor @color.input
    :cellBorderColor @color.border
    :cellRadius 8
    :cellGap 8
    :textColor @color.text))

;;; Buttons and links

(defstyle primary "The main button of a form: the accent, full width."
  (label :height 46
    :anchors {:left 0 :right 10}
    :align center
    :radius @radius.field
    :color @color.accent
    :textColor @color.accentText
    :cursor hand
    :focusable true
    :role button
    :shadow {:y 4 :blur 12 :color @color.glow}
    :hover {:color @color.accentHover}
    :pressed {:color @color.accentPressed}
    :transition {:color 120}))

(defstyle secondary "Another way to go on: outlined, full width."
  {:style primary
   :color transparent
   :textColor @color.text
   :borderWidth 1
   :borderColor @color.border
   :shadow {:color "#00000000"}
   :hover {:color @color.hover}
   :pressed {:color @color.border}})

(defstyle link "A link, centred, full width."
  (label :height 30
    :anchors {:left 0 :right 10}
    :align center
    :textColor @color.accent
    :cursor hand
    :focusable true
    :role link
    :hover {:textColor @color.accentPressed}))

(defstyle smallLink "A small link that fits its text."
  (label :font small
    :textColor @color.accent
    :cursor hand
    :fitText true
    :focusable true
    :role link
    :hover {:textColor @color.accentPressed}))

(defstyle iconButton "A square button that shows only an icon."
  (label :text ""
    :width 32
    :height 32
    :radius 8
    :color transparent
    :textColor @color.muted
    :align center
    :cursor hand
    :focusable true
    :role button
    :hover {:color @color.hover :textColor @color.text}))

(defstyle chip "A language of the top bar."
  (label :font small
    :width 36
    :height 32
    :radius 8
    :align center
    :color transparent
    :textColor @color.muted
    :cursor hand
    :focusable true
    :role radio
    :hover {:color @color.hover :textColor @color.text}))

(defstyle chipOn "The language shown." {:style chip :color @color.accentSoft :textColor @color.accent})

(defstyle smallButton "A small outlined button that fits its text."
  {:style secondary :height 34 :anchors {} :font small :padding 12 :fitText true :shadow {:color "#00000000"}})

(defstyle danger "A button that deletes: the error's colour."
  {:style primary
   :color @color.error
   :textColor @color.accentText
   :shadow {:color "#00000000"}
   :hover {:color @color.weak}
   :pressed {:color @color.error}})

;;; Cards, forms and checks

(defstyle card "The card in the middle of a screen, scrolling when it is taller than the window."
  (column :radius @radius.card
    :color @color.surface
    :borderWidth 1
    :borderColor @color.border
    :shadow {:y 12 :blur 32 :color @color.shadow}
    :fitContent height
    :scroll vertical))

(defstyle form "A card's column of fields and buttons."
  (column :fitContent height :gap 8 :anchors {:left 36 :right 26}))

(defstyle logo "The feather on the accent, over the sign-in form."
  (label :font logo
    :icon feather
    :text ""
    :width 48
    :height 48
    :radius 14
    :align center
    :color @color.accent
    :textColor @color.accentText
    :shadow {:y 6 :blur 16 :color @color.glow}))

(defstyle spacer "Some room between a form's parts." {:height 6 :anchors {:left 0 :right 10} :color transparent})

(defstyle checkRow "A row with a check box and its words."
  (row :height 22 :gap 8 :alignItems center :anchors {:left 0 :right 10}))

(defstyle checkOff "A check box, empty."
  (label :font iconsSmall
    :text ""
    :width 18
    :height 18
    :radius 5
    :align center
    :color @color.input
    :borderWidth 1
    :borderColor @color.border
    :cursor hand
    :focusable true
    :role checkbox
    :hover {:borderColor @color.accent}))

(defstyle checkOn "A check box, ticked."
  {:style checkOff :icon check :color @color.accent :borderColor @color.accent :textColor @color.accentText})

(defstyle alert "Why the server refused, over the form, read out at once."
  (label :font small
    :icon alert-circle
    :wrap true
    :padding 8
    :radius 8
    :role alert
    :live assertive
    :color @color.errorSoft
    :textColor @color.error
    :anchors {:left 0 :right 10}))

(defstyle inlineError "A field's error, under it, read out at once."
  (label :font small
    :icon alert-circle
    :height 18
    :role alert
    :live assertive
    :textColor @color.error
    :anchors {:left 0 :right 10}))

(defstyle screen "A screen: its card centred." (column :fill 0 :justify center :alignItems center :color transparent))

(defstyle accountCard "The account page's card, as tall as the window allows."
  {:style card :gap 14 :scroll none :fitContent false})

(defstyle divider "A thin line across." {:height 1 :color @color.border :anchors {:left 0 :right 0}})

;;; The account page

(defstyle navItem "A section of the account page in the tabs down the side."
  (label :font default
    :height 38
    :radius 8
    :padding 10
    :color transparent
    :textColor @color.muted
    :cursor hand
    :focusable true
    :role tab
    :anchors {:left 0 :right 0}
    :hover {:color @color.hover :textColor @color.text}))

(defstyle navItemOn "The section shown, in the tabs down the side."
  {:style navItem
   :color @color.selected
   :textColor @color.accent
   :hover {:color @color.selected :textColor @color.accent}})

(defstyle itemRow "A row of a list on the account page."
  (row :height 60
    :gap 12
    :alignItems center
    :radius @radius.field
    :color @color.input
    :borderWidth 1
    :borderColor @color.border
    :anchors {:left 0 :right 10}))

(defstyle itemIcon "A list row's icon, on the soft accent."
  (label :font icons
    :text ""
    :width 36
    :height 36
    :radius 8
    :align center
    :color @color.accentSoft
    :textColor @color.accent))

(defstyle itemText "A list row's title and hint, one over the other." (column :grow 1 :height 38 :gap 2))

(defstyle itemTitle "A list row's title." (label :height 20 :textColor @color.text :anchors {:left 0 :right 0}))

(defstyle itemHint "A list row's hint, under its title."
  (label :font small :height 16 :textColor @color.muted :anchors {:left 0 :right 0}))

(defstyle badge "A small label on the soft accent: primary, this device."
  (label :font small :height 22 :radius 999 :padding 8 :fitText true :color @color.accentSoft :textColor @color.accent))

(defstyle badgeWarn "A badge that warns: unverified." {:style badge :color @color.notice :textColor @color.warning})

(defstyle navItemChip "A section of the account page as a chip, on a narrow window."
  {:style navItem :fitText true :anchors {}})

(defstyle navItemOnChip "The section shown, as a chip." {:style navItemOn :fitText true :anchors {}})
