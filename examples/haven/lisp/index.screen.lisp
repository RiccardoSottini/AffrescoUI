;;; index.screen.lisp --- Haven: a smart-home dashboard
;;;
;;; Rooms in a tree by floor, scenes, lights (switch, dim, colour), climate, blinds, device tiles whose calls take a
;;; while (progress, then a toast), a floor plan on a canvas with media, automations as a node graph with schedules on
;;; a timeline, an energy page by day and by device, security modes and notifications, device state from a simulated
;;; service, and a settings page from the settings schema; light and dark themes, English, Spanish and French. Cmd+1
;;; to Cmd+4 and Cmd+, change the page, Cmd+Shift+P runs a command, Cmd+T switches light and dark, Cmd+L the language,
;;; Cmd+Shift+A the security mode, Cmd+Shift+N the notifications, Up and Down the target temperature, Alt and Up or
;;; Down the room.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "../../shared/components/charts.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "state.lisp"
         "computed.lisp"
         "look.lisp"
         "keys.lisp"
         "shell.lisp"
         "search.lisp"
         "home.lisp"
         "edit.lisp"
         "moreinfo.lisp"
         "plan.lisp"
         "security.lisp"
         "automations.lisp"
         "energy.lisp"
         "areas.lisp"
         "history.lisp"
         "settings.lisp"
         "haven.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Haven's one window: the sidebar, the page shown, the status bar, and what opens over them."
  :window {:title "Haven: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :strings {:default en
            :locale ~settings.ui.language
            :keep ["Haven" "Haven: an AffrescoUI template" "Alex" "A" "English" "Español" "Français" "Apache-2.0"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons symbols]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [iconsSmall]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :fallback [icons symbols]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [iconsLarge]}
          :figure {:file "../../shared/fonts/Roboto-Regular.ttf" :size 48}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 26}
          :iconsMedium {:file "../../shared/fonts/Lucide.ttf" :size 22}
          :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 14}}
  :persist [state.theme state.settings state.security]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~computed.themeNow
  (node#panel-root :color @background
    (node#app :dir auto :anchors {:left 0 :right 0 :top 0 :bottom 0} :color transparent
      ~(sidebar)
      ~(home-page)
      ~(page "areasPageView" "areas" "areasPage" "auto")
      ~(page "logbookPageView" "logbook" "logbookPage" "auto")
      ~(page "historyPageView" "history" "historyPage" "auto")
      ~(page "energyPage" "energy" "energyPage" "auto")
      ~(page "planPage" "plan" "planPage" "ltr")
      ~(page "automationsPage" "automations" "automationsPage" "ltr")
      ~(page "settingsPageView" "settings" "settingsPageMain" "auto")
      ~(status-bar)
      ~@(dialogs)
      (node#notificationsPanel :use notificationsPanel)
      (node#simulator :use simulator)
      ~@(notes)
      ~@(search))))
