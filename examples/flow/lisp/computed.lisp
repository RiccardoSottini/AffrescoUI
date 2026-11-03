;;; computed.lisp --- what Flow works out from its state and its records
;;;
;;; Each defcomputed is a value the screen reads as computed.name: worked out when the state it reads changes, at an
;;; expression's speed (SCREEN_FORMAT.md, "Values, expressions and state"). The forms are Lisp; `it` is the item of a
;;; function on each item, as in (filter (fn [it] ...) list).

;;; The board's columns and cards

(defcomputed columns
  (map-indexed (fn [index it] (merge it {:cards (get [data.lane0 data.lane1 data.lane2 data.lane3] index)}))
                 (get state.lanes state.boardId)))

(defcomputed lastColumn (- (count computed.columns) 1))

(defcomputed card (if (nil? state.selected) nil (get-in computed.columns [state.selected.c :cards state.selected.i])))

(defcomputed counts (map (fn [it] (count it.cards)) computed.columns))

(defcomputed allCards
  "Every card shown, with its column `c` and its place `at`: the table's rows, the pickers and the timeline."
  (cardsWithPlaces computed.columns))

(defcomputed over
  (map (fn [it] (and (some? it.limit) (> (count it.cards) it.limit) (= (count computed.activeFilters) 0)))
         computed.columns))

(defcomputed next
  (if (nil? state.selected)
        computed.visible.0
        (find (fn [it] (or (> it.c state.selected.c) (and (= it.c state.selected.c) (> it.at state.selected.i))))
              computed.visible)))

(defcomputed prev
  (if (nil? state.selected)
        nil
        (get computed.visible
             (- (count (filter (fn [it]
                                 (or (< it.c state.selected.c)
                                     (and (= it.c state.selected.c) (< it.at state.selected.i))))
                               computed.visible))
                1))))

;;; What the filters, the search and the scope leave

(defcomputed matches
  (filter (fn [it]
              (and (or (= state.scope "all")
                       (if (= state.scope "backlog")
                           (= it.c 0)
                           (and (> it.c 0) (< it.c (- (count computed.columns) 1)))))
                   (or (= state.filter "all") (= it.tag state.filter))
                   (or (< state.fStatus 0) (= it.c state.fStatus))
                   (or (< state.fPrio 0) (= (or-else it.priority 0) state.fPrio))
                   (or (< state.fProject 0) (= (or-else (get state.projectOf it.tag) 0) state.fProject))
                   (or (= state.query "") (some? (fuzzy state.query it.title)))))
            computed.allCards))

(defcomputed matchRefs (map (fn [it] it.ref) computed.matches))

(defcomputed visible (filter (fn [it] (or (= state.owner "") (= it.who state.owner))) computed.matches))

(defcomputed visibleRefs (map (fn [it] it.ref) computed.visible))

(defcomputed filterOptions
  (cond (= state.filterProp "status") (map-indexed (fn [index it]
                         {:v index
                          :title it.name
                          :icon (or-else (get state.statusIcons index) "circle")
                          :color it.color
                          :on (= state.fStatus index)})
                       computed.columns)
          (= state.filterProp "assignee") (map (fn [it]
                                                 {:v it.who
                                                  :title it.name
                                                  :icon "circle-user"
                                                  :color it.color
                                                  :on (= state.owner it.who)})
               state.members)
          (= state.filterProp "priority") (map-indexed (fn [index it]
                         {:v index
                          :title (t (str "priority." index))
                          :icon it
                          :color "@color.muted"
                          :on (= state.fPrio index)})
                       state.priorities)
          :else (map (fn [it]
                       {:v it :title (t (str "tag." it)) :icon "circle" :color (str "@" it) :on (= state.filter it)})
               (slice state.filters 1))))

(defcomputed activeFilters
  (concat (if (>= state.fStatus 0) [{:prop "status" :value (get-in computed.columns [state.fStatus :name])}] [])
            (if (not= state.owner "")
                [{:prop "assignee"
                  :value (or-else (:name (find (fn [it] (= it.who state.owner)) state.members)) state.owner)}]
                [])
            (if (>= state.fPrio 0) [{:prop "priority" :value (t (str "priority." state.fPrio))}] [])
            (if (not= state.filter "all") [{:prop "label" :value (t (str "tag." state.filter))}] [])
            (if (>= state.fProject 0) [{:prop "project" :value (get-in data.projects [state.fProject :name])}] [])))

(defcomputed filterPropsShown
  (filter (fn [it] (or (= state.filterQuery "") (some? (fuzzy state.filterQuery (t (str "filter." it.0))))))
            state.filterProps))

(defcomputed filterOptionsShown
  (filter (fn [it] (or (= state.filterQuery "") (some? (fuzzy state.filterQuery it.title)))) computed.filterOptions))

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
  (filter (fn [it]
              (and it.enabled
                   (contains ["changeStatus" "changePriority" "changeAssignee" "assignToMe" "editCard" "openIssue"]
                             it.name)))
            commands))

(defcomputed palette
  (map (fn [it] {:title it.title :detail (or-else it.keyLabel "") :run it.name})
         (concat computed.paletteFirst
                 (filter (fn [it]
                           (and it.enabled
                                (= it.category "Issue")
                                (not (contains (map (fn [it] it.name) computed.paletteFirst) it.name))))
                         commands)
                 (filter (fn [it] (and it.enabled (not= it.category "Issue"))) commands))))

(defcomputed propItems
  (cond (= state.propKind "status") (map-indexed (fn [index it]
                         {:title it.name
                          :detail (if (= index state.selected.c) "✓" "")
                          :run "setStatus"
                          :with {:to index}})
                       computed.columns)
          (= state.propKind "priority") (map-indexed (fn [index it]
                         {:title (t (str "priority." index))
                          :detail (if (= index (or-else computed.card.priority 0)) "✓" "")
                          :run "setPriority"
                          :with {:p index}})
                       state.priorities)
          :else (map (fn [it]
                 {:title it.name
                  :detail (if (= it.who computed.card.who) "✓" it.who)
                  :run "setAssignee"
                  :with {:who it.who}})
               state.members)))
