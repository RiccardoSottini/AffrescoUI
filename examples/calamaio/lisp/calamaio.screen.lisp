;;; calamaio.screen.lisp --- Calamaio: a keyboard-first text editor, written as a program
;;;
;;; A frame with a menu bar and a tool bar, windows that split (C-x 2, C-x 3), each with its mode line, and an echo
;;; area with a minibuffer (M-x, C-x b, C-x C-f). Its keys are in keys.lisp, its menus in menus.lisp, its looks in
;;; look.lisp, its variables in settings.lisp: changing how it works is a revision of its program, as an init file
;;; is. Lisp typed in a buffer runs in it (loop.lisp).

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "look.lisp"
         "settings.lisp"
         "state.lisp"
         "home.lisp"
         "languages.lisp"
         "window.lisp"
         "layout.lisp"
         "bars.lisp"
         "splash.lisp"
         "echo.lisp"
         "minibuffer.lisp"
         "motion.lisp"
         "editing.lisp"
         "killring.lisp"
         "search.lisp"
         "buffers.lisp"
         "files.lisp"
         "listings.lisp"
         "help.lisp"
         "options.lisp"
         "guides.lisp"
         "loop.lisp"
         "keys.lisp"
         "menus.lisp"
         "../../shared/addons/assistant/assistant.lisp")

;;; Starting

(defn start-up
  "At start: the assistant above the echo area, the welcome texts, and the start screen unless a variable says not." []
  (set "state.assistant.lift" 44)
  (set "state.echo" (t "echo.welcome"))
  (set "state.bufs.b1.text" (t "scratch.text"))
  (set "state.bufs.b2.text" (str (t "echo.welcome") "\n"))
  (when settings.inhibit-startup-screen (splash-close)))

(defpart start "What runs as Calamaio starts." :onLoad start-up)

;;; The frame, and what is in it, in the order it is drawn

(defscreen main "Calamaio's frame: the top bar, the windows in their groups, the completions and the echo area."
  :window {:title "Calamaio" :width 960 :height 720 :minWidth 640 :minHeight 420 :keyLabels letters :focusRing false}
  :strings {:default en :locale ~settings.appearance.language :keep ["Calamaio" "*Completions*"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons code]}
          :code {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 14 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 20}
          :logo {:file "../../shared/fonts/Lucide.ttf" :size 64}}
  :theme ~(get settings "custom-enabled-themes")
  :persist [state.settings]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/keys)" "agent(/menus)" "agent(/program)"]
                :deny ["*(#echoArea)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  (node#root :color @bar
    (column#column-root :dir ~state.dir :anchors {:left 6 :right 6 :top 0 :bottom 0}
      ~(top-bar)
      ~(windows-in-rows)
      ~(windows-in-columns)
      (node#completions-root :when ~(and state.mb.open (some? state.mb.comp)) :use completions :alignSelf stretch)
      (node#echoArea :use echoArea :alignSelf stretch))))
