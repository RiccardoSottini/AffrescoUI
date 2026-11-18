;;; computed.lisp --- what Flow works out from its state and its records
;;;
;;; Each defcomputed is a value the screen reads as computed.name: worked out when the state it reads changes, at an
;;; expression's speed (SCREEN_FORMAT.md, "Values, expressions and state"). The forms are Lisp; `%` is the item of a
;;; one-line function on each item, as in (filter #(...) list), and `it` the item of a longer one, (fn [it] ...).

;;; The board's columns and cards

(defcomputed columns
  (map-indexed (fn [index it] (merge it {:cards (get [data.lane0 data.lane1 data.lane2 data.lane3] index)}))
               (get state.lanes state.boardId)))

(defcomputed lastColumn (dec (count computed.columns)))

(defcomputed card (when-some [at state.selected] (get-in computed.columns [at.c :cards at.i])))

(defcomputed counts (map #(count (:cards %)) computed.columns))

(defcomputed allCards
  "Every card shown, with its column `c` and its place `at`: the table's rows, the pickers and the timeline."
  (cardsWithPlaces computed.columns))

(defcomputed over
  (map #(and (some? (:limit %)) (> (count (:cards %)) (:limit %)) (empty? computed.activeFilters))
       computed.columns))

(defcomputed next
  (if-some [at state.selected]
    (find #(or (> (:c %) at.c) (and (= (:c %) at.c) (> (:at %) at.i))) computed.visible)
    computed.visible.0))

(defcomputed prev
  (when-some [at state.selected]
    (get computed.visible
         (dec (count (filter #(or (< (:c %) at.c) (and (= (:c %) at.c) (< (:at %) at.i))) computed.visible))))))

;;; What the filters, the search and the scope leave

(defcomputed matches
  (filter (fn [it]
            (and (or (= state.scope :all)
                     (if (= state.scope :backlog)
                       (zero? it.c)
                       (and (pos? it.c) (< it.c (dec (count computed.columns))))))
                 (or (= state.filter :all) (= it.tag state.filter))
                 (or (neg? state.fStatus) (= it.c state.fStatus))
                 (or (neg? state.fPrio) (= (or-else it.priority 0) state.fPrio))
                 (or (neg? state.fProject) (= (or-else (get state.projectOf it.tag) 0) state.fProject))
                 (or (= state.query "") (some? (fuzzy state.query it.title)))))
          computed.allCards))

(defcomputed matchRefs (map :ref computed.matches))

(defcomputed visible (filter #(or (= state.owner "") (= (:who %) state.owner)) computed.matches))

(defcomputed visibleRefs (map :ref computed.visible))

(defcomputed filterOptions
  (case state.filterProp
    :status (map-indexed (fn [index it]
                           {:v index
                            :title it.name
                            :icon (or-else (get state.statusIcons index) "circle")
                            :color it.color
                            :on (= state.fStatus index)})
                         computed.columns)
    :assignee (map (fn [it] {:v it.who :title it.name :icon "circle-user" :color it.color :on (= state.owner it.who)})
                   state.members)
    :priority (map-indexed (fn [index it]
                             {:v index :title (t (str "priority." index)) :icon it :color "@color.muted"
                              :on (= state.fPrio index)})
                           state.priorities)
    (map (fn [it] {:v it :title (t (str "tag." it)) :icon "circle" :color (str "@" it) :on (= state.filter it)})
         (slice state.filters 1))))

(defcomputed activeFilters
  (concat (if (>= state.fStatus 0) [{:prop "status" :value (get-in computed.columns [state.fStatus :name])}] [])
          (if (not= state.owner "")
            [{:prop "assignee" :value (or-else (:name (find #(= (:who %) state.owner) state.members)) state.owner)}]
            [])
          (if (>= state.fPrio 0) [{:prop "priority" :value (t (str "priority." state.fPrio))}] [])
          (if (not= state.filter :all) [{:prop "label" :value (t (str "tag." state.filter))}] [])
          (if (>= state.fProject 0) [{:prop "project" :value (get-in data.projects [state.fProject :name])}] [])))

(defcomputed filterPropsShown
  (filter #(or (= state.filterQuery "") (some? (fuzzy state.filterQuery (t (str "filter." (first %))))))
          state.filterProps))

(defcomputed filterOptionsShown
  (filter #(or (= state.filterQuery "") (some? (fuzzy state.filterQuery (:title %)))) computed.filterOptions))

;;; The table, the palette and the pickers

(defcomputed rows
  (map (fn [it]
         {:id (str it.c "-" it.at)
          :cells [it.ref
                  (t (str "priority." (or-else it.priority 0)))
                  it.title
                  (get-in computed.columns [it.c :name])
                  it.tag
                  it.who
                  it.points]})
       computed.allCards))

(defcomputed tableColumns
  [{:id "ref" :title (t "table.id") :width state.columnWidths.ref}
   {:id "priority" :title (t "table.priority") :width state.columnWidths.priority}
   {:id "title" :title (t "table.card") :width state.columnWidths.title}
   {:id "column" :title (t "table.column") :width state.columnWidths.column}
   {:id "tag" :title (t "table.label") :width state.columnWidths.tag}
   {:id "who" :title (t "table.owner") :width state.columnWidths.who}
   {:id "points" :title (t "table.points") :width state.columnWidths.points}])

(defcomputed picks
  (map (fn [it]
         {:title it.title
          :detail (str it.ref " · " (get-in computed.columns [it.c :name]))
          :run "goToCard"
          :with {:item {:c it.c :at it.at}}})
       computed.allCards))

(defcomputed paletteFirst
  (filter #(and (:enabled %)
                (contains ["changeStatus" "changePriority" "changeAssignee" "assignToMe" "editCard" "openIssue"]
                          (:name %)))
          commands))

(defcomputed palette
  "The palette's rows: the commands, but those an agent runs with arguments (cardMove, cardAdd...), which it can't give."
  (map (fn [it] {:title it.title :detail (or-else it.keyLabel "") :run it.name})
       (concat computed.paletteFirst
               (filter (fn [it]
                         (and it.enabled
                              (= it.category "Issue")
                              (not= (slice it.name 0 4) "card")
                              (not (contains (map :name computed.paletteFirst) it.name))))
                       commands)
               (filter #(and (:enabled %) (not= (:category %) "Issue") (not= (slice (:name %) 0 4) "card")) commands))))

(defcomputed propItems
  (case state.propKind
    :status (map-indexed (fn [index it]
                           {:title it.name :detail (if (= index state.selected.c) "✓" "") :run "setStatus"
                            :with {:to index}})
                         computed.columns)
    :priority (map-indexed (fn [index it]
                             {:title (t (str "priority." index))
                              :detail (if (= index (or-else computed.card.priority 0)) "✓" "")
                              :run "setPriority"
                              :with {:p index}})
                           state.priorities)
    (map (fn [it] {:title it.name :detail (if (= it.who computed.card.who) "✓" it.who) :run "setAssignee"
                   :with {:who it.who}})
         state.members)))
