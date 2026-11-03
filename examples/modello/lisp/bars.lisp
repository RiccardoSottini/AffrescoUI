;;; bars.lisp --- TODO

(defcomponent area "The area component."
  {:slot a}
  `(node :id ~(str "area-" slot) :color transparent :onHover {:set state.hoverSlot :to ~slot}
     (node :when ~(= (get state.areas slot) "viewport") :use viewportArea :fill true)
     (node :when ~(= (get state.areas slot) "timeline") :use timelineArea :fill true)
     (node :when ~(= (get state.areas slot) "outliner") :use outlinerArea :fill true)
     (node :when ~(= (get state.areas slot) "properties") :use propertiesArea :fill true)
     (node :when ~(= (get state.areas slot) "shader") :use shaderArea :fill true)))

(defcomponent topBar "The topBar component."
  {}
  `(row#top-bar :width "100%" :height 28 :color @bar :alignItems center :gap 0
     (node :use gap :with {:width 6})
     (label#app-icon :font icons
       :icon cuboid
       :text ""
       :textColor @kit.accent
       :width 26
       :height 26
       :align center
       :role img
       :label "Modello")
     (node#menu-bar :use menuBar
       :width 300
       :with {:open state.menuBar
              :height 28
              :color @bar
              :textColor @text
              :dimColor @dim
              :highlight @sel
              :menus ~[{:name "file" :title "File" :items menus.file}
                       {:name "edit" :title "Edit" :items menus.edit}
                       {:name "render" :title "Render" :items menus.render}
                       {:name "window" :title "Window" :items menus.window}
                       {:name "help" :title "Help" :items menus.help}]})
     {:grow 1 :height 1 :color transparent}
     (row#workspaces :height 24
       :alignItems center
       :gap 0
       :color @header
       :radius 7
       :role tablist
       :label "Workspaces"
       :arrows horizontal
       :fitContent width
       {:width 2 :height 1 :color transparent}
       ~(repeat [w computed.workspaces]
         (workspace-tab w))
       {:width 2 :height 1 :color transparent})
     {:grow 1 :height 1 :color transparent}
     (node#scene-picker :use hButton
       :with {:icon shapes
              :chevron true
              :width 38
              :disabled true
              :tooltip "Modello has one scene: more scenes aren't available yet"})
     (node#scene-name :width 100 :height 20 :color @field :radius 4
       (textarea#scene-name-field :anchors {:left 3 :top 1 :bottom 1 :right 2}
         :color transparent
         :text ~state.sceneName
         :textColor @text
         :label "Scene name"
         :onSubmit {:if ~(not= (trim event.text) "") :then [{:set state.sceneName :to ~(slice (trim event.text) 0 63)}]}))
     (node :use gap :with {:width 8})
     (node#layer-picker :use hButton
       :with {:icon images
              :chevron true
              :width 38
              :disabled true
              :tooltip "Modello has one view layer: more layers aren't available yet"})
     (node#layer-name :width 100 :height 20 :color @field :radius 4
       (textarea#layer-name-field :anchors {:left 3 :top 1 :bottom 1 :right 2}
         :color transparent
         :text ~state.layerName
         :textColor @text
         :label "View layer name"
         :onSubmit {:if ~(not= (trim event.text) "") :then [{:set state.layerName :to ~(slice (trim event.text) 0 63)}]}))
     (node :use gap :with {:width 6})))

(defcomponent modelloStatusBar "The modelloStatusBar component."
  {}
  `(node#status :width "100%" :height 22 :color @bar
     ~(status-bar)))

(defview workspace-column "#workspace." []
  `(column#workspace :fill true
     (node :use topBar)
     (split#areas :when ~(nil? state.maximized)
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
         (node :use area :with {:slot d})))
     (node :when ~(some? state.maximized) :use area :with {:slot ~state.maximized} :width "100%" :grow 1)
     (node :use modelloStatusBar)))

(defview render-image "#render-image." []
  `(scene#render-image :anchors {:left 20 :right 20 :top 44 :bottom 56}
     :scene ~(merge state.scene
                    {:nodes (map (fn [it]
                                   (if (or (= it.render false)
                                           (contains computed.collectionsOff.render
                                                     (or-else it.collection "collection")))
                                       (merge it {:visible false})
                                       it))
                                 state.scene.nodes)})
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
     :label ~(str "Render of " (or-else computed.cameraNode.name "the camera"))
     :role img))

(defview workspace-tab "#workspace-." [w]
  `(node :id ~(str "workspace-" w.id)
     :height 20
     :width ~(+ (* (count w.title) 6) 18)
     :radius 5
     :role tab
     :focusable true
     :label ~w.title
     :disabled ~(some? w.needs)
     :tooltip ~(if (some? w.needs) (str w.title " isn't available yet: it needs " w.needs) "")
     :selected ~(= state.workspace w.name)
     :color ~(if (= state.workspace w.name) "@widget" "transparent")
     :hover {:color ~(cond (= state.workspace w.name) "@widget" (some? w.needs) "transparent" :else "@view")}
     :onClick {:run setWorkspace :with {:id ~w.id :name ~w.name}}
     (label :text ~w.title
       :textColor ~(cond (= state.workspace w.name) "@active" (some? w.needs) "@faint" :else "@dim")
       :align center
       :fill true)))

(defview status-bar "#status-bar." []
  `(node#status-bar :use statusBar
     :fill true
     :with {:height 22
            :color @bar
            :textColor @dim
            :items ~(if (some? state.modal)
                        [{:text "Confirm" :icon "mouse-left"}
                         {:text "Cancel" :icon "mouse-right"}
                         {:text "X Y Z  Axis lock, Shift for a plane"}
                         {:text "Type a value"}]
                        [{:text "Select" :icon "mouse-left"}
                         {:text "Rotate View" :icon "mouse"}
                         {:text "Object" :icon "mouse-right"}
                         {:text (+ (if (not= state.fileStatus "") (str state.fileStatus " | ") "")
                                   computed.stats
                                   "Modello 1.0")
                          :side "end"}])}))

