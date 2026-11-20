;;; relations.lisp --- parenting: the selected objects follow the active one (Ctrl P), let go of their parent (Alt P),
;;; or take one in the Object tab's Relations panel; each keeps its place, and the outliner nests it under its parent
;;;
;;; A child's translation is in its parent's space: the viewport turns, moves and hides it with its parent. Parenting
;;; and clearing keep where the child's origin is by its ancestors' translations (a turned or scaled parent still turns
;;; or scales the child).

(ns modello.relations)

;;; Walking up the tree

(defn ancestors-of "The ids above an object, its parent first; the walk stops at a loop or a missing parent." [id]
  (loop [at (:parent (by-id state.scene.nodes id)) seen []]
    (if (or (nil? at) (= at "") (= at id) (contains seen at) (nil? (by-id state.scene.nodes at)))
      seen
      (recur (:parent (by-id state.scene.nodes at)) (conj seen at)))))

(defn origin-of "Where an object's origin is: its translation and its ancestors', added." [id]
  (reduce (fn [sum at]
            (let [[x y z] (or-else (:translation (by-id state.scene.nodes at)) [0 0 0])]
              [(+ (get sum 0) x) (+ (get sum 1) y) (+ (get sum 2) z)]))
          [0 0 0]
          (cons id (ancestors-of id))))

(defn under? "Whether an object is another one, or below it." [id other]
  (or (= other id) (contains (ancestors-of other) id)))

(defn name-of "An object's name." [id]
  (:name (by-id state.scene.nodes id)))

(defn parent-choices "The objects an object can follow: every other one that isn't under it." [id]
  (remove #(under? id (:id %)) (sort-by :name state.scene.nodes)))

(defn parent-items "The Relations panel's menu: None, then each object the active one can follow, the parent checked." []
  (into [{:title (t "properties.none") :checked (= (or-else computed.activeNode.parent "") "") :run "set-parent" :with {:parent nil}}]
        (for [n (parent-choices state.active)]
          {:title n.name :checked (= n.id computed.activeNode.parent) :run "set-parent" :with {:parent n.id}})))

(defn outline-row "An object as the outliner sorts it: its names from the root down (each parent before its children), and its depth."
  [n]
  (let [above (ancestors-of n.id)]
    (assoc n
           :outlinePath (->> above (map name-of) (cons n.name) reverse (str/join "/"))
           :outlineDepth (count above))))

;;; Changing a parent

(defn can-parent? "Whether an object can take a parent (nil for none) other than the one it has." [id parent]
  (let [now (or-else (:parent (by-id state.scene.nodes id)) "")]
    (and (not= now (or-else parent ""))
         (or (nil? parent) (not (under? id parent))))))

(defn reparent "Gives an object a parent (nil for none) and keeps its origin where it is; a parent under it is refused."
  [id parent]
  (let [i (node-index id)]
    (when (and (>= i 0) (can-parent? id parent))
      (let [[x y z] (origin-of id)
            [px py pz] (if-some [p parent] (origin-of p) [0 0 0])]
        (set (str "state.scene.nodes[" i "].parent") (or-else parent ""))
        (set (str "state.scene.nodes[" i "].translation") [(- x px) (- y py) (- z pz)])))))

(defcommand parentToActive "Makes the active object the parent of the other selected objects, keeping their places."
  :title ~(t "command.parentToActive") :description ~(t "command.parentToActive.about")
  :category ~(t "category.object") :icon "git-branch-plus"
  :enablement ~(and (some? state.active) (> (count state.view.selection) 1)) []
  (dismiss)
  (when (and (some? state.active) (> (count state.view.selection) 1))
    (snapshot)
    (doseq [id (without state.view.selection state.active)]
      (reparent id state.active))
    (set "state.last" (t "command.parentToActive"))))

(defcommand clearParent "The selected objects let go of their parents, keeping their places."
  :title ~(t "command.clearParent") :description ~(t "command.clearParent.about")
  :category ~(t "category.object") :icon "unlink" :enablement ~(some #(not= (or-else (:parent %) "") "") computed.selectedNodes) []
  (dismiss)
  (when (some #(not= (or-else (:parent %) "") "") computed.selectedNodes)
    (snapshot)
    (doseq [id state.view.selection]
      (reparent id nil))
    (set "state.last" (t "command.clearParent"))))

(defn set-parent "The Relations panel's parent chosen for the active object: one, or none with nil." [parent]
  (dismiss)
  (when (and (some? state.active) (not= (or-else computed.activeNode.parent "") (or-else parent "")))
    (snapshot)
    (reparent state.active parent)))

;;; The Relations panel

(defview relations-panel "The Object tab's Relations: the active object's parent, picked from the others, or none." []
  `(node :when ~(contains computed.propShown "relations") :use propPanel :with {:key relations :body 32}
     (node :height 20 :width "100%" :color transparent
       ~(caption "properties.parent")
       (node :anchors {:left "41%" :right 22 :top 0 :bottom 0} :color transparent
         (node#prop-parent
           :use hButton
           :with {:width 150
                  :menuWidth 180
                  :icon ~(if (= (or-else computed.activeNode.parent "") "") "circle-off" "git-branch")
                  :text ~(or-else (:name (by-id state.scene.nodes computed.activeNode.parent)) (t "properties.none"))
                  :chevron true
                  :back @field
                  :tooltip ~(t "properties.parentTip")
                  :open state.open.parentPicker
                  :items ~(parent-items)})))))
