;;; arrange.lisp --- Groups: objects that share a group are selected, moved, sized and aligned as one
;;;
;;; A group is a name its objects carry (`:group`, from deck.next). Selecting one of them selects them all, so the
;;; canvas moves and sizes them together and Arrange aligns them as one box; Ungroup takes the name away. A pasted
;;; copy of a group is a group of its own.

(ns sinopia.parts.arrange)

;;; Definitions

(defcomputed canGroup "Whether Group can join the selection: two objects or more, not one group already."
  (and (> (count computed.selected) 1)
       (not (every? #(and (some? (:group %)) (= (:group %) computed.first.group)) computed.selected))))

(defcomputed canUngroup "Whether Ungroup has something to take apart: a selected object in a group."
  (not (every? #(= (or-else (:group %) "") "") computed.selected)))

;;; What its commands do

(defn with-groups "A selection of object ids, with every object of the groups it touches." [ids]
  (let [groups (->> computed.objects (filter #(contains ids (:id %))) (keep :group) vec)]
    (if (empty? groups)
      ids
      (->> computed.objects
           (filter #(or (contains ids (:id %)) (contains groups (or-else (:group %) ""))))
           (map :id)))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn groupObjects "Joins the selected objects into one group." []
  (when computed.canGroup
    (setObjects {:group (str "g" state.deck.next)})
    (increment "state.deck.next" :by 1)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn ungroupObjects "Takes the selected objects out of their groups; they stay selected." []
  (when computed.canUngroup
    (setObjects {:group nil})))
