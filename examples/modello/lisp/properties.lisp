;;; properties.lisp --- TODO

(defcomputed propTabs
  [[{:id "tool" :icon "wrench" :title "Tool"}]
     [{:id "render" :icon "camera" :title "Render"}
      {:id "output" :icon "printer" :title "Output"}
      {:id "viewLayer"
       :icon "images"
       :title "View Layer"
       :needs "View layers aren't available yet: Modello keeps one"}
      {:id "scene" :icon "shapes" :title "Scene"}
      {:id "world" :icon "globe" :title "World"}]
     [{:id "collection" :icon "package" :title "Collection"}]
     [{:id "object" :icon "box" :title "Object"}
      {:id "modifiers" :icon "blocks" :title "Modifiers" :needs "Modifiers aren't available yet"}
      {:id "particles" :icon "sparkles" :title "Particles" :needs "Particles aren't available yet"}
      {:id "physics" :icon "orbit" :title "Physics" :needs "Physics isn't available yet"}
      {:id "constraints" :icon "link" :title "Object Constraints" :needs "Object constraints aren't available yet"}
      {:id "data" :icon "triangle" :title "Data"}
      {:id "material" :icon "circle" :title "Material"}
      {:id "texture" :icon "grid-2-x-2" :title "Texture" :needs "Textures aren't available yet"}]])

(defcomputed propPanels
  [{:tab "tool" :key "activeTool" :title "Active Tool" :words "snap gizmos tool" :needs ""}
     {:tab "tool"
      :key "workspacePanel"
      :title "Workspace"
      :words "mode"
      :needs "Workspace settings aren't available yet"}
     {:tab "render" :key "colorManagement" :title "Color Management" :words "exposure" :needs ""}
     {:tab "render"
      :key "sampling"
      :title "Sampling"
      :words "samples"
      :needs "Render samples aren't available yet: a render is the viewport's view"}
     {:tab "output" :key "frameRange" :title "Frame Range" :words "frame start end" :needs ""}
     {:tab "output"
      :key "format"
      :title "Format"
      :words "resolution frame rate"
      :needs "Resolution and frame rate settings aren't available yet"}
     {:tab "scene" :key "scenePanel" :title "Scene" :words "name camera" :needs ""}
     {:tab "scene"
      :key "units"
      :title "Units"
      :words "length metres"
      :needs "Units aren't available yet: lengths are in metres"}
     {:tab "world" :key "surfaceWorld" :title "Surface" :words "color colour background" :needs ""}
     {:tab "collection" :key "restrictions" :title "Restrictions" :words "selectable visible render" :needs ""}
     {:tab "object" :key "transform" :title "Transform" :words "location rotation scale" :needs ""}
     {:tab "object" :key "visibility" :title "Visibility" :words "selectable show viewports renders" :needs ""}
     {:tab "object" :key "relations" :title "Relations" :words "parent" :needs "Parenting isn't available yet"}
     {:tab "object"
      :key "collections"
      :title "Collections"
      :words "link"
      :needs "Linking an object to several collections isn't available yet: M moves it to one"}
     {:tab "object"
      :key "delta"
      :title "Transform Offset"
      :words "delta"
      :needs "Transform offsets aren't available yet"}
     {:tab "object" :key "instancing" :title "Instancing" :words "" :needs "Instancing isn't available yet"}
     {:tab "object" :key "motionPaths" :title "Motion Paths" :words "" :needs "Motion paths aren't available yet"}
     {:tab "object"
      :key "display"
      :title "Viewport Display"
      :words "bounds wire"
      :needs "Viewport display options aren't available yet"}
     {:tab "object" :key "lineArt" :title "Line Drawing" :words "" :needs "Line drawing isn't available yet"}
     {:tab "object"
      :key "custom"
      :title "Custom Properties"
      :words ""
      :needs "Custom properties aren't available yet"}
     {:tab "material"
      :key "materialSettings"
      :title "Settings"
      :words "blend shadow"
      :needs "Blend and shadow settings aren't available yet"}
     {:tab "data"
      :key "dataPanel"
      :title (cond (= computed.activeNode.kind "light") "Light"
                   (= computed.activeNode.kind "camera") "Lens"
                   (= computed.activeNode.kind "mesh") "Geometry"
                   :else "Empty")
      :words "type color colour power focal length primitive"
      :needs (if (= computed.activeNode.kind "empty") "Empty display settings aren't available yet" "")}
     {:tab "data"
      :key "dataMore"
      :title (cond (= computed.activeNode.kind "light") "Shadow"
                   (= computed.activeNode.kind "camera") "Depth of Field"
                   :else "Normals")
      :words ""
      :needs (str (cond (= computed.activeNode.kind "light") "Shadow settings"
                        (= computed.activeNode.kind "camera") "Depth of field"
                        :else "Normals")
                  " aren't available yet")}
     {:tab "material"
      :key "surfaceMaterial"
      :title "Surface"
      :words "base color colour metallic roughness"
      :needs (if (< computed.activeMaterialIndex 0) "This object has no material: New adds one" "")}])

(defcomputed propShown
  (map (fn [it] it.key)
         (filter (fn [it]
                   (and (= it.tab state.propTab)
                        (or (contains ["tool" "render" "output" "scene" "world" "collection"] it.tab)
                            (some? computed.activeNode))
                        (or (= (or-else state.propertiesFilter "") "")
                            (contains (lower-case (str it.title " " it.words))
                                      (lower-case (trim state.propertiesFilter))))))
                 computed.propPanels)))

(defcomputed activeMaterialIndex
  (if (or (nil? computed.activeNode) (nil? computed.activeNode.material))
        (- 1)
        (indexOf (map (fn [it] it.id) state.scene.materials) computed.activeNode.material)))

(defcomponent propertiesArea "The propertiesArea component."
  {}
  `(node#properties :color @panelBack :radius 8
     ~(properties-header)
     (column#properties-tabs :anchors {:right 0 :top 26 :bottom 0}
       :width 30
       :color @bar
       :gap 6
       :alignItems center
       :role tablist
       :label "Properties"
       :arrows vertical
       :radius {:bottomRight 8}
       {:height 0 :width 1 :color transparent}
       ~(repeat [group computed.propTabs]
         `(column :width 24 :height ~(* (count group) 24) :gap 0 :color @panelBack :radius 7
            ~(repeat [t group]
              (prop-tab t)))))
     ~(properties-panels)))

(defview properties-header "#properties-header." []
  `(row#properties-header :anchors {:left 0 :right 0 :top 0}
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
              :tooltip "Editor Type"
              :open state.open.editorProperties
              :items ~(map (fn [it]
                             (merge it
                                    {:checked (= it.id "properties")
                                     :run "swapEditor"
                                     :with {:from "properties" :to it.id}}))
                           computed.editorTypes)})
     (node#properties-search :use searchField :with {:bind state.propertiesFilter :label "Search the properties"})
     (node#properties-pin :use hButton
       :with {:icon pin
              :width 24
              :back transparent
              :disabled true
              :tooltip "Pinning isn't available yet: the properties follow the active object"})
     (node :use gap)))

(defview prop-tab "#prop-tab-." [t]
  `(node :id ~(str "prop-tab-" t.id)
     :width 24
     :height 24
     :radius 7
     :role tab
     :focusable true
     :label ~t.title
     :tooltip ~(or-else t.needs t.title)
     :disabled ~(some? t.needs)
     :selected ~(= state.propTab t.id)
     :color ~(if (= state.propTab t.id) "@panel" "transparent")
     :hover {:color @widget}
     :onClick {:run setOption :with {:key propTab :value ~t.id}}
     (label :font icons
       :icon ~t.icon
       :text ""
       :textColor ~(cond (= state.propTab t.id) "@active" (some? t.needs) "@faint" :else "@dim")
       :align center
       :fill true)))

(defview properties-panels-node "#properties-panels-node." []
  `(node :when ~(contains computed.propShown "visibility") :use propPanel :with {:key visibility :body 80}
     (node :use checkRow
       :with {:id prop-selectable
              :label "Selectable"
              :checked ~(not= computed.activeNode.selectable false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key selectable}})
     (node :use checkRow
       :with {:id prop-visible
              :label "Show in Viewports"
              :checked ~(not= computed.activeNode.visible false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key visible}})
     (node :use checkRow
       :with {:id prop-render
              :label "Show in Renders"
              :checked ~(not= computed.activeNode.render false)
              :run toggleFlag
              :with {:id ~(or-else computed.activeNode.id "") :key render}})))

(defview properties-panels-node-view "#properties-panels-node-view." []
  `(node :when ~(contains computed.propShown "frameRange") :use propPanel :with {:key frameRange :body 56}
     (node :use numberField
       :with {:label "Frame Start"
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
       :with {:label "End"
              :key whole
              :path end
              :value ~state.end
              :step 0.2
              :min ~state.start
              :max 100000
              :first true
              :last true
              :text ~state.end})))

(defview properties-panels-node-2 "#properties-panels-node-2." []
  `(node :when ~(contains computed.propShown "scenePanel") :use propPanel :with {:key scenePanel :body 56}
     (node :height 20 :width "100%" :color transparent
       (label :text "Name"
         :textColor @dim
         :align {:horizontal end :vertical center}
         :anchors {:left 0 :top 0 :bottom 0}
         :width "38%")
       (node :anchors {:left "41%" :right 22 :top 0 :bottom 0} :color @field :radius 4
         (textarea#prop-scene-name :anchors {:left 6 :right 4 :top 1 :bottom 1}
           :text ~state.sceneName
           :textColor @text
           :color transparent
           :label "Scene name"
           :onSubmit {:if ~(not= (trim event.text) "") :then [{:set state.sceneName :to ~(slice (trim event.text) 0 63)}]})))
     (node :use infoRow
       :with {:id scene-camera
              :label "Camera"
              :text ~(or-else (:name (find (fn [it] (= it.kind "camera")) state.scene.nodes)) "None")})))

(defview properties-panels-node-3 "#properties-panels-node-3." []
  `(node :when ~(contains computed.propShown "restrictions") :use propPanel :with {:key restrictions :body 80}
     (node :use checkRow
       :with {:id prop-collection-selectable
              :label "Selectable"
              :checked ~(not= computed.activeCollectionData.selectable false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key selectable}})
     (node :use checkRow
       :with {:id prop-collection-visible
              :label "Show in Viewports"
              :checked ~(not= computed.activeCollectionData.visible false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key visible}})
     (node :use checkRow
       :with {:id prop-collection-render
              :label "Show in Renders"
              :checked ~(not= computed.activeCollectionData.render false)
              :run toggleCollection
              :with {:id ~computed.activeCollectionData.id :key render}})))

(defview properties-panels-node-4 "#properties-panels-node-4." []
  `(node :when ~(= computed.activeNode.kind "camera")
     :use numberField
     :with {:label "Focal Length"
            :key cameraLens
            :value ~(/ (round (* (/ 18 (tan (/ (or-else computed.activeNode.camera.fieldOfView 0.69) 2))) 10)) 10)
            :step 0.5
            :min 2
            :max 250
            :first true
            :last true
            :text ~(str (formatNumber (/ (round (* (/ 18
                                                      (tan (/ (or-else computed.activeNode.camera.fieldOfView 0.69) 2)))
                                                   10))
                                         10)
                                      0
                                      1)
                        " mm")}))

(defview properties-panels-node-5 "#properties-panels-node-5." []
  `(node :when ~(= computed.activeNode.kind "light") :height 20 :width "100%" :color transparent
     (label :text "Type"
       :textColor @dim
       :align {:horizontal end :vertical center}
       :anchors {:left 0 :top 0 :bottom 0}
       :width "38%")
     (row :anchors {:left "41%" :right 22 :top 0 :bottom 0} :gap 1 :color transparent :role radiogroup :label "Light type"
       ~(repeat [lt [["point" "Point"] ["directional" "Sun"] ["spot" "Spot"]]]
         `(node :id ~(str "light-type-" lt.0)
            :grow 1
            :height 20
            :role radio
            :focusable true
            :label ~lt.1
            :selected ~(= computed.activeNode.light.type lt.0)
            :color ~(if (= computed.activeNode.light.type lt.0) "@sel" "@widget")
            :hover {:color @widgetHover}
            :radius ~(cond (= lt.0 "point") {:topLeft 4 :bottomLeft 4} (= lt.0 "spot") {:topRight 4 :bottomRight 4} :else 0)
            :onClick {:run setLight :with {:key type :value ~lt.0}}
            (label :text ~lt.1 :textColor @text :align center :fill true))))))

(defview properties-panels-node-6 "#properties-panels-node-6." []
  `(node :when ~(contains computed.propShown "dataPanel")
     :use propPanel
     :with {:key dataPanel :body ~(if (= computed.activeNode.kind "light") (if state.panels.pickLight 264 80) 32)}
     (node :when ~(= computed.activeNode.kind "mesh")
       :use infoRow
       :with {:id primitive
              :label "Primitive"
              :text ~(+ (upper-case (slice (or-else computed.activeNode.mesh "") 0 1))
                        (slice (or-else computed.activeNode.mesh "") 1))})
     ~(properties-panels-node-4)
     ~(properties-panels-node-5)
     (node :when ~(= computed.activeNode.kind "light")
       :use colorRow
       :with {:label "Color"
              :value ~(or-else (hexColor (or-else computed.activeNode.light.color [1 1 1])) "#ffffff")
              :target light
              :key pickLight})
     (node :when ~(= computed.activeNode.kind "light")
       :use numberField
       :with {:label "Power"
              :path ~(str "scene.nodes[" computed.activeIndex "].light.intensity")
              :value ~(or-else computed.activeNode.light.intensity 1)
              :step 0.5
              :min 0
              :max 100000
              :first true
              :last true
              :text ~(str (formatNumber (or-else computed.activeNode.light.intensity 1) 0 1) " W")})))

(defview material-slot "#material-slot." []
  `(row#material-slot :when ~(and (= state.propTab "material")
                                  (= computed.activeNode.kind "mesh")
                                  (= (or-else state.propertiesFilter "") ""))
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
              :tooltip "Browse Material"
              :open state.open.materialBrowse
              :disabled ~(= (count state.scene.materials) 0)
              :items ~(map (fn [it]
                             {:title it.name
                              :checked (= it.id computed.activeNode.material)
                              :run "assignMaterial"
                              :with {:id it.id}})
                           state.scene.materials)})
     (node :grow 1 :height 20 :radius 4 :color @field
       (label#material-name :anchors {:left 8 :right 4 :top 0 :bottom 0}
         :align {:horizontal start :vertical center}
         :text ~(if (>= computed.activeMaterialIndex 0)
                    (or-else (get-in state.scene.materials [computed.activeMaterialIndex :name]) "")
                    "No material")
         :textColor ~(if (>= computed.activeMaterialIndex 0) "@text" "@dim")))
     (node#material-new :use hButton
       :with {:icon plus :text "New" :width 56 :back @widget :tooltip "New Material" :run newMaterial})
     (node#material-unlink :use hButton
       :with {:icon x
              :width 24
              :back @widget
              :tooltip "Unlink Material"
              :run unlinkMaterial
              :disabled ~(< computed.activeMaterialIndex 0)})))

(defview properties-panels-node-7 "#properties-panels-node-7." []
  `(node :when ~(contains computed.propShown "surfaceMaterial")
     :use propPanel
     :with {:key surfaceMaterial :body ~(if state.panels.pickMaterial 264 80)}
     (node :use colorRow
       :with {:label "Base Color"
              :target material
              :key pickMaterial
              :value ~(or-else (hexColor (get-in state.scene.materials [(max computed.activeMaterialIndex 0) :baseColor]))
                               "#ccccccff")})
     (node :use numberField
       :with {:label "Metallic"
              :path ~(str "scene.materials[" computed.activeMaterialIndex "].metallic")
              :value ~(or-else (get-in state.scene.materials [(max computed.activeMaterialIndex 0) :metallic]) 0)
              :step 0.005
              :min 0
              :max 1
              :first true
              :last true
              :text ~(fixed (or-else (get-in state.scene.materials [(max computed.activeMaterialIndex 0) :metallic]) 0)
                            3)})
     (node :use numberField
       :with {:label "Roughness"
              :path ~(str "scene.materials[" computed.activeMaterialIndex "].roughness")
              :value ~(or-else (get-in state.scene.materials [(max computed.activeMaterialIndex 0) :roughness]) 0.5)
              :step 0.005
              :min 0
              :max 1
              :first true
              :last true
              :text ~(fixed (or-else (get-in state.scene.materials [(max computed.activeMaterialIndex 0) :roughness])
                                     0.5)
                            3)})))

(defview properties-panels "#properties-panels." []
  `(column#properties-panels :anchors {:left 6 :right 36 :top 32 :bottom 0} :gap 2 :color transparent :scroll vertical
     (row#properties-context :width "100%" :height 22 :alignItems center :gap 4 :color transparent
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
         :height 20))
     (node#object-name :when ~(and (= state.propTab "object") (some? computed.activeNode))
       :width "100%"
       :height 24
       :color @field
       :radius 4
       (label :font iconsSmall :icon square :text "" :textColor @object :x 6 :width 14 :anchors {:top 0 :bottom 0})
       (textarea#object-name-field :anchors {:left 24 :right 4 :top 2 :bottom 2}
         :text ~computed.activeNode.name
         :textColor @text
         :color transparent
         :label "Object name"
         :onSubmit {:run rename :with {:id nil :name ~event.text}}))
     (node :when ~(contains computed.propShown "transform") :use transformPanel)
     ~(properties-panels-node)
     (node :when ~(contains computed.propShown "activeTool") :use propPanel :with {:key activeTool :body 80}
       (node :use infoRow :with {:id tool-name :label "Tool" :text ~computed.toolName})
       (node :use checkRow :with {:id prop-snap :label "Snap" :path state.snap :checked ~state.snap})
       (node :use checkRow
         :with {:id prop-gizmos :label "Tool gizmos" :path state.toolGizmos :checked ~(not= state.toolGizmos false)}))
     (node :when ~(contains computed.propShown "colorManagement") :use propPanel :with {:key colorManagement :body 32}
       (node :use numberField
         :with {:label "Exposure"
                :path render.exposure
                :value ~state.render.exposure
                :step 0.01
                :min 0
                :max 8
                :first true
                :last true
                :text ~(fixed state.render.exposure 2)}))
     ~(properties-panels-node-view)
     ~(properties-panels-node-2)
     (node :when ~(contains computed.propShown "surfaceWorld")
       :use propPanel
       :with {:key surfaceWorld :body ~(if state.panels.pickWorld 216 32)}
       (node :use colorRow :with {:label "Color" :value ~state.world.color :target world :key pickWorld}))
     ~(properties-panels-node-3)
     ~(properties-panels-node-6)
     ~(material-slot)
     ~(properties-panels-node-7)
     ~(repeat [p (filter (fn [it] (and (not= it.needs "") (contains computed.propShown it.key) (not (contains ["dataPanel" "surfaceMaterial"] it.key)))) computed.propPanels)]
       `(node :use panelHeader :with {:title ~p.title :key ~p.key :needs ~p.needs}))
     (label#properties-empty :when ~(= (count computed.propShown) 0)
       :height 24
       :fitText true
       :textColor @dim
       :text ~(if (not= (or-else state.propertiesFilter "") "") "Nothing matches the search" "No active object"))))

