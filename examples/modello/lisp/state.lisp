;;; state.lisp --- what Modello keeps while it runs, and where each value starts
;;;
;;; Each defstate is one key of the state, with the value it starts from. The start scene has a camera, a cube and a
;;; light; onLoad keeps it, for New.

;;; The window and the editors

(defstate theme dark "dark or light.")

(defstate workspace "Layout" "The workspace shown, by its title.")

(defstate menuBar {} "Which of the top bar's menus is open.")

(defstate open {} "Which dropdowns of the headers are open.")

(defstate sizes {:main [1102] :left [720] :right [262]}
  "The splits' sizes: the main one, the left column's and the right column's.")

(defstate areas {:a viewport :b timeline :c outliner :d properties}
  "The editor each area shows: a and b on the left, c and d on the right.")

(defstate maximized nil "The area that fills the window, or nil.")

(defstate hoverSlot nil "The area under the pointer: a, b, c or d.")

(defstate ctx {:open false :menu "" :items nil :x 0 :y 0} "The menu that popped up: open, which, its items and where.")

(defstate pie {:open false :menu "" :x 0 :y 0} "The pie menu: open, which, and where.")

(defstate about false "Whether the About dialog is shown.")

(defstate quickPick {:open false :query ""} "F3's search: open, and what is typed.")

(defstate rendering false "Whether the render window is shown.")

(defstate sceneName "Scene" "The scene's name, in the top bar.")

(defstate layerName "Main Layer" "The view layer's name, in the top bar.")

;;; The scene and the selection

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
   :collections [{:id collection :name "Collection"}]}
  "The scene: its objects (nodes), materials and collections.")

(defstate view {:camera {:target [0 0 0] :yaw 0.7 :pitch 0.58 :distance 17.5 :fieldOfView 0.68} :selection [cube]}
  "The viewport's camera and the selected objects' ids.")

(defstate active cube "The active object's id: the last one picked.")

(defstate mode object "The mode: object.")

(defstate tool select "The toolbar's tool: select, cursor, move, rotate, scale, measure or add.")

(defstate added 0 "How many objects have been added: the number in a new id.")

(defstate copied [] "The objects Copy kept, for Paste.")

(defstate renaming nil "The id of what the outliner is renaming (coll: before a collection's), or nil.")

(defstate activeCollection collection "The collection new objects go into.")

;;; Moving, the 3D cursor and snapping

(defstate cursor [0 0 0] "The 3D cursor's place.")

(defstate pivot median "What objects turn and scale about: median, bounds, cursor or active.")

(defstate orientation global "The transform's axes: global, local, normal or cursor.")

(defstate snap false "Whether moving snaps to increments.")

(defstate snapStep unit "The increments: fine, unit or coarse.")

(defstate modal nil "The transform running: its mode, the axis locked and the amount typed; nil when none runs.")

(defstate modalText "" "What the transform running says.")

(defstate measure {:points [] :screen [] :camera nil}
  "The Measure tool's points, where they are on the screen, and the camera they were picked with.")

(defstate lastOp nil "The last operation, for Adjust Last Operation: its title, kind, objects before it and amounts.")

(defstate lastBase [] "The objects of the last transform as they were before it.")

(defstate redoOpen true "Whether the Adjust Last Operation panel is open.")

;;; Undo, and what happened

(defstate undoStack [] "The scenes Undo goes back to.")

(defstate redoStack [] "The scenes Redo brings back.")

(defstate last "" "What the last operation was, for the status bar.")

(defstate fileStatus "" "What the last file operation did.")

(defstate opened nil "A file read by Open, before it is checked.")

(defstate openedName nil "That file's name.")

;;; Shading, overlays and gizmos

(defstate shading solid "The viewport's shading: wireframe, solid, material or rendered.")

(defstate sceneLights false "Whether Material Preview lights the scene with its own lights.")

(defstate sceneWorld false "Whether Material Preview shows the scene's world.")

(defstate world {:color "#3a3f47"} "The world's colour.")

(defstate render {:exposure 1} "The render's exposure.")

(defstate xray false "Whether X-Ray is on.")

(defstate overlays true "Whether the overlays are shown.")

(defstate overlay {:grid true :axes true :cursor true :text true :stats false :extras true}
  "Each overlay, shown or not.")

(defstate gizmos true "Whether the gizmos are shown.")

(defstate navGizmo true "Whether the navigation gizmo is shown.")

(defstate toolGizmos true "Whether the active tool's gizmo is shown.")

(defstate objectGizmo nil "The gizmo the Select tool shows on the selection, or nil.")

(defstate hiddenKinds [] "The kinds of objects not shown.")

(defstate unselectableKinds [] "The kinds of objects that cannot be picked.")

;;; The outliner, the properties and the sidebar

(defstate outlinerMode layer "The outliner's display: layer or list.")

(defstate outlinerHide [] "The kinds of objects the outliner leaves out.")

(defstate outlinerSelectedOnly false "Whether the outliner lists only the selected objects.")

(defstate outlinerOpen {:root true :collection true} "Which of the outliner's rows are open.")

(defstate propertiesFilter "" "The properties editor's search.")

(defstate propTab object "The properties editor's tab.")

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
   :surfaceMaterial true}
  "Each of the properties' panels, open or not.")

(defstate sidebar false "Whether the viewport's sidebar is shown.")

(defstate sidebarTab "Item" "The sidebar's tab.")

;;; Animation

(defstate frame 1 "The current frame.")

(defstate start 1 "The scene's first frame.")

(defstate end 250 "The scene's last frame.")

(defstate playing false "forward, reverse, or false when the animation is stopped.")

(defstate autoKey false "Whether changes are keyed as they are made.")

(defstate previewRange nil "The preview range {start, end} the timeline plays, or nil.")

(defstate markers [] "The timeline's markers.")

(defstate timelineOffset -0.4 "Where the timeline is scrolled to, in seconds.")

;;; The Shader Editor

(defstate shaderSelected [] "The Shader Editor's selected nodes.")

;;; Starting

(defpart start "The scene Modello opens with, kept for New." :onLoad {:set state.startScene :to ~state.scene})
