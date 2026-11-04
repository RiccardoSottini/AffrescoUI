;;; modello.lisp --- what Modello does to its scene: undo and redo, the selection, adding, copying, naming and hiding
;;;
;;; The views (viewport.lisp, outliner.lisp, properties.lisp...) say what is shown; this file and its neighbours say what
;;; happens: transform.lisp moves things after the pointer, collections.lisp sorts the objects, animation.lisp keys and
;;; plays them, materials.lisp edits their shaders, navigation.lisp turns the view and lays out the editors, files.lisp
;;; opens and saves. A function reads the running app through paths (`state.scene`, `state.view.selection`,
;;; `computed.activeNode`) and changes it only through steps (`set`, `push`...). A view runs one by name (`:onPick pick`)
;;; or with its arguments, ~(handler selectNode node.id event.shift); keys.lisp binds the keys, menus.lisp the menus.

;;; Numbers and nodes

(defn round-to "A number rounded at a scale: 10000 keeps four decimals, 100 two." [x scale]
  (/ (round (* x scale)) scale))

(defn rounded "A number to four decimals, as every place and size of the scene is kept." [x]
  (round-to x 10000))

(defn ids "The ids of nodes." [nodes]
  (map :id nodes))

(defn node-index "Where the node of an id is in the scene, or -1." [id]
  (indexOf (ids state.scene.nodes) id))

(defn by-id "The item of a list with an id, or nil." [items id]
  (find (fn [item] (= (:id item) id)) items))

(defn selected? "Whether a node is selected." [n]
  (contains state.view.selection (:id n)))

(defn change-nodes "Changes the nodes a test picks, and leaves the others as they are." [pick change]
  (set "state.scene.nodes" (map (fn [n] (if (pick n) (change n) n)) state.scene.nodes)))

(defn locked? "Whether a node locks an item of one of its lists, such as the y of its translation." [n key i]
  (= (get (get n (str "lock_" key)) i) true))

(defn unique-name "A name no node has: the base, or the base with .001, .002... after it." [base names]
  (loop [n 0 name base]
    (if (contains names name)
      (recur (inc n) (str base "." (pad (inc n) 3 "0")))
      name)))

(defn without "A list without one item." [items item]
  (remove (fn [it] (= it item)) items))

;;; Undo and redo

(defn snapshot "Keeps the scene as it is before a change: Undo goes back to it, up to 32 times, and Redo starts afresh."
  []
  (set "state.lastOp" nil)
  (push "state.undoStack" state.scene)
  (when (> (count state.undoStack) 32) (remove-state "state.undoStack" :at 0))
  (set "state.redoStack" []))

(defn swap-scene "Undo and Redo: the scene as it is goes on one stack, and the last of the other takes its place."
  [from onto stack]
  (push onto state.scene)
  (set "state.scene" (last stack))
  (remove-state from :at (dec (count stack))))

(defcommand undo "Puts the scene back as it was before the last change."
  :title ~(t "command.undo") :description ~(t "command.undo.about")
  :category ~(t "category.edit") :icon "undo-2" :enablement ~(> (count state.undoStack) 0) []
  (set "state.lastOp" nil)
  (swap-scene "state.undoStack" "state.redoStack" state.undoStack)
  (let [left (ids state.scene.nodes)]
    (set "state.view.selection" (filter (fn [id] (contains left id)) state.view.selection))))

(defcommand redo "Makes the change Undo took back again."
  :title ~(t "command.redo") :description ~(t "command.redo.about")
  :category ~(t "category.edit") :icon "redo-2" :enablement ~(> (count state.redoStack) 0) []
  (swap-scene "state.redoStack" "state.undoStack" state.redoStack))

;;; Selecting

(defn selectNode "A click on an object: it alone is selected, or with Shift it joins the selection, or leaves it if active."
  [id shift]
  (cond (not shift) (do (set "state.view.selection" [id])
                        (set "state.active" id))
        (and (contains state.view.selection id) (= state.active id))
          (set "state.view.selection" (without state.view.selection id))
        :else (do (set "state.view.selection" (concat (without state.view.selection id) [id]))
                  (set "state.active" id))))

(defcommand selectAll "Selects every object that can be picked."
  :title ~(t "command.selectAll") :description ~(t "command.selectAll.about")
  :category ~(t "category.select") []
  (set "state.view.selection" computed.pickable))

(defcommand selectNone "Selects nothing."
  :title ~(t "command.selectNone") :description ~(t "command.selectNone.about")
  :category ~(t "category.select") []
  (set "state.view.selection" []))

(defcommand selectInvert "Selects what isn't selected, and only that."
  :title ~(t "command.selectInvert") :description ~(t "command.selectInvert.about")
  :category ~(t "category.select") []
  (set "state.view.selection" (remove (fn [id] (contains state.view.selection id)) computed.pickable)))

;;; Adding and copying

(defn addObject "Adds an object at a place or at the 3D cursor, selects it, and lets Adjust Last move it."
  [kind mesh name light at]
  (dismiss)
  (when (not= state.mode "object") (set "state.mode" "object"))
  (snapshot)
  (increment "state.added" :by 1)
  (let [id (str (lower-case kind) "-" state.added)
        place (or at state.cursor)
        added (t "op.add" {:name name})
        node {:id id
              :name (unique-name name (map :name state.scene.nodes))
              :kind kind
              :collection (when (and (not= state.activeCollection "collection")
                                     (contains computed.collectionIds state.activeCollection))
                            state.activeCollection)
              :mesh mesh
              :material (when (= kind "mesh") "material")
              :light light
              :camera (when (= kind "camera") {:fieldOfView 0.69})
              :translation place
              :rotationDegrees [0 0 0]
              :scale [1 1 1]}]
    (push "state.scene.nodes" node)
    (set "state.view.selection" [id])
    (set "state.active" id)
    (set "state.last" added)
    (set "state.lastOp" {:title added
                         :kind "translate"
                         :label (t "op.location")
                         :base [(merge node {:translation [0 0 0]})]
                         :values place})))

(defn add-copies "Adds copies of nodes, each with a new id and a name no node has, and selects them." [nodes]
  (loop [left nodes made []]
    (if (empty? left)
      (do (set "state.view.selection" made)
          (set "state.active" (last made)))
      (let [m (first left)]
        (increment "state.added" :by 1)
        (let [id (str (first (split (:id m) "-")) "-" state.added)]
          (push "state.scene.nodes"
                (merge m {:id id :name (unique-name (first (split (:name m) ".")) (map :name state.scene.nodes))}))
          (recur (rest left) (conj made id)))))))

(defcommand duplicate "Copies the selected objects, then moves the copies after the pointer."
  :title ~(t "command.duplicate") :description ~(t "command.duplicate.about")
  :category ~(t "category.object") :icon "copy" :enablement ~(> (count state.view.selection) 0) []
  (snapshot)
  (add-copies computed.selectedNodes)
  (after 0 startTransform "translate"))

(defcommand copyObjects "Keeps the selected objects for Paste."
  :title ~(t "command.copyObjects") :description ~(t "command.copyObjects.about")
  :category ~(t "category.edit") :icon "copy" :enablement ~(> (count state.view.selection) 0) []
  (when (not (empty? state.view.selection))
    (set "state.copied" computed.selectedNodes)
    (set "state.last" (t "op.copied" {:count (count state.copied)}))))

(defcommand pasteObjects "Adds the copied objects to the scene again, selected."
  :title ~(t "command.pasteObjects") :description ~(t "command.pasteObjects.about")
  :category ~(t "category.edit") :icon "clipboard-paste" :enablement ~(> (count state.copied) 0) []
  (dismiss)
  (when (not (empty? state.copied))
    (snapshot)
    (add-copies state.copied)
    (set "state.last" (t "command.pasteObjects"))))

;;; Deleting and hiding

(defcommand deleteSelected "Deletes the selected objects."
  :title ~(t "command.deleteSelected") :description ~(t "command.deleteSelected.about")
  :category ~(t "category.object") :icon "trash-2" :enablement ~(> (count state.view.selection) 0) []
  (dismiss)
  (snapshot)
  (set "state.scene.nodes" (remove selected? state.scene.nodes))
  (set "state.view.selection" [])
  (set "state.active" nil)
  (set "state.last" (t "command.deleteSelected")))

(defcommand hideSelected "Hides the selected objects."
  :title ~(t "command.hideSelected") :description ~(t "command.hideSelected.about")
  :category ~(t "category.object") :icon "eye-off" []
  (change-nodes selected? (fn [n] (merge n {:visible false})))
  (set "state.view.selection" []))

(defcommand hideUnselected "Hides every object but the selected ones."
  :title ~(t "command.hideUnselected") :description ~(t "command.hideUnselected.about")
  :category ~(t "category.object") []
  (change-nodes (fn [n] (not (selected? n))) (fn [n] (merge n {:visible false}))))

(defcommand revealHidden "Shows the hidden objects again, and selects them."
  :title ~(t "command.revealHidden") :description ~(t "command.revealHidden.about")
  :category ~(t "category.object") :icon "eye" []
  (set "state.view.selection" (ids (filter (fn [n] (= (:visible n) false)) state.scene.nodes)))
  (change-nodes (fn [n] true) (fn [n] (merge n {:visible true}))))

(defn toggleFlag "An object's eye, arrow or camera in the outliner: shows or hides it, lets it be picked, or rendered."
  [id key]
  (let [i (node-index id)
        now (= (get (get state.scene.nodes i) key) false)]
    (set (str "state.scene.nodes[" i "]." key) now)
    (when (and (not= key "render") (= now false))
      (set "state.view.selection" (without state.view.selection id)))))

;;; Names and fields

(defn rename "Names an object (the active one when id is nil), if the name isn't empty." [id name]
  (let [target (or id state.active)]
    (when (and (not= (trim name) "") (contains (ids state.scene.nodes) target))
      (snapshot)
      (set (str "state.scene.nodes[" (node-index target) "].name") (slice (trim name) 0 63)))))

(defn focusRename "Puts the keyboard in the outliner's name field." []
  (focus "#outliner-rename"))

(defcommand startRename "Names the active object in the outliner, which takes the properties' place if it isn't shown."
  :title ~(t "command.startRename") :description ~(t "command.startRename.about")
  :category ~(t "category.object") :icon "pencil" []
  (dismiss)
  (when (and (some? state.active) (contains state.view.selection state.active))
    (when (not (contains (vals state.areas) "outliner")) (swapEditor "properties" "outliner"))
    (set "state.outlinerHide" (without state.outlinerHide computed.activeNode.kind))
    (set "state.outlinerFilter" "")
    (set "state.outlinerOpen.root" true)
    (set "state.outlinerOpen.collection" true)
    (set "state.renaming" state.active)
    (after 0 focusRename)))

;; key names what the field changes: a list of the active object (translation...) at axis, the lens, the 3D cursor,
;; the last operation (redo); or path, a place in the state, a whole number with whole.
(defn setField "A number field dragged or stepped: the value, kept between low and high, goes where the field says."
  [key axis path low high value]
  (cond (= key "redo") (adjustLast axis value)
        (= key "whole") (set (str "state." path) (round (clamp value low high)))
        (not= path "") (set (str "state." path) (rounded (clamp value low high)))
        (= key "cameraLens")
          (when (and (>= computed.activeIndex 0) (= computed.activeNode.kind "camera"))
            (set (str "state.scene.nodes[" computed.activeIndex "].camera.fieldOfView")
                 (rounded (* 2 (atan (/ 18 (clamp value low high)))))))
        (= key "lens") (set "state.view.camera.fieldOfView" (rounded (* 2 (atan (/ 18 (clamp (round value) 2 250))))))
        (= key "cursor") (set (str "state.cursor[" axis "]") (rounded value))
        (< computed.activeIndex 0) nil
        :else (do (set (str "state.scene.nodes[" computed.activeIndex "]." key "[" axis "]") (rounded value))
                  (autoKey))))

(defn toggleLock "The padlock beside a field: locks or frees one item of the active object's list." [key axis]
  (when (>= computed.activeIndex 0)
    (let [locks (get computed.activeNode (str "lock_" key))]
      (set (str "state.scene.nodes[" computed.activeIndex "].lock_" key)
           (map (fn [i] (if (= i axis) (not= (get locks i) true) (= (get locks i) true))) (range 3))))))

(defn setColor "A colour picked: the world's, the active material's base colour, or the active light's." [target value]
  (let [colour (colorList value)]
    (cond (nil? colour) nil
          (= target "world") (set "state.world.color" (slice (hexColor value) 0 7))
          (and (= target "material") (>= computed.activeMaterialIndex 0))
            (set (str "state.scene.materials[" computed.activeMaterialIndex "].baseColor") colour)
          (and (= target "light") (= computed.activeNode.kind "light"))
            (set (str "state.scene.nodes[" computed.activeIndex "].light.color") (slice colour 0 3)))))

(defn setLight "One of the active light's settings, such as its type." [key value]
  (when (and (>= computed.activeIndex 0)
             (= computed.activeNode.kind "light")
             (not= (get computed.activeNode.light key) value))
    (snapshot)
    (set (str "state.scene.nodes[" computed.activeIndex "].light." key) value)))

;;; The widgets (widgets.lisp)

(defn pressButton "A header button pressed: it opens its dropdown (the state open names), or runs its function." [open action with]
  (if (not= open "")
    (toggle open)
    (when (some? action) (run action :with with))))

(defn pressCheck "A check row pressed: it runs its function, or switches the state at its path." [path action with]
  (if (some? action) (run action :with with) (toggle path)))

(defn runItem "A pie menu's item chosen: the menu closes and its function runs." [action with]
  (dismiss)
  (run action :with with))

(defn togglePanel "A panel's header pressed: the panel opens or closes." [key]
  (toggle (str "state.panels." key)))

(defn startDrag "A field pressed: where a drag starts from." [value]
  (set "state.dragStart" value))

(defn pressField "A number field pressed: where a drag starts from, and an undo step when it changes the scene." [key path value]
  (startDrag value)
  (when (if (= key "") (= (slice path 0 6) "scene.") (not (contains ["redo" "lens" "cursor" "whole"] key)))
    (snapshot)))

(defn openColor "A colour swatch pressed: its picker opens or closes, and an undo step comes first for the scene's colours."
  [target key]
  (when (and (not= target "world") (not= (get state.panels key) true)) (snapshot))
  (togglePanel key))

;;; The top bar and the areas (bars.lisp)

(defn hoverArea "The pointer over an area: Shift A, Delete and the pie menus act on its editor." [slot]
  (set "state.hoverSlot" slot))

(defn renameScene "The scene's name typed in the top bar, if it isn't empty." [text]
  (when (not= (trim text) "") (set "state.sceneName" (slice (trim text) 0 63))))

(defn renameLayer "The view layer's name typed in the top bar, if it isn't empty." [text]
  (when (not= (trim text) "") (set "state.layerName" (slice (trim text) 0 63))))

;;; The outliner (outliner.lisp): a row is an object's (id) or a collection's (collection)

(defn pickRow "A row clicked: its object is selected (with Shift, added), its collection made active, its tab shown."
  [id collection tab shift]
  (when (some? id) (selectNode id shift))
  (when (some? collection) (set "state.activeCollection" collection))
  (when (some? tab) (set "state.propTab" tab)))

(defn startRowRename "A row double-clicked: its object or collection is renamed in place." [id collection data]
  (when (and (some? id) (not= data ""))
    (set "state.renaming" id)
    (focusRename))
  (when (some? collection)
    (set "state.renaming" (str "coll:" collection))
    (focusRename)))

(defn rowMenu "A row right-clicked: its collection's menu, or its object's, the object selected first." [id collection]
  (when (some? collection)
    (set "state.activeCollection" collection)
    (openPopup "collectionContext" event.x event.y))
  (when (some? id)
    (when (not (contains state.view.selection id)) (selectNode id false))
    (openPopup "outlinerContext" event.x event.y)))

(defn submitRowRename "A name typed in a row: its collection or its object takes it." [id collection text]
  (if (some? collection) (renameCollection collection text) (rename id text))
  (set "state.renaming" nil))

(defn toggleOpen "A row's chevron: what is under the row is shown or hidden." [key]
  (set "state.outlinerOpen" (assoc state.outlinerOpen key (not (get state.outlinerOpen key)))))

(defn toggleRowFlag "A row's switch: its object's, or else its collection's." [id collection key]
  (if (some? id) (toggleFlag id key) (toggleCollection collection key)))
