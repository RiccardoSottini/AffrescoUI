;;; menus.lisp --- Modello's menus: the top bar's, the viewport header's, the context menus, the pie menus and the
;;; lists the headers' dropdowns show
;;;
;;; A menu (defmenu) lists commands by name: an item takes its title, icon and key from the command, and `:group` and
;;; `:order` place it. The headers' dropdowns are lists the running app works out (defcomputed): an item names a
;;; function to run (`:run`) with its arguments (`:with`), and `:checked` or `:enabled` from the state.

;;; The top bar

(defmenu file "The File menu."
  {:new {:run newScene :group "1_new" :order 1}
   :open {:run openScene :group "1_new" :order 2}
   :save {:run saveScene :group "2_save"}
   :export {:run exportGltf :group "3_export"}})

(defmenu edit "The Edit menu."
  {:undo {:run undo :group "1_history" :order 1}
   :redo {:run redo :group "1_history" :order 2}
   :adjust {:run adjustLastPanel :group "1_history" :order 3}
   :copy {:run copyObjects :group "2_clipboard" :order 1}
   :paste {:run pasteObjects :group "2_clipboard" :order 2}
   :search {:run search :group "3_search"}})

(defmenu render "The Render menu."
  {:image {:run renderImage :group "1_render"}
   :camera {:run viewCamera :title ~(t "menu.viewCamera") :group "2_view"}})

(defmenu window "The Window menu."
  {:next {:run nextWorkspace :group "0_workspace" :order 1}
   :previous {:run previousWorkspace :group "0_workspace" :order 2}
   :theme {:run toggleTheme :group "1_theme"}
   :sidebar {:run toggleSidebar :group "2_regions" :order 1}
   :toolbar {:run toggleToolbar :group "2_regions" :order 2}
   :assistant {:run assistant.toggle :title ~(t "menu.assistant") :group "3_assistant"}})

(defmenu help "The Help menu." {:about {:run about :group "1_help"}})

;;; The viewport's header

(defmenu view "The viewport's View menu."
  {:toolbar {:run toggleToolbar :group "1_regions" :order 1}
   :sidebar {:run toggleSidebar :group "1_regions" :order 2}
   :camera {:run viewCamera :group "2_frame" :order 1}
   :projection {:run toggleProjection :group "2_frame" :order 2}
   :frameSelected {:run frameSelected :group "3_frame" :order 1}
   :frameAll {:run frameAll :group "3_frame" :order 2}
   :viewpoint {:submenu viewpoint :title ~(t "menu.viewpoint") :group "4_view"}
   :maximize {:run toggleMaximize :group "5_area"}})

(defmenu viewpoint "The View menu's Viewpoint: the six views along the axes."
  {:top {:run viewTop :group "1" :order 1}
   :bottom {:run viewBottom :group "1" :order 2}
   :front {:run viewFront :group "2" :order 1}
   :back {:run viewBack :group "2" :order 2}
   :right {:run viewRight :group "3" :order 1}
   :left {:run viewLeft :group "3" :order 2}})

(defmenu select "The viewport's Select menu."
  {:all {:run selectAll :title ~(t "menu.all") :group "1_all" :order 1}
   :none {:run selectNone :title ~(t "menu.none") :group "1_all" :order 2}
   :invert {:run selectInvert :title ~(t "menu.invert") :group "1_all" :order 3}})

(defmenu add "The Add menu (Shift A): meshes, lights, a camera, an empty."
  {:mesh {:submenu addMesh :title ~(t "menu.mesh") :icon triangle :group "1_add" :order 1}
   :light {:submenu addLight :title ~(t "menu.light") :icon lightbulb :group "2_add" :order 1}
   :camera {:run addObject :with {:kind camera :name "Camera"} :title ~(t "menu.camera") :icon video :group "2_add" :order 2}
   :empty {:run addObject :with {:kind empty :name "Empty"} :title ~(t "menu.empty") :icon axis-3-d :group "2_add" :order 3}})

(defmenu addMesh "The Add menu's meshes."
  {:plane {:run addObject :with {:kind mesh :mesh plane :name "Plane"} :title ~(t "mesh.plane") :icon square :order 1}
   :cube {:run addObject :with {:kind mesh :mesh cube :name "Cube"} :title ~(t "mesh.cube") :icon box :order 2}
   :sphere {:run addObject :with {:kind mesh :mesh sphere :name "Sphere"} :title ~(t "mesh.sphere") :icon globe :order 3}
   :cylinder {:run addObject
              :with {:kind mesh :mesh cylinder :name "Cylinder"}
              :title ~(t "mesh.cylinder")
              :icon cylinder
              :order 4}
   :cone {:run addObject :with {:kind mesh :mesh cone :name "Cone"} :title ~(t "mesh.cone") :icon cone :order 5}
   :torus {:run addObject :with {:kind mesh :mesh torus :name "Torus"} :title ~(t "mesh.torus") :icon torus :order 6}})

(defmenu addLight "The Add menu's lights."
  {:point {:run addObject
           :with {:kind light :name "Point" :light (point :intensity 60)}
           :title ~(t "light.point")
           :icon lightbulb
           :order 1}
   :sun {:run addObject
         :with {:kind light :name "Sun" :light (directional :intensity 3)}
         :title ~(t "light.sun")
         :icon sun
         :order 2}
   :spot {:run addObject
          :with {:kind light :name "Spot" :light (spot :intensity 60)}
          :title ~(t "light.spot")
          :icon cone
          :order 3}})

(defmenu object "The viewport's Object menu."
  {:transform {:submenu objectTransform :title ~(t "menu.transform") :group "1_transform" :order 1}
   :snap {:submenu snap :title ~(t "menu.snap") :group "1_transform" :order 2}
   :duplicate {:run duplicate :group "2_edit" :order 1}
   :delete {:run deleteSelected :group "2_edit" :order 2}
   :keyframe {:run insertKeyframe :group "3_anim"}
   :show {:submenu objectShowHide :title ~(t "menu.showHide") :group "4_show"}})

(defmenu objectTransform "The Object menu's Transform."
  {:move {:run grab :order 1}
   :rotate {:run rotate :order 2}
   :scale {:run resize :order 3}
   :clearLocation {:run clearLocation :group "2_clear" :order 1}
   :clearRotation {:run clearRotation :group "2_clear" :order 2}
   :clearScale {:run clearScale :group "2_clear" :order 3}})

(defmenu snap "The Snap menu (Shift S): the selection to the 3D cursor, or the cursor to something."
  {:selectionToCursor {:run selectionToCursor :group "1_selection"}
   :cursorToSelected {:run cursorToSelected :group "2_cursor" :order 1}
   :cursorToActive {:run cursorToActive :group "2_cursor" :order 2}
   :cursorToOrigin {:run cursorToOrigin :group "2_cursor" :order 3}})

(defmenu objectShowHide "The Object menu's Show/Hide."
  {:reveal {:run revealHidden :order 1} :hide {:run hideSelected :order 2} :hideOthers {:run hideUnselected :order 3}})

(defmenu shading "The shading modes."
  {:wireframe {:run setOption :with {:key shading :value wireframe} :title ~(t "shading.wireframe") :icon globe :order 1}
   :solid {:run setOption :with {:key shading :value solid} :title ~(t "shading.solid") :icon circle :order 2}
   :material {:run setOption :with {:key shading :value material} :title ~(t "shading.material") :icon circle-dot :order 3}
   :rendered {:run setOption :with {:key shading :value rendered} :title ~(t "shading.rendered") :icon sun :order 4}})

;;; Context menus

(defmenu objectContext "A right click on an object in the viewport."
  {:move {:run grab :group "1_transform" :order 1}
   :rotate {:run rotate :group "1_transform" :order 2}
   :scale {:run resize :group "1_transform" :order 3}
   :cursor {:run frameSelected :group "2_view"}
   :duplicate {:run duplicate :group "3_edit" :order 1}
   :hide {:run hideSelected :group "3_edit" :order 2}
   :collection {:run moveToCollectionMenu :group "3_edit" :order 3}
   :keyframe {:run insertKeyframe :group "4_anim"}
   :delete {:run deleteSelected :group "5_delete"}})

(defmenu collectionContext "A right click on a collection in the outliner."
  {:rename {:run startCollectionRename :group "1_name"} :delete {:run deleteCollection :group "2_delete"}})

(defmenu outlinerContext "A right click on an object in the outliner."
  {:rename {:run startRename :group "1_name"}
   :frame {:run frameSelected :group "2_view"}
   :duplicate {:run duplicate :group "3_edit" :order 1}
   :hide {:run hideSelected :group "3_edit" :order 2}
   :delete {:run deleteSelected :group "4_delete"}})

(defmenu delete "The Delete menu (X)." {:delete {:run deleteSelected :title ~(t "menu.delete") :group "1"}})

(defmenu shaderAdd "The Shader Editor's Add menu (Shift A over it)."
  {:color {:run addShaderNode :with {:op color} :title ~(t "shader.color") :icon palette :group "1_input" :order 1}
   :number {:run addShaderNode :with {:op number} :title ~(t "shader.number") :icon hash :group "1_input" :order 2}
   :fresnel {:run addShaderNode :with {:op fresnel} :title ~(t "shader.fresnel") :icon aperture :group "1_input" :order 3}
   :height {:run addShaderNode :with {:op height} :title ~(t "shader.height") :icon arrow-up-down :group "1_input" :order 4}
   :checker {:run addShaderNode :with {:op checker} :title ~(t "shader.checker") :icon grid-3x3 :group "2_texture" :order 1}
   :noise {:run addShaderNode :with {:op noise} :title ~(t "shader.noise") :icon waves :group "2_texture" :order 2}
   :mix {:run addShaderNode :with {:op mix} :title ~(t "shader.mix") :icon blend :group "3_color" :order 1}
   :multiply {:run addShaderNode :with {:op multiply} :title ~(t "shader.multiply") :icon x :group "3_color" :order 2}
   :add {:run addShaderNode :with {:op add} :title ~(t "shader.add") :icon plus :group "3_color" :order 3}
   :scale {:run addShaderNode :with {:op scale} :title ~(t "shader.scale") :icon scaling :group "3_color" :order 4}
   :invert {:run addShaderNode :with {:op invert} :title ~(t "shader.invert") :icon contrast :group "4_converter" :order 1}})

;;; The search and the pie menus

(defcomputed searchItems "F3's list: every command that can run now, but the search itself."
  (map (fn [it] {:title it.title :detail (or-else it.keyLabel it.category) :icon it.icon :run it.name})
       (filter (fn [it] (and it.enabled (not= it.name "search"))) commands)))

(defcomputed pieItems "The pie menu's items: Z's shading modes, or the grave key's views."
  (cond (= state.pie.menu "shading")
          [{:title (t "shading.wireframe") :icon "globe" :run "setOption" :with {:key "shading" :value "wireframe"}
            :checked (= state.shading "wireframe")}
           {:title (t "shading.rendered") :icon "sun" :run "setOption" :with {:key "shading" :value "rendered"}
            :checked (= state.shading "rendered")}
           {:title (t "shading.solid") :icon "circle" :run "setOption" :with {:key "shading" :value "solid"}
            :checked (= state.shading "solid")}
           {:title (t "shading.material") :icon "circle-dot" :run "setOption" :with {:key "shading" :value "material"}
            :checked (= state.shading "material")}
           {:title (t "command.toggleXray") :icon "square-dashed-bottom" :run "toggleXray" :checked state.xray}]
        (= state.pie.menu "view")
          [{:title (t "command.viewLeft") :run "viewLeft"}
           {:title (t "command.viewRight") :run "viewRight"}
           {:title (t "command.viewBottom") :run "viewBottom"}
           {:title (t "command.viewTop") :run "viewTop"}
           {:title (t "command.viewFront") :run "viewFront"}
           {:title (t "command.viewBack") :run "viewBack"}
           {:title (t "menu.camera") :icon "video" :run "viewCamera" :enabled (some? computed.cameraNode)}
           {:title (t "menu.viewSelected") :icon "scan" :run "frameSelected" :enabled (> (count state.view.selection) 0)}]
        :else []))

;;; The viewport header's dropdowns

(defcomputed modeItems "The mode's dropdown: Object Mode, and Edit Mode to come."
  [{:title (t "mode.object") :icon "box" :checked (= state.mode "object") :run "setMode" :with {:mode "object"}}
   {:title (t "mode.edit") :icon "pen-tool" :enabled false :keyLabel (t "menu.notYet")}])

(defcomputed orientationItems "The transform orientations; Gimbal and View are to come."
  (map (fn [it]
         (if (contains ["gimbal" "view"] it)
           {:title (t (str "orientation." it)) :enabled false :keyLabel (t "menu.notYet")}
           {:title (t (str "orientation." it))
            :checked (= state.orientation it)
            :run "setOption"
            :with {:key "orientation" :value it}}))
       ["global" "local" "normal" "gimbal" "view" "cursor"]))

(defcomputed pivotItems "The pivot points; Individual Origins is to come."
  (map (fn [it]
         (if (= it "individual")
           {:title (t (str "pivot." it)) :enabled false :keyLabel (t "menu.notYet")}
           {:title (t (str "pivot." it)) :checked (= state.pivot it) :run "setOption" :with {:key "pivot" :value it}}))
       ["bounds" "cursor" "individual" "median" "active"]))

(defcomputed snapItems "What snapping snaps to (vertices, edges and faces are to come), and its increments."
  (concat (map (fn [it] {:title (t (str "snap." it)) :enabled false :keyLabel (t "menu.notYet")}) ["vertex" "edge" "face"])
          [{:separator true}]
          (map (fn [it]
                 {:title (t (str "snap." it))
                  :checked (= (or-else state.snapStep "unit") it)
                  :run "setOption"
                  :with {:key "snapStep" :value it}})
               ["fine" "unit" "coarse"])))

(defcomputed kinds "The kinds of objects, with the key of their name." [["mesh" "kind.meshes"] ["light" "kind.lights"] ["camera" "kind.cameras"] ["empty" "kind.empties"]])

(defcomputed visibilityItems "Which kinds of objects are shown, and which can be picked."
  (concat (map (fn [it]
                 {:title (t "kind.visible" {:kind (t it.1)})
                  :checked (not (contains state.hiddenKinds it.0))
                  :run "toggleKind"
                  :with {:which "hiddenKinds" :kind it.0}})
               computed.kinds)
          [{:separator true}]
          (map (fn [it]
                 {:title (t "kind.selectable" {:kind (t it.1)})
                  :checked (not (contains state.unselectableKinds it.0))
                  :run "toggleKind"
                  :with {:which "unselectableKinds" :kind it.0}})
               computed.kinds)))

(defcomputed gizmoItems "The gizmos shown: the navigation, the active tool's, and one on the selection for the Select tool."
  [{:title (t "gizmo.navigate") :checked (not= state.navGizmo false) :run "toggleOption" :with {:key "navGizmo"}}
   {:title (t "gizmo.tools") :checked (not= state.toolGizmos false) :run "toggleOption" :with {:key "toolGizmos"}}
   {:separator true}
   {:title (t "gizmo.move") :checked (= state.objectGizmo "translate") :run "setObjectGizmo" :with {:mode "translate"}}
   {:title (t "gizmo.rotate") :checked (= state.objectGizmo "rotate") :run "setObjectGizmo" :with {:mode "rotate"}}
   {:title (t "gizmo.scale") :checked (= state.objectGizmo "scale") :run "setObjectGizmo" :with {:mode "scale"}}])

(defcomputed overlayItems "The overlays shown over the scene."
  (map (fn [it] {:title (t (str "overlay." it)) :checked (= (get state.overlay it) true) :run "toggleOverlay" :with {:key it}})
       ["grid" "axes" "cursor" "text" "stats" "extras"]))

(defcomputed shadingItems "The shading's options: the scene's lights and world in Material Preview, and X-Ray."
  [{:title (t "shading.sceneLights")
    :checked (= state.sceneLights true)
    :run "toggleOption"
    :with {:key "sceneLights"}
    :keyLabel (t "shading.material")}
   {:title (t "shading.sceneWorld")
    :checked (= state.sceneWorld true)
    :run "toggleOption"
    :with {:key "sceneWorld"}
    :keyLabel (t "shading.material")}
   {:separator true}
   {:title (t "shading.xray") :checked (= state.xray true) :run "toggleXray" :keyLabel "Alt Z"}])

(defcomputed shadingModes "The shading modes, as the header's four buttons show them."
  [{:id "wireframe" :icon "globe" :title (t "shading.wireframe")}
   {:id "solid" :icon "circle" :title (t "shading.solid")}
   {:id "material" :icon "circle-dot" :title (t "shading.material")}
   {:id "rendered" :icon "sun" :title (t "shading.rendered")}])

;;; Editors and workspaces

(defcomputed editorTypes "The editors an area can show; the image and text editors are to come."
  (map (fn [it]
         {:id it.0
          :title (t (str "editor." it.0))
          :icon it.1
          :enabled (contains ["viewport" "shader" "timeline" "outliner" "properties"] it.0)
          :keyLabel (if (contains ["viewport" "shader" "timeline" "outliner" "properties"] it.0) "" (t "menu.notYet"))})
       [["viewport" "axis-3-d"]
        ["image" "image"]
        ["shader" "git-fork"]
        ["timeline" "clock-3"]
        ["outliner" "list-tree"]
        ["properties" "sliders-horizontal"]
        ["text" "file-text"]]))

(defcomputed workspaces
  "The top bar's workspaces: name is what state.workspace holds; a workspace with needs says what it waits for."
  [{:id "layout" :name "Layout" :title (t "workspace.layout")}
   {:id "modeling" :name "Model" :title (t "workspace.modeling")}
   {:id "sculpting" :name "Sculpt" :title (t "workspace.sculpting") :needs (t "workspace.sculpting.needs")}
   {:id "uv editing" :name "UV" :title (t "workspace.uv") :needs (t "workspace.uv.needs")}
   {:id "texture paint" :name "Paint" :title (t "workspace.paint") :needs (t "workspace.paint.needs")}
   {:id "shading" :name "Materials" :title (t "workspace.shading")}
   {:id "animation" :name "Animate" :title (t "workspace.animation")}
   {:id "rendering" :name "Render" :title (t "workspace.rendering")}
   {:id "compositing" :name "Compose" :title (t "workspace.compositing") :needs (t "workspace.compositing.needs")}
   {:id "geometry nodes" :name "Nodes" :title (t "workspace.nodes") :needs (t "workspace.nodes.needs")}
   {:id "scripting" :name "Script" :title (t "workspace.scripting") :needs (t "workspace.scripting.needs")}])

;;; The timeline's and the outliner's menus

(defcomputed timelineMenus "The timeline header's menus: Playback, Keying, View and Marker."
  {:playback [{:title (t "command.play") :keyLabel "Space" :checked (= state.playing "forward") :run "play"}
              {:title (t "timeline.playReverse") :keyLabel "Shift Ctrl Space" :checked (= state.playing "reverse") :run "playReverse"}
              {:separator true}
              {:title (t "command.jumpStart") :keyLabel "Shift Left" :run "jumpStart"}
              {:title (t "command.jumpEnd") :keyLabel "Shift Right" :run "jumpEnd"}
              {:title (t "timeline.prevKey") :keyLabel "Down" :run "prevKey"}
              {:title (t "timeline.nextKey") :keyLabel "Up" :run "nextKey"}
              {:separator true}
              {:title (t "command.togglePreviewRange") :checked (some? state.previewRange) :run "togglePreviewRange"}]
   :keying [{:title (t "command.insertKeyframe") :keyLabel "I" :enabled (> (count state.view.selection) 0) :run "insertKeyframe"}
            {:title (t "command.deleteKeyframe")
             :keyLabel "Alt I"
             :enabled (contains computed.keyFrames state.frame)
             :run "deleteKeyframe"}
            {:title (t "command.clearKeyframes")
             :keyLabel "Shift Alt I"
             :enabled (> (count computed.keyFrames) 0)
             :run "clearKeyframes"}
            {:separator true}
            {:title (t "timeline.autoKeying") :checked (= state.autoKey true) :run "toggleOption" :with {:key "autoKey"}}]
   :view [{:title (t "timeline.seconds") :checked (= state.timeSeconds true) :run "toggleOption" :with {:key "timeSeconds"}}
          {:title (t "timeline.markers") :checked (not= state.hideMarkers true) :run "toggleOption" :with {:key "hideMarkers"}}
          {:separator true}
          {:title (t "command.frameCurrent") :run "frameCurrent"}]
   :marker [{:title (t "command.addMarker") :run "addMarker"}
            {:title (t "command.deleteMarker") :enabled (some (fn [it] (= it.frame state.frame)) state.markers) :run "deleteMarker"}
            {:separator true}
            {:title (t "command.prevMarker") :enabled (some (fn [it] (< it.frame state.frame)) state.markers) :run "prevMarker"}
            {:title (t "command.nextMarker") :enabled (some (fn [it] (> it.frame state.frame)) state.markers) :run "nextMarker"}]})

(defcomputed outlinerDisplayItems "The outliner's display modes: the view layer, or a list of objects."
  [{:title (t "outliner.viewLayer")
    :icon "layers-2"
    :checked (not= state.outlinerMode "list")
    :run "setOption"
    :with {:key "outlinerMode" :value "layer"}}
   {:title (t "outliner.objectList")
    :icon "list"
    :checked (= state.outlinerMode "list")
    :run "setOption"
    :with {:key "outlinerMode" :value "list"}}
   {:separator true}
   {:title (t "outliner.scenes") :icon "shapes" :enabled false :keyLabel (t "menu.notYet")}
   {:title (t "outliner.orphans") :icon "trash-2" :enabled false :keyLabel (t "menu.notYet")}])

(defcomputed outlinerFilterItems "The outliner's filter: the kinds of objects listed, or only the selected ones."
  (concat (map (fn [it]
                 {:title (t it.1)
                  :checked (not (contains state.outlinerHide it.0))
                  :run "toggleKind"
                  :with {:which "outlinerHide" :kind it.0}})
               computed.kinds)
          [{:separator true}
           {:title (t "outliner.onlySelected")
            :checked (= state.outlinerSelectedOnly true)
            :run "toggleOption"
            :with {:key "outlinerSelectedOnly"}}]))

(defcomputed shaderAddItems "The Shader Editor's Add dropdown: every operation but the output, by group."
  (map (fn [it]
         {:id it.0
          :title (get-in computed.shaderOps [it.0 :title])
          :icon it.2
          :group (t (str "shader.group." it.1))
          :enabled (>= computed.activeMaterialIndex 0)
          :run "addShaderNode"
          :with {:op it.0}})
       [["color" "input" "palette"]
        ["number" "input" "hash"]
        ["fresnel" "input" "aperture"]
        ["height" "input" "arrow-up-down"]
        ["checker" "texture" "grid-3x3"]
        ["noise" "texture" "waves"]
        ["mix" "color" "blend"]
        ["multiply" "color" "x"]
        ["add" "color" "plus"]
        ["scale" "color" "scaling"]
        ["invert" "converter" "contrast"]]))
