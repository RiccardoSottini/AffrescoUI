;;; navigation.lisp --- the view, the editors and the window: turning and framing the view, the options of the
;;; headers, the menus that pop up, the workspaces, and what the window shows
;;;
;;; The viewport turns its own camera (`camera` steps); the rest is the state the headers and menus show.

(ns modello.navigation)

;;; Menus that pop up

(defn open-popup "Opens a menu of menus.lisp where the pointer is, or in the middle of the viewport when x and y are nil."
  [menu x y]
  (set "state.ctx" {:open true :menu menu :items nil :x (or x computed.centre.x) :y (or y computed.centre.y)}))

(defn open-pie "Opens a pie menu (the view's or the shading's) in the middle of the viewport, if the pointer is over it."
  [menu]
  (let [slot (or state.hoverSlot "a")]
    (when (and (nil? state.modal) (= (get state.areas slot) :viewport))
      (set "state.pie" {:open true
                        :menu menu
                        :x (+ nodes.viewport.x (/ nodes.viewport.width 2))
                        :y (+ nodes.viewport.y (/ nodes.viewport.height 2))}))))

(defn hovered "The editor under the pointer, or nil." []
  (when-some [slot state.hoverSlot] (get state.areas slot)))

(defcommand addMenu "Opens the Add menu: objects in the viewport, nodes in the Shader Editor."
  :title ~(t "command.addMenu") :description ~(t "command.addMenu.about")
  :category ~(t "category.object") :icon "plus" []
  (open-popup (if (= (hovered) :shader) "shaderAdd" "add") nil nil))

(defcommand deleteMenu "Opens the Delete menu, when something is selected."
  :title ~(t "command.deleteMenu") :description ~(t "command.deleteMenu.about")
  :category ~(t "category.object") []
  (when (seq state.view.selection) (open-popup "delete" nil nil)))

(defcommand search "Opens the search for a command."
  :title ~(t "command.search") :description ~(t "command.search.about")
  :category ~(t "category.edit") :icon "search" []
  (dismiss)
  (set "state.quickPick" {:open true :query ""}))

(defcommand about "Shows what Modello is."
  :title ~(t "command.about") :description ~(t "command.about.about")
  :category ~(t "category.help") :icon "info" []
  (dismiss)
  (set "state.about" true))

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.language") :description ~(t "settings.language.about"))

(defcommand changeLanguage "Switches the language: English, Spanish, French."
  :title ~(t "command.changeLanguage") :description ~(t "command.changeLanguage.about")
  :category ~(t "category.window") :icon "languages" []
  (set "settings.appearance.language"
       (case settings.appearance.language :en :es :es :fr :en)))

;;; The headers' options

(defcommand setMode "Changes the mode: Object Mode, or Edit Mode on the active mesh."
  :title ~(t "command.setMode") :description ~(t "command.setMode.about")
  :category ~(t "category.object") [mode]
  (dismiss)
  (if (= mode "edit") (enter-edit) (leave-edit)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setOption "One of the state's options, such as the shading or the pivot, set to a value." [key value]
  (dismiss)
  (set (str "state." key) value))

; style: keep, a string of the code names it, and a rename never changes a string
(defn toggleOption "One of the state's options switched on or off." [key]
  (toggle (str "state." key)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn toggleOverlay "One of the overlays shown or hidden: the grid, the axes, the 3D cursor..." [key]
  (toggle (str "state.overlay." key)))

(defn kinds-in "The kinds a list of the state holds: hiddenKinds, unselectableKinds or outlinerHide." [which]
  (case which
    :hiddenKinds state.hiddenKinds
    :unselectableKinds state.unselectableKinds
    state.outlinerHide))

; style: keep, a string of the code names it, and a rename never changes a string
(defn toggleKind "A kind of object (meshes, lights...) taken out of a list of kinds, or put in: hidden, unselectable, unlisted."
  [which kind]
  (let [kinds (kinds-in which)]
    (set (str "state." which) (if (contains kinds kind) (without kinds kind) (concat kinds [kind]))))
  (set "state.view.selection" (filter #(contains computed.pickable %) state.view.selection)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setObjectGizmo "The gizmo the Select tool shows on the selection: one, or none when it is chosen again." [mode]
  (set "state.objectGizmo" (if (= state.objectGizmo mode) nil mode)))

; style: keep, named in tests/fuzz/corpus.py
(defn setTool "A tool of the toolbar chosen; the Measure tool starts afresh." [tool]
  (set "state.tool" tool)
  (set "state.measure" {:points [] :screen [] :camera nil}))

(defcommand toggleXray "Lets the objects behind be seen and picked."
  :title ~(t "command.toggleXray") :description ~(t "command.toggleXray.about")
  :category ~(t "category.view") :icon "square-dashed-bottom" []
  (toggle "state.xray"))

(defcommand toggleSnap "Turns snapping on or off."
  :title ~(t "command.toggleSnap") :description ~(t "command.toggleSnap.about")
  :category ~(t "category.object") :icon "magnet" []
  (toggle "state.snap"))

(defcommand toggleOverlays "Shows or hides every overlay."
  :title ~(t "command.toggleOverlays") :description ~(t "command.toggleOverlays.about")
  :category ~(t "category.view") []
  (toggle "state.overlays"))

(defcommand toggleSidebar "Shows or hides the viewport's sidebar."
  :title ~(t "command.toggleSidebar") :description ~(t "command.toggleSidebar.about")
  :category ~(t "category.view") []
  (toggle "state.sidebar"))

(defcommand toggleToolbar "Shows or hides the viewport's toolbar."
  :title ~(t "command.toggleToolbar") :description ~(t "command.toggleToolbar.about")
  :category ~(t "category.view") []
  (set "state.toolbar" (false? state.toolbar)))

(defcommand toggleTheme "Switches between the dark and the light theme."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about")
  :category ~(t "category.window") :icon "sun-moon" []
  (set "state.theme" (if (= state.theme :dark) :light :dark)))

;;; Editors and workspaces

(defn slot-of "The area that shows an editor, or nil." [editor]
  (find #(= (get state.areas %) editor) (keys state.areas)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn swapEditor "An area's editor changed: the area that showed the new one takes the old one." [from to]
  (dismiss)
  (let [there (slot-of to)]
    (when-some [here (slot-of from)]
      (set (str "state.areas." here) to)
      (when-some [_ there] (set (str "state.areas." there) from)))))

;; A workspace sets the editors, the timeline's height, the shading and the properties' tab it is made for.
(defn set-workspace "A workspace tab chosen: Stage, Shape, Surface, Motion or Shot." [id name]
  (dismiss)
  (set "state.workspace" name)
  (set "state.areas" {:a "viewport" :b (if (= id :surface) "shader" "timeline") :c "outliner" :d "properties"})
  (set "state.sizes.left" [(case id :motion 520 :surface 470 :shape 790 720)])
  (set "state.shading" (case id :surface :material :shot :rendered :solid))
  (set "state.xray" false)
  (set "state.overlays" (not= id :shot))
  (set "state.propTab" (case id :surface :material :shot :render :shape :data state.propTab)))

(defn workspace-after "The workspace a step away from the one shown, round the tabs." [step]
  (let [tabs computed.workspaces
        at (indexOf (map :name tabs) state.workspace)
        {:keys [id name]} (get tabs (mod (+ at step (count tabs)) (count tabs)))]
    (set-workspace id name)))

(defcommand nextWorkspace "Shows the next workspace."
  :title ~(t "command.nextWorkspace") :description ~(t "command.nextWorkspace.about")
  :category ~(t "category.window") :icon "arrow-right" []
  (workspace-after 1))

(defcommand previousWorkspace "Shows the workspace before."
  :title ~(t "command.previousWorkspace") :description ~(t "command.previousWorkspace.about")
  :category ~(t "category.window") :icon "arrow-left" []
  (workspace-after -1))

(defcommand toggleMaximize "Fills the window with the area under the pointer, or puts the areas back."
  :title ~(t "command.toggleMaximize") :description ~(t "command.toggleMaximize.about")
  :category ~(t "category.view") []
  (dismiss)
  (set "state.maximized" (when (nil? state.maximized) (or state.hoverSlot "a"))))

;;; The view

(defn view-axis "Looks along an axis: front, back, right, left, top or bottom." [view]
  (set "state.inCamera" false)
  (camera "#viewport" :view view))

(defcommand viewFront "Looks at the scene from the front."
  :title ~(t "command.viewFront") :description ~(t "command.viewFront.about")
  :category ~(t "category.view") []
  (view-axis "front"))

(defcommand viewBack "Looks at the scene from the back."
  :title ~(t "command.viewBack") :description ~(t "command.viewBack.about")
  :category ~(t "category.view") []
  (view-axis "back"))

(defcommand viewRight "Looks at the scene from the right."
  :title ~(t "command.viewRight") :description ~(t "command.viewRight.about")
  :category ~(t "category.view") []
  (view-axis "right"))

(defcommand viewLeft "Looks at the scene from the left."
  :title ~(t "command.viewLeft") :description ~(t "command.viewLeft.about")
  :category ~(t "category.view") []
  (view-axis "left"))

(defcommand viewTop "Looks down at the scene."
  :title ~(t "command.viewTop") :description ~(t "command.viewTop.about")
  :category ~(t "category.view") []
  (view-axis "top"))

(defcommand viewBottom "Looks up at the scene."
  :title ~(t "command.viewBottom") :description ~(t "command.viewBottom.about")
  :category ~(t "category.view") []
  (view-axis "bottom"))

(defcommand viewCamera "Looks through the scene's camera, or back to the view before."
  :title ~(t "command.viewCamera") :description ~(t "command.viewCamera.about")
  :category ~(t "category.view") :icon "video" :enablement ~(some? computed.cameraNode) []
  (if state.inCamera
    (set "state.inCamera" false)
    (do (set "state.view.camera" computed.cameraView)
        (set "state.inCamera" true))))

(defcommand toggleProjection "Switches the view between perspective and orthographic."
  :title ~(t "command.toggleProjection") :description ~(t "command.toggleProjection.about")
  :category ~(t "category.view") []
  (camera "#viewport" :projection "toggle"))

(defcommand frameSelected "Fits the selected objects in the view."
  :title ~(t "command.frameSelected") :description ~(t "command.frameSelected.about")
  :category ~(t "category.view") []
  (set "state.inCamera" false)
  (camera "#viewport" :frame "selection"))

(defcommand frameAll "Fits the whole scene in the view."
  :title ~(t "command.frameAll") :description ~(t "command.frameAll.about")
  :category ~(t "category.view") []
  (set "state.inCamera" false)
  (camera "#viewport" :frame "all"))

(defn orbit-view "The numpad's 4, 6, 8 and 2: turns the view by degrees, across and up." [orbit]
  (set "state.inCamera" false)
  (camera "#viewport" :orbit orbit))

(defn zoom-view "The numpad's plus and minus: zooms by steps." [steps]
  (camera "#viewport" :zoom steps))

(defn nav-drag "A drag on the navigation buttons: the magnifier zooms, the hand pans." [id dx dy]
  (let [{:keys [distance target yaw]} state.view.camera
        [x y z] target]
    (case id
      :zoom (camera "#viewport" :zoom (* (- dy) 0.05))
      :pan (set "state.view.camera.target"
                [(- x (* (cos yaw) dx distance 0.002))
                 (+ y (* (sin yaw) dx distance 0.002))
                 (+ z (* dy distance 0.002))])
      nil)))

(defn nav-click "A click on the navigation buttons: the camera looks through the scene's camera, the grid switches projection."
  [id]
  (case id
    :camera (viewCamera)
    :ortho (toggleProjection)
    nil))

(defn orbit-by-drag "The navigation ball dragged: the view turns by half a degree a pixel." []
  (let [[x y] state.dragLast]
    (camera "#viewport" :orbit [(* (- x event.dx) 0.5) (* (- y event.dy) 0.5)]))
  (set "state.dragLast" [event.dx event.dy]))

(defn drag-nav "A navigation button dragged: navDrag with how far the pointer went since the last move." [id]
  (let [[x y] state.dragLast]
    (nav-drag id (- event.dx x) (- event.dy y)))
  (set "state.dragLast" [event.dx event.dy]))

(defn choose-sidebar-tab "A tab of the viewport's sidebar chosen: Item, Tool or View." [tab]
  (set "state.sidebarTab" tab))
