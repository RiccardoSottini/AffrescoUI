;;; index.screen.lisp --- Modello, a 3D modeller written as a program the stock host runs
;;;
;;; Four areas share the window: the 3D viewport, the timeline (or the Shader Editor), the outliner and the properties;
;;; each can show any editor. G, R and S move, turn and scale the selection after the pointer (X, Y and Z lock an axis,
;;; digits type the amount, Enter confirms, Escape cancels); Shift A adds, X deletes, Shift D duplicates, H hides, M
;;; moves to a collection; the numpad turns the view, Z and the grave key open the pie menus; I keys, Space plays;
;;; Ctrl Z undoes, F3 searches the commands. Every change of the scene keeps an undo snapshot first.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "look.lisp"
         "state.lisp"
         "scene.lisp"
         "widgets.lisp"
         "menus.lisp"
         "keys.lisp"
         "bars.lisp"
         "viewport.lisp"
         "sidebar.lisp"
         "outliner.lisp"
         "properties.lisp"
         "timeline.lisp"
         "shader.lisp"
         "dialogs.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "modello.lisp"
         "transform.lisp"
         "collections.lisp"
         "navigation.lisp"
         "animation.lisp"
         "materials.lisp"
         "files.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Modello's one window: the top bar, the four areas and the status bar, and what opens over them."
  :window {:title "Modello" :width 1440 :height 900 :minWidth 1000 :minHeight 640}
  :strings {:default en :locale ~settings.appearance.language :keep ["Modello" "Modello 1.0"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 12}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :theme ~state.theme
  (node#root :color @gap
    ~(workspace-column)
    ~(playback-timer)
    ~(popup-menu)
    ~(pie)
    ~(about-dialog)
    ~(render-window)
    ~(command-search)))
