;;; views.lisp --- Flow's other views, included by index.screen.lisp: the insights beside the board (burndown and activity), swimlanes by member, the timeline of due dates, and the settings.

;;; Definitions

(defcomponent insights "The sprint burndown and the activity feed, under the cards-per-column chart while no card is chosen"
  {}
  `(node#insights :color transparent
     {:id burndownTitle
      :style caption
      :icon trending-down
      :anchors {:left 0 :top 0}
      :text ~(t "burndown.title" {:n computed.remaining})}
     {:id burndownBase :anchors {:left 0 :right 0 :top 104} :height 1 :color @color.border}
     (node#burndown :use lineChart
       :with {:values ~computed.burndownValues :top ~state.burndown.0 :color @color.accent :lineWidth 2}
       :anchors {:left 0 :right 0 :top 26}
       :height 78
       :role img
       :label ~(t "burndown.title" {:n computed.remaining}))
     {:id activityTitle :style caption :icon activity :text ~(t "activity.title") :anchors {:left 0 :top 124}}
     (column#activity :anchors {:left 0 :right 0 :top 148 :bottom 0} :gap 4 :scroll vertical :role list :live polite
       ~(repeat [entry state.activity]
         (activity-item entry)))))

(defcomponent swimlanes "One lane a member, the board's columns across it; a card dropped in another lane changes its owner"
  {}
  `(column#lanes :gap 8 :scroll vertical :color transparent
     ~(repeat [member state.members]
       (lane-view member))))

(defcomponent timelineView "The cards with dates, a track a member: dragging a clip changes its dates, or its owner when it changes track"
  {}
  `(node#timeline :radius @radius.card :color @color.surface
     {:id timelineTitle
      :style caption
      :icon calendar-range
      :anchors {:left 16 :top 12}
      :text ~(t "timeline.title" {:from (formatDate (dateMs state.sprintStart) "medium" "utc")})}
     ~(card-timeline)))

(defcomponent settingsSheet "The settings, in the middle of the window; Escape, a click outside or the close button closes them"
  {}
  `(node#settingsCard :dir auto
     :overlay {:placement center :open state.settingsOpen :dismiss outside :modal true}
     :width 520
     :height 420
     :radius @radius.card
     :color @color.menu
     :shadow {:y 8 :blur 24 :color "#00000040"}
     :role dialog
     :label ~(t "settings.title")
     (label#settingsTitle :font heading
       :icon settings
       :text ~(str " " (t "settings.title"))
       :textColor @color.text
       :anchors {:left 20 :top 16}
       :fitText true)
     (label#settingsClose :icon x
       :text ""
       :width 30
       :height 30
       :radius @radius.chip
       :align center
       :anchors {:right 12 :top 10}
       :focusable true
       :role button
       :label ~(t "settings.close")
       :tooltip ~(t "settings.close")
       :textColor @color.muted
       :hover {:color @color.chipHover}
       :cursor hand
       :onClick closeSettings)
     (node#settingsList :use settingsPage
       :anchors {:left 12 :right 12 :top 52 :bottom 12}
       :with {:color @color.menu :textColor @color.text :dimColor @color.muted :accent @color.accent :field @color.field})))

;;; Views

(defview activity-item "#activityItem." [entry]
  `(node#activityItem :anchors {:left 0 :right 0} :height 34 :color transparent :role listitem
     {:id activityWho
      :style badge
      :text ~entry.who
      :anchors {:left 0 :top 5}
      :width 24
      :height 24
      :radius 999
      :color ~(or-else (:color (find (fn [it] (= it.who entry.who)) state.members)) "@color.muted")
      :textColor "#ffffff"}
     (label#activityText :font small
       :textColor @color.text
       :wrap true
       :anchors {:left 32 :right 0 :top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :text ~(t (str "activity." entry.key)
                 {:who (or-else (:name (find (fn [it] (= it.who entry.who)) state.members)) entry.who)
                  :title entry.title
                  :column (or-else entry.column "")}))))

(defview lane-card "#laneCard." [member column c row]
  `(label#laneCard :let {:i ~row.at :card ~(get column.cards row.at)}
     :when ~(and (= card.who member.who) (contains computed.matchRefs card.ref))
     :font small
     :text ~card.title
     :anchors {:left 0 :right 0}
     :height 26
     :padding 6
     :radius 4
     :color @color.surface
     :textColor @color.text
     :borderWidth ~(if (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)) 2 0)
     :borderColor @color.accent
     :focusable true
     :role button
     :label ~(t "card.label" {:title card.title :tag (t (str "tag." card.tag)) :who member.name :column column.name})
     :cursor hand
     :drag {:data {:from ~c :at ~i}}
     :dragging {:color @color.accentSoft}
     :onClick {:run selectCard :with {:c ~c :i ~i}}
     :onFocus {:run selectCard :with {:c ~c :i ~i}}))

(defview lane-view "#lane." [member]
  `(node#lane :anchors {:left 0 :right 0}
     :height 168
     :radius @radius.card
     :color @color.column
     :role group
     :label ~member.name
     {:id laneWho
      :style badge
      :text ~member.who
      :anchors {:left 12 :top 10}
      :width 24
      :height 24
      :radius 999
      :color ~member.color
      :textColor "#ffffff"}
     (label#laneName :font heading :textColor @color.text :fitText true :anchors {:left 44 :top 12} :text ~member.name)
     {:id laneCount
      :style caption
      :anchors {:left "#laneName.right + 10" :top 15}
      :text ~(t "lane.cards" {:n (count (filter (fn [it] (= it.who member.who)) computed.allCards))})}
     (row#laneColumns :gap 6 :anchors {:left 8 :right 8 :top 42 :bottom 8}
       ~(repeat [column computed.columns :index c]
         `(node#laneColumn :grow 1
            :anchors {:top 0 :bottom 0}
            :radius @radius.chip
            :color @color.field
            :onDrop {:run moveToLane :with {:from ~event.data.from :at ~event.data.at :to ~c :who ~member.who}}
            {:id laneColumnName :style caption :text ~column.name :anchors {:left 8 :top 4}}
            (stack#laneCards :gap 4 :scroll vertical :anchors {:left 4 :right 4 :top 22 :bottom 4}
              ~(repeat [row (sort-by (fn [it] (cond (= state.order "priority") (if (= (or-else it.priority 0) 0) 9 it.priority) (= state.order "due") (or-else it.due "9999-12-31") :else it.at)) (filter (fn [it] (= it.c c)) computed.allCards)) :index k]
                (lane-card member column c row))))))))

(defview card-timeline "#cardTimeline." []
  `(timeline#cardTimeline :anchors {:left 8 :right 8 :top 36 :bottom 8}
     :tracks ~state.timelineTracks
     :duration 28
     :fps 1
     :pixelsPerSecond 32
     :offset 0
     :time ~(daysBetween state.sprintStart state.today)
     :markers [{:time 14 :label ~(t "timeline.sprintEnd")}]
     :headerWidth 150
     :trackHeight 36
     :label ~(t "timeline.title" {:from (formatDate (dateMs state.sprintStart) "medium" "utc")})
     :color @color.surface
     :clipColor @color.accent
     :headerColor @color.field
     :textColor @color.text
     :accentColor @color.accent
     :onClipChange changeClip
     ~(repeat [track state.timelineTracks]
       `(label#timelineHeader :font small
          :text ~track.label
          :textColor @color.text
          :fill 0
          :padding 10
          :align {:horizontal start :vertical center}))))
