{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Flow: an issue tracker with a kanban board. Boards are rows of the sidebar, next to the inbox, the sprints and the projects; the board, a grouped list, swimlanes by member, a table of every card or a timeline of due dates shares a split with the chosen card's drawer, or the board's insights. Drag a card to another column, use the drawer's arrows, Alt and the arrows, or right-click it for its menu; double-click it or press Cmd+Enter to open it as a page, Enter to edit it. Cmd+P goes to a card, Cmd+Shift+P runs a command, Cmd+N types a new card, Cmd+F searches, Cmd+Z undoes, Cmd+J switches light and dark, Cmd+L the language, Cmd+, opens the settings, Escape closes the issue page, then clears the filters."
  :window {:title "Flow: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :include ["../../shared/components/charts.lisp"
    strings.lisp
    board.lisp
    drawer.lisp
    views.lisp
    sidebar.lisp
    list.lisp
    issue.lisp
    pages.lisp
    select.lisp
    "../../shared/addons/assistant/assistant.lisp"]
  :persist [state.theme state.sizes state.view state.swimlanes state.settings]
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
  :theme "{{state.theme}}"
  :tokens {:radius.card 10
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
    :kit.barHover {:light "#e9eee9" :dark "#1e2824"}}
  :permissions {:allow [studio]
    :ask ["agent(/actions)"]
    :deny ["*(#statusBar)"]
    :reviewers [studio]
    :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
  :state {:theme light
    :board "Product launch"
    :boardId launch
    :boards [(node #launch :label "Product launch" :closable false)
      (node #website :label Website :closable false)
      (node #mobile :label "Mobile app" :closable false)]
    :view board
    :me RS
    :nextRef 21
    :priorities [minus flame chevrons-up chevron-up chevron-down]
    :statusIcons [circle loader eye check-circle]
    :filter all
    :scope all
    :order manual
    :hidden []
    :fStatus -1
    :fPrio -1
    :fProject -1
    :filterProp ""
    :filterQuery ""
    :filterProps [[status circle] [assignee circle-user] [priority chevrons-up] [label tag]]
    :scopes [all active backlog]
    :searchOpen false
    :filters [all bug feature design chore]
    :tagIcons {:bug bug :feature sparkles :design palette :chore wrench}
    :newCard ""
    :composer {:open false :c 0 :p 0 :who RS :tag feature :notes "" :more false}
    :filterOpen false
    :displayOpen false
    :workspaceOpen false
    :owner ""
    :membersOpen false
    :selected nil
    :peek false
    :sizes [900]
    :tableSort nil
    :columnWidths {:ref 80 :priority 100 :title 300 :column 120 :tag 110 :who 90 :points 80}
    :cardMenu false
    :columnMenu false
    :menuColumn 0
    :menuAt {:x 0 :y 0}
    :edit {:open false :title ""}
    :toast {:open false :message "" :undo false}
    :toastUndo false
    :quickPick {:open false :query ""}
    :propPick {:open false :query ""}
    :issuePick {:open false :query ""}
    :collapsedGroups []
    :propKind status
    :members [{:who RS :name "Remy Shaw" :color "#2563eb"}
      {:who MK :name "Maya Klein" :color "#9333ea"}
      {:who AL :name "Ada Lewis" :color "#0f766e"}
      {:who JD :name "Jon Doe" :color "#d97706"}]
    :lanes {:launch [{:name Backlog :color "#94a3b8"}
        {:name "In progress" :color "#ca8a04" :limit 3}
        {:name "In review" :color "#16a34a" :limit 3}
        {:name Done :color "#0d9488"}]
      :website [{:name Backlog :color "#94a3b8"}
        {:name "In progress" :color "#ca8a04"}
        {:name "In review" :color "#16a34a"}
        {:name Done :color "#0d9488"}]
      :mobile [{:name Backlog :color "#94a3b8"}
        {:name "In progress" :color "#ca8a04"}
        {:name "In review" :color "#16a34a"}
        {:name Done :color "#0d9488"}]}}
  :stateTypes {:theme {:enum [light dark]}
    :board {:type string}
    :boardId {:type string}
    :view {:enum [board list table timeline]}
    :nextRef {:type number}
    :swimlanes {:type boolean}
    :query {:type string}
    :filter {:type string}
    :filters {:type array :items {:type string}}
    :newCard {:type string}
    :owner {:type string}
    :membersOpen {:type boolean}
    :cardMenu {:type boolean}
    :members {:type array
      :items {:type object :properties {:who {:type string} :name {:type string} :color {:type string}}}}
    :lanes {:type object}}
  :computed {:columns "{{map(state.lanes[state.boardId], merge(it, { cards: [data.lane0, data.lane1, data.lane2, data.lane3][index] }))}}"
    :lastColumn "{{length(computed.columns) - 1}}"
    :card "{{state.selected == null ? null : computed.columns[state.selected.c].cards[state.selected.i]}}"
    :counts "{{map(computed.columns, length(it.cards))}}"
    :allCards "{{parseJson('[' + join(filter(map(computed.columns, slice(join(split(json(map(it.cards, { at: index, ref: it.ref, priority: it.priority, due: it.due, points: it.points, start: it.start, tag: it.tag, title: it.title, who: it.who })), '{\"at\":'), '{\"c\":' + index + ',\"at\":'), 1, -1)), it != ''), ',') + ']')}}"
    :rows "{{map(computed.allCards, { id: it.c + '-' + it.at, cells: [it.ref, t('priority.' + (it.priority ?? 0)), it.title, computed.columns[it.c].name, it.tag, it.who, it.points] })}}"
    :picks "{{map(computed.allCards, { title: it.title, detail: it.ref + ' · ' + computed.columns[it.c].name, run: 'goToCard', with: { item: { c: it.c, at: it.at } } })}}"
    :over "{{map(computed.columns, it.limit != null && length(it.cards) > it.limit && length(computed.activeFilters) == 0)}}"
    :tableColumns "{{[{ id: 'ref', title: t('table.id'), width: state.columnWidths.ref }, { id: 'priority', title: t('table.priority'), width: state.columnWidths.priority }, { id: 'title', title: t('table.card'), width: state.columnWidths.title }, { id: 'column', title: t('table.column'), width: state.columnWidths.column }, { id: 'tag', title: t('table.label'), width: state.columnWidths.tag }, { id: 'who', title: t('table.owner'), width: state.columnWidths.who }, { id: 'points', title: t('table.points'), width: state.columnWidths.points }]}}"
    :paletteFirst "{{filter(commands, it.enabled && contains(['changeStatus', 'changePriority', 'changeAssignee', 'assignToMe', 'editCard', 'openIssue'], it.name))}}"
    :palette "{{map(concat(computed.paletteFirst, filter(commands, it.enabled && it.category == 'Issue' && !contains(map(computed.paletteFirst, it.name), it.name)), filter(commands, it.enabled && it.category != 'Issue')), { title: it.title, detail: it.keyLabel ?? '', run: it.name })}}"
    :matches "{{filter(computed.allCards, (state.scope == 'all' || (state.scope == 'backlog' ? it.c == 0 : it.c > 0 && it.c < length(computed.columns) - 1)) && (state.filter == 'all' || it.tag == state.filter) && (state.fStatus < 0 || it.c == state.fStatus) && (state.fPrio < 0 || (it.priority ?? 0) == state.fPrio) && (state.fProject < 0 || (state.projectOf[it.tag] ?? 0) == state.fProject) && (state.query == '' || fuzzy(state.query, it.title) != null))}}"
    :filterOptions "{{state.filterProp == 'status' ? map(computed.columns, { v: index, title: it.name, icon: state.statusIcons[index] ?? 'circle', color: it.color, on: state.fStatus == index }) : state.filterProp == 'assignee' ? map(state.members, { v: it.who, title: it.name, icon: 'circle-user', color: it.color, on: state.owner == it.who }) : state.filterProp == 'priority' ? map(state.priorities, { v: index, title: t('priority.' + index), icon: it, color: '@color.muted', on: state.fPrio == index }) : map(slice(state.filters, 1), { v: it, title: t('tag.' + it), icon: 'circle', color: '@' + it, on: state.filter == it })}}"
    :activeFilters "{{concat(state.fStatus >= 0 ? [{ prop: 'status', value: computed.columns[state.fStatus].name }] : [], state.owner != '' ? [{ prop: 'assignee', value: find(state.members, it.who == state.owner).name ?? state.owner }] : [], state.fPrio >= 0 ? [{ prop: 'priority', value: t('priority.' + state.fPrio) }] : [], state.filter != 'all' ? [{ prop: 'label', value: t('tag.' + state.filter) }] : [], state.fProject >= 0 ? [{ prop: 'project', value: data.projects[state.fProject].name }] : [])}}"
    :filterPropsShown "{{filter(state.filterProps, state.filterQuery == '' || fuzzy(state.filterQuery, t('filter.' + it[0])) != null)}}"
    :filterOptionsShown "{{filter(computed.filterOptions, state.filterQuery == '' || fuzzy(state.filterQuery, it.title) != null)}}"
    :matchRefs "{{map(computed.matches, it.ref)}}"
    :visible "{{filter(computed.matches, state.owner == '' || it.who == state.owner)}}"
    :visibleRefs "{{map(computed.visible, it.ref)}}"
    :next "{{state.selected == null ? computed.visible[0] : find(computed.visible, it.c > state.selected.c || (it.c == state.selected.c && it.at > state.selected.i))}}"
    :prev "{{state.selected == null ? null : computed.visible[count(computed.visible, it.c < state.selected.c || (it.c == state.selected.c && it.at < state.selected.i)) - 1]}}"
    :propItems "{{state.propKind == 'status' ? map(computed.columns, { title: it.name, detail: index == state.selected.c ? '✓' : '', run: 'setStatus', with: { to: index } }) : state.propKind == 'priority' ? map(state.priorities, { title: t('priority.' + index), detail: index == (computed.card.priority ?? 0) ? '✓' : '', run: 'setPriority', with: { p: index } }) : map(state.members, { title: it.name, detail: it.who == computed.card.who ? '✓' : it.who, run: 'setAssignee', with: { who: it.who } })}}"}
  :styles {:displayPill {:type label
      :font small
      :height 24
      :fitText true
      :padding 8
      :radius @radius.chip
      :borderWidth 1
      :cursor hand
      :focusable true
      :color transparent
      :hover {:color @color.chipHover}
      :focused {:color @color.chipHover}}
    :caption {:type label :font small :textColor @color.muted :fitText true}
    :badge {:type label :font small :align center}
    :cardChip {:type label
      :font small
      :fitText true
      :padding 6
      :height 22
      :radius @radius.chip
      :borderWidth 1
      :borderColor @color.border
      :color transparent
      :textColor @color.muted}
    :chip {:style badge
      :width 84
      :height 32
      :radius @radius.chip
      :color @color.surface
      :hover {:color @color.chipHover}
      :textColor @color.text
      :cursor hand}
    :chipOn {:style chip :color @color.chipOn :hover {:color @color.chipOn} :textColor @color.chipOnText}
    :toolButton {:type label
      :font small
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
      :role button}
    :toolButtonOn {:style toolButton
      :color @color.chipOn
      :borderColor @color.chipOn
      :hover {:color @color.chipOn}
      :textColor @color.chipOnText}
    :menuRow {:type label
      :font small
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
      :align {:horizontal start :vertical center}}
    :topButton {:type label
      :font small
      :height 32
      :radius @radius.chip
      :padding 10
      :fitText true
      :color transparent
      :hover {:color @color.topLine}
      :textColor @color.topText
      :cursor hand
      :focusable true
      :role button}
    :detailButton {:type label
      :font small
      :height 30
      :radius @radius.chip
      :padding 12
      :fitText true
      :borderWidth 1
      :borderColor @color.border
      :color @color.surface
      :hover {:color @color.chipHover}
      :textColor @color.text
      :cursor hand}}
  :components {:layoutChoice {:note "One layout in the Display menu: its icon over its name, outlined while it is the one shown"
      :params {:icon kanban :text "" :on false}
      :node (node #panel-text
        :width 58
        :height 52
        :radius @radius.chip
        :borderWidth 1
        :borderColor "{{on ? '@color.accent' : '@color.border'}}"
        :color "{{on ? '@color.chipOn' : 'transparent'}}"
        :hover {:color "{{on ? '@color.chipOn' : '@color.chipHover'}}"}
        :cursor hand
        :focusable true
        :role radio
        :selected "{{on}}"
        :label "{{text}}"
        :tooltip "{{text}}"
        (node #layout-choice-icon
          :type label
          :icon "{{icon}}"
          :text ""
          :fitText false
          :anchors {:left 0 :right 0 :top 7}
          :height 20
          :align center
          :textColor "{{on ? '@color.chipOnText' : '@color.muted'}}")
        (node #layout-choice-text
          :type label
          :font small
          :text "{{text}}"
          :fitText false
          :anchors {:left 0 :right 0 :bottom 6}
          :height 16
          :align center
          :textColor "{{on ? '@color.chipOnText' : '@color.text'}}"))}}
  :menus {:card {:edit {:run editCard :group "1_edit"}
      :left {:run moveLeft :group "2_move" :order 1}
      :right {:run moveRight :group "2_move" :order 2}
      :delete {:run deleteCard :group "3_delete"}}
    :column {:new {:run columnNew :group "1_add"} :select {:run columnSelect :group "2_select"}}}
  :actions {:addCard [{:if "{{length(trim(state.newCard)) == 0}}" :then [{:stop true}]}
      {:run remember}
      {:insert issues
        :value (node #"FLO-{{state.nextRef}}"
          :ref "FLO-{{state.nextRef}}"
          :board "{{state.boardId}}"
          :status "{{state.composer.c}}"
          :order "{{max(concat([-1], map(computed.columns[state.composer.c].cards, it.order))) + 1}}"
          :priority "{{state.composer.p}}"
          :title "{{trim(state.newCard)}}"
          :tag "{{state.composer.tag}}"
          :who "{{state.composer.who}}"
          :points 1
          :notes "{{trim(state.composer.notes) != '' ? state.composer.notes : null}}")}
      {:run notify
        :with {:message "{{t('toast.added', { title: trim(state.newCard), column: computed.columns[state.composer.c].name })}}"}}
      {:run logActivity :with {:key added :title "{{trim(state.newCard)}}" :column ""}}
      {:set state.nextRef :to "{{state.nextRef + 1}}"}
      {:set state.newCard :to ""}
      {:set state.composer.notes :to ""}
      {:set state.composer.open
        :to "{{state.composer.more}}"
        :note "Create more keeps the composer open for the next one"}]
    :setFilter [{:note "with: prop, v and on, a value from the Filter menu and whether it is applied, which removes it"}
      {:if "{{prop == 'status'}}" :then [{:set state.fStatus :to "{{on ? -1 : v}}"}]}
      {:if "{{prop == 'assignee'}}" :then [{:set state.owner :to "{{on ? '' : v}}"}]}
      {:if "{{prop == 'priority'}}" :then [{:set state.fPrio :to "{{on ? -1 : v}}"}]}
      {:if "{{prop == 'label'}}" :then [{:set state.filter :to "{{on ? 'all' : v}}"}]}
      {:set state.filterOpen :to false}
      {:set state.filterProp :to ""}]
    :clearFilter [{:note "with: prop, the filter a chip's × removes"}
      {:if "{{prop == 'status'}}" :then [{:set state.fStatus :to -1}]}
      {:if "{{prop == 'assignee'}}" :then [{:set state.owner :to ""}]}
      {:if "{{prop == 'priority'}}" :then [{:set state.fPrio :to -1}]}
      {:if "{{prop == 'label'}}" :then [{:set state.filter :to all}]}
      {:if "{{prop == 'project'}}" :then [{:set state.fProject :to -1}]}]
    :openComposer [{:note "with: c, the column the new card goes to"}
      {:set state.composer
        :to {:open true
          :c "{{c}}"
          :p 0
          :who "{{state.me}}"
          :tag "{{state.filter == 'all' ? 'feature' : state.filter}}"
          :notes ""
          :more "{{state.composer.more}}"}}]
    :moveCard [{:note "with: from (column), at (card index), to (column)"}
      {:if "{{from == to || to < 0 || to > computed.lastColumn}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues
        #"{{computed.columns[from].cards[at].id}}"
        :fields {:status "{{to}}" :order "{{max(concat([-1], map(computed.columns[to].cards, it.order))) + 1}}"})
      {:set state.selected :to {:c "{{to}}" :i "{{length(computed.columns[to].cards) - 1}}"}}
      {:run logActivity
        :with {:key moved
          :title "{{computed.columns[to].cards[length(computed.columns[to].cards) - 1].title}}"
          :column "{{computed.columns[to].name}}"}}
      {:run checkLimit :with {:c "{{to}}"}}]
    :notify [{:note "with: message. A toast in the corner for a few seconds"}
      {:note "the toast offers Undo when the action that sent it remembered the columns first"}
      {:set state.toast :to {:open true :message "{{message}}" :undo "{{state.toastUndo}}"}}
      {:set state.toastUndo :to false}
      {:debounce 4000 :do [{:set state.toast.open :to false}]}]
    :selectCard [{:note "with: c (column), i (card index)"}
      {:if "{{state.selected != null && state.selected.c == c && state.selected.i == i}}" :then [{:stop true}]}
      {:set state.selected :to {:c "{{c}}" :i "{{i}}"}}
      {:set state.draft :to {:editing false :notes ""}}
      {:set state.dueOpen :to false}
      {:run announcePresence}]
    :openCardMenu [{:note "with: c, i; at the pointer"}
      {:run selectCard :with {:c "{{c}}" :i "{{i}}"}}
      {:set state.menuAt :to {:x "{{event.x}}" :y "{{event.y}}"}}
      {:set state.cardMenu :to true}]
    :openColumnMenu [{:note "with: c; below the column's … button"}
      {:set state.menuColumn :to "{{c}}"}
      {:note "a keyboard press carries no position: the menu opens at the board's top start corner"}
      {:set state.menuAt :to {:x "{{event.x ?? 244}}" :y "{{(event.y ?? 108) + 12}}"}}
      {:set state.columnMenu :to true}]
    :columnNew {:title "New issue in this status"
      :icon plus
      :enablement "{{state.columnMenu}}"
      :do [{:set state.columnMenu :to false} {:run openComposer :with {:c "{{state.menuColumn}}"}}]}
    :columnSelect {:title "Select all in this status"
      :icon list-checks
      :enablement "{{state.columnMenu}}"
      :do [{:set state.columnMenu :to false}
        {:set state.marked :to "{{map(filter(computed.visible, it.c == state.menuColumn), it.ref)}}"}]}
    :goToCard [{:note "with: item, a quick pick row; from another page, back to the issues, in the board, list or table it showed"}
      {:run showPage :with {:page issues}}
      {:if "{{state.view == 'timeline'}}" :then [{:set state.view :to board}]}
      {:set state.filter :to all}
      {:set state.owner :to ""}
      {:set state.fStatus :to -1}
      {:set state.fPrio :to -1}
      {:set state.fProject :to -1}
      {:set state.scope :to all}
      {:set state.query :to ""}
      {:run selectCard :with {:c "{{item.c}}" :i "{{item.at}}"}}]
    :openRow [{:note "A table row chosen: its id is 'column-index'"}
      {:run selectCard
        :with {:c "{{number(split(event.id ?? event.value, '-')[0])}}"
          :i "{{number(split(event.id ?? event.value, '-')[1])}}"}}]
    :switchBoard [{:note "with: id, a board from the sidebar: keeps this board's columns and brings the other's in"}
      {:if "{{id == state.boardId}}" :then [{:stop true}]}
      {:set state.boardId :to "{{id}}"}
      {:set state.board :to "{{find(state.boards, it.id == id).label}}"}
      {:set state.selected :to nil}
      {:set state.undoStack :to [] :note "undo keeps one board's columns"}]
    :saveCard [{:if "{{computed.card == null || length(trim(state.edit.title)) == 0}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues
        #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}"
        :fields {:title "{{trim(state.edit.title)}}"})
      {:set state.edit.open :to false}
      {:run notify :with {:message "{{t('toast.renamed')}}"}}
      {:run logActivity :with {:key renamed :title "{{trim(state.edit.title)}}" :column ""}}]
    :closeEdit [{:set state.edit.open :to false}]
    :moveSelected [{:note "with: by, -1 or 1"}
      {:run moveCard :with {:from "{{state.selected.c}}" :at "{{state.selected.i}}" :to "{{state.selected.c + by}}"}}]
    :openPropPick [{:note "with: kind, 'status', 'priority' or 'assignee': a picker over the selected card's property"}
      {:set state.propKind :to "{{kind}}"}
      {:note "the current value is highlighted, as Enter would keep it"}
      {:set state.propPick
        :to {:open true
          :query ""
          :top 0
          :index "{{length(state.marked) > 0 ? 0 : kind == 'status' ? state.selected.c : kind == 'priority' ? computed.card.priority ?? 0 : max(0, indexOf(map(state.members, it.who), computed.card.who))}}"}}]
    :selectNext {:title "Select the next issue"
      :category Go
      :icon arrow-down
      :enablement "{{computed.next != null}}"
      :do [{:run selectCard :with {:c "{{computed.next.c}}" :i "{{computed.next.at}}"}}]}
    :selectPrev {:title "Select the previous issue"
      :category Go
      :icon arrow-up
      :enablement "{{computed.prev != null}}"
      :do [{:run selectCard :with {:c "{{computed.prev.c}}" :i "{{computed.prev.at}}"}}]}
    :changeStatus {:title "Change status…"
      :category Issue
      :icon circle-dashed
      :enablement "{{computed.card != null || length(state.marked) > 0}}"
      :do [{:run openPropPick :with {:kind status}}]}
    :changePriority {:title "Change priority…"
      :category Issue
      :icon chevrons-up
      :enablement "{{computed.card != null || length(state.marked) > 0}}"
      :do [{:run openPropPick :with {:kind priority}}]}
    :changeAssignee {:title "Assign to…"
      :category Issue
      :icon user
      :enablement "{{computed.card != null || length(state.marked) > 0}}"
      :do [{:run openPropPick :with {:kind assignee}}]}
    :assignToMe {:title "Assign to me"
      :category Issue
      :icon user-check
      :enablement "{{computed.card != null}}"
      :do [{:run setAssignee :with {:who "{{state.me}}"}}]}
    :newCardCommand {:title "New issue" :category Issue :icon plus :do [{:run openComposer :with {:c 0}}]}
    :goToCardCommand {:title "Go to issue…"
      :category Go
      :icon search
      :do [{:set state.issuePick.query :to ""} {:set state.issuePick.open :to true}]}
    :commandPalette {:title "Show all commands"
      :category Go
      :icon command
      :do [{:set state.quickPick.query :to ""} {:set state.quickPick.open :to true}]}
    :editCard {:title "Edit issue…"
      :category Issue
      :icon pencil
      :enablement "{{computed.card != null}}"
      :do [{:set state.edit :to {:open true :title "{{computed.card.title}}"}}]}
    :deleteCard {:title "Delete issue"
      :category Issue
      :icon trash-2
      :enablement "{{computed.card != null}}"
      :do [{:run remember}
        {:run notify :with {:message "{{t('toast.deleted', { title: computed.card.title })}}"}}
        {:run logActivity :with {:key deleted :title "{{computed.card.title}}" :column ""}}
        (node :delete issues #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}")
        {:set state.selected :to nil}]}
    :moveRight {:title "Move issue right"
      :category Issue
      :icon arrow-right
      :enablement "{{computed.card != null && state.selected.c < computed.lastColumn}}"
      :do [{:run moveSelected :with {:by 1}}]}
    :moveLeft {:title "Move issue left"
      :category Issue
      :icon arrow-left
      :enablement "{{computed.card != null && state.selected.c > 0}}"
      :do [{:run moveSelected :with {:by -1}}]}
    :showBoard {:title "Show the board" :category View :icon kanban :do [{:set state.view :to board}]}
    :showList {:title "Show the list" :category View :icon layout-list :do [{:set state.view :to list}]}
    :showTable {:title "Show the table" :category View :icon table :do [{:set state.view :to table}]}
    :toggleTheme {:title "Switch light and dark"
      :category View
      :icon moon
      :do [{:set state.theme :to "{{state.theme == 'light' ? 'dark' : 'light'}}"}]}
    :togglePeek {:title "Show or hide the side panel" :category View :icon panel-right :do [{:toggle state.peek}]}
    :clearFilters {:title "Clear the filters"
      :category View
      :do [{:set state.filter :to all}
        {:set state.owner :to ""}
        {:set state.fStatus :to -1}
        {:set state.fPrio :to -1}
        {:set state.query :to ""}
        {:set state.fProject :to -1}
        {:set state.scope :to all}
        {:set state.searchOpen :to false}]}}
  :keys {:c {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do newCardCommand}
    :"mod+n" newCardCommand
    :"mod+p" goToCardCommand
    :"mod+shift+p" commandPalette
    :"mod+k" commandPalette
    :f {:when "{{!context.textInputFocus && state.page == 'issues'}}"
      :do [{:set state.filterQuery :to ""}
        {:set state.filterProp :to ""}
        {:toggle state.filterOpen}
        {:focus "#filterField"}]}
    :j [{:when "{{!context.textInputFocus && state.page == 'inbox'}}" :do nextNotification}
      {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do selectNext}]
    :k [{:when "{{!context.textInputFocus && state.page == 'inbox'}}" :do previousNotification}
      {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do selectPrev}]
    :s {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do changeStatus}
    :p {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do changePriority}
    :a {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do changeAssignee}
    :i {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do assignToMe}
    :slash {:when "{{!context.textInputFocus}}" :do focusSearch}
    :"mod+e" editCard
    :"mod+j" toggleTheme
    :"mod+right" moveRight
    :"mod+left" moveLeft
    :"mod+enter" openIssue
    :escape [{:when "{{state.issueOpen}}" :do closeIssue}
      {:when "{{length(state.marked) > 0}}" :do clearMarks}
      {:do clearFilters}]}
  :root (node #panel-root :color @color.sidebar
    (node #topBar :dir auto :anchors {:left 220 :right 0 :top 0} :height 48 :color @color.topBar
      (node #logo
        :type label
        :runs [{:icon box}
          "  Flow  "
          {:text "›  {{find(state.boards, it.id == state.boardId).label}}" :color @color.topMuted}]
        :textColor @color.topText
        :anchors {:left 16 :top 0 :bottom 0}
        :width 196
        :align {:horizontal start :vertical center})
      (node #panel-top-bar :anchors {:left 216 :top 16 :bottom 16} :width 1 :color @color.topLine)
      (node #scopeTabs
        :type row
        :gap 2
        :anchors {:left 228 :top 10}
        :width 360
        :height 28
        :role tablist
        :arrows horizontal
        (node #scopeTab
          :repeat {:for scope :in state.scopes}
          :style "{{state.scope == scope ? 'toolButtonOn' : 'toolButton'}}"
          :borderWidth "{{state.scope == scope ? 1 : 0}}"
          :text "{{t('scope.' + scope)}}"
          :role tab
          :selected "{{state.scope == scope}}"
          :onClick {:set state.scope :to "{{scope}}"}))
      (node #members
        :type row
        :gap -5
        :anchors {:right 16 :top 14}
        :width 70
        :height 20
        :cursor hand
        :focusable true
        :role button
        :label "{{t('members.label')}}. {{t('presence.here', { n: length(computed.present) })}}"
        :tooltip "{{t('presence.here', { n: length(computed.present) })}}"
        :isOpen "{{state.membersOpen}}"
        :keys {:enter {:toggle state.membersOpen} :space {:toggle state.membersOpen}}
        :onClick {:toggle state.membersOpen}
        (node #memberMenu
          :overlay {:placement below-end :open state.membersOpen :dismiss outside}
          :y 8
          :width 240
          :height "{{length(state.members) * 40 + 56}}"
          :radius @radius.card
          :shadow {:y 6 :blur 16 :color "#0000002e"}
          :color @color.menu
          (node #label-show-the-cards :style caption :text "{{t('members.title')}}" :anchors {:left 16 :top 12})
          (node #column-member-menu
            :type column
            :anchors {:left 8 :right 8 :top 36}
            :height "{{length(state.members) * 40 + 12}}"
            (node #panel-member
              :repeat {:for member :in state.members}
              :anchors {:left 0 :right 0}
              :height 40
              :radius @radius.chip
              :color "{{state.owner == member.who ? '@color.accentSoft' : '@color.menu'}}"
              :hover {:color @color.chipHover}
              :focusable true
              :role menuitemradio
              :selected "{{state.owner == member.who}}"
              :label "{{member.name}}"
              :onClick [{:set state.owner :to "{{state.owner == member.who ? '' : member.who}}"}
                {:set state.membersOpen :to false}]
              (node #label-member-who
                :style badge
                :text "{{member.who}}"
                :anchors {:left 8 :top 6}
                :width 28
                :height 28
                :radius 999
                :color "{{member.color}}"
                :textColor "#ffffff")
              (node #label-member-name
                :type label
                :text "{{member.name}}"
                :textColor @color.text
                :anchors {:left 46 :top 0 :bottom 0}
                :width 150
                :align {:horizontal start :vertical center}))))
        (node #member
          :repeat {:for member :in state.members}
          :style badge
          :font tiny
          :text "{{member.who}}"
          :width 20
          :height 20
          :radius 999
          :color "{{member.color}}"
          :textColor "#ffffff"
          :borderWidth 2
          :borderColor "{{contains(computed.present, member.who) ? '@color.accent' : '@color.topBar'}}")))
    (node #toolbar :dir auto :anchors {:left 220 :right 0 :top "#topBar.bottom"} :height 44 :color @color.surface
      (node #filterBar :type row :gap 4 :alignItems center :anchors {:left 12 :right 200 :top 8} :height 28
        (node #filterButton
          :style toolButton
          :icon list-filter
          :text "{{length(computed.activeFilters) > 0 ? '' : ' ' + t('filter.button')}}"
          :label "{{t('filter.button')}}"
          :tooltip "{{t('filter.button')}} (F)"
          :isOpen "{{state.filterOpen}}"
          :keys {:enter {:when "{{!context.textInputFocus}}" :do {:toggle state.filterOpen}}
            :space {:when "{{!context.textInputFocus}}" :do {:toggle state.filterOpen}}}
          :onClick [{:set state.filterProp :to ""} {:set state.filterQuery :to ""} {:toggle state.filterOpen}]
          (node #filterMenu
            :overlay {:placement below :open state.filterOpen :dismiss outside}
            :y 4
            :width 220
            :height "{{(state.filterProp == '' ? length(computed.filterPropsShown) : length(computed.filterOptionsShown)) * 30 + 48}}"
            :radius @radius.card
            :shadow {:y 6 :blur 16 :color "#0000002e"}
            :borderWidth 1
            :borderColor @color.border
            :color @color.menu
            (node #filterField
              :type textarea
              :anchors {:left 6 :right 6 :top 4}
              :height 32
              :padding 8
              :color transparent
              :focused {:color transparent}
              :textColor @color.text
              :placeholder "{{t('filter.placeholder')}}"
              :placeholderColor @color.muted
              :label "{{t('filter.placeholder')}}"
              :autofocus true
              :bind state.filterQuery
              :onSubmit [{:if "{{state.filterProp == '' && length(computed.filterPropsShown) > 0}}"
                  :then [{:set state.filterProp :to "{{computed.filterPropsShown[0][0]}}"}
                    {:set state.filterQuery :to ""}
                    {:focus "#filterField"}]
                  :else [{:if "{{state.filterProp != '' && length(computed.filterOptionsShown) > 0}}"
                      :then [{:run setFilter
                          :with {:prop "{{state.filterProp}}"
                            :v "{{computed.filterOptionsShown[0].v}}"
                            :on "{{computed.filterOptionsShown[0].on}}"}}]}]}]
              :keys {:escape {:set state.filterOpen :to false}})
            (node #filterFieldLine :anchors {:left 0 :right 0 :top 38} :height 1 :color @color.border)
            (node #filterList :type column :anchors {:left 6 :right 6 :top 42 :bottom 6} :role menu :arrows vertical
              (node #filterProp
                :repeat {:for p :in computed.filterPropsShown}
                :when "{{state.filterProp == ''}}"
                :style menuRow
                :runs [{:icon "{{p[1]}}" :color @color.muted} "  {{t('filter.' + p[0])}}"]
                :role menuitem
                :label "{{t('filter.' + p[0])}}"
                :onClick [{:set state.filterProp :to "{{p[0]}}"}
                  {:set state.filterQuery :to ""}
                  {:focus "#filterField"}]
                (node #label-filter-prop
                  :type label
                  :font small
                  :icon chevron-right
                  :mirror true
                  :text ""
                  :textColor @color.muted
                  :anchors {:right 0 :top 0 :bottom 0}
                  :width 16
                  :align center))
              (node #filter
                :repeat {:for opt :in computed.filterOptionsShown}
                :when "{{state.filterProp != ''}}"
                :style menuRow
                :runs [{:icon "{{opt.icon}}" :color "{{opt.color}}"} "  {{opt.title}}"]
                :color "{{opt.on ? '@color.accentSoft' : 'transparent'}}"
                :role menuitemcheckbox
                :selected "{{opt.on}}"
                :label "{{opt.title}}"
                :onClick {:run setFilter :with {:prop "{{state.filterProp}}" :v "{{opt.v}}" :on "{{opt.on}}"}}))))
        (node #searchButton
          :style toolButton
          :when "{{!state.searchOpen && state.query == ''}}"
          :icon search
          :text ""
          :width 28
          :fitText false
          :padding 0
          :align center
          :label "{{t('search.label')}}"
          :tooltip "{{t('search.label')}} (Cmd+F)"
          :onClick focusSearch)
        (node #search
          :type textarea
          :when "{{state.searchOpen || state.query != ''}}"
          :width 220
          :height 28
          :padding 8
          :radius @radius.chip
          :color transparent
          :focused {:borderColor @color.accent}
          :borderWidth 1
          :borderColor @color.border
          :textColor @color.text
          :placeholder "{{t('search.placeholder')}}"
          :placeholderColor @color.muted
          :label "{{t('search.label')}}"
          :autofocus true
          :bind state.query
          :keys {:escape [{:set state.query :to ""} {:set state.searchOpen :to false}]})
        (node #filterChip
          :repeat {:for f :in computed.activeFilters}
          :style toolButtonOn
          :borderWidth 1
          :runs [{:text "{{t('filter.' + f.prop)}}" :color @color.muted}
            "  {{t('filter.is')}}  "
            {:text "{{f.value}}" :color @color.text}
            "     "]
          (node #filterChipClear
            :type label
            :font small
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
            :label "{{t('filter.clear')}}"
            :onClick {:run clearFilter :with {:prop "{{f.prop}}"}})))
      (node #displayButton
        :style "{{state.displayOpen ? 'toolButtonOn' : 'toolButton'}}"
        :anchors {:right 12 :top 8}
        :icon sliders-horizontal
        :text " {{t('display.button')}}"
        :label "{{t('display.button')}}"
        :tooltip "{{t('display.button')}}"
        :isOpen "{{state.displayOpen}}"
        :keys {:enter {:toggle state.displayOpen} :space {:toggle state.displayOpen}}
        :onClick {:toggle state.displayOpen}
        (node #displayMenu
          :overlay {:placement below-end :open state.displayOpen :dismiss outside}
          :y 4
          :width 280
          :height 288
          :radius @radius.card
          :shadow {:y 6 :blur 16 :color "#0000002e"}
          :borderWidth 1
          :borderColor @color.border
          :color @color.menu
          (node #displayLayout :style caption :text "{{t('display.layout')}}" :anchors {:left 12 :top 12})
          (node #views
            :type row
            :gap 4
            :anchors {:left 12 :right 12 :top 34}
            :height 52
            :role radiogroup
            :arrows horizontal
            (node #viewBoard
              :use layoutChoice
              :with {:icon kanban :text "{{trim(t('view.board'))}}" :on "{{state.view == 'board'}}"}
              :onClick showBoard)
            (node #viewList
              :use layoutChoice
              :with {:icon layout-list :text "{{trim(t('view.list'))}}" :on "{{state.view == 'list'}}"}
              :onClick showList)
            (node #viewTable
              :use layoutChoice
              :with {:icon table :text "{{trim(t('view.table'))}}" :on "{{state.view == 'table'}}"}
              :onClick showTable)
            (node #viewTimeline
              :use layoutChoice
              :with {:icon chart-gantt :text "{{trim(t('view.timeline'))}}" :on "{{state.view == 'timeline'}}"}
              :onClick showTimeline))
          (node #displayGrouping :style caption :text "{{t('display.grouping')}}" :anchors {:left 12 :top 100})
          (node #groupings
            :type row
            :gap 4
            :anchors {:left 120 :right 12 :top 96}
            :height 24
            :role radiogroup
            :arrows horizontal
            (node #grouping
              :repeat {:for g :in "['status', 'assignee']"}
              :let {:on "{{state.swimlanes == (g == 'assignee')}}"}
              :style displayPill
              :role radio
              :text "{{t('prop.' + g)}}"
              :selected "{{on}}"
              :disabled "{{state.view != 'board'}}"
              :borderColor "{{on ? '@color.accent' : '@color.border'}}"
              :textColor "{{on && state.view == 'board' ? '@color.text' : '@color.muted'}}"
              :tooltip "{{g == 'assignee' ? t('view.lanes') : ''}}"
              :onClick [{:if "{{!on}}" :then [{:run toggleSwimlanes}]}]))
          (node #displayOrdering :style caption :text "{{t('display.ordering')}}" :anchors {:left 12 :top 134})
          (node #orderings
            :type row
            :gap 4
            :anchors {:left 12 :right 12 :top 154}
            :height 24
            :role radiogroup
            :arrows horizontal
            (node #ordering
              :repeat {:for o :in "['manual', 'priority', 'due']"}
              :style displayPill
              :role radio
              :text "{{t('order.' + o)}}"
              :selected "{{state.order == o}}"
              :borderColor "{{state.order == o ? '@color.accent' : '@color.border'}}"
              :textColor "{{state.order == o ? '@color.text' : '@color.muted'}}"
              :onClick {:set state.order :to "{{o}}"}))
          (node #displayProperties :style caption :text "{{t('display.properties')}}" :anchors {:left 12 :top 192})
          (node #displayProps :type row :wrap true :gap 4 :anchors {:left 12 :right 12 :top 212} :height 76
            (node #displayProp
              :repeat {:for prop :in "['id', 'priority', 'label', 'due', 'estimate', 'assignee']"}
              :style displayPill
              :role switch
              :let {:on "{{!contains(state.hidden, prop)}}"}
              :text "{{t('prop.' + prop)}}"
              :selected "{{on}}"
              :color "{{on ? '@color.chipHover' : 'transparent'}}"
              :borderColor "{{on ? '@color.chipHover' : '@color.border'}}"
              :textColor "{{on ? '@color.text' : '@color.muted'}}"
              :onClick {:set state.hidden
                :to "{{on ? concat(state.hidden, [prop]) : filter(state.hidden, it != prop)}}"})))))
    (node #main
      :dir auto
      :type split
      :anchors {:left 220 :right 0 :top "#toolbar.bottom" :bottom "#statusBar.top"}
      :color @color.surface
      :bind state.sizes
      :minSize 280
      :sashSize 6
      :sashColor transparent
      :sashActiveColor @color.accent
      (node #content :color transparent
        (node #lanesView :use swimlanes :when "{{state.view == 'board' && state.swimlanes}}" :fill true)
        (node #listView :use listView :when "{{state.view == 'list'}}" :fill 6)
        (node #timelineView :use timelineView :when "{{state.view == 'timeline'}}" :fill 6)
        (node #board :type row :when "{{state.view == 'board' && !state.swimlanes}}" :fill true
          (node #column
            :repeat {:for column :in computed.columns :index c}
            :anchors {:top 0 :bottom 0}
            :grow 1
            :color transparent
            :onDrop {:run moveCard :with {:from "{{event.data.from}}" :at "{{event.data.at}}" :to "{{c}}"}}
            (node #columnBody :fill 4 :radius @radius.card :color @color.column
              (node #columnStatus
                :type label
                :icon "{{state.statusIcons[c] ?? 'circle'}}"
                :text ""
                :textColor "{{column.color}}"
                :fitText false
                :width 18
                :anchors {:left 10 :top 0}
                :height 40
                :align center)
              (node #columnTitle :type row :gap 8 :alignItems center :anchors {:left 36 :right 72 :top 0} :height 40
                (node #columnName
                  :type label
                  :font small
                  :text "{{column.name}}"
                  :textColor @color.text
                  :fitText true
                  :height 40
                  :align {:horizontal start :vertical center})
                (node #columnCount
                  :type label
                  :font small
                  :text "{{count(computed.visible, it.c == c)}}"
                  :fitText true
                  :height 40
                  :align {:horizontal start :vertical center}
                  :textColor "{{computed.over[c] ? '@bug' : '@color.muted'}}"
                  :label "{{computed.over[c] ? t('column.over', { name: column.name, count: length(column.cards), limit: column.limit }) : length(column.cards)}}")
                (node #columnLimit
                  :style caption
                  :when "{{column.limit != null && computed.over[c]}}"
                  :text "{{t('column.limit', { limit: column.limit })}}"
                  :height 40
                  :align {:horizontal start :vertical center}
                  :textColor "{{computed.over[c] ? '@bug' : '@color.muted'}}"))
              (node #columnMore
                :type label
                :icon ellipsis
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
                :tooltip "{{t('column.more')}}"
                :label "{{t('column.more')}}"
                :onClick {:run openColumnMenu :with {:c "{{c}}"}})
              (node #columnAdd
                :type label
                :icon plus
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
                :tooltip "{{t('composer.in', { column: column.name })}}"
                :label "{{t('composer.in', { column: column.name })}}"
                :onClick {:run openComposer :with {:c "{{c}}"}})
              (node #cards :type stack :anchors {:left 6 :right 6 :top 40 :bottom 6} :gap 6 :scroll vertical
                (node #card
                  :repeat {:for row
                    :in "sort(filter(computed.allCards, it.c == c), state.order == 'priority' ? ((it.priority ?? 0) == 0 ? 9 : it.priority) : state.order == 'due' ? (it.due ?? '9999-12-31') : it.at)"
                    :index k}
                  :let {:i "{{row.at}}" :card "{{column.cards[row.at]}}"}
                  :when "{{contains(computed.visibleRefs, card.ref)}}"
                  :anchors {:left 0 :right 0}
                  :height "{{length(card.title) > ((window.width - 260) / 4 - 36) / 5.8 ? 96 : 82}}"
                  :radius @radius.card
                  :color "{{contains(state.marked, card.ref) ? '@color.accentSoft' : '@color.card'}}"
                  :borderWidth 1
                  :borderColor "{{(state.selected != null && state.selected.c == c && state.selected.i == i) || contains(state.marked, card.ref) ? '@color.accent' : '@color.border'}}"
                  :shadow {:y 1 :blur 2 :color "#0000000f"}
                  :hover {:borderColor "{{state.selected != null && state.selected.c == c && state.selected.i == i ? '@color.accent' : '@color.muted'}}"}
                  :cursor hand
                  :focusable true
                  :role button
                  :label "{{t('card.label', { title: card.title, tag: t('tag.' + card.tag), who: find(state.members, it.who == card.who).name ?? card.who, column: column.name })}}"
                  :onFocus {:run selectCard :with {:c "{{c}}" :i "{{i}}"}}
                  :keys {:enter editCard :space togglePeek}
                  :drag {:data {:from "{{c}}" :at "{{i}}"}}
                  :dragging {:color @color.column :borderColor @color.border :shadow {:y 0 :blur 0 :color "#00000000"}}
                  :onClick {:run selectCard :with {:c "{{c}}" :i "{{i}}"}}
                  :onDoubleClick [{:run selectCard :with {:c "{{c}}" :i "{{i}}"}} {:run openIssue}]
                  :onContextMenu {:run openCardMenu :with {:c "{{c}}" :i "{{i}}"}}
                  (node #cardRef
                    :when "{{!contains(state.hidden, 'id')}}"
                    :type label
                    :font small
                    :text "{{card.ref ?? ''}}"
                    :textColor @color.muted
                    :fitText true
                    :anchors {:left 12 :top 10})
                  (node #cardOwner
                    :when "{{!contains(state.hidden, 'assignee')}}"
                    :style badge
                    :font tiny
                    :text "{{card.who}}"
                    :width 18
                    :height 18
                    :radius 999
                    :anchors {:right 10 :top 8}
                    :color "{{find(state.members, it.who == card.who).color ?? '@color.muted'}}"
                    :textColor "#ffffff"
                    :tooltip "{{find(state.members, it.who == card.who).name ?? card.who}}")
                  (node #cardTitle
                    :type label
                    :text "{{card.title}}"
                    :textColor @color.text
                    :anchors {:left 12 :right 12 :top 30}
                    :height "{{length(card.title) > ((window.width - 260) / 4 - 36) / 5.8 ? 34 : 17}}"
                    :wrap true
                    :align {:horizontal start :vertical start})
                  (node #cardChips
                    :type row
                    :gap 4
                    :alignItems center
                    :anchors {:left 10 :right 10 :bottom 9}
                    :height 22
                    (node #cardPriority
                      :style cardChip
                      :borderWidth 0
                      :when "{{!contains(state.hidden, 'priority')}}"
                      :icon "{{state.priorities[card.priority ?? 0]}}"
                      :text ""
                      :width 22
                      :fitText false
                      :align center
                      :textColor "{{card.priority == 1 ? '@bug' : '@color.muted'}}"
                      :tooltip "{{t('priority.' + (card.priority ?? 0))}}")
                    (node #cardTag
                      :when "{{!contains(state.hidden, 'label')}}"
                      :style cardChip
                      :runs [{:text "●" :font symbols :size 10 :color "@{{card.tag}}"} " {{t('tag.' + card.tag)}}"]
                      :tooltip "{{t('tag.' + card.tag)}}")
                    (node #cardLabels
                      :when "{{!contains(state.hidden, 'label') && length(card.labels ?? []) > 0}}"
                      :style cardChip
                      :text "+{{length(card.labels ?? [])}}"
                      :tooltip "{{join(map(card.labels ?? [], t('label.' + it)), ', ')}}")
                    (node #cardDue
                      :style cardChip
                      :when "{{!contains(state.hidden, 'due') && (card.due != null)}}"
                      :icon calendar
                      :text "{{formatDate(dateMs(card.due), 'MMM d', 'utc')}}"
                      :tooltip "{{formatDate(dateMs(card.due), 'medium', 'utc')}}"
                      :textColor "{{daysBetween(state.today, card.due) < 0 && c != computed.lastColumn ? '@bug' : '@color.muted'}}")
                    (node #cardNotes
                      :style cardChip
                      :borderWidth 0
                      :when "{{length(card.comments ?? []) > 0}}"
                      :icon message-square
                      :text " {{length(card.comments ?? [])}}"
                      :tooltip "{{t('drawer.comments', { n: length(card.comments ?? []) })}}")
                    (node #cardPoints
                      :when "{{!contains(state.hidden, 'estimate')}}"
                      :style cardChip
                      :borderWidth 0
                      :icon triangle
                      :text "{{card.points}}"
                      :tooltip "{{t('card.points', { n: card.points })}}")))))))
        (node #cardTable
          :type table
          :when "{{state.view == 'table'}}"
          :fill 6
          :radius @radius.card
          :color @color.surface
          :rowHeight 32
          :items "{{computed.rows}}"
          :value "{{state.selected == null ? null : state.selected.c + '-' + state.selected.i}}"
          :sort "{{state.tableSort}}"
          :columns "{{computed.tableColumns}}"
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.accentSoft
          :accentColor @color.accent
          :onChange openRow
          :keys {:space togglePeek}
          :onOpen [{:run openRow} {:run editCard}]
          :onSort {:set state.tableSort :to {:column "{{event.column}}" :descending "{{event.descending}}"}}
          :onColumnResize {:set "state.columnWidths[event.column]" :to "{{event.width}}"}))
      (node #details :when "{{state.peek}}" :color transparent
        (node #detailBody :fill 6 :radius @radius.card :color @color.surface
          (node #chartTitle
            :when "{{computed.card == null}}"
            :style caption
            :text "{{t('chart.title')}}"
            :icon bar-chart-3
            :anchors {:left 20 :top 18})
          (node #chart
            :when "{{computed.card == null}}"
            :use barChart
            :with {:values "{{computed.counts}}" :color @color.accent :gap 0.4}
            :anchors {:left 20 :right 20 :top 44}
            :height 96)
          (node #chartLabels
            :when "{{computed.card == null}}"
            :type row
            :anchors {:left 20 :right 20 :top 146}
            :height 32
            (node #chartLabel
              :repeat {:for column :in computed.columns}
              :type label
              :grow 1
              :anchors {:top 0 :bottom 0}
              :font small
              :align {:horizontal center :vertical start}
              :textColor @color.muted
              :text "{{column.name}}: {{length(column.cards)}}"))
          (node #detailLine
            :when "{{computed.card == null}}"
            :anchors {:left 20 :right 20 :top 186}
            :height 1
            :color @color.border)
          (node #noCard
            :when "{{computed.card == null}}"
            :type label
            :anchors {:left 20 :right 20 :top 206}
            :height 60
            :wrap true
            :textColor @color.muted
            :runs ["{{t('noCard.select')}}"
              {:text "{{t('noCard.rightClick')}}" :bold true}
              "{{t('noCard.menu')}}"
              {:text "Cmd+P" :bold true}
              "{{t('noCard.goTo')}}"])
          (node #insightsPanel
            :use insights
            :when "{{computed.card == null}}"
            :anchors {:left 20 :right 20 :top 276 :bottom 16})
          (node #drawer :use cardDrawer :when "{{computed.card != null}}" :anchors {:left 8 :right 8 :top 18 :bottom 16}))))
    (node #statusBar
      :use statusBar
      :anchors {:left 220 :right 0 :bottom 0}
      :with {:height 32
        :color @color.surface
        :textColor @color.muted
        :highlight @color.chipHover
        :items [{:text "{{t('status.revision', { number: revision.number, author: revision.author, message: revision.message })}}"}
          {:text "{{window.width}} × {{window.height}}" :side end}]})
    (node #issuePage
      :use issuePage
      :when "{{state.issueOpen && computed.card != null}}"
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node #cardMenu
      :use contextMenu
      :with {:open state.cardMenu
        :items "{{menus.card}}"
        :x "{{min(state.menuAt.x, window.width - 228)}}"
        :y "{{state.menuAt.y}}"
        :width 220
        :color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accentSoft})
    (node #columnMenuList
      :use contextMenu
      :with {:open state.columnMenu
        :items "{{menus.column}}"
        :x "{{min(state.menuAt.x, window.width - 228)}}"
        :y "{{state.menuAt.y}}"
        :width 220
        :color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accentSoft})
    (node #editDialog
      :use dialog
      :with {:open state.edit.open
        :title "{{t('edit.title')}}"
        :message "{{t('edit.message')}}"
        :width 440
        :height 180
        :color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('detail.save')}}" :run saveCard} {:title "{{t('detail.cancel')}}" :run closeEdit}]}
      (node #editTitle
        :type textarea
        :anchors {:left 20 :right 20 :top 76}
        :height 34
        :padding 10
        :radius @radius.chip
        :borderWidth 1
        :borderColor @color.border
        :color @color.input
        :focused {:borderColor @color.accent}
        :textColor @color.text
        :autofocus true
        :label "{{t('edit.title')}}"
        :bind state.edit.title
        :onSubmit saveCard))
    (node #settings :use settingsSheet)
    (node #composer
      :overlay {:placement window :open state.composer.open :dismiss outside :modal true}
      :fill true
      :color @color.backdrop
      (node #composerBox
        :overlay {:placement window :open state.composer.open}
        :role dialog
        :label "{{t('composer.title')}}"
        :x "{{max(0, round((window.width - 560) / 2))}}"
        :y "{{round(window.height * 0.15)}}"
        :width 560
        :height 210
        :radius 10
        :color @color.menu
        :borderWidth 1
        :borderColor @color.border
        :shadow {:y 12 :blur 32 :color "#00000040"}
        (node #composerTeam :style cardChip :icon box :text " Flow" :anchors {:left 16 :top 14})
        (node #composerTitle
          :style cardChip
          :borderWidth 0
          :text "›  {{t('composer.title')}}"
          :anchors {:left "#composerTeam.right" :top 14})
        (node #newCard
          :type textarea
          :anchors {:left 10 :right 16 :top 42}
          :height 40
          :padding 6
          :font title
          :color transparent
          :focused {:color transparent}
          :textColor @color.text
          :placeholder "{{t('composer.placeholder')}}"
          :label "{{t('composer.placeholder')}}"
          :placeholderColor @color.muted
          :autofocus true
          :bind state.newCard
          :onSubmit addCard
          :keys {:escape {:set state.composer.open :to false}})
        (node #composerNotes
          :type textarea
          :anchors {:left 10 :right 16 :top 84}
          :height 30
          :padding 6
          :color transparent
          :focused {:color transparent}
          :textColor @color.text
          :placeholder "{{t('composer.notes')}}"
          :label "{{t('composer.notes')}}"
          :placeholderColor @color.muted
          :bind state.composer.notes
          :keys {:escape {:set state.composer.open :to false}})
        (node #composerChips :type row :gap 6 :anchors {:left 16 :right 16 :bottom 60} :height 26
          (node #composerStatus
            :style cardChip
            :runs [{:icon "{{state.statusIcons[state.composer.c] ?? 'circle'}}"
                :color "{{computed.columns[state.composer.c].color}}"}
              " {{computed.columns[state.composer.c].name}}"]
            :cursor hand
            :focusable true
            :role button
            :label "{{t('composer.in', { column: computed.columns[state.composer.c].name })}}"
            :onClick {:set state.composer.c :to "{{(state.composer.c + 1) % length(computed.columns)}}"})
          (node #composerPriority
            :style cardChip
            :icon "{{state.priorities[state.composer.p]}}"
            :text " {{t('priority.' + state.composer.p)}}"
            :cursor hand
            :focusable true
            :role button
            :label "{{t('quick.priority')}}"
            :onClick {:set state.composer.p :to "{{(state.composer.p + 1) % length(state.priorities)}}"})
          (node #composerWho
            :style cardChip
            :icon circle-user
            :text " {{find(state.members, it.who == state.composer.who).name}}"
            :cursor hand
            :focusable true
            :role button
            :label "{{t('quick.assignee')}}"
            :onClick {:set state.composer.who
              :to "{{state.members[(indexOf(map(state.members, it.who), state.composer.who) + 1) % length(state.members)].who}}"})
          (node #composerTag
            :style cardChip
            :runs [{:text "●" :font symbols :size 10 :color "@{{state.composer.tag}}"}
              " {{t('tag.' + state.composer.tag)}}"]
            :cursor hand
            :focusable true
            :role button
            :label "{{t('issue.labels')}}"
            :onClick {:set state.composer.tag
              :to "{{state.filters[indexOf(state.filters, state.composer.tag) % (length(state.filters) - 1) + 1]}}"}))
        (node #composerLine :anchors {:left 0 :right 0 :bottom 48} :height 1 :color @color.border)
        (node #composerMore
          :type label
          :font small
          :fitText true
          :height 28
          :anchors {:right "#composerCreate.left" :bottom 10}
          :padding 10
          :runs [{:icon "{{state.composer.more ? 'toggle-right' : 'toggle-left'}}"
              :color "{{state.composer.more ? '@color.accent' : '@color.muted'}}"}
            "  {{t('composer.more')}}"]
          :textColor @color.muted
          :cursor hand
          :focusable true
          :role switch
          :selected "{{state.composer.more}}"
          :label "{{t('composer.more')}}"
          :onClick {:set state.composer.more :to "{{!state.composer.more}}"})
        (node #composerCreate
          :type label
          :font small
          :text "{{t('composer.create')}}"
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
    (node #toast
      :use toast
      :with {:open state.toast.open
        :message "{{state.toast.message}}"
        :width 320
        :actions "{{state.toast.undo ? [{ title: t('toast.undo'), run: 'undoChange' }] : []}}"
        :closeLabel "{{t('toast.close')}}"
        :color @color.menu
        :textColor @color.text
        :accent @color.accent})
    (node #quickPick
      :use quickPick
      :with {:color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :accent @color.accent
        :highlight @color.accentSoft
        :field @color.field
        :providers [{:items "{{concat(computed.palette, computed.picks)}}" :placeholder "{{t('quick.commands')}}"}
          {:prefix "#" :items "{{computed.picks}}" :placeholder "{{t('quick.cards')}}"}]})
    (node #issuePick
      :use quickPick
      :with {:name issuePick
        :color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :accent @color.accent
        :highlight @color.accentSoft
        :field @color.field
        :providers [{:items "{{computed.picks}}" :placeholder "{{t('quick.cards')}}"}]})
    (node #propPick
      :use quickPick
      :with {:name propPick
        :width 360
        :recentLimit 0
        :color @color.menu
        :textColor @color.text
        :dimColor @color.muted
        :accent @color.accent
        :highlight @color.accentSoft
        :field @color.field
        :providers [{:items "{{computed.propItems}}" :placeholder "{{t('quick.' + (state.propKind ?? 'status'))}}"}]})
    (node #inboxPage
      :use inboxPage
      :when "{{state.page == 'inbox'}}"
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node #cyclesPage
      :use cyclesPage
      :when "{{state.page == 'cycles'}}"
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node #projectsPage
      :use projectsPage
      :when "{{state.page == 'projects'}}"
      :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"})
    (node #sidebar :use sidebar)
    (node #bulkBar
      :when "{{state.page == 'issues' && length(state.marked) > 0}}"
      :x "{{228 + round((window.width - 236 - 340) / 2)}}"
      :anchors {:bottom 56}
      :width 340
      :height 40
      :radius 8
      :color @color.menu
      :borderWidth 1
      :borderColor @color.border
      :shadow {:y 6 :blur 18 :color "#00000030"}
      :role toolbar
      :label "{{t('bulk.count', { n: length(state.marked) })}}"
      (node #row-bulk-bar
        :type row
        :gap 4
        :alignItems center
        :color transparent
        :anchors {:left 6 :right 6 :top 6 :bottom 6}
        (node #bulkCount
          :type label
          :font small
          :text "{{t('bulk.count', { n: length(state.marked) })}}"
          :textColor @color.text
          :fitText true
          :height 28
          :padding 8
          :radius 6
          :borderWidth 1
          :borderColor @color.border
          :align {:horizontal start :vertical center})
        (node #bulkClear
          :style propValue
          :icon x
          :text ""
          :width 28
          :fitText false
          :padding 0
          :align center
          :tooltip "{{t('bulk.clear')}}"
          :label "{{t('bulk.clear')}}"
          :onClick clearMarks)
        (node #bulkStatus
          :style propValue
          :icon circle-dashed
          :text " {{t('issue.status')}}"
          :tooltip S
          :onClick changeStatus)
        (node #bulkPriority
          :style propValue
          :icon chevrons-up
          :text " {{t('issue.priority')}}"
          :tooltip P
          :onClick changePriority)
        (node #bulkAssignee
          :style propValue
          :icon user
          :text " {{t('issue.assignee')}}"
          :tooltip A
          :onClick changeAssignee)
        (node #bulkDelete
          :style propValue
          :icon trash-2
          :text ""
          :width 28
          :fitText false
          :padding 0
          :align center
          :textColor @bug
          :tooltip "{{t('bulk.delete')}}"
          :label "{{t('bulk.delete')}}"
          :onClick bulkDelete))))}
