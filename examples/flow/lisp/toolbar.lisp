;;; toolbar.lisp --- the top bar and the toolbar: scopes, members, filters, search and the Display menu

;;; The top bar

(defview top-bar "The top bar: the logo and the board's name, the scope tabs and who is on the board." []
  `(node#topBar :dir auto :anchors {:left 220 :right 0 :top 0} :height 48 :color @color.topBar
     (label#logo :runs [{:icon box}
                        "  Flow  "
                        {:text ~(str "›  " (:label (find (fn [it] (= it.id state.boardId)) state.boards)))
                         :color @color.topMuted}]
       :textColor @color.topText
       :anchors {:left 16 :top 0 :bottom 0}
       :width 196
       :align {:horizontal start :vertical center})
     {:id panel-top-bar :anchors {:left 216 :top 16 :bottom 16} :width 1 :color @color.topLine}
     (row#scopeTabs :gap 2 :anchors {:left 228 :top 10} :width 360 :height 28 :role tablist :arrows horizontal
       ~(repeat [scope state.scopes]
         `{:id scopeTab
           :style ~(if (= state.scope scope) "toolButtonOn" "toolButton")
           :borderWidth ~(if (= state.scope scope) 1 0)
           :text ~(t (str "scope." scope))
           :role tab
           :selected ~(= state.scope scope)
           :onClick ~(handler chooseScope scope)}))
     ~(members-menu)))

(defview members-menu "The avatars at the top bar's end, with a ring on whoever is here, and the menu that filters by one." []
  `(row#members :gap -5
     :anchors {:right 16 :top 14}
     :width 70
     :height 20
     :cursor hand
     :focusable true
     :role button
     :label ~(str (t "members.label") ". " (t "presence.here" {:n (count computed.present)}))
     :tooltip ~(t "presence.here" {:n (count computed.present)})
     :isOpen ~state.membersOpen
     :keys {:enter {:toggle state.membersOpen} :space {:toggle state.membersOpen}}
     :onClick {:toggle state.membersOpen}
     (node#memberMenu :overlay {:placement below-end :open state.membersOpen :dismiss outside}
       :y 8
       :width 240
       :height ~(+ (* (count state.members) 40) 56)
       :radius @radius.card
       :shadow {:y 6 :blur 16 :color "#0000002e"}
       :color @color.menu
       {:id label-show-the-cards :style caption :text ~(t "members.title") :anchors {:left 16 :top 12}}
       (column#column-member-menu :anchors {:left 8 :right 8 :top 36} :height ~(+ (* (count state.members) 40) 12)
         ~(repeat [member state.members]
           (member-row member))))
     ~(repeat [member state.members]
       `{:id member
         :style badge
         :font tiny
         :text ~member.who
         :width 20
         :height 20
         :radius 999
         :color ~member.color
         :textColor "#ffffff"
         :borderWidth 2
         :borderColor ~(if (contains computed.present member.who) "@color.accent" "@color.topBar")})))

(defview member-row "A member in the members menu: their initials and name; choosing one shows only their cards." [member]
  `(node#panel-member :anchors {:left 0 :right 0}
     :height 40
     :radius @radius.chip
     :color ~(if (= state.owner member.who) "@color.accentSoft" "@color.menu")
     :hover {:color @color.chipHover}
     :focusable true
     :role menuitemradio
     :selected ~(= state.owner member.who)
     :label ~member.name
     :onClick ~(handler toggleOwner member.who)
     {:id label-member-who
      :style badge
      :text ~member.who
      :anchors {:left 8 :top 6}
      :width 28
      :height 28
      :radius 999
      :color ~member.color
      :textColor "#ffffff"}
     (label#label-member-name :text ~member.name
       :textColor @color.text
       :anchors {:left 46 :top 0 :bottom 0}
       :width 150
       :align {:horizontal start :vertical center})))

;;; The toolbar

(defview filter-bar "The start of the toolbar: the Filter button, the search, and a chip for each filter on." []
  `(row#filterBar :gap 4 :alignItems center :anchors {:left 12 :right 200 :top 8} :height 28
     ~(filter-button)
     {:id searchButton
      :style toolButton
      :when ~(and (not state.searchOpen) (= state.query ""))
      :icon search
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align center
      :label ~(t "search.label")
      :tooltip ~(str (t "search.label") " (Cmd+F)")
      :onClick focusSearch}
     ~(search-field)
     ~(repeat [f computed.activeFilters]
       (filter-chip f))))

(defview filter-button "The Filter button, which opens the Filter menu below it." []
  `(node#filterButton :style toolButton
     :icon list-filter
     :text ~(if (> (count computed.activeFilters) 0) "" (str " " (t "filter.button")))
     :label ~(t "filter.button")
     :tooltip ~(str (t "filter.button") " (F)")
     :isOpen ~state.filterOpen
     :keys {:enter {:when ~(not context.textInputFocus) :do {:toggle state.filterOpen}}
            :space {:when ~(not context.textInputFocus) :do {:toggle state.filterOpen}}}
     :onClick [{:set state.filterProp :to ""} {:set state.filterQuery :to ""} {:toggle state.filterOpen}]
     (node#filterMenu :overlay {:placement below :open state.filterOpen :dismiss outside}
       :y 4
       :width 220
       :height ~(+ (* (if (= state.filterProp "") (count computed.filterPropsShown) (count computed.filterOptionsShown))
                      30)
                   48)
       :radius @radius.card
       :shadow {:y 6 :blur 16 :color "#0000002e"}
       :borderWidth 1
       :borderColor @color.border
       :color @color.menu
       ~(filter-field)
       {:id filterFieldLine :anchors {:left 0 :right 0 :top 38} :height 1 :color @color.border}
       ~(filter-list))))

(defview filter-field "The Filter menu's field: it narrows the properties, then the values of the one chosen." []
  `(textarea#filterField :anchors {:left 6 :right 6 :top 4}
     :height 32
     :padding 8
     :color transparent
     :focused {:color transparent}
     :textColor @color.text
     :placeholder ~(t "filter.placeholder")
     :placeholderColor @color.muted
     :label ~(t "filter.placeholder")
     :autofocus true
     :bind state.filterQuery
     :onSubmit submitFilter
     :keys {:escape {:set state.filterOpen :to false}}))

(defview filter-list "The Filter menu's list: the properties, or the values of the one chosen, each with its icon." []
  `(column#filterList :anchors {:left 6 :right 6 :top 42 :bottom 6} :role menu :arrows vertical
     ~(repeat [p computed.filterPropsShown]
       `(node#filterProp :when ~(= state.filterProp "")
          :style menuRow
          :runs [{:icon ~p.1 :color @color.muted} ~(str "  " (t (str "filter." p.0)))]
          :role menuitem
          :label ~(t (str "filter." p.0))
          :onClick ~(handler pickFilterProp p.0)
          (label#label-filter-prop :font small
            :icon chevron-right
            :mirror true
            :text ""
            :textColor @color.muted
            :anchors {:right 0 :top 0 :bottom 0}
            :width 16
            :align center)))
     ~(repeat [opt computed.filterOptionsShown]
       `{:id filter
         :when ~(not= state.filterProp "")
         :style menuRow
         :runs [{:icon ~opt.icon :color ~opt.color} ~(str "  " opt.title)]
         :color ~(if opt.on "@color.accentSoft" "transparent")
         :role menuitemcheckbox
         :selected ~opt.on
         :label ~opt.title
         :onClick ~(handler setFilter state.filterProp opt.v opt.on)})))

(defview filter-chip "A filter that is on, as a chip: its property and value, and an x that removes it." [f]
  `(node#filterChip :style toolButtonOn
     :borderWidth 1
     :runs [{:text ~(t (str "filter." f.prop)) :color @color.muted}
            ~(str "  " (t "filter.is") "  ")
            {:text ~f.value :color @color.text}
            "     "]
     (label#filterChipClear :font small
       :icon x
       :text ""
       :width 20
       :anchors {:right 2 :top 4 :bottom 4}
       :radius 4
       :align center
       :textColor @color.muted
       :hover {:color @color.chipHover}
       :cursor hand
       :focusable true
       :role button
       :label ~(t "filter.clear")
       :onClick ~(handler clearFilter f.prop))))

(defview search-field "The search field, shown while it is open or holds a search." []
  `(textarea#search :when ~(or state.searchOpen (not= state.query ""))
     :width 220
     :height 28
     :padding 8
     :radius @radius.chip
     :color transparent
     :focused {:borderColor @color.accent}
     :borderWidth 1
     :borderColor @color.border
     :textColor @color.text
     :placeholder ~(t "search.placeholder")
     :placeholderColor @color.muted
     :label ~(t "search.label")
     :autofocus true
     :bind state.query
     :keys {:escape [{:set state.query :to ""} {:set state.searchOpen :to false}]}))

(defview display-menu "The Display menu: the layout, the grouping, the ordering and the properties the cards show." []
  `(node#displayMenu :overlay {:placement below-end :open state.displayOpen :dismiss outside}
     :y 4
     :width 280
     :height 288
     :radius @radius.card
     :shadow {:y 6 :blur 16 :color "#0000002e"}
     :borderWidth 1
     :borderColor @color.border
     :color @color.menu
     {:id displayLayout :style caption :text ~(t "display.layout") :anchors {:left 12 :top 12}}
     (row#views :gap 4 :anchors {:left 12 :right 12 :top 34} :height 52 :role radiogroup :arrows horizontal
       (node#viewBoard :use layoutChoice
         :with {:icon kanban :text ~(trim (t "view.board")) :on ~(= state.view "board")}
         :onClick showBoard)
       (node#viewList :use layoutChoice
         :with {:icon layout-list :text ~(trim (t "view.list")) :on ~(= state.view "list")}
         :onClick showList)
       (node#viewTable :use layoutChoice
         :with {:icon table :text ~(trim (t "view.table")) :on ~(= state.view "table")}
         :onClick showTable)
       (node#viewTimeline :use layoutChoice
         :with {:icon chart-gantt :text ~(trim (t "view.timeline")) :on ~(= state.view "timeline")}
         :onClick showTimeline))
     {:id displayGrouping :style caption :text ~(t "display.grouping") :anchors {:left 12 :top 100}}
     (row#groupings :gap 4 :anchors {:left 120 :right 12 :top 96} :height 24 :role radiogroup :arrows horizontal
       ~(repeat [g ["status" "assignee"]]
         `{:id grouping
           :let {:on ~(= state.swimlanes (= g "assignee"))}
           :style displayPill
           :role radio
           :text ~(t (str "prop." g))
           :selected ~on
           :disabled ~(not= state.view "board")
           :borderColor ~(if on "@color.accent" "@color.border")
           :textColor ~(if (and on (= state.view "board")) "@color.text" "@color.muted")
           :tooltip ~(if (= g "assignee") (t "view.lanes") "")
           :onClick ~(handler groupBy on)}))
     {:id displayOrdering :style caption :text ~(t "display.ordering") :anchors {:left 12 :top 134}}
     (row#orderings :gap 4 :anchors {:left 12 :right 12 :top 154} :height 24 :role radiogroup :arrows horizontal
       ~(repeat [o ["manual" "priority" "due"]]
         `{:id ordering
           :style displayPill
           :role radio
           :text ~(t (str "order." o))
           :selected ~(= state.order o)
           :borderColor ~(if (= state.order o) "@color.accent" "@color.border")
           :textColor ~(if (= state.order o) "@color.text" "@color.muted")
           :onClick ~(handler orderBy o)}))
     {:id displayProperties :style caption :text ~(t "display.properties") :anchors {:left 12 :top 192}}
     (row#displayProps :wrap true :gap 4 :anchors {:left 12 :right 12 :top 212} :height 76
       ~(repeat [prop ["id" "priority" "label" "due" "estimate" "assignee"]]
         `{:id displayProp
           :style displayPill
           :role switch
           :let {:on ~(not (contains state.hidden prop))}
           :text ~(t (str "prop." prop))
           :selected ~on
           :color ~(if on "@color.chipHover" "transparent")
           :borderColor ~(if on "@color.chipHover" "@color.border")
           :textColor ~(if on "@color.text" "@color.muted")
           :onClick ~(handler toggleProperty prop)}))))
