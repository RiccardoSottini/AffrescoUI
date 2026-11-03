;;; state.lisp --- TODO

(defstate theme dark)

(defstate workspace "Layout")

(defstate mode object)

(defstate tool select)

(defstate menuBar {})

(defstate open {})

(defstate view {:camera {:target [0 0 0] :yaw 0.7 :pitch 0.58 :distance 17.5 :fieldOfView 0.68} :selection [cube]})

(defstate active cube)

(defstate scene
  {:nodes [{:id camera
            :name "Camera"
            :kind camera
            :camera {:fieldOfView 0.69}
            :translation [7.36 -6.93 4.96]
            :rotationDegrees [63.6 0 46.7]
            :scale [1 1 1]}
           {:id cube
            :name "Cube"
            :kind mesh
            :mesh cube
            :material material
            :translation [0 0 0]
            :rotationDegrees [0 0 0]
            :scale [1 1 1]}
           {:id light
            :name "Light"
            :kind light
            :light (point :intensity 60)
            :translation [4.08 1.0 5.9]
            :rotationDegrees [37.3 3.2 106.9]
            :scale [1 1 1]}]
   :materials [{:id material :name "Material" :baseColor [0.8 0.8 0.8 1] :roughness 0.5}]
   :collections [{:id collection :name "Collection"}]})

(defstate cursor [0 0 0])

(defstate pivot median)

(defstate orientation global)

(defstate snap false)

(defstate snapStep unit)

(defstate shading solid)

(defstate sceneLights false)

(defstate sceneWorld false)

(defstate world {:color "#3a3f47"})

(defstate render {:exposure 1})

(defstate propertiesFilter "")

(defstate overlays true)

(defstate overlay {:grid true :axes true :cursor true :text true :stats false :extras true})

(defstate gizmos true)

(defstate navGizmo true)

(defstate toolGizmos true)

(defstate objectGizmo nil)

(defstate hiddenKinds [])

(defstate activeCollection collection)

(defstate outlinerMode layer)

(defstate outlinerHide [])

(defstate outlinerSelectedOnly false)

(defstate outlinerOpen {:root true :collection true})

(defstate renaming nil)

(defstate unselectableKinds [])

(defstate measure {:points [] :screen [] :camera nil})

(defstate xray false)

(defstate modal nil)

(defstate modalText "")

(defstate undoStack [])

(defstate redoStack [])

(defstate last "")

(defstate added 0)

(defstate frame 1)

(defstate start 1)

(defstate end 250)

(defstate playing false)

(defstate autoKey false)

(defstate maximized nil)

(defstate hoverSlot nil)

(defstate previewRange nil)

(defstate markers [])

(defstate timelineOffset -0.4)

(defstate propTab object)

(defstate panels
  {:transform true
   :activeTool true
   :viewPanel true
   :cursorPanel true
   :colorManagement true
   :frameRange true
   :scenePanel true
   :surfaceWorld true
   :restrictions true
   :dataPanel true
   :surfaceMaterial true})

(defstate sidebar false)

(defstate sidebarTab "Item")

(defstate ctx {:open false :menu "" :items nil :x 0 :y 0})

(defstate pie {:open false :menu "" :x 0 :y 0})

(defstate about false)

(defstate lastOp nil)

(defstate lastBase [])

(defstate redoOpen true)

(defstate quickPick {:open false :query ""})

(defstate fileStatus "")

(defstate copied [])

(defstate shaderSelected [])

(defstate rendering false)

(defstate opened nil)

(defstate openedName nil)

(defstate sizes {:main [1102] :left [720] :right [262]})

(defstate areas {:a viewport :b timeline :c outliner :d properties})

(defstate sceneName "Scene")

(defstate layerName "Main Layer")

(defpart index "What index adds." :onLoad {:set state.startScene :to ~state.scene})

