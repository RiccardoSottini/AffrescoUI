;;; menus.lisp --- TODO

(defmenu file
  {:new {:run newScene :group "1_new" :order 1}
   :open {:run openScene :group "1_new" :order 2}
   :save {:run saveScene :group "2_save"}
   :export {:run exportGltf :group "3_export"}})

(defmenu edit
  {:undo {:run undo :group "1_history" :order 1}
   :redo {:run redo :group "1_history" :order 2}
   :adjust {:run adjustLastPanel :group "1_history" :order 3}
   :copy {:run copyObjects :group "2_clipboard" :order 1}
   :paste {:run pasteObjects :group "2_clipboard" :order 2}
   :search {:run search :group "3_search"}})

(defmenu render
  {:image {:run renderImage :group "1_render"} :camera {:run viewCamera :title "View Camera" :group "2_view"}})

(defmenu window
  {:next {:run nextWorkspace :group "0_workspace" :order 1}
   :previous {:run previousWorkspace :group "0_workspace" :order 2}
   :theme {:run toggleTheme :group "1_theme"}
   :sidebar {:run toggleSidebar :group "2_regions" :order 1}
   :toolbar {:run toggleToolbar :group "2_regions" :order 2}
   :assistant {:run assistant.toggle :title "Assistant" :group "3_assistant"}})

(defmenu help {:about {:run about :group "1_help"}})

(defmenu view
  {:toolbar {:run toggleToolbar :group "1_regions" :order 1}
   :sidebar {:run toggleSidebar :group "1_regions" :order 2}
   :camera {:run viewCamera :group "2_frame" :order 1}
   :projection {:run toggleProjection :group "2_frame" :order 2}
   :frameSelected {:run frameSelected :group "3_frame" :order 1}
   :frameAll {:run frameAll :group "3_frame" :order 2}
   :viewpoint {:submenu viewpoint :title "Viewpoint" :group "4_view"}
   :maximize {:run toggleMaximize :group "5_area"}})

(defmenu viewpoint
  {:top {:run viewTop :group "1" :order 1}
   :bottom {:run viewBottom :group "1" :order 2}
   :front {:run viewFront :group "2" :order 1}
   :back {:run viewBack :group "2" :order 2}
   :right {:run viewRight :group "3" :order 1}
   :left {:run viewLeft :group "3" :order 2}})

(defmenu select
  {:all {:run selectAll :title "All" :group "1_all" :order 1}
   :none {:run selectNone :title "None" :group "1_all" :order 2}
   :invert {:run selectInvert :title "Invert" :group "1_all" :order 3}})

(defmenu add
  {:mesh {:submenu addMesh :title "Mesh" :icon triangle :group "1_add" :order 1}
   :light {:submenu addLight :title "Light" :icon lightbulb :group "2_add" :order 1}
   :camera {:run addObject
            :with {:kind camera :name "Camera"}
            :title "Camera"
            :icon video
            :group "2_add"
            :order 2}
   :empty {:run addObject :with {:kind empty :name "Empty"} :title "Empty" :icon axis-3-d :group "2_add" :order 3}})

(defmenu shaderAdd
  {:color {:run addShaderNode :with {:op color} :title "Color" :icon palette :group "1_input" :order 1}
   :number {:run addShaderNode :with {:op number} :title "Value" :icon hash :group "1_input" :order 2}
   :fresnel {:run addShaderNode :with {:op fresnel} :title "Fresnel" :icon aperture :group "1_input" :order 3}
   :height {:run addShaderNode :with {:op height} :title "Height" :icon arrow-up-down :group "1_input" :order 4}
   :checker {:run addShaderNode
             :with {:op checker}
             :title "Checker Texture"
             :icon grid-3x3
             :group "2_texture"
             :order 1}
   :noise {:run addShaderNode :with {:op noise} :title "Noise Texture" :icon waves :group "2_texture" :order 2}
   :mix {:run addShaderNode :with {:op mix} :title "Mix" :icon blend :group "3_color" :order 1}
   :multiply {:run addShaderNode :with {:op multiply} :title "Multiply" :icon x :group "3_color" :order 2}
   :add {:run addShaderNode :with {:op add} :title "Add" :icon plus :group "3_color" :order 3}
   :scale {:run addShaderNode :with {:op scale} :title "Scale" :icon scaling :group "3_color" :order 4}
   :invert {:run addShaderNode :with {:op invert} :title "Invert" :icon contrast :group "4_converter" :order 1}})

(defmenu addMesh
  {:plane {:run addObject :with {:kind mesh :mesh plane :name "Plane"} :title "Plane" :icon square :order 1}
   :cube {:run addObject :with {:kind mesh :mesh cube :name "Cube"} :title "Cube" :icon box :order 2}
   :sphere {:run addObject :with {:kind mesh :mesh sphere :name "Sphere"} :title "UV Sphere" :icon globe :order 3}
   :cylinder {:run addObject
              :with {:kind mesh :mesh cylinder :name "Cylinder"}
              :title "Cylinder"
              :icon cylinder
              :order 4}
   :cone {:run addObject :with {:kind mesh :mesh cone :name "Cone"} :title "Cone" :icon cone :order 5}
   :torus {:run addObject :with {:kind mesh :mesh torus :name "Torus"} :title "Torus" :icon torus :order 6}})

(defmenu addLight
  {:point {:run addObject
           :with {:kind light :name "Point" :light (point :intensity 60)}
           :title "Point"
           :icon lightbulb
           :order 1}
   :sun {:run addObject
         :with {:kind light :name "Sun" :light (directional :intensity 3)}
         :title "Sun"
         :icon sun
         :order 2}
   :spot {:run addObject
          :with {:kind light :name "Spot" :light (spot :intensity 60)}
          :title "Spot"
          :icon cone
          :order 3}})

(defmenu object
  {:transform {:submenu objectTransform :title "Transform" :group "1_transform" :order 1}
   :snap {:submenu snap :title "Snap" :group "1_transform" :order 2}
   :duplicate {:run duplicate :group "2_edit" :order 1}
   :delete {:run deleteSelected :group "2_edit" :order 2}
   :keyframe {:run insertKeyframe :group "3_anim"}
   :show {:submenu objectShowHide :title "Show/Hide" :group "4_show"}})

(defmenu objectTransform
  {:move {:run grab :order 1}
   :rotate {:run rotate :order 2}
   :scale {:run resize :order 3}
   :clearLocation {:run clearLocation :group "2_clear" :order 1}
   :clearRotation {:run clearRotation :group "2_clear" :order 2}
   :clearScale {:run clearScale :group "2_clear" :order 3}})

(defmenu snap
  {:selectionToCursor {:run selectionToCursor :group "1_selection"}
   :cursorToSelected {:run cursorToSelected :group "2_cursor" :order 1}
   :cursorToActive {:run cursorToActive :group "2_cursor" :order 2}
   :cursorToOrigin {:run cursorToOrigin :group "2_cursor" :order 3}})

(defmenu objectShowHide
  {:reveal {:run revealHidden :order 1} :hide {:run hideSelected :order 2} :hideOthers {:run hideUnselected :order 3}})

(defmenu objectContext
  {:move {:run grab :group "1_transform" :order 1}
   :rotate {:run rotate :group "1_transform" :order 2}
   :scale {:run resize :group "1_transform" :order 3}
   :cursor {:run frameSelected :group "2_view"}
   :duplicate {:run duplicate :group "3_edit" :order 1}
   :hide {:run hideSelected :group "3_edit" :order 2}
   :collection {:run moveToCollectionMenu :group "3_edit" :order 3}
   :keyframe {:run insertKeyframe :group "4_anim"}
   :delete {:run deleteSelected :group "5_delete"}})

(defmenu collectionContext
  {:rename {:run startCollectionRename :group "1_name"} :delete {:run deleteCollection :group "2_delete"}})

(defmenu outlinerContext
  {:rename {:run startRename :group "1_name"}
   :frame {:run frameSelected :group "2_view"}
   :duplicate {:run duplicate :group "3_edit" :order 1}
   :hide {:run hideSelected :group "3_edit" :order 2}
   :delete {:run deleteSelected :group "4_delete"}})

(defmenu delete {:delete {:run deleteSelected :title "Delete" :group "1"}})

(defmenu shading
  {:wireframe {:run setOption :with {:key shading :value wireframe} :title "Wireframe" :icon globe :order 1}
   :solid {:run setOption :with {:key shading :value solid} :title "Solid" :icon circle :order 2}
   :material {:run setOption
              :with {:key shading :value material}
              :title "Material Preview"
              :icon circle-dot
              :order 3}
   :rendered {:run setOption :with {:key shading :value rendered} :title "Rendered" :icon sun :order 4}})

(defcomputed searchItems
  (map (fn [it] {:title it.title :detail (or-else it.keyLabel it.category) :icon it.icon :run it.name})
         (filter (fn [it] (and it.enabled (not= it.name "search"))) commands)))

(defcomputed pieItems
  (cond (= state.pie.menu "shading") [{:title "Wireframe"
            :icon "globe"
            :run "setOption"
            :with {:key "shading" :value "wireframe"}
            :checked (= state.shading "wireframe")}
           {:title "Rendered"
            :icon "sun"
            :run "setOption"
            :with {:key "shading" :value "rendered"}
            :checked (= state.shading "rendered")}
           {:title "Solid"
            :icon "circle"
            :run "setOption"
            :with {:key "shading" :value "solid"}
            :checked (= state.shading "solid")}
           {:title "Material Preview"
            :icon "circle-dot"
            :run "setOption"
            :with {:key "shading" :value "material"}
            :checked (= state.shading "material")}
           {:title "Toggle X-Ray" :icon "square-dashed-bottom" :run "toggleXray" :checked state.xray}]
          (= state.pie.menu "view") [{:title "Left" :run "viewLeft"}
           {:title "Right" :run "viewRight"}
           {:title "Bottom" :run "viewBottom"}
           {:title "Top" :run "viewTop"}
           {:title "Front" :run "viewFront"}
           {:title "Back" :run "viewBack"}
           {:title "Camera" :icon "video" :run "viewCamera" :enabled (some? computed.cameraNode)}
           {:title "View Selected" :icon "scan" :run "frameSelected" :enabled (> (count state.view.selection) 0)}]
          :else []))

(defcomputed modeItems
  [{:title "Object Mode" :icon "box" :checked (= state.mode "object") :run "setMode" :with {:mode "object"}}
     {:title "Edit Mode" :icon "pen-tool" :enabled false :keyLabel "Not yet"}])

(defcomputed orientationItems
  (map (fn [it]
           (if (contains ["Gimbal" "View"] it)
               {:title it :enabled false :keyLabel "Not yet"}
               {:title it
                :checked (= state.orientation (lower-case it))
                :run "setOption"
                :with {:key "orientation" :value (lower-case it)}}))
         ["Global" "Local" "Normal" "Gimbal" "View" "Cursor"]))

(defcomputed pivotItems
  (map (fn [it]
           (if (= it.0 "individual")
               {:title it.1 :enabled false :keyLabel "Not yet"}
               {:title it.1 :checked (= state.pivot it.0) :run "setOption" :with {:key "pivot" :value it.0}}))
         [["bounds" "Bounding Box Center"]
          ["cursor" "3D Cursor"]
          ["individual" "Individual Origins"]
          ["median" "Median Point"]
          ["active" "Active Element"]]))

(defcomputed snapItems
  (concat (map (fn [it] {:title it :enabled false :keyLabel "Not yet"}) ["Vertex" "Edge" "Face"])
            [{:separator true}]
            (map (fn [it]
                   {:title it.1
                    :checked (= (or-else state.snapStep "unit") it.0)
                    :run "setOption"
                    :with {:key "snapStep" :value it.0}})
                 [["fine" "Increment 0.1 m, 1°"] ["unit" "Increment 1 m, 5°"] ["coarse" "Increment 10 m, 15°"]])))

(defcomputed kinds [["mesh" "Meshes"] ["light" "Lights"] ["camera" "Cameras"] ["empty" "Empties"]])

(defcomputed visibilityItems
  (concat (map (fn [it]
                   {:title (str it.1 " Visible")
                    :checked (not (contains state.hiddenKinds it.0))
                    :run "toggleKind"
                    :with {:which "hiddenKinds" :kind it.0}})
                 computed.kinds)
            [{:separator true}]
            (map (fn [it]
                   {:title (str it.1 " Selectable")
                    :checked (not (contains state.unselectableKinds it.0))
                    :run "toggleKind"
                    :with {:which "unselectableKinds" :kind it.0}})
                 computed.kinds)))

(defcomputed gizmoItems
  [{:title "Navigate" :checked (not= state.navGizmo false) :run "toggleOption" :with {:key "navGizmo"}}
     {:title "Active Tools"
      :checked (not= state.toolGizmos false)
      :run "toggleOption"
      :with {:key "toolGizmos"}}
     {:separator true}
     {:title "Object Gizmo: Move"
      :checked (= state.objectGizmo "translate")
      :run "setObjectGizmo"
      :with {:mode "translate"}}
     {:title "Object Gizmo: Rotate"
      :checked (= state.objectGizmo "rotate")
      :run "setObjectGizmo"
      :with {:mode "rotate"}}
     {:title "Object Gizmo: Scale"
      :checked (= state.objectGizmo "scale")
      :run "setObjectGizmo"
      :with {:mode "scale"}}])

(defcomputed overlayItems
  (map (fn [it]
           {:title it.1 :checked (= (get state.overlay it.0) true) :run "toggleOverlay" :with {:key it.0}})
         [["grid" "Grid"]
          ["axes" "Floor Axes"]
          ["cursor" "3D Cursor"]
          ["text" "Text Info"]
          ["stats" "Statistics"]
          ["extras" "Extras (Lights, Cameras)"]]))

(defcomputed shadingItems
  [{:title "Scene Lights"
      :checked (= state.sceneLights true)
      :run "toggleOption"
      :with {:key "sceneLights"}
      :keyLabel "Material Preview"}
     {:title "Scene World"
      :checked (= state.sceneWorld true)
      :run "toggleOption"
      :with {:key "sceneWorld"}
      :keyLabel "Material Preview"}
     {:separator true}
     {:title "X-Ray" :checked (= state.xray true) :run "toggleXray" :keyLabel "Alt Z"}])

(defcomputed shadingModes
  [{:id "wireframe" :icon "globe" :title "Wireframe"}
     {:id "solid" :icon "circle" :title "Solid"}
     {:id "material" :icon "circle-dot" :title "Material Preview"}
     {:id "rendered" :icon "sun" :title "Rendered"}])

(defcomputed editorTypes
  (map (fn [it]
           {:id it.0
            :title it.1
            :icon it.2
            :enabled (contains ["viewport" "shader" "timeline" "outliner" "properties"] it.0)
            :keyLabel (if (contains ["viewport" "shader" "timeline" "outliner" "properties"] it.0) "" "Not yet")})
         [["viewport" "3D Viewport" "axis-3-d"]
          ["image" "Image Editor" "image"]
          ["shader" "Shader Editor" "git-fork"]
          ["timeline" "Timeline" "clock-3"]
          ["outliner" "Outliner" "list-tree"]
          ["properties" "Properties" "sliders-horizontal"]
          ["text" "Text Editor" "file-text"]]))

(defcomputed workspaces
  [{:id "layout" :title "Layout"}
     {:id "modeling" :title "Model"}
     {:id "sculpting" :title "Sculpt" :needs "sculpting a mesh"}
     {:id "uv editing" :title "UV" :needs "unwrapping a mesh"}
     {:id "texture paint" :title "Paint" :needs "painting textures"}
     {:id "shading" :title "Materials"}
     {:id "animation" :title "Animate"}
     {:id "rendering" :title "Render"}
     {:id "compositing" :title "Compose" :needs "a compositor"}
     {:id "geometry nodes" :title "Nodes" :needs "geometry nodes"}
     {:id "scripting" :title "Script" :needs "a script console"}])

(defcomputed timelineMenus
  {:playback [{:title "Play Animation" :keyLabel "Space" :checked (= state.playing "forward") :run "play"}
                {:title "Play in Reverse"
                 :keyLabel "Shift Ctrl Space"
                 :checked (= state.playing "reverse")
                 :run "playReverse"}
                {:separator true}
                {:title "Jump to Start" :keyLabel "Shift Left" :run "jumpStart"}
                {:title "Jump to End" :keyLabel "Shift Right" :run "jumpEnd"}
                {:title "Previous Keyframe" :keyLabel "Down" :run "prevKey"}
                {:title "Next Keyframe" :keyLabel "Up" :run "nextKey"}
                {:separator true}
                {:title "Use Preview Range" :checked (some? state.previewRange) :run "togglePreviewRange"}]
     :keying [{:title "Insert Keyframe"
               :keyLabel "I"
               :enabled (> (count state.view.selection) 0)
               :run "insertKeyframe"}
              {:title "Delete Keyframes"
               :keyLabel "Alt I"
               :enabled (contains computed.keyFrames state.frame)
               :run "deleteKeyframe"}
              {:title "Clear Keyframes"
               :keyLabel "Shift Alt I"
               :enabled (> (count computed.keyFrames) 0)
               :run "clearKeyframes"}
              {:separator true}
              {:title "Auto Keying"
               :checked (= state.autoKey true)
               :run "toggleOption"
               :with {:key "autoKey"}}]
     :view [{:title "Show Seconds"
             :checked (= state.timeSeconds true)
             :run "toggleOption"
             :with {:key "timeSeconds"}}
            {:title "Show Markers"
             :checked (not= state.hideMarkers true)
             :run "toggleOption"
             :with {:key "hideMarkers"}}
            {:separator true}
            {:title "Go to Current Frame" :run "frameCurrent"}]
     :marker [{:title "Add Marker" :run "addMarker"}
              {:title "Delete Marker"
               :enabled (some (fn [it] (= it.frame state.frame)) state.markers)
               :run "deleteMarker"}
              {:separator true}
              {:title "Jump to Previous Marker"
               :enabled (some (fn [it] (< it.frame state.frame)) state.markers)
               :run "prevMarker"}
              {:title "Jump to Next Marker"
               :enabled (some (fn [it] (> it.frame state.frame)) state.markers)
               :run "nextMarker"}]})

(defcomputed outlinerDisplayItems
  [{:title "View Layer"
      :icon "layers-2"
      :checked (not= state.outlinerMode "list")
      :run "setOption"
      :with {:key "outlinerMode" :value "layer"}}
     {:title "Object List"
      :icon "list"
      :checked (= state.outlinerMode "list")
      :run "setOption"
      :with {:key "outlinerMode" :value "list"}}
     {:separator true}
     {:title "Scenes" :icon "shapes" :enabled false :keyLabel "Not yet"}
     {:title "Orphan Data" :icon "trash-2" :enabled false :keyLabel "Not yet"}])

(defcomputed outlinerFilterItems
  (concat (map (fn [it]
                   {:title it.1
                    :checked (not (contains state.outlinerHide it.0))
                    :run "toggleKind"
                    :with {:which "outlinerHide" :kind it.0}})
                 [["mesh" "Meshes"] ["light" "Lights"] ["camera" "Cameras"] ["empty" "Empties"]])
            [{:separator true}
             {:title "Only Selected"
              :checked (= state.outlinerSelectedOnly true)
              :run "toggleOption"
              :with {:key "outlinerSelectedOnly"}}]))

(defcomputed shaderAddItems
  (map (fn [it]
           {:id it.0
            :title (get-in computed.shaderOps [it.0 :title])
            :icon it.2
            :group it.1
            :enabled (>= computed.activeMaterialIndex 0)
            :run "addShaderNode"
            :with {:op it.0}})
         [["color" "Input" "palette"]
          ["number" "Input" "hash"]
          ["fresnel" "Input" "aperture"]
          ["height" "Input" "arrow-up-down"]
          ["checker" "Texture" "grid-3x3"]
          ["noise" "Texture" "waves"]
          ["mix" "Color" "blend"]
          ["multiply" "Color" "x"]
          ["add" "Color" "plus"]
          ["scale" "Color" "scaling"]
          ["invert" "Converter" "contrast"]]))

