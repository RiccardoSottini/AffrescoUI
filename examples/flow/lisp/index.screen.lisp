;;; index.screen.lisp --- Flow: an issue tracker with a kanban board
;;;
;;; Boards are rows of the sidebar, next to the inbox, the sprints and the projects; the board, a grouped list,
;;; swimlanes by member, a table of every card or a timeline of due dates shares a split with the chosen card's
;;; drawer, or the board's insights. Drag a card to another column, use the drawer's arrows, Alt and the arrows, or
;;; right-click it for its menu; double-click it or press Cmd+Enter to open it as a page, Enter to edit it. Cmd+P goes
;;; to a card, Cmd+Shift+P runs a command, Cmd+N types a new card, Cmd+F searches, Cmd+Z undoes, Cmd+J switches light
;;; and dark, Cmd+L the language, Cmd+, opens the settings, Escape closes the issue page, then clears the filters.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "../../shared/components/charts.lisp"
         "board.lisp"
         "drawer.lisp"
         "views.lisp"
         "sidebar.lisp"
         "list.lisp"
         "issue.lisp"
         "pages.lisp"
         "select.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "flow.lisp")

;;; Definitions

(defstate theme light :type {:enum [light dark]})

(defstate board "Product launch" :type (string))

(defstate boardId launch :type (string))

(defstate boards
  [{:id launch :label "Product launch" :closable false}
   {:id website :label "Website" :closable false}
   {:id mobile :label "Mobile app" :closable false}])

(defstate view board :type {:enum [board list table timeline]})

(defstate me "RS")

(defstate nextRef 21 :type (number))

(defstate priorities [minus flame chevrons-up chevron-up chevron-down])

(defstate statusIcons [circle loader eye check-circle])

(defstate filter all :type (string))

(defstate scope all)

(defstate order manual)

(defstate hidden [])

(defstate fStatus -1)

(defstate fPrio -1)

(defstate fProject -1)

(defstate filterProp "")

(defstate filterQuery "")

(defstate filterProps [[status circle] [assignee circle-user] [priority chevrons-up] [label tag]])

(defstate scopes [all active backlog])

(defstate searchOpen false)

(defstate filters [all bug feature design chore] :type (array :items (string)))

(defstate tagIcons {:bug bug :feature sparkles :design palette :chore wrench})

(defstate newCard "" :type (string))

(defstate composer {:open false :c 0 :p 0 :who "RS" :tag feature :notes "" :more false})

(defstate filterOpen false)

(defstate displayOpen false)

(defstate workspaceOpen false)

(defstate owner "" :type (string))

(defstate membersOpen false :type (boolean))

(defstate selected nil)

(defstate peek false)

(defstate sizes [900])

(defstate tableSort nil)

(defstate columnWidths {:ref 80 :priority 100 :title 300 :column 120 :tag 110 :who 90 :points 80})

(defstate cardMenu false :type (boolean))

(defstate columnMenu false)

(defstate menuColumn 0)

(defstate menuAt {:x 0 :y 0})

(defstate edit {:open false :title ""})

(defstate toast {:open false :message "" :undo false})

(defstate toastUndo false)

(defstate quickPick {:open false :query ""})

(defstate propPick {:open false :query ""})

(defstate issuePick {:open false :query ""})

(defstate collapsedGroups [])

(defstate propKind status)

(defstate members
  [{:who "RS" :name "Remy Shaw" :color "#2563eb"}
   {:who "MK" :name "Maya Klein" :color "#9333ea"}
   {:who "AL" :name "Ada Lewis" :color "#0f766e"}
   {:who "JD" :name "Jon Doe" :color "#d97706"}] :type (array :items (object :properties {:who (string) :name (string) :color (string)})))

(defstate lanes
  {:launch [{:name "Backlog" :color "#94a3b8"}
            {:name "In progress" :color "#ca8a04" :limit 3}
            {:name "In review" :color "#16a34a" :limit 3}
            {:name "Done" :color "#0d9488"}]
   :website [{:name "Backlog" :color "#94a3b8"}
             {:name "In progress" :color "#ca8a04"}
             {:name "In review" :color "#16a34a"}
             {:name "Done" :color "#0d9488"}]
   :mobile [{:name "Backlog" :color "#94a3b8"}
            {:name "In progress" :color "#ca8a04"}
            {:name "In review" :color "#16a34a"}
            {:name "Done" :color "#0d9488"}]} :type (object))

(defcomputed columns
  (map-indexed (fn [index it] (merge it {:cards (get [data.lane0 data.lane1 data.lane2 data.lane3] index)}))
                 (get state.lanes state.boardId)))

(defcomputed lastColumn (- (count computed.columns) 1))

(defcomputed card (if (nil? state.selected) nil (get-in computed.columns [state.selected.c :cards state.selected.i])))

(defcomputed counts (map (fn [it] (count it.cards)) computed.columns))

(defcomputed allCards
  (parseJson (str "["
                    (join ","
                          (filter (fn [it] (not= it ""))
                                  (map-indexed (fn [index it]
                                                 (slice (join (str "{\"c\":" index ",\"at\":")
                                                              (split (json (map-indexed (fn [index it]
                                                                                          {:at index
                                                                                           :ref it.ref
                                                                                           :priority it.priority
                                                                                           :due it.due
                                                                                           :points it.points
                                                                                           :start it.start
                                                                                           :tag it.tag
                                                                                           :title it.title
                                                                                           :who it.who})
                                                                                        it.cards))
                                                                     "{\"at\":"))
                                                        1
                                                        (- 1)))
                                               computed.columns)))
                    "]")))

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

(defcomputed picks
  (map (fn [it]
           {:title it.title
            :detail (str it.ref " · " (get-in computed.columns [it.c :name]))
            :run "goToCard"
            :with {:item {:c it.c :at it.at}}})
         computed.allCards))

(defcomputed over
  (map (fn [it] (and (some? it.limit) (> (count it.cards) it.limit) (= (count computed.activeFilters) 0)))
         computed.columns))

(defcomputed tableColumns
  [{:id "ref" :title (t "table.id") :width state.columnWidths.ref}
     {:id "priority" :title (t "table.priority") :width state.columnWidths.priority}
     {:id "title" :title (t "table.card") :width state.columnWidths.title}
     {:id "column" :title (t "table.column") :width state.columnWidths.column}
     {:id "tag" :title (t "table.label") :width state.columnWidths.tag}
     {:id "who" :title (t "table.owner") :width state.columnWidths.who}
     {:id "points" :title (t "table.points") :width state.columnWidths.points}])

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

(defcomputed filterOptions
  (cond (= state.filterProp "status") (map-indexed (fn [index it]
                         {:v index
                          :title it.name
                          :icon (or-else (get state.statusIcons index) "circle")
                          :color it.color
                          :on (= state.fStatus index)})
                       computed.columns)
          (= state.filterProp "assignee") (map (fn [it] {:v it.who :title it.name :icon "circle-user" :color it.color :on (= state.owner it.who)})
               state.members)
          (= state.filterProp "priority") (map-indexed (fn [index it]
                         {:v index
                          :title (t (str "priority." index))
                          :icon it
                          :color "@color.muted"
                          :on (= state.fPrio index)})
                       state.priorities)
          :else (map (fn [it] {:v it :title (t (str "tag." it)) :icon "circle" :color (str "@" it) :on (= state.filter it)})
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

(defcomputed matchRefs (map (fn [it] it.ref) computed.matches))

(defcomputed visible (filter (fn [it] (or (= state.owner "") (= it.who state.owner))) computed.matches))

(defcomputed visibleRefs (map (fn [it] it.ref) computed.visible))

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

(defstyle displayPill
  (label :font small
    :height 24
    :fitText true
    :padding 8
    :radius @radius.chip
    :borderWidth 1
    :cursor hand
    :focusable true
    :color transparent
    :hover {:color @color.chipHover}
    :focused {:color @color.chipHover}))

(defstyle caption (label :font small :textColor @color.muted :fitText true))

(defstyle badge (label :font small :align center))

(defstyle cardChip
  (label :font small
    :fitText true
    :padding 6
    :height 22
    :radius @radius.chip
    :borderWidth 1
    :borderColor @color.border
    :color transparent
    :textColor @color.muted))

(defstyle chip
  {:style badge
   :width 84
   :height 32
   :radius @radius.chip
   :color @color.surface
   :hover {:color @color.chipHover}
   :textColor @color.text
   :cursor hand})

(defstyle chipOn {:style chip :color @color.chipOn :hover {:color @color.chipOn} :textColor @color.chipOnText})

(defstyle toolButton
  (label :font small
    :height 28
    :radius @radius.chip
    :padding 10
    :fitText true
    :borderWidth 1
    :borderColor @color.border
    :color @color.surface
    :hover {:color @color.chipHover}
    :textColor @color.text
    :cursor hand
    :focusable true
    :role button))

(defstyle toolButtonOn
  {:style toolButton
   :color @color.chipOn
   :borderColor @color.chipOn
   :hover {:color @color.chipOn}
   :textColor @color.chipOnText})

(defstyle menuRow
  (label :font small
    :height 30
    :padding 10
    :radius @radius.chip
    :anchors {:left 0 :right 0}
    :textColor @color.text
    :color transparent
    :hover {:color @color.chipHover}
    :focused {:color @color.chipHover}
    :cursor hand
    :focusable true
    :align {:horizontal start :vertical center}))

(defstyle topButton
  (label :font small
    :height 32
    :radius @radius.chip
    :padding 10
    :fitText true
    :color transparent
    :hover {:color @color.topLine}
    :textColor @color.topText
    :cursor hand
    :focusable true
    :role button))

(defstyle detailButton
  (label :font small
    :height 30
    :radius @radius.chip
    :padding 12
    :fitText true
    :borderWidth 1
    :borderColor @color.border
    :color @color.surface
    :hover {:color @color.chipHover}
    :textColor @color.text
    :cursor hand))

(defcomponent layoutChoice "One layout in the Display menu: its icon over its name, outlined while it is the one shown"
  {:icon kanban :text "" :on false}
  `(node#panel-text :width 58
     :height 52
     :radius @radius.chip
     :borderWidth 1
     :borderColor ~(if on "@color.accent" "@color.border")
     :color ~(if on "@color.chipOn" "transparent")
     :hover {:color ~(if on "@color.chipOn" "@color.chipHover")}
     :cursor hand
     :focusable true
     :role radio
     :selected ~on
     :label ~text
     :tooltip ~text
     (label#layout-choice-icon :icon ~icon
       :text ""
       :fitText false
       :anchors {:left 0 :right 0 :top 7}
       :height 20
       :align center
       :textColor ~(if on "@color.chipOnText" "@color.muted"))
     (label#layout-choice-text :font small
       :text ~text
       :fitText false
       :anchors {:left 0 :right 0 :bottom 6}
       :height 16
       :align center
       :textColor ~(if on "@color.chipOnText" "@color.text"))))

(defmenu card
  {:edit {:run editCard :group "1_edit"}
   :left {:run moveLeft :group "2_move" :order 1}
   :right {:run moveRight :group "2_move" :order 2}
   :delete {:run deleteCard :group "3_delete"}})

(defmenu column {:new {:run columnNew :group "1_add"} :select {:run columnSelect :group "2_select"}})

(deftokens tokens "Tokens."
  {:radius.card 10
   :assistant.accent @color.accent
   :assistant.accentHover @color.accent
   :assistant.surface @color.menu
   :assistant.raised @color.column
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.muted
   :radius.chip 12
   :color.background {:light "#e9eee9" :dark "#17201d"}
   :color.topBar {:light "#ffffff" :dark "#101614"}
   :color.topText {:light "#2b2925" :dark "#f6f5f2"}
   :color.topMuted {:light "#73706a" :dark "#918d86"}
   :color.topLine {:light "#e4e9e4" :dark "#1e2824"}
   :color.surface {:light "#ffffff" :dark "#101614"}
   :color.column {:light "#f3f6f3" :dark "#141c19"}
   :color.text {:light "#2b2925" :dark "#f6f5f2"}
   :color.muted {:light "#73706a" :dark "#918d86"}
   :color.border {:light "#dde4de" :dark "#24302b"}
   :color.input {:light "#ffffff" :dark "#141c19"}
   :color.inputFocused {:light "#effaf5" :dark "#15201c"}
   :color.accent {:light "#0d7f63" :dark "#3ecf9e"}
   :color.accentSoft {:light "#def5ec" :dark "#13281f"}
   :color.chipHover {:light "#e9eee9" :dark "#1e2824"}
   :color.chipOn {:light "#def5ec" :dark "#173326"}
   :color.chipOnText {:light "#0b6a52" :dark "#c4f0de"}
   :color.menu {:light "#ffffff" :dark "#1b2420"}
   :color.field {:light "#f3f6f3" :dark "#141c19"}
   :color.backdrop {:light "#0000004d" :dark "#00000099"}
   :color.card {:light "#ffffff" :dark "#18211e"}
   :color.sidebar {:light "#e9eee9" :dark "#17201d"}
   :color.sidebarHover {:light "#dfe6e0" :dark "#1f2a26"}
   :color.sidebarOn {:light "#d2ddd5" :dark "#283530"}
   :color.urgent {:light "#eb5757" :dark "#f2787a"}
   :color.high {:light "#73706a" :dark "#918d86"}
   :bug {:light "#d93f3f" :dark "#f2787a"}
   :bugSoft {:light "#fbe9e9" :dark "#2e1617"}
   :feature {:light "#3d7ae0" :dark "#78a6f0"}
   :featureSoft {:light "#e7effc" :dark "#16213a"}
   :design {:light "#8e4fd9" :dark "#b48cf0"}
   :designSoft {:light "#f1e9fb" :dark "#24183a"}
   :chore {:light "#26a17a" :dark "#4cc79e"}
   :choreSoft {:light "#e2f5ef" :dark "#122a22"}
   :kit.surface {:light "#ffffff" :dark "#1b2420"}
   :kit.text {:light "#2b2925" :dark "#f6f5f2"}
   :kit.body {:light "#73706a" :dark "#918d86"}
   :kit.dim {:light "#73706a" :dark "#918d86"}
   :kit.border {:light "#dde4de" :dark "#2b3833"}
   :kit.primary {:light "#0d7f63" :dark "#3ecf9e"}
   :kit.secondary {:light "#ffffff" :dark "#1b2420"}
   :kit.onSecondary {:light "#2b2925" :dark "#f6f5f2"}
   :kit.focus {:light "#0d7f63" :dark "#3ecf9e"}
   :kit.selection {:light "#def5ec" :dark "#13281f"}
   :kit.bar {:light "#ffffff" :dark "#101614"}
   :kit.barHover {:light "#e9eee9" :dark "#1e2824"}})

(defkey "c" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do newCardCommand})

(defkey "mod+n" newCardCommand)

(defkey "mod+p" goToCardCommand)

(defkey "mod+shift+p" commandPalette)

(defkey "mod+k" commandPalette)

(defkey "f"
  {:when ~(and (not context.textInputFocus) (= state.page "issues"))
   :do [{:set state.filterQuery :to ""} {:set state.filterProp :to ""} {:toggle state.filterOpen} {:focus "#filterField"}]})

(defkey "j"
  [{:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do nextNotification}
   {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do selectNext}])

(defkey "k"
  [{:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do previousNotification}
   {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do selectPrev}])

(defkey "s" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changeStatus})

(defkey "p" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changePriority})

(defkey "a" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changeAssignee})

(defkey "i" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do assignToMe})

(defkey "slash" {:when ~(not context.textInputFocus) :do focusSearch})

(defkey "mod+e" editCard)

(defkey "mod+j" toggleTheme)

(defkey "mod+right" moveRight)

(defkey "mod+left" moveLeft)

(defkey "mod+enter" openIssue)

(defkey "escape"
  [{:when ~state.issueOpen :do closeIssue} {:when ~(> (count state.marked) 0) :do clearMarks} {:do clearFilters}])

;;; Views

(defview panel-member "#panel-member." [member]
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

(defview members-view "#members." []
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
           (panel-member member))))
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

(defview top-bar "#topBar." []
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
     ~(members-view)))

(defview filter-field "#filterField." []
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

(defview filter-list "#filterList." []
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

(defview filter-button "#filterButton." []
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

(defview search-view "#search." []
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

(defview filter-chip "#filterChip." [f]
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

(defview filter-bar "#filterBar." []
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
     ~(search-view)
     ~(repeat [f computed.activeFilters]
       (filter-chip f))))

(defview display-menu "#displayMenu." []
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

(defview column-title "#columnTitle." [column c]
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

(defview column-more "#columnMore." [column c]
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

(defview column-add "#columnAdd." [column c]
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

(defview card-chips "#cardChips." [c]
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
     {:id cardDue
      :style cardChip
      :when ~(and (not (contains state.hidden "due")) (some? card.due))
      :icon calendar
      :text ~(formatDate (dateMs card.due) "MMM d" "utc")
      :tooltip ~(formatDate (dateMs card.due) "medium" "utc")
      :textColor ~(if (and (< (daysBetween state.today card.due) 0) (not= c computed.lastColumn)) "@bug" "@color.muted")}
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

(defview card-view "#card." [column c row]
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
     (label#cardRef :when ~(not (contains state.hidden "id"))
       :font small
       :text ~(or-else card.ref "")
       :textColor @color.muted
       :fitText true
       :anchors {:left 12 :top 10})
     {:id cardOwner
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
      :tooltip ~(or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who)}
     (label#cardTitle :text ~card.title
       :textColor @color.text
       :anchors {:left 12 :right 12 :top 30}
       :height ~(if (> (count card.title) (/ (- (/ (- window.width 260) 4) 36) 5.8)) 34 17)
       :wrap true
       :align {:horizontal start :vertical start})
     ~(card-chips c)))

(defview column-view "#column." [column c]
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
         ~(repeat [row (sort-by (fn [it] (cond (= state.order "priority") (if (= (or-else it.priority 0) 0) 9 it.priority) (= state.order "due") (or-else it.due "9999-12-31") :else it.at)) (filter (fn [it] (= it.c c)) computed.allCards)) :index k]
           (card-view column c row))))))

(defview card-table "#cardTable." []
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

(defview detail-body "#detailBody." []
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
     {:id detailLine :when ~(nil? computed.card) :anchors {:left 20 :right 20 :top 186} :height 1 :color @color.border}
     (label#noCard :when ~(nil? computed.card)
       :anchors {:left 20 :right 20 :top 206}
       :height 60
       :wrap true
       :textColor @color.muted
       :runs [~(t "noCard.select")
              {:text ~(t "noCard.rightClick") :bold true}
              ~(t "noCard.menu")
              {:text "Cmd+P" :bold true}
              ~(t "noCard.goTo")])
     (node#insightsPanel :use insights :when ~(nil? computed.card) :anchors {:left 20 :right 20 :top 276 :bottom 16})
     (node#drawer :use cardDrawer :when ~(some? computed.card) :anchors {:left 8 :right 8 :top 18 :bottom 16})))

(defview main-view "#main." []
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

(defview edit-dialog "#editDialog." []
  `(node#editDialog :use dialog
     :with {:open state.edit.open
            :title ~(t "edit.title")
            :message ~(t "edit.message")
            :width 440
            :height 180
            :color @color.menu
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "detail.save") :run saveCard} {:title ~(t "detail.cancel") :run closeEdit}]}
     (textarea#editTitle :anchors {:left 20 :right 20 :top 76}
       :height 34
       :padding 10
       :radius @radius.chip
       :borderWidth 1
       :borderColor @color.border
       :color @color.input
       :focused {:borderColor @color.accent}
       :textColor @color.text
       :autofocus true
       :label ~(t "edit.title")
       :bind state.edit.title
       :onSubmit saveCard)))

(defview composer-chips "#composerChips." []
  `(row#composerChips :gap 6 :anchors {:left 16 :right 16 :bottom 60} :height 26
     {:id composerStatus
      :style cardChip
      :runs [{:icon ~(or-else (get state.statusIcons state.composer.c) "circle")
              :color ~(get-in computed.columns [state.composer.c :color])}
             ~(str " " (get-in computed.columns [state.composer.c :name]))]
      :cursor hand
      :focusable true
      :role button
      :label ~(t "composer.in" {:column (get-in computed.columns [state.composer.c :name])})
      :onClick nextComposerColumn}
     {:id composerPriority
      :style cardChip
      :icon ~(get state.priorities state.composer.p)
      :text ~(str " " (t (str "priority." state.composer.p)))
      :cursor hand
      :focusable true
      :role button
      :label ~(t "quick.priority")
      :onClick nextComposerPriority}
     {:id composerWho
      :style cardChip
      :icon circle-user
      :text ~(str " " (:name (find (fn [it] (= it.who state.composer.who)) state.members)))
      :cursor hand
      :focusable true
      :role button
      :label ~(t "quick.assignee")
      :onClick {:set state.composer.who
                :to ~(get-in state.members
                             [(mod (+ (indexOf (map (fn [it] it.who) state.members) state.composer.who) 1)
                                   (count state.members))
                              :who])}}
     {:id composerTag
      :style cardChip
      :runs [{:text "●" :font symbols :size 10 :color ~(str "@" state.composer.tag)}
             ~(str " " (t (str "tag." state.composer.tag)))]
      :cursor hand
      :focusable true
      :role button
      :label ~(t "issue.labels")
      :onClick {:set state.composer.tag
                :to ~(get state.filters
                          (+ (mod (indexOf state.filters state.composer.tag) (- (count state.filters) 1)) 1))}}))

(defview composer-box "#composerBox." []
  `(node#composerBox :overlay {:placement window :open state.composer.open}
     :role dialog
     :label ~(t "composer.title")
     :x ~(max 0 (round (/ (- window.width 560) 2)))
     :y ~(round (* window.height 0.15))
     :width 560
     :height 210
     :radius 10
     :color @color.menu
     :borderWidth 1
     :borderColor @color.border
     :shadow {:y 12 :blur 32 :color "#00000040"}
     {:id composerTeam :style cardChip :icon box :text " Flow" :anchors {:left 16 :top 14}}
     {:id composerTitle
      :style cardChip
      :borderWidth 0
      :text ~(str "›  " (t "composer.title"))
      :anchors {:left "#composerTeam.right" :top 14}}
     (textarea#newCard :anchors {:left 10 :right 16 :top 42}
       :height 40
       :padding 6
       :font title
       :color transparent
       :focused {:color transparent}
       :textColor @color.text
       :placeholder ~(t "composer.placeholder")
       :label ~(t "composer.placeholder")
       :placeholderColor @color.muted
       :autofocus true
       :bind state.newCard
       :onSubmit addCard
       :keys {:escape {:set state.composer.open :to false}})
     (textarea#composerNotes :anchors {:left 10 :right 16 :top 84}
       :height 30
       :padding 6
       :color transparent
       :focused {:color transparent}
       :textColor @color.text
       :placeholder ~(t "composer.notes")
       :label ~(t "composer.notes")
       :placeholderColor @color.muted
       :bind state.composer.notes
       :keys {:escape {:set state.composer.open :to false}})
     ~(composer-chips)
     {:id composerLine :anchors {:left 0 :right 0 :bottom 48} :height 1 :color @color.border}
     (label#composerMore :font small
       :fitText true
       :height 28
       :anchors {:right "#composerCreate.left" :bottom 10}
       :padding 10
       :runs [{:icon ~(if state.composer.more "toggle-right" "toggle-left")
               :color ~(if state.composer.more "@color.accent" "@color.muted")}
              ~(str "  " (t "composer.more"))]
       :textColor @color.muted
       :cursor hand
       :focusable true
       :role switch
       :selected ~state.composer.more
       :label ~(t "composer.more")
       :onClick {:toggle state.composer.more})
     (label#composerCreate :font small
       :text ~(t "composer.create")
       :fitText true
       :padding 12
       :height 28
       :radius @radius.chip
       :color @color.accent
       :textColor "#ffffff"
       :anchors {:right 12 :bottom 10}
       :cursor hand
       :focusable true
       :role button
       :align center
       :onClick addCard)))

(defview row-bulk-bar "#row-bulk-bar." []
  `(row#row-bulk-bar :gap 4 :alignItems center :color transparent :anchors {:left 6 :right 6 :top 6 :bottom 6}
     (label#bulkCount :font small
       :text ~(t "bulk.count" {:n (count state.marked)})
       :textColor @color.text
       :fitText true
       :height 28
       :padding 8
       :radius 6
       :borderWidth 1
       :borderColor @color.border
       :align {:horizontal start :vertical center})
     {:id bulkClear
      :style propValue
      :icon x
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align center
      :tooltip ~(t "bulk.clear")
      :label ~(t "bulk.clear")
      :onClick clearMarks}
     {:id bulkStatus
      :style propValue
      :icon circle-dashed
      :text ~(str " " (t "issue.status"))
      :tooltip "S"
      :onClick changeStatus}
     {:id bulkPriority
      :style propValue
      :icon chevrons-up
      :text ~(str " " (t "issue.priority"))
      :tooltip "P"
      :onClick changePriority}
     {:id bulkAssignee
      :style propValue
      :icon user
      :text ~(str " " (t "issue.assignee"))
      :tooltip "A"
      :onClick changeAssignee}
     {:id bulkDelete
      :style propValue
      :icon trash-2
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align center
      :textColor @bug
      :tooltip ~(t "bulk.delete")
      :label ~(t "bulk.delete")
      :onClick bulkDelete}))

;;; The screen

(defscreen main
  :window {:title "Flow: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :strings {:default en :locale ~settings.appearance.language}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons symbols]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [iconsSmall]}
          :tiny {:file "../../shared/fonts/Roboto-Regular.ttf" :size 9}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons symbols]}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 20 :fallback [iconsLarge]}
          :display {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 15}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 20}
          :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13}}
  :persist [state.theme state.sizes state.view state.swimlanes state.settings]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :theme ~state.theme
  (node#panel-root :color @color.sidebar
    ~(top-bar)
    (node#toolbar :dir auto :anchors {:left 220 :right 0 :top "#topBar.bottom"} :height 44 :color @color.surface
      ~(filter-bar)
      (node#displayButton :style ~(if state.displayOpen "toolButtonOn" "toolButton")
        :anchors {:right 12 :top 8}
        :icon sliders-horizontal
        :text ~(str " " (t "display.button"))
        :label ~(t "display.button")
        :tooltip ~(t "display.button")
        :isOpen ~state.displayOpen
        :keys {:enter {:toggle state.displayOpen} :space {:toggle state.displayOpen}}
        :onClick {:toggle state.displayOpen}
        ~(display-menu)))
    ~(main-view)
    (node#statusBar :use statusBar
      :anchors {:left 220 :right 0 :bottom 0}
      :with {:height 32
             :color @color.surface
             :textColor @color.muted
             :highlight @color.chipHover
             :items [{:text ~(t "status.revision"
                                {:number revision.number :author revision.author :message revision.message})}
                     {:text ~(str window.width " × " window.height) :side end}]})
    (node#issuePage :use issuePage
      :when ~(and state.issueOpen (some? computed.card))
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node#cardMenu :use contextMenu
      :with {:open state.cardMenu
             :items ~menus.card
             :x ~(min state.menuAt.x (- window.width 228))
             :y ~state.menuAt.y
             :width 220
             :color @color.menu
             :textColor @color.text
             :dimColor @color.muted
             :highlight @color.accentSoft})
    (node#columnMenuList :use contextMenu
      :with {:open state.columnMenu
             :items ~menus.column
             :x ~(min state.menuAt.x (- window.width 228))
             :y ~state.menuAt.y
             :width 220
             :color @color.menu
             :textColor @color.text
             :dimColor @color.muted
             :highlight @color.accentSoft})
    ~(edit-dialog)
    (node#settings :use settingsSheet)
    (node#composer :overlay {:placement window :open state.composer.open :dismiss outside :modal true}
      :fill true
      :color @color.backdrop
      ~(composer-box))
    (node#toast :use toast
      :with {:open state.toast.open
             :message ~state.toast.message
             :width 320
             :actions ~(if state.toast.undo [{:title (t "toast.undo") :run "undoChange"}] [])
             :closeLabel ~(t "toast.close")
             :color @color.menu
             :textColor @color.text
             :accent @color.accent})
    (node#quickPick :use quickPick
      :with {:color @color.menu
             :textColor @color.text
             :dimColor @color.muted
             :accent @color.accent
             :highlight @color.accentSoft
             :field @color.field
             :providers [{:items ~(concat computed.palette computed.picks) :placeholder ~(t "quick.commands")}
                         {:prefix "#" :items ~computed.picks :placeholder ~(t "quick.cards")}]})
    (node#issuePick :use quickPick
      :with {:name issuePick
             :color @color.menu
             :textColor @color.text
             :dimColor @color.muted
             :accent @color.accent
             :highlight @color.accentSoft
             :field @color.field
             :providers [{:items ~computed.picks :placeholder ~(t "quick.cards")}]})
    (node#propPick :use quickPick
      :with {:name propPick
             :width 360
             :recentLimit 0
             :color @color.menu
             :textColor @color.text
             :dimColor @color.muted
             :accent @color.accent
             :highlight @color.accentSoft
             :field @color.field
             :providers [{:items ~computed.propItems :placeholder ~(t (str "quick." (or-else state.propKind "status")))}]})
    (node#inboxPage :use inboxPage
      :when ~(= state.page "inbox")
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node#cyclesPage :use cyclesPage
      :when ~(= state.page "cycles")
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node#projectsPage :use projectsPage
      :when ~(= state.page "projects")
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node#sidebar :use sidebar)
    (node#bulkBar :when ~(and (= state.page "issues") (> (count state.marked) 0))
      :x ~(+ 228 (round (/ (- (- window.width 236) 340) 2)))
      :anchors {:bottom 56}
      :width 340
      :height 40
      :radius 8
      :color @color.menu
      :borderWidth 1
      :borderColor @color.border
      :shadow {:y 6 :blur 18 :color "#00000030"}
      :role toolbar
      :label ~(t "bulk.count" {:n (count state.marked)})
      ~(row-bulk-bar))))
