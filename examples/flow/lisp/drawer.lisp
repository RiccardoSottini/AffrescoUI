{:note "Flow's card drawer, included by index.screen.json: the selected card's description (rich text, edited in place), due date with a date picker, labels, assignees, checklist, comments and attachments."
  :styles {:section {:type label
      :font small
      :textColor @color.muted
      :height 18
      :align {:horizontal start :vertical center}}
    :drawerInput {:type textarea
      :height 30
      :padding 8
      :radius @radius.chip
      :borderWidth 1
      :borderColor @color.border
      :color @color.input
      :focused {:borderColor @color.accent}
      :textColor @color.text
      :placeholderColor @color.muted}
    :pill {:type label
      :font small
      :height 26
      :padding 8
      :radius 999
      :fitText true
      :focusable true
      :role checkbox
      :cursor hand}
    :removeButton {:type label
      :font small
      :icon x
      :text ""
      :width 24
      :height 24
      :radius 4
      :align center
      :focusable true
      :role button
      :textColor @color.muted
      :hover {:color @color.chipHover}
      :cursor hand}}
  :components {:cardDrawer {:note "The selected card, in a scrolling column"
      :params {}
      :node (node #cardDetail :type column :gap 10 :scroll vertical :color transparent
        (node #detailHead :anchors {:left 12 :right 12} :height 24 :color transparent
          (node #detailTag
            :type label
            :text " {{t('tag.' + computed.card.tag)}}"
            :icon "{{state.tagIcons[computed.card.tag]}}"
            :font small
            :anchors {:left 0 :top 0}
            :padding 4
            :radius 4
            :fitText true
            :color "@{{computed.card.tag}}Soft"
            :textColor "@{{computed.card.tag}}")
          (node #detailColumn
            :style caption
            :text "{{computed.columns[state.selected.c].name}} · {{t('card.points', { n: computed.card.points })}}"
            :anchors {:right 0 :top 2}))
        (node #detailTitle
          :type label
          :font heading
          :text "{{computed.card.title}}"
          :textColor @color.text
          :wrap true
          :anchors {:left 12 :right 12}
          :height 44)
        (node #detailOwner
          :style caption
          :icon user
          :text " {{find(state.members, it.who == computed.card.who).name ?? computed.card.who}}"
          :anchors {:left 12}
          :height 18)
        (node #detailNotes
          :when "{{!state.draft.editing}}"
          :type label
          :anchors {:left 12 :right 12}
          :height 64
          :wrap true
          :textColor @color.text
          :runs "{{concat(computed.card.notes == null || computed.card.notes == '' ? [t('detail.noNotes')] : map(split(computed.card.notes, '*'), { text: it, bold: index % 2 == 1 }), [' ', { text: computed.card.linkText ?? '', link: computed.card.link ?? '' }])}}"
          :onLink [{:copy "{{event.link}}"}
            {:run notify :with {:message "{{t('toast.linkCopied', { link: event.link })}}"}}])
        (node #notesEditor
          :when "{{state.draft.editing}}"
          :type editor
          :anchors {:left 12 :right 12}
          :height 110
          :radius @radius.chip
          :color @color.input
          :textColor @color.text
          :wrap true
          :grammar markdown
          :bind state.draft.notes
          :autofocus true
          :label "{{t('detail.editNotes')}}"
          :keys {:"mod+enter" saveNotes :escape cancelNotes})
        (node #notesButtons :when "{{state.draft.editing}}" :type row :gap 8 :anchors {:left 12 :right 12} :height 30
          (node #notesSave
            :style detailButton
            :text "{{t('detail.save')}}"
            :focusable true
            :role button
            :onClick saveNotes)
          (node #notesCancel
            :style detailButton
            :text "{{t('detail.cancel')}}"
            :focusable true
            :role button
            :onClick cancelNotes))
        (node #detailActions :type row :gap 8 :anchors {:left 12 :right 12} :height 30 :arrows horizontal :role toolbar
          (node #detailEdit
            :style detailButton
            :text "{{window.width < 1440 ? '' : t('detail.edit')}}"
            :label "{{t('detail.edit')}}"
            :tooltip "{{t('detail.edit')}}"
            :icon pencil
            :focusable true
            :role button
            :onClick editCard)
          (node #detailNotesEdit
            :style detailButton
            :text ""
            :icon file-pen-line
            :focusable true
            :role button
            :tooltip "{{t('detail.editNotes')}}"
            :label "{{t('detail.editNotes')}}"
            :onClick editNotes)
          (node #detailLeft
            :style detailButton
            :text ""
            :label "{{t('detail.left')}}"
            :tooltip "{{t('detail.left')}}"
            :icon arrow-left
            :mirror true
            :focusable true
            :role button
            :when "{{state.selected.c > 0}}"
            :onClick moveLeft)
          (node #detailRight
            :style detailButton
            :text "{{window.width < 1440 ? '' : t('detail.right')}}"
            :label "{{t('detail.right')}}"
            :tooltip "{{t('detail.right')}}"
            :icon arrow-right
            :mirror true
            :focusable true
            :role button
            :when "{{state.selected.c < computed.lastColumn}}"
            :onClick moveRight)
          (node #detailDelete
            :style detailButton
            :text "{{window.width < 1440 ? '' : t('detail.delete')}}"
            :label "{{t('detail.delete')}}"
            :tooltip "{{t('detail.delete')}}"
            :icon trash-2
            :textColor @bug
            :focusable true
            :role button
            :onClick deleteCard)
          (node #detailMore
            :style detailButton
            :text ""
            :icon ellipsis
            :focusable true
            :role button
            :tooltip "{{t('detail.more')}}"
            :label "{{t('detail.more')}}"
            :onClick {:run openCardMenu :with {:c "{{state.selected.c}}" :i "{{state.selected.i}}"}}))
        (node #dueTitle :style section :text "{{t('drawer.due')}}" :anchors {:left 12 :right 12})
        (node #dueRow :type row :gap 8 :anchors {:left 12 :right 12} :height 30
          (node #dueButton
            :style detailButton
            :icon calendar
            :focusable true
            :role button
            :isOpen "{{state.dueOpen}}"
            :text " {{computed.card.due == null ? t('drawer.noDue') : formatDate(dateMs(computed.card.due), 'MMM d', 'utc')}}"
            :onClick {:toggle state.dueOpen})
          (node #dueState
            :type label
            :font small
            :when "{{computed.dueDays != null}}"
            :fitText true
            :anchors {:top 0 :bottom 0}
            :align {:horizontal start :vertical center}
            :textColor "{{computed.dueDays < 0 && state.selected.c != computed.lastColumn ? '@bug' : '@color.muted'}}"
            :text "{{computed.dueDays < 0 ? t('drawer.late', { n: -computed.dueDays }) : t('drawer.dueIn', { n: computed.dueDays })}}")
          (node #dueClear
            :style removeButton
            :when "{{computed.card.due != null}}"
            :anchors {:top 3}
            :tooltip "{{t('drawer.clearDue')}}"
            :label "{{t('drawer.clearDue')}}"
            :onClick {:run setDue :with {:date nil}})
          (node #duePicker
            :overlay {:placement below :open state.dueOpen :dismiss outside :modal true}
            :y 6
            :width 280
            :height 292
            :radius @radius.card
            :shadow {:y 6 :blur 16 :color "#0000002e"}
            :color @color.menu
            (node #dueCalendar
              :type calendar
              :fill 10
              :focusable true
              :label "{{t('drawer.due')}}"
              :value "{{computed.card.due ?? state.today}}"
              :today "{{state.today}}"
              :marked "{{filter(map(computed.allCards, it.due), it != null)}}"
              :firstDayOfWeek "{{settings.board.weekStart}}"
              :monthNames "{{split(t('calendar.months'), ',')}}"
              :dayNames "{{split(t('calendar.days'), ',')}}"
              :textColor @color.text
              :accentColor @color.accent
              :pickedTextColor @color.chipOnText
              :keys {:escape {:set state.dueOpen :to false}}
              :onChange {:run setDue :with {:date "{{event.value}}"}})))
        (node #labelsTitle :style section :text "{{t('drawer.labels')}}" :anchors {:left 12 :right 12})
        (node #labels :type row :gap 6 :anchors {:left 12 :right 12} :height 26 :arrows horizontal
          (node #labelChip
            :repeat {:for name :in state.labels}
            :style pill
            :let {:on "{{contains(computed.card.labels ?? [], name)}}"}
            :text " {{t('label.' + name)}}"
            :selected "{{on}}"
            :icon "{{on ? 'check' : 'plus'}}"
            :color transparent
            :borderWidth 1
            :borderColor "{{on ? '@color.accent' : '@color.border'}}"
            :textColor "{{on ? '@color.text' : '@color.muted'}}"
            :onClick {:run toggleListItem :with {:field labels :value "{{name}}"}}))
        (node #assigneesTitle :style section :text "{{t('drawer.assignees')}}" :anchors {:left 12 :right 12})
        (node #assignees :type row :gap 6 :anchors {:left 12 :right 12} :height 30 :arrows horizontal
          (node #assignee
            :repeat {:for member :in state.members}
            :style badge
            :let {:on "{{contains(computed.card.assignees ?? [], member.who)}}"}
            :text "{{member.who}}"
            :width 30
            :height 30
            :radius 999
            :focusable true
            :role checkbox
            :selected "{{on}}"
            :label "{{member.name}}"
            :tooltip "{{member.name}}"
            :cursor hand
            :color "{{on ? member.color : '@color.field'}}"
            :textColor "{{on ? '#ffffff' : '@color.muted'}}"
            :onClick {:run toggleListItem :with {:field assignees :value "{{member.who}}"}}))
        (node #checklistTitle
          :style section
          :anchors {:left 12 :right 12}
          :text "{{t('drawer.checklist', { done: computed.checklistDone, total: length(computed.card.checklist ?? []) })}}")
        (node #checklistBar :type row :anchors {:left 12 :right 12} :height 4 :radius 2 :color @color.field
          (node #checklistDone :grow "{{computed.checklistDone}}" :anchors {:top 0 :bottom 0} :radius 2 :color @chore)
          (node #checklistLeft
            :grow "{{length(computed.card.checklist ?? []) - computed.checklistDone}}"
            :anchors {:top 0 :bottom 0}
            :color transparent))
        (node #checklist
          :type column
          :anchors {:left 12 :right 12}
          :height "{{length(computed.card.checklist ?? []) * 28}}"
          (node #checkRow
            :repeat {:for item :in computed.card.checklist :index k}
            :anchors {:left 0 :right 0}
            :height 28
            :color transparent
            (node #checkBox
              :type checkbox
              :checked "{{item.done}}"
              :label "{{item.text}}"
              :anchors {:left 0 :top 5}
              :width 18
              :height 18
              :activeColor @color.accent
              :trackColor @color.border
              :onChange {:run checkItem :with {:at "{{k}}" :value "{{event.value}}"}})
            (node #checkText
              :type label
              :text "{{item.text}}"
              :textColor "{{item.done ? '@color.muted' : '@color.text'}}"
              :anchors {:left 28 :right 30 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #checkRemove
              :style removeButton
              :anchors {:right 0 :top 2}
              :label "{{t('drawer.remove', { name: item.text })}}"
              :onClick {:run removeFromCard :with {:field checklist :at "{{k}}"}})))
        (node #newItem
          :style drawerInput
          :anchors {:left 12 :right 12}
          :placeholder "{{t('drawer.addItem')}}"
          :label "{{t('drawer.addItem')}}"
          :bind state.newItem
          :onSubmit [{:run addToCard
              :with {:field checklist :input "{{state.newItem}}" :value {:text "{{trim(state.newItem)}}" :done false}}}
            {:set state.newItem :to ""}])
        (node #commentsTitle
          :style section
          :text "{{t('drawer.comments', { n: length(computed.card.comments ?? []) })}}"
          :anchors {:left 12 :right 12})
        (node #comments
          :type column
          :gap 6
          :anchors {:left 12 :right 12}
          :height "{{length(computed.card.comments ?? []) * 46}}"
          (node #comment
            :repeat {:for note :in computed.card.comments}
            :type label
            :anchors {:left 0 :right 0}
            :height 40
            :wrap true
            :padding 6
            :radius @radius.chip
            :color @color.field
            :textColor @color.text
            :font small
            :runs [{:text "{{find(state.members, it.who == note.who).name ?? note.who}}" :bold true} "  {{note.text}}"]))
        (node #newComment
          :style drawerInput
          :anchors {:left 12 :right 12}
          :placeholder "{{t('drawer.addComment')}}"
          :label "{{t('drawer.addComment')}}"
          :bind state.newComment
          :onSubmit [{:run addToCard
              :with {:field comments :input "{{state.newComment}}" :value {:who RS :text "{{trim(state.newComment)}}"}}}
            {:set state.newComment :to ""}])
        (node #attachmentsTitle :style section :text "{{t('drawer.attachments')}}" :anchors {:left 12 :right 12})
        (node #attachments
          :type column
          :anchors {:left 12 :right 12}
          :height "{{length(computed.card.attachments ?? []) * 28}}"
          (node #attachment
            :repeat {:for file :in computed.card.attachments :index k}
            :anchors {:left 0 :right 0}
            :height 28
            :color transparent
            (node #attachmentName
              :type label
              :icon paperclip
              :text " {{file}}"
              :textColor @color.text
              :font small
              :anchors {:left 0 :right 30 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #attachmentRemove
              :style removeButton
              :anchors {:right 0 :top 2}
              :label "{{t('drawer.remove', { name: file })}}"
              :onClick {:run removeFromCard :with {:field attachments :at "{{k}}"}})))
        (node #newAttachment
          :style drawerInput
          :anchors {:left 12 :right 12}
          :placeholder "{{t('drawer.addAttachment')}}"
          :label "{{t('drawer.addAttachment')}}"
          :bind state.newAttachment
          :onSubmit [{:run addToCard
              :with {:field attachments :input "{{state.newAttachment}}" :value "{{trim(state.newAttachment)}}"}}
            {:set state.newAttachment :to ""}]))}}}
