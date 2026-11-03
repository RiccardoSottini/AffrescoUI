;;; list.lisp --- the list of habits: the app bar (add, filter, the menu, the pick bar), the days' header, a row a habit
;;; with its last days to tick, and what the list says while empty

;;; The list
(defview app-bar "The app bar: the flame and the name, add, filter and the menu; the pick bar over it while picking." []
  `(node#appBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @c.bar :shadow {:y 2 :blur 4 :color "#00000030"}
     (label#appMark :font icons
       :icon flame
       :width 24
       :height 24
       :align center
       :textColor @c.primary
       :anchors {:left 16 :top 16})
     (label#label-t-app :text ~(t "app")
       :font title
       :textColor @c.text
       :anchors {:left 48 :top 16}
       :fitText true
       :role heading)
     (row#appActions :anchors {:right 4 :top 8} :width 128 :height 40 :gap 4 :arrows horizontal
       ~(add-button)
       ~(filter-button)
       (node#moreButton :style iconButton
         :icon ellipsis
         :label ~(t "more")
         :tooltip ~(t "more")
         :textColor @c.muted
         :onClick {:set state.menus.more :to true}
         :children [~(menu-more-button)]
         :isOpen ~state.menus.more))
     ~(pick-bar)))

(defview pick-bar "While rows are picked: how many, archive and delete them, or let them go." []
  `(node#pickBar :when ~computed.picking
     :anchors {:left 0 :right 0 :top 0 :bottom 0}
     :color @c.actionBar
     :role toolbar
     :label ~(t "picked" {:n (count state.picked)})
     (node#clearPick
     :style iconButton
     :icon x
     :label ~(t "clearPick")
     :tooltip ~(t "clearPick")
     :textColor "#ffffff"
     :onClick {:set state.picked :to []}
     :anchors {:left 4 :top 8})
     (label#label-t-picked-n :text ~(t "picked" {:n (count state.picked)})
       :font title
       :textColor "#ffffff"
       :anchors {:left 56 :top 16}
       :fitText true
       :live polite)
     (row#row-pick-bar :anchors {:right 4 :top 8} :width 84 :height 40 :gap 4 :arrows horizontal
       (node#archivePicked
       :style iconButton
       :icon archive
       :label ~(t "archive")
       :tooltip ~(t "archive")
       :textColor "#ffffff"
       :onClick archivePicked)
       (node#deletePicked
       :style iconButton
       :icon trash-2
       :label ~(t "delete")
       :tooltip ~(t "delete")
       :textColor "#ffffff"
       :onClick {:set state.confirmDelete :to true}))))

(defview add-button "Add, and its menu: a yes-or-no habit or a measurable one." []
  `(node#addButton :style iconButton
     :icon plus
     :label ~(t "add")
     :tooltip ~(t "add")
     :textColor @c.muted
     :onClick {:set state.menus.add :to true}
     :children [(node#menu-add-button :use menu
                  :with {:open state.menus.add
                         :width 200
                         :placement below-end
                         :icons true
                         :items ~[{:title (t "kind.yesNo")
                                   :icon "circle-check"
                                   :keyLabel (:keyLabel (find (fn [it] (= it.name "addYesNo")) commands))
                                   :run "newHabit"
                                   :with {:kind "yesNo"}}
                                  {:title (t "kind.measurable")
                                   :icon "ruler"
                                   :keyLabel (:keyLabel (find (fn [it] (= it.name "addMeasurable")) commands))
                                   :run "newHabit"
                                   :with {:kind "measurable"}}]})]
     :isOpen ~state.menus.add))

(defview filter-button "Filter, and its menu: the sort, and what is shown." []
  `(node#filterButton :style iconButton
     :icon sliders-horizontal
     :label ~(t "filter")
     :tooltip ~(t "filter")
     :textColor @c.muted
     :onClick {:set state.menus.filter :to true}
     :children [(node#menu-filter-button :use menu
                  :with {:open state.menus.filter
                         :width 220
                         :placement below-end
                         :items ~(concat (map (fn [it]
                                                {:title (t (str "sort." it))
                                                 :checked (= state.sort it)
                                                 :run "setSort"
                                                 :with {:by it}})
                                              ["manual" "name" "color" "score" "status"])
                                         [{:separator true}
                                          {:title (t "hideCompleted") :checked state.hideCompleted :run "toggleHideCompleted"}
                                          {:title (t "showArchived") :checked state.showArchived :run "toggleShowArchived"}])})]
     :isOpen ~state.menus.filter))

(defview menu-more-button "The menu: dark mode, settings, export, sync and signing in." []
  `(node#menu-more-button :use menu
     :with {:open state.menus.more
            :width 220
            :placement below-end
            :items ~[{:title (t "darkMode")
                      :checked (not= computed.theme "light")
                      :keyLabel (:keyLabel (find (fn [it] (= it.name "toggleTheme")) commands))
                      :run "toggleTheme"}
                     {:title (t "settings")
                      :keyLabel (:keyLabel (find (fn [it] (= it.name "openSettings")) commands))
                      :run "openSettings"}
                     {:title (t "exportAll") :run "exportAll"}
                     {:separator true}
                     {:title (t "syncNow")
                      :keyLabel (:keyLabel (find (fn [it] (= it.name "syncNow")) commands))
                      :run "syncNow"}
                     {:title (if computed.signedIn (t "signOut") (t "signIn"))
                      :run (if computed.signedIn "signOutOfServer" "signInToServer")}]}))

(defview list-pane "The list: the days' header, the habits, the empty message, and the app bar." []
  `(node#listPane :anchors {:left 0 :top 0 :bottom 0}
     :width ~computed.listWidth
     :visible ~(or computed.wide (= state.page "list"))
     :color @c.bg
     (node#dayHeader :anchors {:left 0 :right 0 :top 56} :height 44 :color @c.bg
       ~(row-day-header))
     (column#habitList :anchors {:left 0 :right 0 :top 100 :bottom 0}
       :gap 4
       :scroll vertical
       :arrows vertical
       :role list
       :label ~(t "app")
       ~(repeat [h computed.order :key (get-in data.ordered [h :id])]
         (habit-row h)))
     ~(empty-list)
     ~(app-bar)))

(defview row-day-header "The days over the rows, today on a pill." []
  `(row#row-day-header :anchors {:right 8 :top 4 :bottom 0} :width ~(* computed.cols 44)
     ~(repeat [d computed.days :key d]
       `(node#dayColumn :width 44 :anchors {:top 0 :bottom 0} :color transparent
          (node#todayPill :when ~(= d state.today) :anchors {:left 5 :right 5 :top 0 :bottom 4} :radius 12 :color @c.primary)
          (label#label-format-date-date-2 :text ~(formatDate (dateMs d) "d" "utc")
            :font default
            :textColor ~(if (= d state.today) "@c.onPrimary" "@c.text")
            :anchors {:left 0 :right 0 :top 4}
            :height 18
            :align center)
          (node#label-format-date-date
          :style tiny
          :text ~(formatDate (dateMs d) "EEE" "utc")
          :anchors {:left 0 :right 0 :top 23}
          :height 14
          :align center
          :textColor ~(if (= d state.today) "@c.onPrimary" "@c.muted"))))))

(defview habit-row "A habit: its ring, its name, its last days; a click shows it, a right click picks it, a drag moves it." [h]
  `(node#habitRow :let {:hb ~(get data.ordered h)
                        :col ~(if hb.archived "@c.muted" (get computed.pal hb.color))
                        :on ~(and computed.wide (= hb.id state.selected) (not computed.picking))
                        :pk ~(contains state.picked hb.id)}
     :style card
     :anchors {:left 8 :right 8}
     :height 56
     :borderWidth ~(if pk 2 0)
     :borderColor ~col
     :focusable true
     :role listitem
     :label ~hb.name
     :description ~(formatPercent (get computed.now h))
     :selected ~(or on pk)
     :focused {:borderWidth 2 :borderColor @c.primary}
     :drag {:data {:from ~h}}
     :dragOver {:borderWidth 2 :borderColor ~col}
     :onDrop ~(handler moveHabit event.data.from h)
     :onClick ~(handler clickRow h hb.id)
     :onContextMenu ~(handler togglePick hb.id)
     :keys {:space ~(handler cellClick h state.today (/ window.width 2) (/ window.height 3))
            "shift+space" {:run openPopup
                           :with {:h ~h :d ~state.today :x ~(/ window.width 2) :y ~(/ window.height 3)}}}
     (node#tint
     :visible ~(or on pk)
     :anchors {:left 0 :right 0 :top 0 :bottom 0}
     :radius 14
     :color ~(+ (get computed.pal hb.color) (if pk "30" "18")))
     (node#rowAccent :anchors {:left 0 :top 10 :bottom 10} :width 4 :radius 2 :color ~col)
     (node#ring-habit-row :use ring :with {:value ~(get computed.now h) :color ~col} :anchors {:left 18 :top 18})
     (label#label-hb-name :text ~hb.name
       :textColor ~col
       :wrap true
       :anchors {:left 50 :right ~(+ (* computed.cols 44) 6) :top 4 :bottom 4}
       :align {:horizontal start :vertical center})
     (row#cells :anchors {:right 0 :top 0 :bottom 0} :width ~(* computed.cols 44)
       ~(repeat [d computed.days :key d]
         (cell-view h d)))))

(defview cell-view "A day of a habit's row: a box to tick, or the number entered; a dot when it has a note." [h d]
  `(node#cell :let {:li ~(daysBetween hb.from d)
                    :v ~(if (>= li 0) (get-in computed.vals [h li]) nil)
                    :r ~(if (>= li 0) (get-in computed.ratio [h li :r]) 0)
                    :date ~(formatDate (dateMs d) "EEE d MMM" "utc")}
     :width 44
     :anchors {:top 0 :bottom 0}
     :color transparent
     :cursor hand
     :hover {:color @c.hover}
     :role ~(if (= hb.kind "measurable") "button" "checkbox")
     :label ~(cond (nil? v) (t "cell.none" {:date date})
                   (= hb.kind "measurable") (t "cell.value" {:date date :value v :unit hb.unit})
                   (= v 1) (t "cell.yes" {:date date})
                   (= v (- 1)) (t "cell.skip" {:date date})
                   (>= r 1) (t "cell.auto" {:date date})
                   :else (t "cell.no" {:date date}))
     :onClick ~(handler cellClick h d event.x event.y)
     :onContextMenu ~(handler openPopup h d event.x event.y)
     (label#label-cell :when ~(not= hb.kind "measurable")
       :font icons
       :align center
       :icon ~(cond (or (nil? v) (and (= v 0) (< r 1))) "" (= v (- 1)) "chevrons-right" :else "check")
       :textColor ~(if (= v 1) computed.onColor col)
       :anchors {:left 8 :top 14}
       :width 28
       :height 28
       :radius 8
       :color ~(cond (= v 1) col (or (= v (- 1)) (>= r 1)) (str col "2e") :else "transparent")
       :borderWidth ~(if (and (= v 0) (< r 1)) 1.5 0)
       :borderColor @c.faint)
     (label#label-format-number-v :when ~(and (= hb.kind "measurable") (some? v))
       :anchors {:left 0 :right 0 :top 9}
       :height 18
       :align center
       :text ~(formatNumber v 0 1)
       :textColor ~(cond (= hb.frequency.days 1) (if (and (>= r 1) (> v 0)) col "@c.faint") (> v 0) col :else "@c.faint"))
     (label#label-hb-unit :when ~(and (= hb.kind "measurable") (some? v))
       :anchors {:left 0 :right 0 :top 27}
       :height 14
       :align center
       :font tiny
       :text ~hb.unit
       :textColor ~(cond (= hb.frequency.days 1) (if (and (>= r 1) (> v 0)) col "@c.faint") (> v 0) col :else "@c.faint"))
     (node#panel-cell
     :when ~(not= (trim (or-else (get hb.notes d) "")) "")
     :anchors {:top 8 :right 8}
     :width 5
     :height 5
     :radius 3
     :color ~col)))

(defview empty-list "What the list says while it has no habit." []
  `(node#emptyList :when ~(= (count computed.order) 0) :anchors {:left 0 :right 0 :top 160} :height 120 :color transparent
     (label#label-empty-list :text ""
       :font icons
       :icon list-checks
       :textColor @c.faint
       :anchors {:left 0 :right 0 :top 0}
       :height 32
       :align center)
     (label#label-t-empty :text ~(t "empty")
       :font heading
       :textColor @c.muted
       :anchors {:left 0 :right 0 :top 40}
       :height 22
       :align center)
     (node#label-t-empty-hint
     :style small
     :text ~(t "emptyHint")
     :anchors {:left 0 :right 0 :top 66}
     :height 18
     :align center)))

