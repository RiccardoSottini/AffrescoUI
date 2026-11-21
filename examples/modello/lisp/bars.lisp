;;; bars.lisp --- the window's frame: the top bar (menus, workspaces, scene and layer), the four areas, the status bar,
;;; and the render window's image

(ns modello.bars)

;;; The workspace

(defview workspace-column "The window's column: the top bar, the areas (or the one maximized), the status bar." []
  `(column#workspace :fill true
     (node :use topBar)
     ~(areas)
     (node :when ~(some? state.maximized) :use area :with {:slot ~state.maximized} :width "100%" :grow 1)
     (node :use modelloStatusBar)))

(defview areas "The four areas: a and b on the left, over each other, c and d on the right; each split drags." []
  `(split#areas :when ~(nil? state.maximized)
     :width "100%"
     :grow 1
     :bind state.sizes.main
     :minSize 240
     :sashSize 4
     :defaultSizes [1102]
     (split#left-areas :direction column :bind state.sizes.left :minSize [200 60] :sashSize 4 :defaultSizes [720]
       (node :use area :with {:slot a})
       (node :use area :with {:slot b}))
     (split#right-areas :direction column :bind state.sizes.right :minSize [80 160] :sashSize 4 :defaultSizes [262]
       (node :use area :with {:slot c})
       (node :use area :with {:slot d}))))

(defcomponent area "An area: the editor state.areas gives its slot (a, b, c or d)." {:slot a}
  `(node :id ~(str "area-" slot) :color transparent :onHover ~(handler hover-area slot)
     (node :when ~(= (get state.areas slot) :viewport) :use viewportArea :fill true)
     (node :when ~(= (get state.areas slot) :timeline) :use timelineArea :fill true)
     (node :when ~(= (get state.areas slot) :outliner) :use outlinerArea :fill true)
     (node :when ~(= (get state.areas slot) :properties) :use propertiesArea :fill true)
     (node :when ~(= (get state.areas slot) :shader) :use shaderArea :fill true)
     (node :when ~(= (get state.areas slot) :image) :use imageArea :fill true)
     (node :when ~(= (get state.areas slot) :text) :use textArea :fill true)))

;;; The top bar

(defcomponent topBar "The top bar: the app's icon and menus, the workspaces in the middle, the scene and the layer at the end." {}
  `(row#top-bar :width "100%" :height 28 :color @bar :alignItems center :gap 0
     (node :use gap :with {:width 6})
     (label#app-icon
       :font icons
       :icon cuboid
       :text ""
       :textColor @kit.accent
       :width 26
       :height 26
       :align center
       :role img
       :label "Modello")
     ~(menu-bar)
     {:grow 1 :height 1 :color transparent}
     ~(workspace-tabs)
     {:grow 1 :height 1 :color transparent}
     ~@(scene-picker)
     (node :use gap :with {:width 8})
     ~@(layer-picker)
     (node :use gap :with {:width 6})))

(defview menu-bar "The top bar's menus: File, Edit, Render, Window and Help." []
  `(node#menu-bar :use menuBar
     :width 300
     :with {:open state.menuBar
            :height 28
            :color @bar
            :textColor @text
            :dimColor @dim
            :highlight @sel
            :menus ~[{:name "file" :title (t "menubar.file") :items menus.file}
                     {:name "edit" :title (t "menubar.edit") :items menus.edit}
                     {:name "render" :title (t "menubar.render") :items menus.render}
                     {:name "window" :title (t "menubar.window") :items menus.window}
                     {:name "help" :title (t "menubar.help") :items menus.help}]}))

(defview workspace-tabs "The workspaces, as a segmented control: an icon and a name each." []
  `(row#workspaces :height 26
     :alignItems center
     :gap 2
     :color @header
     :radius 13
     :role tablist
     :label ~(t "topbar.workspaces")
     :arrows horizontal
     :fitContent width
     {:width 1 :height 1 :color transparent}
     ~(repeat [w computed.workspaces] (workspace-tab w))
     {:width 1 :height 1 :color transparent}))

(defview workspace-tab "A workspace's tab: its icon and name, filled with the accent when chosen." [w]
  `(row :id ~(str "workspace-" w.id)
     :height 22
     :width ~(+ (* (count w.title) 6.5) 34)
     :radius 11
     :role tab
     :focusable true
     :label ~w.title
     :alignItems center
     :gap 5
     :selected ~(= state.workspace w.name)
     :color ~(if (= state.workspace w.name) "@kit.accent" "transparent")
     :hover {:color ~(if (= state.workspace w.name) "@kit.accent" "@view")}
     :onClick ~(handler set-workspace w.id w.name)
     {:width 5 :height 1 :color transparent}
     (label :font iconsSmall
       :icon ~w.icon
       :text ""
       :textColor ~(if (= state.workspace w.name) "#ffffff" "@dim")
       :width 14
       :height 20
       :align center)
     (label :text ~w.title :textColor ~(if (= state.workspace w.name) "#ffffff" "@text") :fitText true :height 20)))

(defview scene-picker "The scene's picker (one scene for now), then its name, typed in place: two nodes." []
  [`(node#scene-picker
      :use hButton
      :with {:icon shapes :chevron true :width 38 :disabled true :tooltip ~(t "topbar.oneScene")})
   `(node#scene-name :width 100 :height 20 :color @field :radius 4
      (textarea#scene-name-field :anchors {:left 3 :top 1 :bottom 1 :right 2}
        :color transparent
        :text ~state.sceneName
        :textColor @text
        :label ~(t "topbar.sceneName")
        :onSubmit ~(handler rename-scene event.text)))])

(defview layer-picker "The view layer's picker (one layer for now), then its name, typed in place: two nodes." []
  [`(node#layer-picker
      :use hButton
      :with {:icon images :chevron true :width 38 :disabled true :tooltip ~(t "topbar.oneLayer")})
   `(node#layer-name :width 100 :height 20 :color @field :radius 4
      (textarea#layer-name-field :anchors {:left 3 :top 1 :bottom 1 :right 2}
        :color transparent
        :text ~state.layerName
        :textColor @text
        :label ~(t "topbar.layerName")
        :onSubmit ~(handler rename-layer event.text)))])

;;; The status bar

(defcomponent modelloStatusBar "The status bar: what the mouse does, and the scene's counts at the end." {}
  `(node#status :width "100%" :height 22 :color @bar
     ~(status-bar)))

(defview status-bar "What the mouse buttons do (or, while transforming, how to confirm, cancel and lock), and the counts." []
  `(node#status-bar :use statusBar
     :fill true
     :with {:height 22
            :color @bar
            :textColor @dim
            :items ~(if (some? state.modal)
                      [{:text (t "status.confirm") :icon "mouse-left"}
                       {:text (t "status.cancel") :icon "mouse-right"}
                       {:text (t "status.axisLock")}
                       {:text (t "status.typeValue")}]
                      [{:text (t "status.select") :icon "mouse-left"}
                       {:text (t "status.rotateView") :icon "mouse"}
                       {:text (t "status.object") :icon "mouse-right"}
                       {:text (str (if (not= state.fileStatus "") (str state.fileStatus " | ") "") computed.stats "Modello 1.0")
                        :side "end"}])}))

;;; The render window

(defcomputed renderScene "The scene as a render shows it: what isn't rendered hidden, the modifiers applied."
  (modifiers-applied
   (merge state.scene
         {:nodes (map (fn [it]
                        (if (or (false? it.render) (contains computed.collectionsOff.render (or-else it.collection "collection")))
                          (merge it {:visible false})
                          it))
                      state.scene.nodes)})))

(defview render-image "The render: the scene through its camera, lit by its lights, without what isn't rendered." []
  `(scene#render-image :anchors {:left 20 :right 20 :top 44 :bottom 56}
     :scene ~computed.renderScene
     :view ~{:camera computed.cameraView :selection []}
     :up z
     :extras false
     :shading material
     :lighting scene
     :exposure ~state.render.exposure
     :grid false
     :axes false
     :background ~state.world.color
     :controls {:orbit none :pan none :select none :zoom false :selects false}
     :label ~(t "render.label" {:camera (or-else computed.cameraNode.name (t "render.theCamera"))})
     :role img))
