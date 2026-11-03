;;; transform.lisp --- what a click in the viewport does, and moving, turning and scaling after the pointer
;;;
;;; G, R and S start a transform the viewport runs itself: X, Y and Z lock an axis (Shift a plane), digits type the
;;; amount, Enter or a click confirms, Escape or a right click cancels. When it ends, `transformed` keeps the new places,
;;; and the Adjust Last Operation panel can change the amount afterwards. Here too: Clear Location, Rotation and Scale,
;;; and the 3D cursor's snapping.

;;; A click in the viewport

(defn box-select "A box drawn in the viewport: what it holds is selected, or with Shift added to the selection." []
  (set "state.view.selection"
       (concat (if event.shift (remove (fn [id] (contains event.ids id)) state.view.selection) [])
               (filter (fn [id] (contains computed.pickable id)) event.ids)))
  (when (not (contains state.view.selection state.active))
    (set "state.active" (last state.view.selection))))

(defn pick "A click or a box in the viewport: selects, opens the menu, places the 3D cursor, measures or adds, by the tool."
  []
  (cond (some? state.modal) nil
        (= event.type "boxSelect") (box-select)
        (not= event.type "pick") nil
        (and (= event.button "right") event.shift) (set "state.cursor" (map rounded event.point))
        (= event.button "right") (openPopup "objectContext" (+ nodes.viewport.x event.x) (+ nodes.viewport.y event.y))
        (= state.tool "cursor") (set "state.cursor" (map rounded event.point))
        (= state.tool "measure") (measurePoint event.point event.x event.y)
        (= state.tool "add") (addObject "mesh" "cube" "Cube" nil (map rounded event.point))
        (or (nil? event.id) (not (contains computed.pickable event.id)))
          (when (not event.shift) (set "state.view.selection" []))
        :else (selectNode event.id event.shift)))

(defn measurePoint "A point the Measure tool picked: two make a line, and a third, or a turned view, starts again."
  [point x y]
  (when (or (>= (count state.measure.points) 2) (not= state.measure.camera state.view.camera))
    (set "state.measure" {:points [] :screen [] :camera state.view.camera}))
  (push "state.measure.points" (map rounded point))
  (push "state.measure.screen" [x y]))

;;; Moving, turning and scaling after the pointer

(defn startTransform "Starts moving, turning or scaling the selection after the pointer." [mode]
  (when (not (empty? state.view.selection))
    (set "state.modal" {:mode mode :axis "view" :typed ""})
    (set "state.modalText" computed.modalName)
    (transform "#viewport" :mode mode)))

(defcommand grab "Moves the selection after the pointer."
  :title ~(t "command.grab") :description ~(t "command.grab.about")
  :category ~(t "category.object") :icon "move" []
  (startTransform "translate"))

(defcommand rotate "Turns the selection after the pointer."
  :title ~(t "command.rotate") :description ~(t "command.rotate.about")
  :category ~(t "category.object") :icon "rotate-cw" []
  (startTransform "rotate"))

(defcommand resize "Scales the selection after the pointer."
  :title ~(t "command.resize") :description ~(t "command.resize.about")
  :category ~(t "category.object") :icon "scale-3-d" []
  (startTransform "scale"))

(defn lockAxis "X, Y or Z while transforming: moves along that axis only; the same key again frees it." [axis]
  (set "state.modal.axis" (if (= state.modal.axis axis) "view" axis))
  (transform "#viewport" :axis state.modal.axis))

(defn typeDigit "A digit, a point, a minus or Backspace while transforming: the amount typed." [key]
  (let [typed state.modal.typed]
    (set "state.modal.typed"
         (cond (= key "back") (slice typed 0 (max 0 (dec (count typed))))
               (= key "-") (if (= (slice typed 0 1) "-") (slice typed 1) (str "-" typed))
               :else (str typed key))))
  (transform "#viewport" :value (number state.modal.typed)))

(defn confirmTransform "Enter while transforming: keeps it." []
  (transform "#viewport" :end "confirm"))

(defn cancelTransform "Escape while transforming: puts everything back." []
  (transform "#viewport" :end "cancel")
  (set "state.modal" nil))

(defn kept "A node's list, item by item: a locked item keeps its own, the others take the new value, rounded."
  [n key values scale]
  (map-indexed (fn [i v] (if (locked? n key i) (get (get n key) i) (round-to v scale))) values))

(defn placed "A node where a transform left it, as moved, but its locked items." [n moved]
  (merge n {:translation (kept n "translation" (:translation moved) 10000)
            :rotationDegrees (kept n "rotationDegrees" (:rotationDegrees moved) 1000)
            :scale (kept n "scale" (:scale moved) 10000)}))

(defn transform-name "What a transform is called: Move, Rotate or Resize." [mode]
  (cond (= mode "translate") (t "op.move")
        (= mode "rotate") (t "op.rotate")
        :else (t "op.resize")))

(defn change-of "What a transform changed, item by item, for Adjust Last to change again: nil for several turned objects."
  [mode base moved]
  (cond (= mode "translate")
          (map-indexed (fn [i v] (round-to (- v (get (:translation base) i)) 10000)) (:translation moved))
        (= mode "scale")
          (map-indexed (fn [i v] (let [was (get (:scale base) i)] (if (= was 0) 1 (round-to (/ v was) 10000))))
                       (:scale moved))
        (= (count state.lastBase) 1)
          (map-indexed (fn [i v] (round-to (- v (get (:rotationDegrees base) i)) 100)) (:rotationDegrees moved))
        :else nil))

(defn transform-ended "A transform confirmed: the new places, kept for Undo, and what Adjust Last can change." []
  (snapshot)
  (let [before (:nodes (last state.undoStack))
        moved event.nodes]
    (set "state.lastBase" (map (fn [m] (by-id before (:id m))) moved))
    (change-nodes (fn [n] (some? (by-id moved (:id n)))) (fn [n] (placed n (by-id moved (:id n)))))
    (set "state.last" event.text)
    (set "state.lastOp" {:title (transform-name event.mode)
                         :kind event.mode
                         :base state.lastBase
                         :values (change-of event.mode (first state.lastBase) (first moved))}))
  (autoKey))

(defn transformed "What the viewport says while transforming: the amount as it changes, then how it ended." []
  (if (= event.phase "preview")
    (set "state.modalText" event.text)
    (do (set "state.modal" nil)
        (set "state.modalText" "")
        (when (= event.phase "end") (transform-ended)))))

;;; Adjust Last Operation

(defcommand adjustLastPanel "Opens the Adjust Last Operation panel."
  :title ~(t "command.adjustLastPanel") :description ~(t "command.adjustLastPanel.about")
  :category ~(t "category.edit") []
  (set "state.redoOpen" true))

(defn adjusted "What an object of the last operation becomes with its amounts changed." [op centre b]
  (let [values (:values op)]
    (cond (= (:kind op) "scale")
            {:translation (map-indexed (fn [i v]
                                         (if (locked? b "translation" i)
                                           v
                                           (+ (get centre i) (* (- v (get centre i)) (get values i)))))
                                       (:translation b))
             :scale (map-indexed (fn [i v] (if (locked? b "scale" i) v (round-to (* v (get values i)) 10000))) (:scale b))}
          (= (:kind op) "rotate")
            {:rotationDegrees (map-indexed (fn [i v] (if (locked? b "rotationDegrees" i) v (round-to (+ v (get values i)) 100)))
                                           (:rotationDegrees b))}
          :else
            {:translation (map-indexed (fn [i v] (if (locked? b "translation" i) v (round-to (+ v (get values i)) 10000)))
                                       (:translation b))})))

(defn adjustLast "A field of the Adjust Last Operation panel: one amount of the last operation, done again from before it."
  [axis value]
  (when (and (some? state.lastOp) (some? state.lastOp.values))
    (set (str "state.lastOp.values[" axis "]") (rounded value))
    (let [op state.lastOp
          centre computed.lastCentre]
      (change-nodes (fn [n] (some? (by-id (:base op) (:id n))))
                    (fn [n] (let [b (by-id (:base op) (:id n))] (merge b (adjusted op centre b))))))
    (autoKey)))

;;; Clearing

;; key: the list (translation, rotationDegrees or scale); value: what each item goes back to; title: what Adjust Last
;; Operation shows.
(defn clearTransform "Puts a list of the selected objects back to a value, item by item, but where an object locks the item."
  [key value title]
  (dismiss)
  (when (and (nil? state.modal) (not (empty? state.view.selection)))
    (snapshot)
    (change-nodes selected?
                  (fn [n] (assoc n key (map-indexed (fn [i v] (if (locked? n key i) v value)) (get n key)))))
    (set "state.last" title)
    (autoKey)))

(defcommand clearLocation "Puts the selected objects back at the origin."
  :title ~(t "command.clearLocation") :description ~(t "command.clearLocation.about")
  :category ~(t "category.object") :enablement ~(> (count state.view.selection) 0) []
  (clearTransform "translation" 0 (t "command.clearLocation")))

(defcommand clearRotation "Turns the selected objects back straight."
  :title ~(t "command.clearRotation") :description ~(t "command.clearRotation.about")
  :category ~(t "category.object") :enablement ~(> (count state.view.selection) 0) []
  (clearTransform "rotationDegrees" 0 (t "command.clearRotation")))

(defcommand clearScale "Gives the selected objects their own size back."
  :title ~(t "command.clearScale") :description ~(t "command.clearScale.about")
  :category ~(t "category.object") :enablement ~(> (count state.view.selection) 0) []
  (clearTransform "scale" 1 (t "command.clearScale")))

;;; Snapping and the 3D cursor

(defcommand snapMenu "Opens the Snap menu."
  :title ~(t "command.snapMenu") :description ~(t "command.snapMenu.about")
  :category ~(t "category.object") []
  (when (nil? state.modal) (openPopup "snap" nil nil)))

(defcommand selectionToCursor "Moves the selected objects to the 3D cursor, but the items they lock."
  :title ~(t "command.selectionToCursor") :description ~(t "command.selectionToCursor.about")
  :category ~(t "category.object") :enablement ~(> (count state.view.selection) 0) []
  (dismiss)
  (when (not (empty? state.view.selection))
    (snapshot)
    (change-nodes selected?
                  (fn [n] (assoc n "translation"
                                 (map-indexed (fn [i c] (if (locked? n "translation" i) (get (:translation n) i) c))
                                              state.cursor))))
    (set "state.last" (t "command.selectionToCursor"))
    (autoKey)))

(defn middle "The middle of the places of nodes, rounded." [nodes]
  (map (fn [i] (rounded (/ (reduce + 0 (map (fn [n] (get (:translation n) i)) nodes)) (count nodes)))) (range 3)))

(defcommand cursorToSelected "Moves the 3D cursor to the middle of the selected objects."
  :title ~(t "command.cursorToSelected") :description ~(t "command.cursorToSelected.about")
  :category ~(t "category.object") :enablement ~(> (count state.view.selection) 0) []
  (dismiss)
  (let [chosen (filter selected? state.scene.nodes)]
    (when (not (empty? chosen)) (set "state.cursor" (middle chosen)))))

(defcommand cursorToActive "Moves the 3D cursor to the active object."
  :title ~(t "command.cursorToActive") :description ~(t "command.cursorToActive.about")
  :category ~(t "category.object") :enablement ~(some? computed.activeNode) []
  (dismiss)
  (when (some? computed.activeNode) (set "state.cursor" computed.activeNode.translation)))

(defcommand cursorToOrigin "Moves the 3D cursor back to the world's origin."
  :title ~(t "command.cursorToOrigin") :description ~(t "command.cursorToOrigin.about")
  :category ~(t "category.object") []
  (dismiss)
  (set "state.cursor" [0 0 0]))
