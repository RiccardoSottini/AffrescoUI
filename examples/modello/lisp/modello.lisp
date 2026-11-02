;;; modello.lisp --- what Modello does to its scene beyond one step: undo and redo, and the selected objects
;;;
;;; The parts (parts/*.lisp) say what is shown, and keep the actions that are one step, the tools that follow the
;;; pointer, and those that reach outside (opening, saving, exporting). This file holds Undo and Redo, and the edits of
;;; the selected objects that a list of steps had to make one path at a time. A function reads the running app through
;;; paths (`state.scene`, `state.view.selection`) and changes it only through steps (`set`, `push`...). A handler runs
;;; one by name, its arguments in `args`, as it would an action: {:run clearTransform :with {:args {:key scale ...}}}.

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

(defn ids "The ids of nodes." [nodes]
  (map (fn [n] (:id n)) nodes))

(defcommand undo "" :title "Undo" :category "Edit" :icon undo-2 :enablement "{{length(state.undoStack) > 0}}"
  []
  (set "state.lastOp" nil)
  (swap-scene "state.undoStack" "state.redoStack" state.undoStack)
  (let [left (ids state.scene.nodes)]
    (set "state.view.selection" (filter (fn [id] (contains left id)) state.view.selection))))

(defcommand redo "" :title "Redo" :category "Edit" :icon redo-2 :enablement "{{length(state.redoStack) > 0}}"
  []
  (swap-scene "state.redoStack" "state.undoStack" state.redoStack))

;;; The selected objects

(defn selected? "Whether a node is selected." [n]
  (contains state.view.selection (:id n)))

(defn change-nodes "Changes the nodes a test picks, and leaves the others as they are." [pick change]
  (set "state.scene.nodes" (map (fn [n] (if (pick n) (change n) n)) state.scene.nodes)))

(defn locked? "Whether a node locks an item of one of its lists, such as the y of its translation." [n key i]
  (= (get (get n (str "lock_" key)) i) true))

(defcommand deleteSelected "" :title "Delete" :category "Object" :icon trash-2
  :enablement "{{length(state.view.selection) > 0}}"
  []
  (dismiss)
  (snapshot)
  (set "state.scene.nodes" (remove selected? state.scene.nodes))
  (set "state.view.selection" [])
  (set "state.active" nil)
  (set "state.last" "Delete"))

(defcommand hideSelected "" :title "Hide Selected" :category "Object" :icon eye-off []
  (change-nodes selected? (fn [n] (merge n {:visible false})))
  (set "state.view.selection" []))

(defcommand hideUnselected "" :title "Hide Unselected" :category "Object" []
  (change-nodes (fn [n] (not (selected? n))) (fn [n] (merge n {:visible false}))))

(defcommand revealHidden "" :title "Show Hidden Objects" :category "Object" :icon eye []
  (set "state.view.selection" (ids (filter (fn [n] (= (:visible n) false)) state.scene.nodes)))
  (change-nodes (fn [n] true) (fn [n] (merge n {:visible true}))))

;; args: the list's key (translation, rotationDegrees or scale), the value, and the title Adjust Last shows
(defn clearTransform "Puts a list of the selected nodes back to a value, item by item, but where a node locks the item."
  [args]
  (dismiss)
  (when (and (nil? state.modal) (> (count state.view.selection) 0))
    (snapshot)
    (let [key (:key args)]
      (change-nodes selected?
                    (fn [n] (assoc n key (map-indexed (fn [i v] (if (locked? n key i) v (:value args))) (get n key))))))
    (set "state.last" (:title args))
    (run "autoKey")))

(defcommand selectionToCursor "" :title "Selection to Cursor" :category "Object"
  :enablement "{{length(state.view.selection) > 0}}"
  []
  (dismiss)
  (when (> (count state.view.selection) 0)
    (snapshot)
    (change-nodes selected?
                  (fn [n] (assoc n "translation"
                                 (map-indexed (fn [i c] (if (locked? n "translation" i) (get (:translation n) i) c))
                                              state.cursor))))
    (set "state.last" "Selection to Cursor")
    (run "autoKey")))
