;;; index.screen.lisp --- Modello, a 3D modeller

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
         "../../shared/addons/assistant/assistant.lisp"
         "modello.lisp"
         "transform.lisp"
         "collections.lisp"
         "navigation.lisp"
         "animation.lisp"
         "materials.lisp"
         "files.lisp")

(defscreen main
  :window {:title "Modello" :width 1440 :height 900 :minWidth 1000 :minHeight 640}
  :strings {:default en :locale ~settings.appearance.language :keep ["Modello"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 12}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :theme ~state.theme
  (node#root :color @gap
    ~(workspace-column)
    {:id playback :when ~(not= state.playing false) :width 0 :height 0 :every {:ms 42 :do tick}}
    (node#popup :use contextMenu
      :with {:open state.ctx.open
             :x ~state.ctx.x
             :y ~state.ctx.y
             :items ~(or-else state.ctx.items (get menus state.ctx.menu) [])
             :icons ~(some (fn [it] (some? it.icon)) (or-else state.ctx.items (get menus state.ctx.menu) []))})
    (node#pie :use pieMenu
      :with {:open state.pie.open
             :x ~state.pie.x
             :y ~state.pie.y
             :items ~computed.pieItems
             :title ~(if (= state.pie.menu "view") "View" "Shading")})
    (node#about-dialog :use dialog
      :with {:open state.about
             :title "Modello 1.0"
             :width 360
             :height 170
             :message "A 3D modeller written only as AffrescoUI screen documents: the stock host runs it."
             :buttons [{:title "Close"}]})
    (node#render-window :use dialog
      :with {:open state.rendering
             :title "Render"
             :width 760
             :height 518
             :buttons [{:title "Close"} {:title "Save As Image…" :run saveRender :keepOpen true}]}
      ~(render-image))
    (node#search :when ~state.quickPick.open
      :use quickPick
      :with {:width 460
             :rows 12
             :rowHeight 22
             :providers [{:items ~computed.searchItems :placeholder "Search for a command"}]})))
