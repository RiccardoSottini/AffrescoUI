;;; navigation.lisp --- the view, the editors and the window: turning and framing the view, the options of the
;;; headers, the menus that pop up, the workspaces, and what the window shows
;;;
;;; The viewport turns its own camera (`camera` steps); the rest is the state the headers and menus show.

;;; Menus that pop up

(defn openPopup "Opens a menu of menus.lisp where the pointer is, or in the middle of the viewport when x and y are nil."
  [menu x y]
  (set "state.ctx" {:open true :menu menu :items nil :x (or x computed.centre.x) :y (or y computed.centre.y)}))

(defn openPie "Opens a pie menu (the view's or the shading's) in the middle of the viewport, if the pointer is over it."
  [menu]
  (when (and (nil? state.modal) (= (get state.areas (or state.hoverSlot "a")) "viewport"))
    (set "state.pie" {:open true
                      :menu menu
                      :x (+ nodes.viewport.x (/ nodes.viewport.width 2))
                      :y (+ nodes.viewport.y (/ nodes.viewport.height 2))})))

(defn hovered "The editor under the pointer, or nil." []
  (when (some? state.hoverSlot) (get state.areas state.hoverSlot)))

(defcommand addMenu "Opens the Add menu: objects in the viewport, nodes in the Shader Editor."
  :title ~(t "command.addMenu") :category ~(t "category.object") :icon "plus" []
  (openPopup (if (= (hovered) "shader") "shaderAdd" "add") nil nil))

(defcommand deleteMenu "Opens the Delete menu, when something is selected."
  :title ~(t "command.deleteMenu") :category ~(t "category.object") []
  (when (not (empty? state.view.selection)) (openPopup "delete" nil nil)))

(defcommand search "Opens the search for a command."
  :title ~(t "command.search") :category ~(t "category.edit") :icon "search" []
  (dismiss)
  (set "state.quickPick" {:open true :query ""}))

(defcommand about "Shows what Modello is."
  :title ~(t "command.about") :category ~(t "category.help") :icon "info" []
  (dismiss)
  (set "state.about" true))

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.language") :description ~(t "settings.language.about"))

(defcommand changeLanguage "Switches the language: English, Spanish, French."
  :title ~(t "command.changeLanguage") :category ~(t "category.window") :icon "languages" []
  (set "settings.appearance.language"
       (cond (= settings.appearance.language "en") "es" (= settings.appearance.language "es") "fr" :else "en")))

;;; The headers' options

(defcommand setMode "Changes the mode: Object Mode is the one there is." :title ~(t "command.setMode") :category ~(t "category.object")
  [mode]
  (dismiss)
  (set "state.mode" mode))

(defn setOption "One of the state's options, such as the shading or the pivot, set to a value." [key value]
  (dismiss)
  (set (str "state." key) value))

(defn toggleOption "One of the state's options switched on or off." [key]
  (toggle (str "state." key)))

(defn toggleOverlay "One of the overlays shown or hidden: the grid, the axes, the 3D cursor..." [key]
  (toggle (str "state.overlay." key)))

(defn kinds-in "The kinds a list of the state holds: hiddenKinds, unselectableKinds or outlinerHide." [which]
  (cond (= which "hiddenKinds") state.hiddenKinds
        (= which "unselectableKinds") state.unselectableKinds
        :else state.outlinerHide))

(defn toggleKind "A kind of object (meshes, lights...) taken out of a list of kinds, or put in: hidden, unselectable, unlisted."
  [which kind]
  (let [kinds (kinds-in which)]
    (set (str "state." which) (if (contains kinds kind) (without kinds kind) (concat kinds [kind]))))
  (set "state.view.selection" (filter (fn [id] (contains computed.pickable id)) state.view.selection)))

(defn setObjectGizmo "The gizmo the Select tool shows on the selection: one, or none when it is chosen again." [mode]
  (set "state.objectGizmo" (if (= state.objectGizmo mode) nil mode)))

(defn setTool "A tool of the toolbar chosen; the Measure tool starts afresh." [tool]
  (set "state.tool" tool)
  (set "state.measure" {:points [] :screen [] :camera nil}))

(defcommand toggleXray "Lets the objects behind be seen and picked."
  :title ~(t "command.toggleXray") :category ~(t "category.view") :icon "square-dashed-bottom" []
  (toggle "state.xray"))

(defcommand toggleSnap "Turns snapping on or off."
  :title ~(t "command.toggleSnap") :category ~(t "category.object") :icon "magnet" []
  (toggle "state.snap"))

(defcommand toggleOverlays "Shows or hides every overlay."
  :title ~(t "command.toggleOverlays") :category ~(t "category.view") []
  (toggle "state.overlays"))

(defcommand toggleSidebar "Shows or hides the viewport's sidebar."
  :title ~(t "command.toggleSidebar") :category ~(t "category.view") []
  (toggle "state.sidebar"))

(defcommand toggleToolbar "Shows or hides the viewport's toolbar."
  :title ~(t "command.toggleToolbar") :category ~(t "category.view") []
  (set "state.toolbar" (= state.toolbar false)))

(defcommand toggleTheme "Switches between the dark and the light theme."
  :title ~(t "command.toggleTheme") :category ~(t "category.window") :icon "sun-moon" []
  (set "state.theme" (if (= state.theme "dark") "light" "dark")))

;;; Editors and workspaces

(defn slot-of "The area that shows an editor, or nil." [editor]
  (find (fn [slot] (= (get state.areas slot) editor)) (keys state.areas)))

(defn swapEditor "An area's editor changed: the area that showed the new one takes the old one." [from to]
  (dismiss)
  (let [here (slot-of from)
        there (slot-of to)]
    (when (some? here)
      (set (str "state.areas." here) to)
      (when (some? there) (set (str "state.areas." there) from)))))

;; A workspace sets the editors, the timeline's height, the shading and the properties' tab it is made for.
(defn setWorkspace "A workspace tab chosen: Layout, Model, Materials, Animate or Render." [id name]
  (dismiss)
  (set "state.workspace" name)
  (set "state.areas" {:a "viewport" :b (if (= id "shading") "shader" "timeline") :c "outliner" :d "properties"})
  (set "state.sizes.left" [(cond (= id "animation") 520 (= id "shading") 470 (= id "modeling") 790 :else 720)])
  (set "state.shading" (cond (= id "shading") "material" (= id "rendering") "rendered" :else "solid"))
  (set "state.xray" false)
  (set "state.overlays" (not= id "rendering"))
  (set "state.propTab" (cond (= id "shading") "material" (= id "rendering") "render" :else state.propTab)))

(defn workspace-after "The workspace a step away from the one shown, round the tabs that need nothing." [step]
  (let [tabs (filter (fn [w] (nil? (:needs w))) computed.workspaces)
        next (get tabs (mod (+ (indexOf (map :name tabs) state.workspace) step (count tabs)) (count tabs)))]
    (setWorkspace (:id next) (:name next))))

(defcommand nextWorkspace "Shows the next workspace."
  :title ~(t "command.nextWorkspace") :category ~(t "category.window") :icon "arrow-right" []
  (workspace-after 1))

(defcommand previousWorkspace "Shows the workspace before."
  :title ~(t "command.previousWorkspace") :category ~(t "category.window") :icon "arrow-left" []
  (workspace-after -1))

(defcommand toggleMaximize "Fills the window with the area under the pointer, or puts the areas back."
  :title ~(t "command.toggleMaximize") :category ~(t "category.view") []
  (dismiss)
  (set "state.maximized" (if (some? state.maximized) nil (or state.hoverSlot "a"))))

;;; The view

(defn viewAxis "Looks along an axis: front, back, right, left, top or bottom." [view]
  (set "state.inCamera" false)
  (camera "#viewport" :view view))

(defcommand viewFront "Looks at the scene from the front." :title ~(t "command.viewFront") :category ~(t "category.view") []
  (viewAxis "front"))

(defcommand viewBack "Looks at the scene from the back." :title ~(t "command.viewBack") :category ~(t "category.view") []
  (viewAxis "back"))

(defcommand viewRight "Looks at the scene from the right." :title ~(t "command.viewRight") :category ~(t "category.view") []
  (viewAxis "right"))

(defcommand viewLeft "Looks at the scene from the left." :title ~(t "command.viewLeft") :category ~(t "category.view") []
  (viewAxis "left"))

(defcommand viewTop "Looks down at the scene." :title ~(t "command.viewTop") :category ~(t "category.view") []
  (viewAxis "top"))

(defcommand viewBottom "Looks up at the scene." :title ~(t "command.viewBottom") :category ~(t "category.view") []
  (viewAxis "bottom"))

(defcommand viewCamera "Looks through the scene's camera, or back to the view before."
  :title ~(t "command.viewCamera") :category ~(t "category.view") :icon "video" :enablement ~(some? computed.cameraNode)
  []
  (if state.inCamera
    (set "state.inCamera" false)
    (do (set "state.view.camera" computed.cameraView)
        (set "state.inCamera" true))))

(defcommand toggleProjection "Switches the view between perspective and orthographic."
  :title ~(t "command.toggleProjection") :category ~(t "category.view") []
  (camera "#viewport" :projection "toggle"))

(defcommand frameSelected "Fits the selected objects in the view."
  :title ~(t "command.frameSelected") :category ~(t "category.view") []
  (set "state.inCamera" false)
  (camera "#viewport" :frame "selection"))

(defcommand frameAll "Fits the whole scene in the view."
  :title ~(t "command.frameAll") :category ~(t "category.view") []
  (set "state.inCamera" false)
  (camera "#viewport" :frame "all"))

(defn orbitView "The numpad's 4, 6, 8 and 2: turns the view by degrees, across and up." [orbit]
  (set "state.inCamera" false)
  (camera "#viewport" :orbit orbit))

(defn zoomView "The numpad's plus and minus: zooms by steps." [steps]
  (camera "#viewport" :zoom steps))

(defn navDrag "A drag on the navigation buttons: the magnifier zooms, the hand pans." [id dx dy]
  (let [view state.view.camera
        distance (:distance view)
        target (:target view)]
    (cond (= id "zoom") (camera "#viewport" :zoom (* (- dy) 0.05))
          (= id "pan") (set "state.view.camera.target"
                            [(- (get target 0) (* (cos (:yaw view)) dx distance 0.002))
                             (+ (get target 1) (* (sin (:yaw view)) dx distance 0.002))
                             (+ (get target 2) (* dy distance 0.002))]))))

(defn navClick "A click on the navigation buttons: the camera looks through the scene's camera, the grid switches projection."
  [id]
  (cond (= id "camera") (viewCamera)
        (= id "ortho") (toggleProjection)))

(defn orbitByDrag "The navigation ball dragged: the view turns by half a degree a pixel." []
  (camera "#viewport" :orbit [(* (- (first state.dragLast) event.dx) 0.5) (* (- (second state.dragLast) event.dy) 0.5)])
  (set "state.dragLast" [event.dx event.dy]))

(defn dragNav "A navigation button dragged: navDrag with how far the pointer went since the last move." [id]
  (navDrag id (- event.dx (first state.dragLast)) (- event.dy (second state.dragLast)))
  (set "state.dragLast" [event.dx event.dy]))
