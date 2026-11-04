;;; index.screen.lisp --- The AffrescoUI Launcher: the home of the example apps and of the person's own
;;;
;;; Apps (your apps and the examples as cards, each tagged Example or Yours; a search; a click or Enter opens one), My
;;; apps (open, duplicate, show in the folder, delete after a confirm), Create (a name, a starting point, a
;;; description for the assistant, features from other apps) and Settings. It is a screen like any other, with no C++
;;; of its own: affrescoui_launcher opens it when started without a file and grants it the folders it reads and writes
;;; (the services examples and apps) and the programs that open the apps (the service launch). Where nothing is
;;; granted (the rendering check), the services are refused and it shows the examples from apps.lisp and an empty My
;;; apps.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "apps.lisp"
         "state.lisp"
         "look.lisp"
         "keys.lisp"
         "sidebar.lisp"
         "catalog.lisp"
         "mine.lisp"
         "create.lisp"
         "settings.lisp"
         "../../shared/addons/intelligence/settings.lisp"
         "launcher.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "The Launcher's one window: the sidebar, and the page shown beside it."
  :window {:title "AffrescoUI"
           :icon "../assets/affrescoui-icon.png"
           :width 1200
           :height 780
           :minWidth 800
           :minHeight 600
           :focusRing {:color @c.accent :width 2 :offset 2}}
  :strings {:default en
            :locale ~(if (= state.locale "system") "" state.locale)
            :keep ["AffrescoUI" "English" "Español" "Français" "JSON" "Lisp"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons symbols]}
          :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13}
          :strong {:file "../../shared/fonts/Roboto-Regular.ttf" :size 16 :bold true :fallback [icons]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 19 :bold true :fallback [icons]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 30 :bold true}
          :brand {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :bold true}
          :cover {:file "../../shared/fonts/Roboto-Regular.ttf" :size 64 :bold true}
          :initial {:file "../../shared/fonts/Roboto-Regular.ttf" :size 20 :bold true}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 15}
          :bigIcons {:file "../../shared/fonts/Lucide.ttf" :size 22}
          :heroIcons {:file "../../shared/fonts/Lucide.ttf" :size 44}}
  :persist [state.theme state.locale state.notation]
  :permissions {:allow [studio]
                :ask [agent]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~(if (= state.theme "system") window.colorScheme state.theme)
  (node#launcher :color @c.bg
    ~(sidebar)
    (node#main :anchors {:left "#sidebar.right" :right 0 :top 0 :bottom 0} :color transparent :dir auto
      ~(apps-page)
      ~(mine-page)
      ~(create-page)
      ~(settings-page))))
