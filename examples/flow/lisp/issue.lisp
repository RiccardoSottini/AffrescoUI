{:note "Flow's issue page, included by index.screen.json: the selected card on a page of its own, its description and activity on the left, its properties on the right."
  :state {:issueOpen false
    :subOpen false
    :issueStatusOpen false
    :issuePriorityOpen false
    :issueLabelsOpen false
    :issueWhoOpen false
    :issueDueOpen false
    :issueEstimateOpen false
    :issueTagOpen false
    :estimates [0 1 2 3 5 8]}
  :computed {:issueEvents "{{computed.card == null ? [] : filter(state.activity, it.title == computed.card.title)}}"}
  :styles {:propName {:type label
      :font small
      :textColor @color.muted
      :width 84
      :height 32
      :align {:horizontal start :vertical center}}
    :propValue {:type label
      :font small
      :height 28
      :padding 8
      :fitText true
      :radius @radius.chip
      :color transparent
      :hover {:color @color.sidebarHover}
      :textColor @color.text
      :cursor hand
      :focusable true
      :role button}
    :pickRow {:type label
      :font small
      :height 30
      :padding 10
      :anchors {:left 4 :right 4}
      :radius @radius.chip
      :color transparent
      :hover {:color @color.chipHover}
      :textColor @color.text
      :align {:horizontal start :vertical center}
      :cursor hand
      :focusable true
      :role menuitem}
    :pickMenu {:type column
      :y 6
      :width 200
      :justify center
      :radius @radius.card
      :shadow {:y 6 :blur 16 :color "#0000002e"}
      :color @color.menu}}
  :actions {:setPriority [{:note "with: p, the priority from 0 (none) to 4 (low); every marked card while some are"}
      {:if "{{length(state.marked) > 0}}" :then [{:run bulkPriority :with {:p "{{p}}"}} {:stop true}]}
      {:if "{{computed.card == null}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues
        #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}"
        :fields {:priority "{{p}}"})
      {:set state.issuePriorityOpen :to false}]
    :setEstimate [{:note "with: n, the points"}
      {:set state.issueEstimateOpen :to false}
      {:if "{{computed.card == null}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues
        #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}"
        :fields {:points "{{n}}"})]
    :setTag [{:note "with: tag, the issue's main label; its project follows it"}
      {:set state.issueTagOpen :to false}
      {:if "{{computed.card == null}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}" :fields {:tag "{{tag}}"})]
    :setAssignee [{:note "with: who, a member's initials; every marked card while some are"}
      {:if "{{length(state.marked) > 0}}" :then [{:run bulkAssignee :with {:who "{{who}}"}} {:stop true}]}
      {:if "{{computed.card == null}}" :then [{:stop true}]}
      {:run remember}
      (node :update issues #"{{computed.columns[state.selected.c].cards[state.selected.i].id}}" :fields {:who "{{who}}"})
      {:set state.issueWhoOpen :to false}]
    :setStatus [{:note "with: to, the column the selected card moves to; every marked card while some are"}
      {:if "{{length(state.marked) > 0}}"
        :then [{:set state.issueStatusOpen :to false} {:run bulkStatus :with {:to "{{to}}"}} {:stop true}]}
      {:set state.issueStatusOpen :to false}
      {:if "{{computed.card == null}}" :then [{:stop true}]}
      {:run moveCard :with {:from "{{state.selected.c}}" :at "{{state.selected.i}}" :to "{{to}}"}}]
    :openIssue {:title "Open issue"
      :category Issue
      :icon maximize-2
      :enablement "{{computed.card != null}}"
      :do [{:set state.issueOpen :to true}]}
    :closeIssue {:title "Close issue"
      :category Issue
      :icon arrow-left
      :enablement "{{state.issueOpen}}"
      :do [{:set state.issueOpen :to false} {:set state.issueDueOpen :to false} {:set state.subOpen :to false}]}}
  :components {:issuePage {:note "The selected card as a page: a header with the way back, the title, description, sub-issues and activity, and a properties panel"
      :params {}
      :node (node #panel-issue-page
        :color @color.surface
        :role dialog
        :label "{{computed.card.ref}} {{computed.card.title}}"
        (node #issueHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
          (node #issueBack
            :style propValue
            :icon arrow-left
            :text ""
            :width 28
            :fitText false
            :padding 0
            :align {:horizontal center :vertical center}
            :anchors {:left 12 :top 10}
            :mirror true
            :tooltip "{{t('issue.back')}}"
            :label "{{t('issue.back')}}"
            :onClick closeIssue)
          (node #issueCrumb
            :type label
            :font small
            :anchors {:left 48 :top 10}
            :height 28
            :width 400
            :textColor @color.muted
            :align {:horizontal start :vertical center}
            :runs ["Flow  ›  " {:text "{{computed.card.ref ?? ''}}" :bold true}])
          (node #issueNav
            :type row
            :gap 2
            :alignItems center
            :justify end
            :anchors {:right 12 :top 10}
            :width 140
            :height 28
            (node #issuePosition
              :type label
              :font small
              :fitText true
              :padding 8
              :textColor @color.muted
              :text "{{indexOf(computed.visibleRefs, computed.card.ref) + 1}} / {{length(computed.visibleRefs)}}")
            (node #issueUp
              :style propValue
              :icon chevron-up
              :text ""
              :width 28
              :fitText false
              :padding 0
              :align center
              :tooltip "{{t('issue.previous')}} (K)"
              :label "{{t('issue.previous')}}"
              :disabled "{{computed.prev == null}}"
              :onClick selectPrev)
            (node #issueDown
              :style propValue
              :icon chevron-down
              :text ""
              :width 28
              :fitText false
              :padding 0
              :align center
              :tooltip "{{t('issue.next')}} (J)"
              :label "{{t('issue.next')}}"
              :disabled "{{computed.next == null}}"
              :onClick selectNext))
          (node #issueHeaderLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border))
        (node #issueMain
          :type column
          :anchors {:left 0 :right 301 :top 49 :bottom 0}
          :scroll vertical
          :gap 12
          :color transparent
          (node #issueTop :height 28 :width 1 :color transparent)
          (node #issueTitle
            :type label
            :font display
            :text "{{computed.card.title}}"
            :textColor @color.text
            :wrap true
            :anchors {:left 40 :right 40}
            :height 44
            :onDoubleClick editCard)
          (node #issueNotes
            :when "{{!state.draft.editing}}"
            :type label
            :anchors {:left 40 :right 40}
            :height 64
            :wrap true
            :cursor ibeam
            :textColor "{{computed.card.notes == null || computed.card.notes == '' ? '@color.muted' : '@color.text'}}"
            :runs "{{concat(computed.card.notes == null || computed.card.notes == '' ? [t('detail.noNotes')] : map(split(computed.card.notes, '*'), { text: it, bold: index % 2 == 1 }), [' ', { text: computed.card.linkText ?? '', link: computed.card.link ?? '' }])}}"
            :onClick editNotes
            :onLink [{:copy "{{event.link}}"}
              {:run notify :with {:message "{{t('toast.linkCopied', { link: event.link })}}"}}])
          (node #issueNotesEditor
            :when "{{state.draft.editing}}"
            :type editor
            :anchors {:left 40 :right 40}
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
          (node #issueNotesButtons
            :when "{{state.draft.editing}}"
            :type row
            :gap 8
            :anchors {:left 40 :right 40}
            :height 30
            (node #issueNotesSave
              :style detailButton
              :text "{{t('detail.save')}}"
              :focusable true
              :role button
              :onClick saveNotes)
            (node #issueNotesCancel
              :style detailButton
              :text "{{t('detail.cancel')}}"
              :focusable true
              :role button
              :onClick cancelNotes))
          (node #issueAddSub
            :when "{{length(computed.card.checklist ?? []) == 0 && !state.subOpen}}"
            :type row
            :anchors {:left 40 :right 40}
            :height 28
            (node #issueAddSubButton
              :style propValue
              :icon plus
              :text " {{t('issue.addSubs')}}"
              :textColor @color.muted
              :onClick {:set state.subOpen :to true}))
          (node #issueSubTitle
            :when "{{length(computed.card.checklist ?? []) > 0}}"
            :style section
            :anchors {:left 40 :right 40}
            :height 28
            :text "{{t('issue.subIssues', { done: computed.checklistDone, total: length(computed.card.checklist ?? []) })}}")
          (node #issueSubs
            :type column
            :anchors {:left 40 :right 40}
            :height "{{length(computed.card.checklist ?? []) * 34}}"
            (node #issueSub
              :repeat {:for item :in computed.card.checklist :index k}
              :anchors {:left 0 :right 0}
              :height 34
              :color transparent
              :hover {:color @color.column}
              (node #issueSubLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
              (node #issueSubBox
                :type checkbox
                :checked "{{item.done}}"
                :label "{{item.text}}"
                :anchors {:left 8 :top 8}
                :width 18
                :height 18
                :activeColor @color.accent
                :trackColor @color.border
                :onChange {:run checkItem :with {:at "{{k}}" :value "{{event.value}}"}})
              (node #issueSubText
                :type label
                :text "{{item.text}}"
                :textColor "{{item.done ? '@color.muted' : '@color.text'}}"
                :anchors {:left 36 :right 30 :top 0 :bottom 0}
                :align {:horizontal start :vertical center})
              (node #issueSubRemove
                :style removeButton
                :anchors {:right 4 :top 5}
                :label "{{t('drawer.remove', { name: item.text })}}"
                :onClick {:run removeFromCard :with {:field checklist :at "{{k}}"}})))
          (node #issueNewSub
            :when "{{length(computed.card.checklist ?? []) > 0 || state.subOpen}}"
            :autofocus "{{state.subOpen}}"
            :style drawerInput
            :anchors {:left 40 :right 40}
            :placeholder "{{t('issue.addSub')}}"
            :label "{{t('issue.addSub')}}"
            :bind state.newItem
            :onSubmit [{:run addToCard
                :with {:field checklist :input "{{state.newItem}}" :value {:text "{{trim(state.newItem)}}" :done false}}}
              {:set state.newItem :to ""}])
          (node #issueActivityTitle
            :type label
            :font heading
            :text "{{t('issue.activity')}}"
            :textColor @color.text
            :anchors {:left 40 :right 40}
            :height 40
            :align {:horizontal start :vertical end})
          (node #issueCreated
            :type label
            :font small
            :icon circle-plus
            :textColor @color.muted
            :anchors {:left 40 :right 40}
            :height 22
            :align {:horizontal start :vertical center}
            :text "  {{t('issue.created', { who: find(state.members, it.who == computed.card.who).name ?? computed.card.who })}}")
          (node #issueEvent
            :repeat {:for entry :in computed.issueEvents}
            :type label
            :font small
            :icon history
            :textColor @color.muted
            :anchors {:left 40 :right 40}
            :height 22
            :align {:horizontal start :vertical center}
            :text "  {{t('activity.' + entry.key, { who: find(state.members, it.who == entry.who).name ?? entry.who, title: entry.title, column: entry.column })}}")
          (node #issueComment
            :repeat {:for note :in computed.card.comments}
            :anchors {:left 40 :right 40}
            :height 64
            :radius @radius.card
            :borderWidth 1
            :borderColor @color.border
            :color @color.card
            (node #issueCommentWho
              :style badge
              :text "{{note.who}}"
              :width 20
              :height 20
              :radius 999
              :anchors {:left 12 :top 10}
              :color "{{find(state.members, it.who == note.who).color ?? '@color.muted'}}"
              :textColor "#ffffff")
            (node #issueCommentName
              :type label
              :font small
              :text "{{find(state.members, it.who == note.who).name ?? note.who}}"
              :textColor @color.text
              :anchors {:left 40 :top 10}
              :height 20
              :fitText true
              :align {:horizontal start :vertical center})
            (node #issueCommentText
              :type label
              :text "{{note.text}}"
              :textColor @color.text
              :wrap true
              :anchors {:left 40 :right 12 :top 32 :bottom 6}))
          (node #issueNewComment
            :style drawerInput
            :anchors {:left 40 :right 40}
            :height 44
            :placeholder "{{t('issue.comment')}}"
            :label "{{t('issue.comment')}}"
            :bind state.newComment
            :onSubmit [{:run addToCard
                :with {:field comments
                  :input "{{state.newComment}}"
                  :value {:who "{{state.me}}" :text "{{trim(state.newComment)}}"}}}
              {:set state.newComment :to ""}]))
        (node #issueSideLine :anchors {:right 300 :top 48 :bottom 0} :width 1 :color @color.border)
        (node #issueSide
          :type column
          :anchors {:right 0 :top 49 :bottom 0}
          :width 300
          :gap 2
          :color @color.sidebar
          :role region
          :label "{{t('issue.properties')}}"
          (node #issueSideTop :height 12 :width 1 :color transparent)
          (node #issueSideTitle
            :style section
            :text "{{t('issue.properties')}}"
            :anchors {:left 16 :right 16}
            :height 28)
          (node #propStatus :type row :anchors {:left 8 :right 16} :height 32 :alignItems center
            (node #propStatusValue
              :style propValue
              :isOpen "{{state.issueStatusOpen}}"
              :label "{{computed.columns[state.selected.c].name}}"
              :runs [{:icon "{{state.statusIcons[state.selected.c] ?? 'circle'}}"
                  :color "{{computed.columns[state.selected.c].color}}"}
                "  {{computed.columns[state.selected.c].name}}"]
              :onClick {:toggle state.issueStatusOpen})
            (node #statusPicker
              :style pickMenu
              :overlay {:placement below :open state.issueStatusOpen :dismiss outside :modal true}
              :height "{{8 + 30 * length(computed.columns)}}"
              :role menu
              (node #statusPick
                :repeat {:for column :in computed.columns :index k}
                :style pickRow
                :icon "{{state.statusIcons[k] ?? 'circle'}}"
                :text "  {{column.name}}"
                :selected "{{k == state.selected.c}}"
                :children [(node #statusCheck
                    :when "{{k == state.selected.c}}"
                    :type label
                    :icon check
                    :text ""
                    :width 28
                    :align center
                    :textColor @color.text
                    :anchors {:right 0 :top 0 :bottom 0})]
                :onClick {:run setStatus :with {:to "{{k}}"}})))
          (node #propPriority :type row :anchors {:left 8 :right 16} :height 32 :alignItems center
            (node #propPriorityValue
              :style propValue
              :isOpen "{{state.issuePriorityOpen}}"
              :label "{{t('priority.' + (computed.card.priority ?? 0))}}"
              :runs [{:icon "{{state.priorities[computed.card.priority ?? 0]}}"
                  :color "{{computed.card.priority == 1 ? '@bug' : '@color.muted'}}"}
                "  {{t('priority.' + (computed.card.priority ?? 0))}}"]
              :onClick {:toggle state.issuePriorityOpen})
            (node #priorityPicker
              :style pickMenu
              :overlay {:placement below :open state.issuePriorityOpen :dismiss outside :modal true}
              :height 158
              :role menu
              (node #priorityPick
                :repeat {:for icon :in state.priorities :index p}
                :style pickRow
                :icon "{{icon}}"
                :text "  {{t('priority.' + p)}}"
                :selected "{{p == (computed.card.priority ?? 0)}}"
                :children [(node #priorityCheck
                    :when "{{p == (computed.card.priority ?? 0)}}"
                    :type label
                    :icon check
                    :text ""
                    :width 28
                    :align center
                    :textColor @color.text
                    :anchors {:right 0 :top 0 :bottom 0})]
                :onClick {:run setPriority :with {:p "{{p}}"}})))
          (node #propWho :type row :anchors {:left 8 :right 16} :height 32 :alignItems center
            (node #propWhoValue
              :style propValue
              :icon circle-user
              :isOpen "{{state.issueWhoOpen}}"
              :text "  {{find(state.members, it.who == computed.card.who).name ?? t('issue.noAssignee')}}"
              :onClick {:toggle state.issueWhoOpen})
            (node #whoPicker
              :style pickMenu
              :overlay {:placement below :open state.issueWhoOpen :dismiss outside :modal true}
              :height "{{8 + 30 * length(state.members)}}"
              :role menu
              (node #whoPick
                :repeat {:for member :in state.members}
                :style pickRow
                :text "{{member.name}}"
                :padding 36
                :selected "{{member.who == computed.card.who}}"
                :onClick {:run setAssignee :with {:who "{{member.who}}"}}
                (node #whoCheck
                  :when "{{member.who == computed.card.who}}"
                  :type label
                  :icon check
                  :text ""
                  :width 28
                  :align center
                  :textColor @color.text
                  :anchors {:right 0 :top 0 :bottom 0})
                (node #whoPickBadge
                  :style badge
                  :text "{{member.who}}"
                  :width 18
                  :height 18
                  :radius 999
                  :anchors {:left 10 :top 6}
                  :color "{{member.color}}"
                  :textColor "#ffffff"))))
          (node #propDue :type row :anchors {:left 8 :right 16} :height 32 :alignItems center
            (node #propDueValue
              :style propValue
              :icon calendar
              :isOpen "{{state.issueDueOpen}}"
              :textColor "{{computed.dueDays != null && computed.dueDays < 0 && state.selected.c != computed.lastColumn ? '@bug' : '@color.text'}}"
              :text "  {{computed.card.due == null ? t('drawer.noDue') : formatDate(dateMs(computed.card.due), 'MMM d', 'utc')}}"
              :onClick {:toggle state.issueDueOpen})
            (node #issueDuePicker
              :overlay {:placement below :open state.issueDueOpen :dismiss outside :modal true}
              :y 6
              :width 280
              :height 292
              :radius @radius.card
              :shadow {:y 6 :blur 16 :color "#0000002e"}
              :color @color.menu
              (node #issueCalendar
                :type calendar
                :fill 10
                :focusable true
                :label "{{t('issue.due')}}"
                :value "{{computed.card.due ?? state.today}}"
                :today "{{state.today}}"
                :firstDayOfWeek "{{settings.board.weekStart}}"
                :monthNames "{{split(t('calendar.months'), ',')}}"
                :dayNames "{{split(t('calendar.days'), ',')}}"
                :textColor @color.text
                :accentColor @color.accent
                :pickedTextColor @color.chipOnText
                :onChange [{:run setDue :with {:date "{{event.value}}"}} {:set state.issueDueOpen :to false}])))
          (node #propEstimate :type row :anchors {:left 8 :right 16} :height 32 :alignItems center
            (node #propEstimateValue
              :style propValue
              :icon triangle
              :isOpen "{{state.issueEstimateOpen}}"
              :label "{{t('card.points', { n: computed.card.points ?? 0 })}}"
              :text "  {{t('card.points', { n: computed.card.points ?? 0 })}}"
              :onClick {:toggle state.issueEstimateOpen})
            (node #estimatePicker
              :style pickMenu
              :overlay {:placement below :open state.issueEstimateOpen :dismiss outside :modal true}
              :height "{{8 + 30 * length(state.estimates)}}"
              :role menu
              (node #estimatePick
                :repeat {:for n :in state.estimates}
                :style pickRow
                :icon triangle
                :text "  {{t('card.points', { n: n })}}"
                :selected "{{n == (computed.card.points ?? 0)}}"
                :onClick {:run setEstimate :with {:n "{{n}}"}})))
          (node #issueSideGap :height 10 :width 1 :color transparent)
          (node #propLabels
            :type column
            :anchors {:left 8 :right 16}
            :height "{{length(computed.card.labels ?? []) > 2 ? 84 : 56}}"
            :alignItems start
            (node #propLabelsName :style propName :height 24 :padding 8 :width 200 :text "{{t('issue.labels')}}")
            (node #propLabelChips :type row :wrap true :gap 4 :x 8 :width 268 :height 60
              (node #propTag
                :style cardChip
                :height 24
                :textColor @color.text
                :tooltip "{{t('issue.changeTag')}}"
                :label "{{t('tag.' + computed.card.tag)}}"
                :cursor hand
                :focusable true
                :role button
                :isOpen "{{state.issueTagOpen}}"
                :onClick {:toggle state.issueTagOpen}
                :runs [{:text "●" :font symbols :size 10 :color "@{{computed.card.tag}}"}
                  " {{t('tag.' + computed.card.tag)}}"]
                (node #tagPicker
                  :style pickMenu
                  :overlay {:placement below :open state.issueTagOpen :dismiss outside :modal true}
                  :height "{{8 + 30 * length(keys(state.tagIcons))}}"
                  :role menu
                  (node #tagPick
                    :repeat {:for tag :in "keys(state.tagIcons)"}
                    :style pickRow
                    :icon "{{state.tagIcons[tag]}}"
                    :text "  {{t('tag.' + tag)}}"
                    :selected "{{tag == computed.card.tag}}"
                    :onClick {:run setTag :with {:tag "{{tag}}"}})))
              (node #propLabel
                :repeat {:for name :in "computed.card.labels ?? []"}
                :style cardChip
                :height 24
                :runs [{:text "●" :font symbols :size 10 :color @color.accent} " {{t('label.' + name)}}"]
                :textColor @color.text
                :tooltip "{{t('issue.removeLabel')}}"
                :cursor hand
                :focusable true
                :role button
                :label "{{t('label.' + name)}}"
                :onClick {:run toggleListItem :with {:field labels :value "{{name}}"}})
              (node #propLabelAdd
                :style cardChip
                :height 24
                :width 24
                :fitText false
                :padding 0
                :align center
                :icon plus
                :text ""
                :label "{{t('issue.addLabel')}}"
                :tooltip "{{t('issue.addLabel')}}"
                :cursor hand
                :focusable true
                :role button
                :isOpen "{{state.issueLabelsOpen}}"
                :onClick {:toggle state.issueLabelsOpen}
                (node #labelPicker
                  :style pickMenu
                  :overlay {:placement below :open state.issueLabelsOpen :dismiss outside :modal true}
                  :height "{{8 + 30 * length(state.labels)}}"
                  :role menu
                  (node #labelPick
                    :repeat {:for name :in state.labels}
                    :style pickRow
                    :icon "{{contains(computed.card.labels ?? [], name) ? 'square-check' : 'square'}}"
                    :text "  {{t('label.' + name)}}"
                    :selected "{{contains(computed.card.labels ?? [], name)}}"
                    :onClick {:run toggleListItem :with {:field labels :value "{{name}}"}})))))
          (node #propProject :type column :anchors {:left 8 :right 16} :height 56 :alignItems start
            (node #propProjectName :style propName :height 24 :padding 8 :width 200 :text "{{t('issue.project')}}")
            (node #propProjectValue
              :style propValue
              :focusable false
              :role text
              :cursor arrow
              :hover {:color transparent}
              :tooltip "{{t('issue.projectFollows')}}"
              :let {:project "{{data.projects[state.projectOf[computed.card.tag] ?? 0]}}"}
              :runs [{:icon "{{project.icon}}" :color "{{project.color}}"} "  {{project.name}}"]))
          (node #propCycle :type column :anchors {:left 8 :right 16} :height 56 :alignItems start
            (node #propCycleName :style propName :height 24 :padding 8 :width 200 :text "{{t('issue.cycle')}}")
            (node #propCycleValue
              :style propValue
              :focusable false
              :role text
              :cursor arrow
              :hover {:color transparent}
              :icon refresh-cw
              :textColor @color.text
              :text "  {{t('cycle.name', { n: state.cycle.number })}}"))))}}}
