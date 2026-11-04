;;; chrome.lisp --- what is around every screen: the glows behind, the status line, the top bar, the footer and the
;;; toast
;;;
;;; The screens themselves are drawn in the outlet between the top bar and the footer (index.screen.lisp).

;;; Behind and around the screens

(defview glows "The two soft glows behind the card, top left and bottom right: two nodes, spliced in with ~@." []
  [`(node#glow :width 520
      :height 520
      :radius 999
      :x -160
      :y -200
      :color @color.glow
      :shadow {:blur 120 :color @color.glow})
   `(node#glow-2 :width 460
      :height 460
      :radius 999
      :anchors {:right -140 :bottom -180}
      :color @color.glow2
      :shadow {:blur 120 :color @color.glow2})])

(defview announcer "The status line, top left: what the fake server is doing, read out politely." []
  `(label#announcer :dir auto
     :font small
     :role status
     :live polite
     :label ~computed.busyText
     :icon ~(if (= state.busy "") "" "loader")
     :text ~computed.busyText
     :textColor @color.muted
     :anchors {:left 20 :top 22}
     :height 24
     :fitText true))

(defview footer "The line under the screens, on windows wide enough for it." []
  `(label#footer :when ~(>= window.width 640)
     :font small
     :textColor @color.muted
     :align center
     :anchors {:left 0 :right 0 :bottom 18}
     :height 18
     :text ~(t "footer")))

;;; The top bar

(defview top-bar "The top bar, top right: the languages, Slow network, the shortcuts and the theme." []
  `(row#topBar :dir auto :gap 6 :width 226 :height 32 :alignItems center :anchors {:right 16 :top 14}
     (row#languages :role radiogroup :label ~(t "language") :arrows horizontal :gap 2 :height 32 :width 112
       ~(repeat [language ["en" "it" "ar"]]
          (language-chip language)))
     ~(slow-toggle)
     (node#shortcutsButton :style iconButton
       :font iconsSmall
       :icon keyboard
       :label ~(t "shortcuts")
       :tooltip ~(t "shortcutsTooltip" {:key computed.shortcutsKey})
       :isOpen ~state.shortcutsOpen
       :onClick openShortcuts)
     (node#themeToggle :style iconButton
       :icon ~(if (= state.theme "light") "moon" "sun")
       :label ~(t "themeTooltip" {:dark (= state.theme "dark") :key computed.themeKey})
       :tooltip ~(t "themeTooltip" {:dark (= state.theme "dark") :key computed.themeKey})
       :onClick toggleTheme)))

(defview language-chip "A language of the top bar, by its code, lit while it is the one shown." [language]
  `(node#lang :style ~(if (= state.locale language) "chipOn" "chip")
     :text ~(upper-case language)
     :label ~(t "langName" {:code language})
     :tooltip ~(t "langName" {:code language})
     :selected ~(= state.locale language)
     :onClick ~(handler setLocale language)))

(defview slow-toggle "The Slow network switch: a snail while the fake server takes its time." []
  `(node#slowToggle :style iconButton
     :font iconsSmall
     :icon ~(if state.slow "snail" "wifi")
     :textColor ~(if state.slow "@color.accent" "@color.muted")
     :role switch
     :label ~(t "slow")
     :tooltip ~(t "slowTooltip" {:on state.slow})
     :selected ~state.slow
     :onClick toggleSlow))

;;; The toast

(defview toast "The notice at the bottom, centred, over everything." []
  `(row#toast :dir auto
     :overlay {:placement window :open ~state.toast.open}
     :anchors {:left 0 :right 0 :bottom 58}
     :height 40
     :justify center
     :color transparent
     :role status
     :live polite
     ~(toast-pill)))

(defview toast-pill "The toast's pill: a tick, the message, Stay signed in when the session is ending, and a close."
  []
  `(row#toastPill :fitContent width
     :height 40
     :gap 10
     :alignItems center
     :radius 20
     :color @color.toast
     :shadow {:y 6 :blur 18 :color @color.shadow}
     (node#toast-start :width 6 :color transparent)
     (label#toastIcon :font iconsSmall :icon check :textColor @color.toastText :fitText true)
     (label#toastMessage :font small :text ~state.toast.message :textColor @color.toastText :fitText true)
     (label#toastStay :when ~(= state.toast.action "stay")
       :font small
       :text ~(t "stay")
       :textColor @color.toastAction
       :fitText true
       :cursor hand
       :focusable true
       :role button
       :onClick staySignedIn)
     (label#toastClose :font iconsSmall
       :icon x
       :text ""
       :textColor @color.toastText
       :fitText true
       :cursor hand
       :focusable true
       :role button
       :label ~(t "close")
       :onClick hideToast)
     (node#toast-end :width 6 :color transparent)))
