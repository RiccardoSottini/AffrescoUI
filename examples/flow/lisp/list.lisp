{:note "Flow's list view, included by index.screen.json: every card in one list, grouped by column, a row each."
  :components {:listView {:note "The cards grouped by column: a header with the column's name and count, then a row a card"
      :params {}
      :node (node #list :type column :scroll vertical :radius @radius.card :color @color.surface :role list
        (node #listGroup
          :repeat {:for column :in computed.columns :index c}
          :anchors {:left 0 :right 0}
          :height "{{36 + (contains(state.collapsedGroups, c) ? 0 : 36 * count(column.cards, contains(computed.visibleRefs, it.ref)))}}"
          :color transparent
          :role group
          :label "{{column.name}}"
          :onDrop {:run moveCard :with {:from "{{event.data.from}}" :at "{{event.data.at}}" :to "{{c}}"}}
          (node #listHeader :anchors {:left 0 :right 0 :top 0} :height 36 :color @color.column
            (node #listCollapse
              :type label
              :icon "{{contains(state.collapsedGroups, c) ? 'chevron-right' : 'chevron-down'}}"
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
              :label "{{t('list.collapse', { name: column.name })}}"
              :onClick {:set state.collapsedGroups
                :to "{{contains(state.collapsedGroups, c) ? filter(state.collapsedGroups, it != c) : concat(state.collapsedGroups, [c])}}"})
            (node #listStatus
              :type label
              :icon "{{state.statusIcons[c] ?? 'circle'}}"
              :text ""
              :textColor "{{column.color}}"
              :fitText true
              :anchors {:left 36 :top 0 :bottom 0}
              :align center)
            (node #listTitle :type row :gap 10 :alignItems center :anchors {:left 60 :right 52 :top 0 :bottom 0}
              (node #listName
                :type label
                :font small
                :text "{{column.name}}"
                :textColor @color.text
                :fitText true
                :height 36
                :align {:horizontal start :vertical center})
              (node #listCount
                :style caption
                :text "{{length(column.cards)}}"
                :textColor "{{computed.over[c] ? '@bug' : '@color.muted'}}"
                :fitText true
                :height 36
                :align {:horizontal start :vertical center}))
            (node #listAdd
              :type label
              :icon plus
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
              :label "{{t('composer.in', { column: column.name })}}"
              :tooltip "{{t('composer.in', { column: column.name })}}"
              :onClick {:run openComposer :with {:c "{{c}}"}}))
          (node #listRows
            :when "{{!contains(state.collapsedGroups, c)}}"
            :type column
            :anchors {:left 0 :right 0 :top 36 :bottom 0}
            (node #listRow
              :repeat {:for row
                :in "sort(filter(computed.allCards, it.c == c), state.order == 'priority' ? ((it.priority ?? 0) == 0 ? 9 : it.priority) : state.order == 'due' ? (it.due ?? '9999-12-31') : it.at)"
                :index k}
              :let {:i "{{row.at}}" :card "{{column.cards[row.at]}}"}
              :when "{{contains(computed.visibleRefs, card.ref)}}"
              :anchors {:left 0 :right 0}
              :height 36
              :color "{{contains(state.marked, card.ref) || (state.selected != null && state.selected.c == c && state.selected.i == i) ? '@color.accentSoft' : 'transparent'}}"
              :hover {:color "{{contains(state.marked, card.ref) || (state.selected != null && state.selected.c == c && state.selected.i == i) ? '@color.accentSoft' : '@color.column'}}"}
              :cursor hand
              :focusable true
              :role listitem
              :label "{{t('card.label', { title: card.title, tag: t('tag.' + card.tag), who: find(state.members, it.who == card.who).name ?? card.who, column: column.name })}}"
              :onFocus {:run selectCard :with {:c "{{c}}" :i "{{i}}"}}
              :keys {:enter editCard :space togglePeek}
              :drag {:data {:from "{{c}}" :at "{{i}}"}}
              :onClick {:run selectCard :with {:c "{{c}}" :i "{{i}}"}}
              :onDoubleClick [{:run selectCard :with {:c "{{c}}" :i "{{i}}"}} {:run openIssue}]
              :onContextMenu {:run openCardMenu :with {:c "{{c}}" :i "{{i}}"}}
              (node #rowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
              (node #rowPriority
                :when "{{!contains(state.hidden, 'priority')}}"
                :type label
                :icon "{{state.priorities[card.priority ?? 0]}}"
                :text ""
                :fitText true
                :textColor "{{card.priority == 1 ? '@bug' : '@color.muted'}}"
                :anchors {:right 136 :top 0 :bottom 0}
                :width 22
                :align center
                :tooltip "{{t('priority.' + (card.priority ?? 0))}}")
              (node #rowRef
                :when "{{!contains(state.hidden, 'id')}}"
                :type label
                :font small
                :text "{{card.ref ?? ''}}"
                :textColor @color.muted
                :width 56
                :anchors {:right 252 :top 0 :bottom 0}
                :align {:horizontal end :vertical center})
              (node #rowStatus
                :type label
                :icon "{{state.statusIcons[c] ?? 'circle'}}"
                :text ""
                :fitText true
                :textColor "{{column.color}}"
                :anchors {:left 18 :top 0 :bottom 0}
                :align center
                :tooltip "{{column.name}}")
              (node #rowTitle
                :type label
                :text "{{card.title}}"
                :textColor @color.text
                :anchors {:left 44 :right 316 :top 0 :bottom 0}
                :align {:horizontal start :vertical center})
              (node #rowTag
                :when "{{!contains(state.hidden, 'label')}}"
                :type label
                :font small
                :runs [{:text "●" :font symbols :size 10 :color "@{{card.tag}}"} " {{t('tag.' + card.tag)}}"]
                :fitText true
                :padding 6
                :height 22
                :radius 999
                :borderWidth 1
                :borderColor @color.border
                :color transparent
                :textColor @color.text
                :anchors {:right 166 :top 7})
              (node #rowDue
                :style caption
                :when "{{!contains(state.hidden, 'due') && (card.due != null)}}"
                :text "{{formatDate(dateMs(card.due), 'MMM d', 'utc')}}"
                :textColor "{{daysBetween(state.today, card.due) < 0 && c != computed.lastColumn ? '@bug' : '@color.muted'}}"
                :width 48
                :anchors {:right 80 :top 0 :bottom 0}
                :align {:horizontal end :vertical center})
              (node #rowPoints
                :when "{{!contains(state.hidden, 'estimate')}}"
                :style caption
                :text "{{card.points}}"
                :width 20
                :anchors {:right 52 :top 0 :bottom 0}
                :align {:horizontal end :vertical center}
                :tooltip "{{t('card.points', { n: card.points })}}")
              (node #rowWho
                :when "{{!contains(state.hidden, 'assignee')}}"
                :style badge
                :text "{{card.who}}"
                :width 22
                :height 22
                :radius 999
                :anchors {:right 20 :top 7}
                :color "{{find(state.members, it.who == card.who).color ?? '@color.muted'}}"
                :textColor "#ffffff"
                :tooltip "{{find(state.members, it.who == card.who).name ?? card.who}}")))))}}}
