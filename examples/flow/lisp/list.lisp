;;; list.lisp --- Flow's list view, included by index.screen.lisp: every card in one list, grouped by column, a row
;;; each.

;;; Components

(defcomponent listView "The cards grouped by column: a header with the column's name and count, then a row a card"
  {}
  `(column#list :scroll vertical :radius @radius.card :color @color.surface :role list
     ~(repeat [column computed.columns :index c]
       `(node#listGroup :anchors {:left 0 :right 0}
          :height ~(+ 36
                      (if (contains state.collapsedGroups c)
                          0
                          (* 36 (count (filter (fn [it] (contains computed.visibleRefs it.ref)) column.cards)))))
          :color transparent
          :role group
          :label ~column.name
          :onDrop ~(handler moveCard event.data.from event.data.at c)
          ~(list-header column c)
          (column#listRows :when ~(not (contains state.collapsedGroups c)) :anchors {:left 0
                                                                                     :right 0
                                                                                     :top 36
                                                                                     :bottom 0}
            ~(repeat [row (cards-in-order c) :index k]
              (list-row column c row)))))))

;;; Views

(defview list-collapse "A group's chevron in the list, which folds its rows." [column c]
  `(label#listCollapse :icon ~(if (contains state.collapsedGroups c) "chevron-right" "chevron-down")
     :text ""
     :textColor @color.muted
     :width 22
     :height 22
     :radius @radius.chip
     :align center
     :anchors {:left 8 :top 7}
     :hover {:color @color.chipHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "list.collapse" {:name column.name})
     :onClick {:set state.collapsedGroups
               :to ~(if (contains state.collapsedGroups c)
                        (filter (fn [it] (not= it c)) state.collapsedGroups)
                        (concat state.collapsedGroups [c]))}))

(defview list-header "A group's header in the list: its column's name and count." [column c]
  `(node#listHeader :anchors {:left 0 :right 0 :top 0} :height 36 :color @color.column
     ~(list-collapse column c)
     (label#listStatus :icon ~(or-else (get state.statusIcons c) "circle")
       :text ""
       :textColor ~column.color
       :fitText true
       :anchors {:left 36 :top 0 :bottom 0}
       :align center)
     (row#listTitle :gap 10 :alignItems center :anchors {:left 60 :right 52 :top 0 :bottom 0}
       (label#listName :font small
         :text ~column.name
         :textColor @color.text
         :fitText true
         :height 36
         :align {:horizontal start :vertical center})
       {:id listCount
        :style caption
        :text ~(count column.cards)
        :textColor ~(if (get computed.over c) "@bug" "@color.muted")
        :fitText true
        :height 36
        :align {:horizontal start :vertical center}})
     (label#listAdd :icon plus
       :text ""
       :textColor @color.muted
       :width 28
       :height 28
       :radius @radius.chip
       :align center
       :anchors {:right 12 :top 4}
       :hover {:color @color.chipHover}
       :cursor hand
       :focusable true
       :role button
       :label ~(t "composer.in" {:column column.name})
       :tooltip ~(t "composer.in" {:column column.name})
       :onClick ~(handler openComposer c))))

(defview list-row "A card as a row of the list." [column c row]
  `(node#listRow :let {:i ~row.at :card ~(get column.cards row.at)}
     :when ~(contains computed.visibleRefs card.ref)
     :anchors {:left 0 :right 0}
     :height 36
     :color ~(if (or (contains state.marked card.ref)
                     (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)))
                 "@color.accentSoft"
                 "transparent")
     :hover {:color ~(if (or (contains state.marked card.ref)
                             (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)))
                         "@color.accentSoft"
                         "@color.column")}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(t "card.label"
                {:title card.title
                 :tag (t (str "tag." card.tag))
                 :who (or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who)
                 :column column.name})
     :onFocus ~(handler selectCard c i)
     :keys {:enter editCard :space togglePeek}
     :drag {:data {:from ~c :at ~i}}
     :onClick ~(handler selectCard c i)
     :onDoubleClick [~(handler selectCard c i) ~(handler openIssue)]
     :onContextMenu ~(handler openCardMenu c i)
     {:id rowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}
     ~(row-priority)
     ~(row-ref)
     ~(row-status column c)
     (label#rowTitle :text ~card.title
       :textColor @color.text
       :anchors {:left 44 :right 316 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     ~(row-tag)
     ~(row-due c)
     ~(row-points)
     ~(row-who)))

(defview row-status "A list row's status icon." [column c]
  `(label#rowStatus :icon ~(or-else (get state.statusIcons c) "circle")
     :text ""
     :fitText true
     :textColor ~column.color
     :anchors {:left 18 :top 0 :bottom 0}
     :align center
     :tooltip ~column.name))

(defview row-ref "A list row's id." []
  `(label#rowRef :when ~(not (contains state.hidden "id"))
     :font small
     :text ~(or-else card.ref "")
     :textColor @color.muted
     :width 56
     :anchors {:right 252 :top 0 :bottom 0}
     :align {:horizontal end :vertical center}))

(defview row-points "A list row's points." []
  `{:id rowPoints
    :when ~(not (contains state.hidden "estimate"))
    :style caption
    :text ~card.points
    :width 20
    :anchors {:right 52 :top 0 :bottom 0}
    :align {:horizontal end :vertical center}
    :tooltip ~(t "card.points" {:n card.points})})

(defview row-priority "A list row's priority icon." []
  `(label#rowPriority :when ~(not (contains state.hidden "priority"))
     :icon ~(get state.priorities (or-else card.priority 0))
     :text ""
     :fitText true
     :textColor ~(if (= card.priority 1) "@bug" "@color.muted")
     :anchors {:right 136 :top 0 :bottom 0}
     :width 22
     :align center
     :tooltip ~(t (str "priority." (or-else card.priority 0)))))

(defview row-who "A list row's owner." []
  `{:id rowWho
    :when ~(not (contains state.hidden "assignee"))
    :style badge
    :text ~card.who
    :width 22
    :height 22
    :radius 999
    :anchors {:right 20 :top 7}
    :color ~(or-else (:color (find (fn [it] (= it.who card.who)) state.members)) "@color.muted")
    :textColor "#ffffff"
    :tooltip ~(or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who)})

(defview row-tag "A list row's label." []
  `(label#rowTag :when ~(not (contains state.hidden "label"))
     :font small
     :runs [{:text "●" :font symbols :size 10 :color ~(str "@" card.tag)} ~(str " " (t (str "tag." card.tag)))]
     :fitText true
     :padding 6
     :height 22
     :radius 999
     :borderWidth 1
     :borderColor @color.border
     :color transparent
     :textColor @color.text
     :anchors {:right 166 :top 7}))

(defview row-due "A list row's due date." [c]
  `{:id rowDue
    :style caption
    :when ~(and (not (contains state.hidden "due")) (some? card.due))
    :text ~(formatDate (dateMs card.due) "MMM d" "utc")
    :textColor ~(if (and (< (daysBetween state.today card.due) 0) (not= c computed.lastColumn))
                    "@bug"
                    "@color.muted")
    :width 48
    :anchors {:right 80 :top 0 :bottom 0}
    :align {:horizontal end :vertical center}})
