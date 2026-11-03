;;; properties.lisp --- the properties editor: its tabs down the side, and the panels of the tab chosen (the tool, the
;;; render, the output, the scene, the world, the collection, the object, its data and its material), searchable
;;;
;;; A panel that needs something Modello doesn't have yet shows greyed, its tooltip saying what.

;;; Tabs and panels

(defcomputed propTabs "The tabs, in groups; some are to come, and say so."
  [[{:id "tool" :icon "wrench" :title (t "tab.tool")}]
   [{:id "render" :icon "camera" :title (t "tab.render")}
    {:id "output" :icon "printer" :title (t "tab.output")}
    {:id "viewLayer" :icon "images" :title (t "tab.viewLayer") :needs (t "tab.viewLayer.needs")}
    {:id "scene" :icon "shapes" :title (t "tab.scene")}
    {:id "world" :icon "globe" :title (t "tab.world")}]
   [{:id "collection" :icon "package" :title (t "tab.collection")}]
   [{:id "object" :icon "box" :title (t "tab.object")}
    {:id "modifiers" :icon "blocks" :title (t "tab.modifiers") :needs (t "tab.modifiers.needs")}
    {:id "particles" :icon "sparkles" :title (t "tab.particles") :needs (t "tab.particles.needs")}
    {:id "physics" :icon "orbit" :title (t "tab.physics") :needs (t "tab.physics.needs")}
    {:id "constraints" :icon "link" :title (t "tab.constraints") :needs (t "tab.constraints.needs")}
    {:id "data" :icon "triangle" :title (t "tab.data")}
    {:id "material" :icon "circle" :title (t "tab.material")}
    {:id "texture" :icon "grid-2-x-2" :title (t "tab.texture") :needs (t "tab.texture.needs")}]])

;; words are what the search finds a panel by, besides its title.
(defcomputed propPanels "Every panel: its tab, its title, the words the search finds it by, and what it needs, if anything."
  [{:tab "tool" :key "activeTool" :title (t "panel.activeTool") :words "snap gizmos tool" :needs ""}
   {:tab "tool" :key "workspacePanel" :title (t "panel.workspace") :words "mode" :needs (t "panel.workspace.needs")}
   {:tab "render" :key "colorManagement" :title (t "panel.colorManagement") :words "exposure" :needs ""}
   {:tab "render" :key "sampling" :title (t "panel.sampling") :words "samples" :needs (t "panel.sampling.needs")}
   {:tab "output" :key "frameRange" :title (t "panel.frameRange") :words "frame start end" :needs ""}
   {:tab "output" :key "format" :title (t "panel.format") :words "resolution frame rate" :needs (t "panel.format.needs")}
   {:tab "scene" :key "scenePanel" :title (t "panel.scene") :words "name camera" :needs ""}
   {:tab "scene" :key "units" :title (t "panel.units") :words "length metres" :needs (t "panel.units.needs")}
   {:tab "world" :key "surfaceWorld" :title (t "panel.surface") :words "color colour background" :needs ""}
   {:tab "collection" :key "restrictions" :title (t "panel.restrictions") :words "selectable visible render" :needs ""}
   {:tab "object" :key "transform" :title (t "transform.title") :words "location rotation scale" :needs ""}
   {:tab "object" :key "visibility" :title (t "panel.visibility") :words "selectable show viewports renders" :needs ""}
   {:tab "object" :key "relations" :title (t "panel.relations") :words "parent" :needs (t "panel.relations.needs")}
   {:tab "object" :key "collections" :title (t "panel.collections") :words "link" :needs (t "panel.collections.needs")}
   {:tab "object" :key "delta" :title (t "panel.delta") :words "delta" :needs (t "panel.delta.needs")}
   {:tab "object" :key "instancing" :title (t "panel.instancing") :words "" :needs (t "panel.instancing.needs")}
   {:tab "object" :key "motionPaths" :title (t "panel.motionPaths") :words "" :needs (t "panel.motionPaths.needs")}
   {:tab "object" :key "display" :title (t "panel.display") :words "bounds wire" :needs (t "panel.display.needs")}
   {:tab "object" :key "lineArt" :title (t "panel.lineArt") :words "" :needs (t "panel.lineArt.needs")}
   {:tab "object" :key "custom" :title (t "panel.custom") :words "" :needs (t "panel.custom.needs")}
   {:tab "material" :key "materialSettings" :title (t "panel.materialSettings") :words "blend shadow" :needs (t "panel.materialSettings.needs")}
   {:tab "data"
    :key "dataPanel"
    :title (cond (= computed.activeNode.kind "light") (t "panel.light")
                 (= computed.activeNode.kind "camera") (t "panel.lens")
                 (= computed.activeNode.kind "mesh") (t "panel.geometry")
                 :else (t "panel.empty"))
    :words "type color colour power focal length primitive"
    :needs (if (= computed.activeNode.kind "empty") (t "panel.empty.needs") "")}
   {:tab "data"
    :key "dataMore"
    :title (cond (= computed.activeNode.kind "light") (t "panel.shadow")
                 (= computed.activeNode.kind "camera") (t "panel.depthOfField")
                 :else (t "panel.normals"))
    :words ""
    :needs (cond (= computed.activeNode.kind "light") (t "panel.shadow.needs")
                 (= computed.activeNode.kind "camera") (t "panel.depthOfField.needs")
                 :else (t "panel.normals.needs"))}
   {:tab "material"
    :key "surfaceMaterial"
    :title (t "panel.surface")
    :words "base color colour metallic roughness"
    :needs (if (< computed.activeMaterialIndex 0) (t "panel.surfaceMaterial.needs") "")}])

(defcomputed propShown "The keys of the panels shown: the tab's, if there is an object for them, that the search finds."
  (map (fn [it] it.key)
       (filter (fn [it]
                 (and (= it.tab state.propTab)
                      (or (contains ["tool" "render" "output" "scene" "world" "collection"] it.tab) (some? computed.activeNode))
                      (or (= (or-else state.propertiesFilter "") "")
                          (contains (lower-case (str it.title " " it.words)) (lower-case (trim state.propertiesFilter))))))
               computed.propPanels)))

(defcomputed activeMaterialIndex "Where the active object's material is in the scene's materials, or -1."
  (if (or (nil? computed.activeNode) (nil? computed.activeNode.material))
    -1
    (indexOf (map (fn [it] it.id) state.scene.materials) computed.activeNode.material)))

;;; The area

(defcomponent propertiesArea "The properties: the header, the tabs down the right side, and the panels." {}
  `(node#properties :color @panelBack :radius 8
     ~(properties-header)
     (column#properties-tabs :anchors {:right 0 :top 26 :bottom 0}
       :width 30
       :color @bar
       :gap 6
       :alignItems center
       :role tablist
       :label ~(t "editor.properties")
       :arrows vertical
       :radius {:bottomRight 8}
       {:height 0 :width 1 :color transparent}
       ~(repeat [group computed.propTabs]
          `(column :width 24 :height ~(* (count group) 24) :gap 0 :color @panelBack :radius 7
             ~(repeat [tab group] (prop-tab tab)))))
     ~(properties-panels)))

(defview properties-header "The header: editor type, the search, and the pin (to come)." []
  `(row#properties-header
     :anchors {:left 0 :right 0 :top 0}
     :height 26
     :color @header
     :radius {:topLeft 8 :topRight 8}
     :alignItems center
     :gap 2
     (node :use gap)
     (node#editor-type-properties :use hButton
       :with {:icon sliders-horizontal
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open state.open.editorProperties
              :items ~(editor-choices "properties")})
     (node#properties-search :use searchField :with {:bind state.propertiesFilter :label ~(t "properties.search")})
     (node#properties-pin
       :use hButton
       :with {:icon pin :width 24 :back transparent :disabled true :tooltip ~(t "properties.pinTip")})
     (node :use gap)))

(defview prop-tab "A tab: its icon; one to come is greyed, and says why." [tab]
  `(node :id ~(str "prop-tab-" tab.id)
     :width 24
     :height 24
     :radius 7
     :role tab
     :focusable true
     :label ~tab.title
     :tooltip ~(or-else tab.needs tab.title)
     :disabled ~(some? tab.needs)
     :selected ~(= state.propTab tab.id)
     :color ~(if (= state.propTab tab.id) "@panel" "transparent")
     :hover {:color @widget}
     :onClick ~(handler setOption "propTab" tab.id)
     (label :font icons
       :icon ~tab.icon
       :text ""
       :textColor ~(cond (= state.propTab tab.id) "@active" (some? tab.needs) "@faint" :else "@dim")
       :align center
       :fill true)))

;;; The panels, in the order they are drawn

(defview properties-panels "The panels of the tab chosen, scrolling; a panel shows when propShown has it." []
  `(column#properties-panels :anchors {:left 6 :right 36 :top 32 :bottom 0} :gap 2 :color transparent :scroll vertical
     ~(properties-context)
     ~(object-name)
     (node :when ~(contains computed.propShown "transform") :use transformPanel)
     ~(visibility-panel)
     ~(active-tool-panel)
     ~(color-management-panel)
     ~(frame-range-panel)
     ~(scene-panel)
     (node
       :when ~(contains computed.propShown "surfaceWorld")
       :use propPanel
       :with {:key surfaceWorld :body ~(if state.panels.pickWorld 216 32)}
       (node
         :use colorRow
         :with {:label ~(t "properties.color") :value ~state.world.color :target world :key pickWorld}))
     ~(restrictions-panel)
     ~(data-panel)
     ~(material-slot)
     ~(surface-panel)
     ~(repeat [p (filter (fn [it] (and (not= it.needs "") (contains computed.propShown it.key) (not (contains ["dataPanel" "surfaceMaterial"] it.key))))
                         computed.propPanels)]
        `(node :use panelHeader :with {:title ~p.title :key ~p.key :needs ~p.needs}))
     (label#properties-empty :when ~(= (count computed.propShown) 0)
       :height 24
       :fitText true
       :textColor @dim
       :text ~(if (not= (or-else state.propertiesFilter "") "") (t "properties.noMatch") (t "properties.noObject")))))

(defview properties-context "What the panels are about: the active collection, or the active object." []
  `(row#properties-context :width "100%" :height 22 :alignItems center :gap 4 :color transparent
     (label :font iconsSmall
       :icon ~(if (= state.propTab "collection") "package-open" "square")
       :text ""
       :textColor ~(if (= state.propTab "collection") "@text" "@object")
       :width 16
       :height 20
       :align center)
     (label#properties-context-name :text ~(cond (= state.propTab "collection") computed.activeCollectionData.name
                                                 computed.activeNode computed.activeNode.name
                                                 :else "")
       :textColor @text
       :fitText true
       :height 20)))

(defview object-name "The Object tab's name field." []
  `(node#object-name
     :when ~(and (= state.propTab "object") (some? computed.activeNode))
     :width "100%"
     :height 24
     :color @field
     :radius 4
     (label :font iconsSmall :icon square :text "" :textColor @object :x 6 :width 14 :anchors {:top 0 :bottom 0})
     (textarea#object-name-field :anchors {:left 24 :right 4 :top 2 :bottom 2}
       :text ~computed.activeNode.name
       :textColor @text
       :color transparent
       :label ~(t "properties.objectName")
       :onSubmit ~(handler rename nil event.text))))

(defview visibility-panel "Whether the active object can be picked, and shows in the viewports and the renders." []
  `(node :when ~(contains computed.propShown "visibility") :use propPanel :with {:key visibility :body 80}
     (node :use checkRow
       :with {:id prop-selectable
              :label ~(t "properties.selectable")
              :checked ~(not= computed.activeNode.selectable false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key selectable}})
     (node :use checkRow
       :with {:id prop-visible
              :label ~(t "properties.showInViewports")
              :checked ~(not= computed.activeNode.visible false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key visible}})
     (node :use checkRow
       :with {:id prop-render
              :label ~(t "properties.showInRenders")
              :checked ~(not= computed.activeNode.render false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key render}})))

(defview active-tool-panel "The Tool tab: the tool, snapping and its gizmos." []
  `(node :when ~(contains computed.propShown "activeTool") :use propPanel :with {:key activeTool :body 80}
     (node :use infoRow :with {:id tool-name :label ~(t "properties.tool") :text ~computed.toolName})
     (node :use checkRow :with {:id prop-snap :label ~(t "sidebar.snap") :path state.snap :checked ~state.snap})
     (node
       :use checkRow
       :with {:id prop-gizmos :label ~(t "sidebar.toolGizmos") :path state.toolGizmos :checked ~(not= state.toolGizmos false)})))

(defview color-management-panel "The Render tab's exposure." []
  `(node :when ~(contains computed.propShown "colorManagement") :use propPanel :with {:key colorManagement :body 32}
     (node :use numberField
       :with {:label ~(t "properties.exposure")
              :path render.exposure
              :value ~state.render.exposure
              :step 0.01
              :min 0
              :max 8
              :first true
              :last true
              :text ~(fixed state.render.exposure 2)})))

(defview frame-range-panel "The Output tab's frame range: the first and the last frame." []
  `(node :when ~(contains computed.propShown "frameRange") :use propPanel :with {:key frameRange :body 56}
     (node :use numberField
       :with {:label ~(t "properties.frameStart")
              :key whole
              :path start
              :value ~state.start
              :step 0.2
              :min 0
              :max ~state.end
              :first true
              :last true
              :text ~state.start})
     (node :use numberField
       :with {:label ~(t "properties.frameEnd")
              :key whole
              :path end
              :value ~state.end
              :step 0.2
              :min ~state.start
              :max 100000
              :first true
              :last true
              :text ~state.end})))

(defview scene-panel "The Scene tab: the scene's name, and its camera." []
  `(node :when ~(contains computed.propShown "scenePanel") :use propPanel :with {:key scenePanel :body 56}
     (node :height 20 :width "100%" :color transparent
       ~(caption "properties.name")
       (node :anchors {:left "41%" :right 22 :top 0 :bottom 0} :color @field :radius 4
         (textarea#prop-scene-name :anchors {:left 6 :right 4 :top 1 :bottom 1}
           :text ~state.sceneName
           :textColor @text
           :color transparent
           :label ~(t "topbar.sceneName")
           :onSubmit ~(handler renameScene event.text))))
     (node :use infoRow
       :with {:id scene-camera
              :label ~(t "menu.camera")
              :text ~(or-else (:name (find (fn [it] (= it.kind "camera")) state.scene.nodes)) (t "properties.none"))})))

(defview restrictions-panel "The Collection tab: whether the active collection's objects can be picked, and show." []
  `(node :when ~(contains computed.propShown "restrictions") :use propPanel :with {:key restrictions :body 80}
     (node :use checkRow
       :with {:id prop-collection-selectable
              :label ~(t "properties.selectable")
              :checked ~(not= computed.activeCollectionData.selectable false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key selectable}})
     (node :use checkRow
       :with {:id prop-collection-visible
              :label ~(t "properties.showInViewports")
              :checked ~(not= computed.activeCollectionData.visible false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key visible}})
     (node :use checkRow
       :with {:id prop-collection-render
              :label ~(t "properties.showInRenders")
              :checked ~(not= computed.activeCollectionData.render false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key render}})))

;;; The Data tab

(defview data-panel "The Data tab: a mesh's primitive, a camera's lens, or a light's type, colour and power." []
  `(node :when ~(contains computed.propShown "dataPanel")
     :use propPanel
     :with {:key dataPanel :body ~(if (= computed.activeNode.kind "light") (if state.panels.pickLight 264 80) 32)}
     (node :when ~(= computed.activeNode.kind "mesh")
       :use infoRow
       :with {:id primitive
              :label ~(t "properties.primitive")
              :text ~(+ (upper-case (slice (or-else computed.activeNode.mesh "") 0 1)) (slice (or-else computed.activeNode.mesh "") 1))})
     ~(camera-lens)
     ~(light-type)
     (node :when ~(= computed.activeNode.kind "light")
       :use colorRow
       :with {:label ~(t "properties.color")
              :value ~(or-else (hexColor (or-else computed.activeNode.light.color [1 1 1])) "#ffffff")
              :target light
              :key pickLight})
     (node :when ~(= computed.activeNode.kind "light")
       :use numberField
       :with {:label ~(t "properties.power")
              :path ~(str "scene.nodes[" computed.activeIndex "].light.intensity")
              :value ~(or-else computed.activeNode.light.intensity 1)
              :step 0.5
              :min 0
              :max 100000
              :first true
              :last true
              :text ~(t "unit.watts" {:value (formatNumber (or-else computed.activeNode.light.intensity 1) 0 1)})})))

(defmacro camera-mm []
  ;; A macro, not a function: what it writes stays an expression.
  "The active camera's focal length in millimetres, to a tenth."
  `(/ (round (* (/ 18 (tan (/ (or-else computed.activeNode.camera.fieldOfView 0.69) 2))) 10)) 10))

(defview camera-lens "A camera's focal length." []
  `(node :when ~(= computed.activeNode.kind "camera")
     :use numberField
     :with {:label ~(t "sidebar.focalLength")
            :key cameraLens
            :value ~(camera-mm)
            :step 0.5
            :min 2
            :max 250
            :first true
            :last true
            :text ~(t "unit.mm" {:value (formatNumber (camera-mm) 0 1)})}))

(defview light-type "A light's type: point, sun or spot, as three buttons in a row." []
  `(node :when ~(= computed.activeNode.kind "light") :height 20 :width "100%" :color transparent
     ~(caption "properties.type")
     (row
       :anchors {:left "41%" :right 22 :top 0 :bottom 0}
       :gap 1
       :color transparent
       :role radiogroup
       :label ~(t "properties.lightType")
       ~(repeat [kind ["point" "directional" "spot"]] (light-type-button kind)))))

(defview light-type-button "One light type; the ends round the row's corners." [kind]
  `(node :id ~(str "light-type-" kind)
     :grow 1
     :height 20
     :role radio
     :focusable true
     :label ~(t (str "lightType." kind))
     :selected ~(= computed.activeNode.light.type kind)
     :color ~(if (= computed.activeNode.light.type kind) "@sel" "@widget")
     :hover {:color @widgetHover}
     :radius ~(cond (= kind "point") {:topLeft 4 :bottomLeft 4} (= kind "spot") {:topRight 4 :bottomRight 4} :else 0)
     :onClick ~(handler setLight "type" kind)
     (label :text ~(t (str "lightType." kind)) :textColor @text :align center :fill true)))

;;; The Material tab

(defview material-slot "The active mesh's material: browse the materials, its name, a new one, unlink it." []
  `(row#material-slot
     :when ~(and (= state.propTab "material") (= computed.activeNode.kind "mesh") (= (or-else state.propertiesFilter "") ""))
     :width "100%"
     :height 24
     :gap 2
     :color transparent
     :alignItems center
     (node#material-browse :use hButton
       :with {:icon circle-dot
              :chevron true
              :width 38
              :back @widget
              :tooltip ~(t "properties.browseMaterial")
              :open state.open.materialBrowse
              :disabled ~(= (count state.scene.materials) 0)
              :items ~(map (fn [it] {:title it.name :checked (= it.id computed.activeNode.material) :run "assignMaterial" :with {:id it.id}})
                           state.scene.materials)})
     (node :grow 1 :height 20 :radius 4 :color @field
       (label#material-name :anchors {:left 8 :right 4 :top 0 :bottom 0}
         :align {:horizontal start :vertical center}
         :text ~(if (>= computed.activeMaterialIndex 0)
                  (or-else (get-in state.scene.materials [computed.activeMaterialIndex :name]) "")
                  (t "properties.noMaterial"))
         :textColor ~(if (>= computed.activeMaterialIndex 0) "@text" "@dim")))
     (node#material-new :use hButton
       :with {:icon plus :text ~(t "properties.new") :width 56 :back @widget :tooltip ~(t "command.newMaterial") :run newMaterial})
     (node#material-unlink :use hButton
       :with {:icon x :width 24 :back @widget :tooltip ~(t "command.unlinkMaterial") :run unlinkMaterial :disabled ~(< computed.activeMaterialIndex 0)})))

(defmacro material-value [key default]
  ;; A macro, not a function: what it writes stays an expression.
  "A value of the active material, or a default."
  `(or-else (get-in state.scene.materials [(max computed.activeMaterialIndex 0) ~key]) ~default))

(defview surface-panel "The material's surface: its base colour, metallic and roughness." []
  `(node
     :when ~(contains computed.propShown "surfaceMaterial")
     :use propPanel
     :with {:key surfaceMaterial :body ~(if state.panels.pickMaterial 264 80)}
     (node :use colorRow
       :with {:label ~(t "properties.baseColor")
              :target material
              :key pickMaterial
              :value ~(or-else (hexColor (get-in state.scene.materials [(max computed.activeMaterialIndex 0)
                                                                         :baseColor])) "#ccccccff")})
     (node :use numberField
       :with {:label ~(t "properties.metallic")
              :path ~(str "scene.materials[" computed.activeMaterialIndex "].metallic")
              :value ~(material-value :metallic 0)
              :step 0.005
              :min 0
              :max 1
              :first true
              :last true
              :text ~(fixed (material-value :metallic 0) 3)})
     (node :use numberField
       :with {:label ~(t "properties.roughness")
              :path ~(str "scene.materials[" computed.activeMaterialIndex "].roughness")
              :value ~(material-value :roughness 0.5)
              :step 0.005
              :min 0
              :max 1
              :first true
              :last true
              :text ~(fixed (material-value :roughness 0.5) 3)})))
