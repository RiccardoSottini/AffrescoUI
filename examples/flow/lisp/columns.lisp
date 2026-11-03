;;; columns.lisp --- the work area: the board's columns and cards, the table, and the side panel

;;; The work area

(defview work-area
  "The work area: the board, the swimlanes, the list, the table or the timeline, beside the side panel."
  []
  `(split#main :dir auto
     :anchors {:left 220 :right 0 :top "#toolbar.bottom" :bottom "#statusBar.top"}
     :color @color.surface
     :bind state.sizes
     :minSize 280
     :sashSize 6
     :sashColor transparent
     :sashActiveColor @color.accent
     (node#content :color transparent
       (node#lanesView :use swimlanes :when ~(and (= state.view "board") state.swimlanes) :fill true)
       (node#listView :use listView :when ~(= state.view "list") :fill 6)
       (node#timelineView :use timelineView :when ~(= state.view "timeline") :fill 6)
       (row#board :when ~(and (= state.view "board") (not state.swimlanes)) :fill true
         ~(repeat [column computed.columns :index c]
           (column-view column c)))
       ~(card-table))
     (node#details :when ~state.peek :color transparent
       ~(detail-body))))

(defview detail-body "The side panel: the chosen card's drawer, or the cards-per-column chart and the insights." []
  `(node#detailBody :fill 6 :radius @radius.card :color @color.surface
     {:id chartTitle
      :when ~(nil? computed.card)
      :style caption
      :text ~(t "chart.title")
      :icon bar-chart-3
      :anchors {:left 20 :top 18}}
     (node#chart :when ~(nil? computed.card)
       :use barChart
       :with {:values ~computed.counts :color @color.accent :gap 0.4}
       :anchors {:left 20 :right 20 :top 44}
       :height 96)
     (row#chartLabels :when ~(nil? computed.card) :anchors {:left 20 :right 20 :top 146} :height 32
       ~(repeat [column computed.columns]
         `(label#chartLabel :grow 1
            :anchors {:top 0 :bottom 0}
            :font small
            :align {:horizontal center :vertical start}
            :textColor @color.muted
            :text ~(str column.name ": " (count column.cards)))))
     {:id detailLine
      :when ~(nil? computed.card)
      :anchors {:left 20 :right 20 :top 186}
      :height 1
      :color @color.border}
     (label#noCard :when ~(nil? computed.card)
       :anchors {:left 20 :right 20 :top 206}
       :height 60
       :wrap true
       :textColor @color.muted
       :runs [~(t "noCard.select")
              {:text ~(t "noCard.rightClick") :bold true}
              ~(t "noCard.menu")
              {:text ~(t "key.goTo") :bold true}
              ~(t "noCard.goTo")])
     (node#insightsPanel :use insights :when ~(nil? computed.card) :anchors {:left 20 :right 20 :top 276 :bottom 16})
     (node#drawer :use cardDrawer :when ~(some? computed.card) :anchors {:left 8 :right 8 :top 18 :bottom 16})))

;;; The board

(defmacro cards-in-order
  ;; A macro, not a function: what it writes is a repeat's list, which stays an expression, worked out at its speed.
  "A column's cards, in the order the Display menu chose: by priority, by due date (none last), or as placed."
  [c]
  `(sort-by (fn [it]
              (cond (= state.order "priority") (if (= (or-else it.priority 0) 0) 9 it.priority)
                    (= state.order "due") (or-else it.due "9999-12-31")
                    :else it.at))
            (filter (fn [it] (= it.c ~c)) computed.allCards)))

(defview column-view "A column of the board: its title, its cards, and a drop target for cards from another."
  [column c]
  `(node#column :anchors {:top 0 :bottom 0}
     :grow 1
     :color transparent
     :onDrop ~(handler moveCard event.data.from event.data.at c)
     (node#columnBody :fill 4 :radius @radius.card :color @color.column
       (label#columnStatus :icon ~(or-else (get state.statusIcons c) "circle")
         :text ""
         :textColor ~column.color
         :fitText false
         :width 18
         :anchors {:left 10 :top 0}
         :height 40
         :align center)
       ~(column-title column c)
       ~(column-more column c)
       ~(column-add column c)
       (stack#cards :anchors {:left 6 :right 6 :top 40 :bottom 6} :gap 6 :scroll vertical
         ~(repeat [row (cards-in-order c) :index k]
           (card-view column c row))))))

(defview column-title "A column's title: its colour, name and count, red while it is over its limit." [column c]
  `(row#columnTitle :gap 8 :alignItems center :anchors {:left 36 :right 72 :top 0} :height 40
     (label#columnName :font small
       :text ~column.name
       :textColor @color.text
       :fitText true
       :height 40
       :align {:horizontal start :vertical center})
     (label#columnCount :font small
       :text ~(count (filter (fn [it] (= it.c c)) computed.visible))
       :fitText true
       :height 40
       :align {:horizontal start :vertical center}
       :textColor ~(if (get computed.over c) "@bug" "@color.muted")
       :label ~(if (get computed.over c)
                   (t "column.over" {:name column.name :count (count column.cards) :limit column.limit})
                   (count column.cards)))
     {:id columnLimit
      :style caption
      :when ~(and (some? column.limit) (get computed.over c))
      :text ~(t "column.limit" {:limit column.limit})
      :height 40
      :align {:horizontal start :vertical center}
      :textColor ~(if (get computed.over c) "@bug" "@color.muted")}))

(defview column-more "A column's menu button, which opens its menu below it." [column c]
  `(label#columnMore :icon ellipsis
     :text ""
     :fitText false
     :textColor @color.muted
     :width 26
     :height 26
     :radius @radius.chip
     :align center
     :anchors {:right 36 :top 7}
     :hover {:color @color.chipHover}
     :cursor hand
     :focusable true
     :role button
     :tooltip ~(t "column.more")
     :label ~(t "column.more")
     :onClick ~(handler openColumnMenu c)))

(defview column-add "A column's + button, which opens the composer on that column." [column c]
  `(label#columnAdd :icon plus
     :text ""
     :fitText false
     :textColor @color.muted
     :width 26
     :height 26
     :radius @radius.chip
     :align center
     :anchors {:right 8 :top 7}
     :hover {:color @color.chipHover}
     :cursor hand
     :focusable true
     :role button
     :tooltip ~(t "composer.in" {:column column.name})
     :label ~(t "composer.in" {:column column.name})
     :onClick ~(handler openComposer c)))

(defview card-view
  "A card on the board: its id, title and chips; it is dragged to another column, and right-clicked for its menu."
  [column c row]
  `(node#card :let {:i ~row.at :card ~(get column.cards row.at)}
     :when ~(contains computed.visibleRefs card.ref)
     :anchors {:left 0 :right 0}
     :height ~(if (> (count card.title) (/ (- (/ (- window.width 260) 4) 36) 5.8)) 96 82)
     :radius @radius.card
     :color ~(if (contains state.marked card.ref) "@color.accentSoft" "@color.card")
     :borderWidth 1
     :borderColor ~(if (or (and (some? state.selected) (= state.selected.c c) (= state.selected.i i))
                           (contains state.marked card.ref))
                       "@color.accent"
                       "@color.border")
     :shadow {:y 1 :blur 2 :color "#0000000f"}
     :hover {:borderColor ~(if (and (some? state.selected) (= state.selected.c c) (= state.selected.i i))
                               "@color.accent"
                               "@color.muted")}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "card.label"
                {:title card.title
                 :tag (t (str "tag." card.tag))
                 :who (or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who)
                 :column column.name})
     :onFocus ~(handler selectCard c i)
     :keys {:enter editCard :space togglePeek}
     :drag {:data {:from ~c :at ~i}}
     :dragging {:color @color.column :borderColor @color.border :shadow {:y 0 :blur 0 :color "#00000000"}}
     :onClick ~(handler selectCard c i)
     :onDoubleClick [~(handler selectCard c i) ~(handler openIssue)]
     :onContextMenu ~(handler openCardMenu c i)
     ~(card-ref)
     ~(card-owner)
     ~(card-title)
     ~(card-chips c)))

(defview card-ref "A card's id, struck through once it is done." []
  `(label#cardRef :when ~(not (contains state.hidden "id"))
     :font small
     :text ~(or-else card.ref "")
     :textColor @color.muted
     :fitText true
     :anchors {:left 12 :top 10}))

(defview card-title "A card's title, which Enter edits in place." []
  `(label#cardTitle :text ~card.title
     :textColor @color.text
     :anchors {:left 12 :right 12 :top 30}
     :height ~(if (> (count card.title) (/ (- (/ (- window.width 260) 4) 36) 5.8)) 34 17)
     :wrap true
     :align {:horizontal start :vertical start}))

(defview card-owner "A card's owner, as their initials in a circle." []
  `{:id cardOwner
    :when ~(not (contains state.hidden "assignee"))
    :style badge
    :font tiny
    :text ~card.who
    :width 18
    :height 18
    :radius 999
    :anchors {:right 10 :top 8}
    :color ~(or-else (:color (find (fn [it] (= it.who card.who)) state.members)) "@color.muted")
    :textColor "#ffffff"
    :tooltip ~(or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who)})

(defview card-chips "A card's chips: its priority, label, due date, points and owner, as the Display menu shows them."
  [c]
  `(row#cardChips :gap 4 :alignItems center :anchors {:left 10 :right 10 :bottom 9} :height 22
     {:id cardPriority
      :style cardChip
      :borderWidth 0
      :when ~(not (contains state.hidden "priority"))
      :icon ~(get state.priorities (or-else card.priority 0))
      :text ""
      :width 22
      :fitText false
      :align center
      :textColor ~(if (= card.priority 1) "@bug" "@color.muted")
      :tooltip ~(t (str "priority." (or-else card.priority 0)))}
     {:id cardTag
      :when ~(not (contains state.hidden "label"))
      :style cardChip
      :runs [{:text "●" :font symbols :size 10 :color ~(str "@" card.tag)} ~(str " " (t (str "tag." card.tag)))]
      :tooltip ~(t (str "tag." card.tag))}
     {:id cardLabels
      :when ~(and (not (contains state.hidden "label")) (> (count (or-else card.labels [])) 0))
      :style cardChip
      :text ~(str "+" (count (or-else card.labels [])))
      :tooltip ~(join ", " (map (fn [it] (t (str "label." it))) (or-else card.labels [])))}
     ~(card-due c)
     {:id cardNotes
      :style cardChip
      :borderWidth 0
      :when ~(> (count (or-else card.comments [])) 0)
      :icon message-square
      :text ~(str " " (count (or-else card.comments [])))
      :tooltip ~(t "drawer.comments" {:n (count (or-else card.comments []))})}
     {:id cardPoints
      :when ~(not (contains state.hidden "estimate"))
      :style cardChip
      :borderWidth 0
      :icon triangle
      :text ~card.points
      :tooltip ~(t "card.points" {:n card.points})}))

(defview card-due "A card's due date, red once it is past." [c]
  `{:id cardDue
    :style cardChip
    :when ~(and (not (contains state.hidden "due")) (some? card.due))
    :icon calendar
    :text ~(formatDate (dateMs card.due) "MMM d" "utc")
    :tooltip ~(formatDate (dateMs card.due) "medium" "utc")
    :textColor ~(if (and (< (daysBetween state.today card.due) 0) (not= c computed.lastColumn))
                    "@bug"
                    "@color.muted")})

;;; The table

(defview card-table "The table of every card, sortable and with columns to resize, while the table is the layout." []
  `(table#cardTable :when ~(= state.view "table")
     :fill 6
     :radius @radius.card
     :color @color.surface
     :rowHeight 32
     :items ~computed.rows
     :value ~(if (nil? state.selected) nil (str state.selected.c "-" state.selected.i))
     :sort ~state.tableSort
     :columns ~computed.tableColumns
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.accentSoft
     :accentColor @color.accent
     :onChange openRow
     :keys {:space togglePeek}
     :onOpen [~(handler openRow) ~(handler editCard)]
     :onSort sortTable
     :onColumnResize resizeTableColumn))
