;;; modello.lisp --- what Modello does to its scene: undo and redo, the selection, adding, copying, naming and hiding
;;;
;;; The views (viewport.lisp, outliner.lisp, properties.lisp...) say what is shown; this file and its neighbours say what
;;; happens: transform.lisp moves things after the pointer, collections.lisp sorts the objects, animation.lisp keys and
;;; plays them, materials.lisp edits their shaders, navigation.lisp turns the view and lays out the editors, files.lisp
;;; opens and saves. A function reads the running app through paths (`state.scene`, `state.view.selection`,
;;; `computed.activeNode`) and changes it only through steps (`set`, `push`...). A view runs one by name (`:onPick pick`)
;;; or with its arguments, ~(handler selectNode node.id event.shift); keys.lisp binds the keys, menus.lisp the menus.

(ns modello.modello)

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
  (find #(= (:id %) id) items))

(defn selected? "Whether a node is selected." [n]
  (contains state.view.selection (:id n)))

(defn change-nodes "Changes the nodes a test picks, and leaves the others as they are." [pick change]
  (set "state.scene.nodes" (map #(if (pick %) (change %) %) state.scene.nodes)))

(defn locked? "Whether a node locks an item of one of its lists, such as the y of its translation." [n key i]
  (true? (get-in n [(str "lock_" key) i])))

(defn unique-name "A name no node has: the base, or the base with .001, .002... after it." [base names]
  (loop [n 1 name base]
    (if (contains names name)
      (recur (inc n) (str base "." (pad n 3 "0")))
      name)))

(defn without "A list without one item." [items item]
  (remove #(= % item) items))

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
  :category ~(t "category.edit") :icon "undo-2" :enablement ~(pos? (count state.undoStack)) []
  (set "state.lastOp" nil)
  (swap-scene "state.undoStack" "state.redoStack" state.undoStack)
  (let [left (ids state.scene.nodes)]
    (set "state.view.selection" (filter #(contains left %) state.view.selection)))
  (edit-after-history))

(defcommand redo "Makes the change Undo took back again."
  :title ~(t "command.redo") :description ~(t "command.redo.about")
  :category ~(t "category.edit") :icon "redo-2" :enablement ~(pos? (count state.redoStack)) []
  (swap-scene "state.redoStack" "state.undoStack" state.redoStack)
  (edit-after-history))

;;; Selecting

(defn select-node "A click on an object: it alone is selected, or with Shift it joins the selection, or leaves it if active."
  [id shift]
  (cond (not shift) (do (set "state.view.selection" [id])
                        (set "state.active" id))
        (and (contains state.view.selection id) (= state.active id))
          (set "state.view.selection" (without state.view.selection id))
        :else (do (set "state.view.selection" (concat (without state.view.selection id) [id]))
                  (set "state.active" id))))

(defcommand selectAll "Selects every object that can be picked, or in Edit Mode every vertex or face."
  :title ~(t "command.selectAll") :description ~(t "command.selectAll.about")
  :category ~(t "category.select") []
  (if (= state.mode :edit) (select-all-elements true) (set "state.view.selection" computed.pickable)))

(defcommand selectNone "Selects nothing."
  :title ~(t "command.selectNone") :description ~(t "command.selectNone.about")
  :category ~(t "category.select") []
  (if (= state.mode :edit) (select-all-elements false) (set "state.view.selection" [])))

(defcommand selectInvert "Selects what isn't selected, and only that."
  :title ~(t "command.selectInvert") :description ~(t "command.selectInvert.about")
  :category ~(t "category.select") []
  (set "state.view.selection" (remove #(contains state.view.selection %) computed.pickable)))

;;; Adding and copying

(defn add-object "Adds an object at a place or at the 3D cursor, selects it, and lets Adjust Last move it."
  [kind mesh name light at]
  (dismiss)
  (when (not= state.mode :object) (set "state.mode" :object))
  (snapshot)
  (increment "state.added" :by 1)
  (let [id (str (str/lower-case kind) "-" state.added)
        place (or at state.cursor)
        added (t "op.add" {:name name})
        node {:id id
              :name (unique-name name (map :name state.scene.nodes))
              :kind kind
              :collection (when (and (not= state.activeCollection "collection")
                                     (contains computed.collectionIds state.activeCollection))
                            state.activeCollection)
              :mesh mesh
              :material (when (= kind :mesh) "material")
              :light light
              :camera (when (= kind :camera) {:fieldOfView 0.69})
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
      (let [{:keys [id name] :as m} (first left)
            prefix (first (str/split id "-"))
            base (first (str/split name "."))]
        (increment "state.added" :by 1)
        (let [copy (str prefix "-" state.added)
              names (map :name state.scene.nodes)]
          (push "state.scene.nodes" (assoc m :id copy :name (unique-name base names)))
          (recur (rest left) (conj made copy)))))))

(defcommand duplicate "Copies the selected objects, then moves the copies after the pointer."
  :title ~(t "command.duplicate") :description ~(t "command.duplicate.about")
  :category ~(t "category.object") :icon "copy" :enablement ~(pos? (count state.view.selection)) []
  (snapshot)
  (add-copies computed.selectedNodes)
  (after 0 start-transform "translate"))

(defcommand copyObjects "Keeps the selected objects for Paste."
  :title ~(t "command.copyObjects") :description ~(t "command.copyObjects.about")
  :category ~(t "category.edit") :icon "copy" :enablement ~(pos? (count state.view.selection)) []
  (when (seq state.view.selection)
    (set "state.copied" computed.selectedNodes)
    (set "state.last" (t "op.copied" {:count (count state.copied)}))))

(defcommand pasteObjects "Adds the copied objects to the scene again, selected."
  :title ~(t "command.pasteObjects") :description ~(t "command.pasteObjects.about")
  :category ~(t "category.edit") :icon "clipboard-paste" :enablement ~(pos? (count state.copied)) []
  (dismiss)
  (when (seq state.copied)
    (snapshot)
    (add-copies state.copied)
    (set "state.last" (t "command.pasteObjects"))))

;;; Deleting and hiding

(defcommand deleteSelected "Deletes the selected objects."
  :title ~(t "command.deleteSelected") :description ~(t "command.deleteSelected.about")
  :category ~(t "category.object") :icon "trash-2" :enablement ~(pos? (count state.view.selection)) []
  (dismiss)
  (if (= state.mode :edit) (delete-elements) (delete-objects)))

(defn delete-objects "Deletes the selected objects; their children stay where they are." []
  (snapshot)
  (doseq [n state.scene.nodes]
    (when (and (not (selected? n)) (contains state.view.selection (:parent n)))
      (reparent (:id n) nil)))
  (set "state.scene.nodes" (remove selected? state.scene.nodes))
  (set "state.view.selection" [])
  (set "state.active" nil)
  (set "state.last" (t "command.deleteSelected")))

(defcommand hideSelected "Hides the selected objects."
  :title ~(t "command.hideSelected") :description ~(t "command.hideSelected.about")
  :category ~(t "category.object") :icon "eye-off" []
  (change-nodes selected? #(assoc % :visible false))
  (set "state.view.selection" []))

(defcommand hideUnselected "Hides every object but the selected ones."
  :title ~(t "command.hideUnselected") :description ~(t "command.hideUnselected.about")
  :category ~(t "category.object") []
  (change-nodes (complement selected?) #(assoc % :visible false)))

(defcommand revealHidden "Shows the hidden objects again, and selects them."
  :title ~(t "command.revealHidden") :description ~(t "command.revealHidden.about")
  :category ~(t "category.object") :icon "eye" []
  (set "state.view.selection" (ids (filter #(false? (:visible %)) state.scene.nodes)))
  (change-nodes (constantly true) #(assoc % :visible true)))

(defn toggle-flag "An object's eye, arrow or camera in the outliner: shows or hides it, lets it be picked, or rendered."
  [id key]
  (let [i (node-index id)
        now (false? (get-in state.scene.nodes [i key]))]
    (set (str "state.scene.nodes[" i "]." key) now)
    (when (and (not= key "render") (not now))
      (set "state.view.selection" (without state.view.selection id)))))

;;; Names and fields

(defn rename "Names an object (the active one when id is nil), if the name isn't empty." [id name]
  (let [target (or id state.active)
        trimmed (str/trim name)]
    (when (and (not (str/blank? trimmed)) (contains (ids state.scene.nodes) target))
      (snapshot)
      (set (str "state.scene.nodes[" (node-index target) "].name") (slice trimmed 0 63)))))

(defn focus-rename "Puts the keyboard in the outliner's name field." []
  (focus "#outliner-rename"))

(defcommand startRename "Names the active object in the outliner, which takes the properties' place if it isn't shown."
  :title ~(t "command.startRename") :description ~(t "command.startRename.about")
  :category ~(t "category.object") :icon "pencil" []
  (dismiss)
  (when (and (some? state.active) (contains state.view.selection state.active))
    (when-not (contains (vals state.areas) "outliner") (swapEditor "properties" "outliner"))
    (set "state.outlinerHide" (without state.outlinerHide computed.activeNode.kind))
    (set "state.outlinerFilter" "")
    (set "state.outlinerOpen.root" true)
    (set "state.outlinerOpen.collection" true)
    (set "state.renaming" state.active)
    (after 0 focus-rename)))

;; key names what the field changes: a list of the active object (translation...) at axis, the lens, the 3D cursor,
;; the last operation (redo); or path, a place in the state, a whole number with whole.
(defn set-field "A number field dragged or stepped: the value, kept between low and high, goes where the field says."
  [key axis path low high value]
  (cond (= key :redo) (adjust-last axis value)
        (= key :whole) (set (str "state." path) (round (clamp value low high)))
        (not= path "") (set (str "state." path) (rounded (clamp value low high)))
        (= key :cameraLens)
          (when (and (>= computed.activeIndex 0) (= computed.activeNode.kind :camera))
            (set (str "state.scene.nodes[" computed.activeIndex "].camera.fieldOfView")
                 (->> (clamp value low high) (/ 18) atan (* 2) rounded)))
        (= key :lens) (set "state.view.camera.fieldOfView" (->> (clamp (round value) 2 250) (/ 18) atan (* 2) rounded))
        (= key :cursor) (set (str "state.cursor[" axis "]") (rounded value))
        (< computed.activeIndex 0) nil
        :else (do (set (str "state.scene.nodes[" computed.activeIndex "]." key "[" axis "]") (rounded value))
                  (autoKey))))

(defn toggle-lock "The padlock beside a field: locks or frees one item of the active object's list." [key axis]
  (when (>= computed.activeIndex 0)
    (let [locks (get computed.activeNode (str "lock_" key))
          locked (map #(true? (get locks %)) (range 3))]
      (set (str "state.scene.nodes[" computed.activeIndex "].lock_" key) (update locked axis not)))))

(defn set-color "A colour picked: the world's, the active material's base colour, or the active light's." [target value]
  (when-some [colour (colorList value)]
    (case target
      :world (set "state.world.color" (slice (hexColor value) 0 7))
      :material (when (>= computed.activeMaterialIndex 0)
                  (set (str "state.scene.materials[" computed.activeMaterialIndex "].baseColor") colour))
      :light (when (= computed.activeNode.kind :light)
               (set (str "state.scene.nodes[" computed.activeIndex "].light.color") (slice colour 0 3)))
      nil)))

(defn set-light "One of the active light's settings, such as its type." [key value]
  (when (and (>= computed.activeIndex 0)
             (= computed.activeNode.kind :light)
             (not= (get computed.activeNode.light key) value))
    (snapshot)
    (set (str "state.scene.nodes[" computed.activeIndex "].light." key) value)))

(defn set-display "How the viewport draws the active mesh: solid, wire or bounds; undoable." [mode]
  (when (and (>= computed.activeIndex 0) (not= (or-else computed.activeNode.display "solid") mode))
    (snapshot)
    (set (str "state.scene.nodes[" computed.activeIndex "].display") mode)))

;;; The widgets (widgets.lisp)

(defn press-button "A header button pressed: it opens its dropdown (the state open names), or runs its function." [open action with]
  (if (not= open "")
    (toggle open)
    (when-some [f action] (run f :with with))))

(defn press-check "A check row pressed: it runs its function, or switches the state at its path." [path action with]
  (if-some [f action] (run f :with with) (toggle path)))

(defn run-item "A pie menu's item chosen: the menu closes and its function runs." [action with]
  (dismiss)
  (run action :with with))

(defn toggle-panel "A panel's header pressed: the panel opens or closes." [key]
  (toggle (str "state.panels." key)))

(defn start-drag "A field pressed: where a drag starts from." [value]
  (set "state.dragStart" value))

(defn press-field "A number field pressed: where a drag starts from, and an undo step when it changes the scene." [key path value]
  (start-drag value)
  (when (if (= key "") (str/starts-with? path "scene.") (not (#{:redo :lens :cursor :whole} key)))
    (snapshot)))

(defn open-color "A colour swatch pressed: its picker opens or closes, and an undo step comes first for the scene's colours."
  [target key]
  (let [open (true? (get state.panels key))]
    (when-not (or open (= target :world)) (snapshot)))
  (toggle-panel key))

;;; The top bar and the areas (bars.lisp)

(defn hover-area "The pointer over an area: Shift A, Delete and the pie menus act on its editor." [slot]
  (set "state.hoverSlot" slot))

(defn rename-scene "The scene's name typed in the top bar, if it isn't empty; the picker lists it so." [text]
  (let [name (slice (str/trim text) 0 63)]
    (when-not (str/blank? name)
      (set "state.sceneName" name)
      (set "state.scenes" (map #(if (= (:id %) state.activeScene) (assoc % :name name) %) state.scenes)))))

(defn rename-layer "The view layer's name typed in the top bar, if it isn't empty." [text]
  (let [name (str/trim text)]
    (when-not (str/blank? name) (set "state.layerName" (slice name 0 63)))))

;;; The outliner (outliner.lisp): a row is an object's (id) or a collection's (collection)

(defn pick-row "A row clicked: its object is selected (with Shift, added), its collection made active, its tab shown."
  [id collection tab shift]
  (when-some [_ id] (select-node id shift))
  (when-some [_ collection] (set "state.activeCollection" collection))
  (when-some [_ tab] (set "state.propTab" tab)))

(defn start-row-rename "A row double-clicked: its object or collection is renamed in place." [id collection data]
  (when (and (some? id) (not= data ""))
    (set "state.renaming" id)
    (focus-rename))
  (when-some [_ collection]
    (set "state.renaming" (str "coll:" collection))
    (focus-rename)))

(defn row-menu "A row right-clicked: its collection's menu, or its object's, the object selected first." [id collection]
  (when-some [_ collection]
    (set "state.activeCollection" collection)
    (open-popup "collectionContext" event.x event.y))
  (when-some [_ id]
    (when-not (contains state.view.selection id) (select-node id false))
    (open-popup "outlinerContext" event.x event.y)))

(defn submit-row-rename "A name typed in a row: its collection or its object takes it." [id collection text]
  (if-some [_ collection] (rename-collection collection text) (rename id text))
  (set "state.renaming" nil))

(defn toggle-open "A row's chevron: what is under the row is shown or hidden." [key]
  (set "state.outlinerOpen" (update state.outlinerOpen key not)))

(defn toggle-row-flag "A row's switch: its object's, or else its collection's." [id collection key]
  (if-some [_ id] (toggle-flag id key) (toggle-collection collection key)))
