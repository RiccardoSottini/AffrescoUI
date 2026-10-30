{:note "Flow's team pages, included by index.screen.json: the inbox of notifications, the cycles with their burn-up, and the projects. The sidebar and the G chords switch between them and the issues."
  :state {:page issues
    :inboxAt 0
    :cycle {:number 12
      :start "2026-03-09"
      :end "2026-03-22"
      :scopeHistory [22 24 24 27 30 30 31]
      :doneHistory [0 2 5 5 6 8 8]}
    :cycles [{:number 13 :start "2026-03-23" :end "2026-04-05" :scope 9 :done 0}
      {:number 11 :start "2026-02-23" :end "2026-03-08" :scope 26 :done 24}
      {:number 10 :start "2026-02-09" :end "2026-02-22" :scope 21 :done 21}]
    :projectScope all
    :projectOf {:feature 0 :bug 0 :design 1 :chore 2}
    :healthColors {:onTrack "#16a34a" :atRisk "#d97706" :offTrack "#dc2626" :completed "#0d9488"}}
  :computed {:inboxShown "{{filter(data.inbox, !it.snoozed)}}"
    :unread "{{count(data.inbox, !it.read && !it.snoozed)}}"
    :notification "{{computed.inboxShown[state.inboxAt]}}"
    :notificationCard "{{computed.notification == null ? null : find(computed.allCards, it.ref == computed.notification.ref)}}"
    :cycleScope "{{sum(computed.allCards, it.points ?? 0)}}"
    :cycleDone "{{sum(filter(computed.allCards, it.c == computed.lastColumn), it.points ?? 0)}}"
    :cycleTop "{{max(concat(state.cycle.scopeHistory, [computed.cycleScope])) + 4}}"}
  :actions {:addProject {:title "New project"
      :category Go
      :icon plus
      :do [{:insert projects
          :value {:name "{{t('projects.untitled', { n: length(data.projects) + 1 })}}"
            :icon box
            :color "#0d9488"
            :health onTrack
            :lead "{{state.me}}"
            :target "{{state.cycle.end}}"
            :progress 0
            :issues 0}}
        {:set state.projectScope :to all}]}
    :showPage [{:note "with: page, 'issues', 'inbox', 'cycles' or 'projects'"}
      {:set state.page :to "{{page}}"}
      {:set state.issueOpen :to false}]
    :goToInbox {:title "Go to inbox" :category Go :icon inbox :do [{:run showPage :with {:page inbox}}]}
    :goToIssues {:title "Go to issues" :category Go :icon copy :do [{:run showPage :with {:page issues}}]}
    :goToCycles {:title "Go to sprints" :category Go :icon timer :do [{:run showPage :with {:page cycles}}]}
    :goToProjects {:title "Go to projects" :category Go :icon box :do [{:run showPage :with {:page projects}}]}
    :openProject [{:note "with: at, the project's index: its issues, behind a Project filter whose chip removes it"}
      {:set state.fProject :to "{{at}}"}
      {:run showPage :with {:page issues}}]
    :pickNotification [{:note "with: at, the notification's place among the shown ones; marks it read"}
      {:set state.inboxAt :to "{{at}}"}
      (node :update inbox #"{{computed.inboxShown[at].id}}" :fields {:read true})]
    :nextNotification {:title "Next notification"
      :category Inbox
      :icon arrow-down
      :enablement "{{state.page == 'inbox' && state.inboxAt + 1 < length(computed.inboxShown)}}"
      :do [{:run pickNotification :with {:at "{{state.inboxAt + 1}}"}}]}
    :previousNotification {:title "Previous notification"
      :category Inbox
      :icon arrow-up
      :enablement "{{state.page == 'inbox' && state.inboxAt > 0}}"
      :do [{:run pickNotification :with {:at "{{state.inboxAt - 1}}"}}]}
    :toggleRead {:title "Mark read or unread"
      :category Inbox
      :icon mail-check
      :enablement "{{computed.notification != null}}"
      :do [(node :update inbox #"{{computed.notification.id}}" :fields {:read "{{!computed.notification.read}}"})]}
    :markAllRead {:title "Mark all notifications read"
      :category Inbox
      :icon check-check
      :do [{:update inbox :where {:read false} :fields {:read true}}]}
    :snoozeNotification {:title "Snooze notification"
      :category Inbox
      :icon alarm-clock
      :enablement "{{computed.notification != null}}"
      :do [(node :update inbox #"{{computed.notification.id}}" :fields {:snoozed true})
        {:set state.inboxAt :to "{{max(min(state.inboxAt, length(computed.inboxShown) - 2), 0)}}"}
        {:run notify :with {:message "{{t('inbox.snoozed')}}"}}]}
    :openNotification {:title "Open the notification's issue"
      :category Inbox
      :icon maximize-2
      :enablement "{{computed.notificationCard != null}}"
      :do [{:run selectCard :with {:c "{{computed.notificationCard.c}}" :i "{{computed.notificationCard.at}}"}}
        {:set state.page :to issues}
        {:set state.issueOpen :to true}]}}
  :keys {:"g i" {:when "{{!context.textInputFocus}}" :do goToInbox}
    :"g a" {:when "{{!context.textInputFocus}}" :do goToIssues}
    :"g v" {:when "{{!context.textInputFocus}}" :do goToCycles}
    :"g p" {:when "{{!context.textInputFocus}}" :do goToProjects}
    :u {:when "{{!context.textInputFocus && state.page == 'inbox'}}" :do toggleRead}
    :h {:when "{{!context.textInputFocus && state.page == 'inbox'}}" :do snoozeNotification}
    :enter {:when "{{!context.textInputFocus && state.page == 'inbox'}}" :do openNotification}}
  :components {:pageHeader {:note "A page's top bar: an icon, a title and room for buttons on the end side"
      :params {:icon circle :text ""}
      :node (node #pageHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
        (node #pageTitle
          :type label
          :font small
          :icon "{{icon}}"
          :text " {{text}}"
          :textColor @color.text
          :fitText true
          :anchors {:left 20 :top 0 :bottom 0}
          :align {:horizontal start :vertical center}
          :role heading)
        (node #pageLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border))}
    :progressBar {:note "A thin bar filled to a ratio from 0 to 1"
      :params {:ratio 0 :color "#0d9488" :width 120}
      :node (node #progressTrack
        :width "{{width}}"
        :height 4
        :radius 2
        :color @color.border
        :role progressbar
        :value "{{ratio}}"
        (node #progressFill
          :anchors {:left 0 :top 0 :bottom 0}
          :width "{{round(width * clamp(ratio, 0, 1))}}"
          :radius 2
          :color "{{color}}"))}
    :cycleStat {:note "One figure of the current cycle: a caption over a number"
      :params {:text "" :value "" :color "#111827"}
      :node (node #cycleStat :width 130 :height 44 :color transparent
        (node #cycleStatName
          :style caption
          :text "{{text}}"
          :anchors {:left 0 :right 0 :top 0}
          :height 20
          :align {:horizontal start :vertical center})
        (node #cycleStatValue
          :type label
          :font title
          :text "{{value}}"
          :textColor "{{color}}"
          :anchors {:left 0 :right 0 :top 20}
          :height 28
          :align {:horizontal start :vertical center}))}
    :inboxPage {:note "The notifications on the start side, the one picked on the end side with its issue"
      :params {}
      :node (node #panel-inbox-page :color @color.surface :role region :label "{{t('nav.inbox')}}"
        (node #inboxHeader :use pageHeader :with {:icon inbox :text "{{t('nav.inbox')}}"})
        (node #inboxAllRead
          :style topButton
          :icon check-check
          :text ""
          :width 28
          :height 28
          :padding 0
          :align center
          :anchors {:right 12 :top 10}
          :tooltip "{{t('inbox.allRead')}}"
          :label "{{t('inbox.allRead')}}"
          :onClick markAllRead)
        (node #inboxList :type column :anchors {:left 0 :top 48 :bottom 0} :width 360 :scroll vertical :role list
          (node #inboxItem
            :repeat {:for n :in computed.inboxShown :index k :key "{{n.id}}"}
            :anchors {:left 0 :right 0}
            :height 60
            :color "{{k == state.inboxAt ? '@color.accentSoft' : 'transparent'}}"
            :hover {:color "{{k == state.inboxAt ? '@color.accentSoft' : '@color.column'}}"}
            :cursor hand
            :focusable true
            :role listitem
            :label "{{n.ref}} {{find(computed.allCards, it.ref == n.ref).title ?? ''}}: {{t('inbox.' + n.kind, { who: find(state.members, it.who == n.who).name ?? n.who, to: n.to ?? '' })}}"
            :onClick {:run pickNotification :with {:at "{{k}}"}}
            :onDoubleClick openNotification
            (node #inboxUnread
              :when "{{!n.read}}"
              :width 6
              :height 6
              :radius 3
              :color @color.accent
              :anchors {:left 8 :top 27})
            (node #inboxWho
              :style badge
              :text "{{n.who}}"
              :width 24
              :height 24
              :radius 999
              :anchors {:left 20 :top 10}
              :color "{{find(state.members, it.who == n.who).color ?? '@color.muted'}}"
              :textColor "#ffffff")
            (node #inboxRef
              :type label
              :font small
              :width 44
              :textColor @color.muted
              :text "{{n.ref}}"
              :padding 0
              :anchors {:left 54 :top 8}
              :height 22
              :align {:horizontal start :vertical center})
            (node #inboxTitle
              :type label
              :font small
              :anchors {:left 98 :right 64 :top 8}
              :height 22
              :padding 0
              :textColor "{{n.read ? '@color.muted' : '@color.text'}}"
              :align {:horizontal start :vertical center}
              :text "{{find(computed.allCards, it.ref == n.ref).title ?? ''}}")
            (node #inboxWhat
              :style caption
              :anchors {:left 54 :right 16 :top 30}
              :height 20
              :align {:horizontal start :vertical center}
              :text "{{t('inbox.' + n.kind, { who: find(state.members, it.who == n.who).name ?? n.who, to: n.to ?? '' })}}")
            (node #inboxWhen
              :style caption
              :anchors {:right 16 :top 8}
              :height 22
              :width 48
              :fitText false
              :align {:horizontal end :vertical center}
              :text "{{formatDate(dateMs(n.when), 'MMM d', 'utc')}}")))
        (node #inboxDivider :anchors {:left 360 :top 48 :bottom 0} :width 1 :color @color.border)
        (node #inboxEmpty
          :when "{{computed.notification == null}}"
          :type label
          :icon inbox
          :text " {{t('inbox.empty')}}"
          :textColor @color.muted
          :anchors {:left 361 :right 0 :top 48 :bottom 0}
          :align center)
        (node #inboxDetail
          :when "{{computed.notification != null}}"
          :anchors {:left 361 :right 0 :top 48 :bottom 0}
          :color transparent
          (node #inboxDetailRef
            :style caption
            :text "{{computed.notification.ref}}"
            :anchors {:left 40 :top 32}
            :height 20)
          (node #inboxDetailTitle
            :type label
            :font title
            :text "{{computed.notificationCard.title ?? ''}}"
            :textColor @color.text
            :anchors {:left 40 :right 40 :top 54}
            :height 36
            :align {:horizontal start :vertical center})
          (node #inboxDetailEvent
            :anchors {:left 40 :right 40 :top 144}
            :height 64
            :radius @radius.card
            :color @color.column
            (node #inboxDetailWho
              :style badge
              :text "{{computed.notification.who}}"
              :width 24
              :height 24
              :radius 999
              :anchors {:left 14 :top 10}
              :color "{{find(state.members, it.who == computed.notification.who).color ?? '@color.muted'}}"
              :textColor "#ffffff")
            (node #inboxDetailWhat
              :type label
              :font small
              :textColor @color.text
              :anchors {:left 48 :right 14 :top 10}
              :height 24
              :align {:horizontal start :vertical center}
              :text "{{t('inbox.' + computed.notification.kind, { who: find(state.members, it.who == computed.notification.who).name ?? computed.notification.who, to: computed.notification.to ?? '' })}}")
            (node #inboxDetailText
              :style caption
              :anchors {:left 48 :right 14 :top 34}
              :height 20
              :align {:horizontal start :vertical center}
              :text "{{computed.notification.text ?? ''}}"))
          (node #inboxProps
            :type row
            :gap 6
            :anchors {:left 40 :right 40 :top 102}
            :height 24
            :alignItems center
            :let {:card "{{computed.notificationCard}}" :col "{{computed.columns[computed.notificationCard.c]}}"}
            (node #inboxStatus
              :style cardChip
              :textColor @color.text
              :runs [{:icon "{{state.statusIcons[card.c] ?? 'circle'}}" :color "{{col.color}}"} " {{col.name}}"])
            (node #inboxPriority
              :style cardChip
              :textColor @color.text
              :runs [{:icon "{{state.priorities[card.priority ?? 0]}}"
                  :color "{{card.priority == 1 ? '@bug' : '@color.muted'}}"}
                " {{t('priority.' + (card.priority ?? 0))}}"])
            (node #inboxAssignee
              :style cardChip
              :textColor @color.text
              :icon circle-user
              :text " {{find(state.members, it.who == card.who).name ?? card.who}}")
            (node #inboxTag
              :style cardChip
              :textColor @color.text
              :runs [{:text "●" :font symbols :size 10 :color "@{{card.tag}}"} " {{t('tag.' + card.tag)}}"]))
          (node #inboxActions :type row :gap 8 :anchors {:left 40 :right 40 :top 224} :height 28
            (node #inboxOpen
              :style propValue
              :icon maximize-2
              :text " {{t('inbox.open')}}"
              :onClick openNotification
              :tooltip Enter)
            (node #inboxSnooze
              :style propValue
              :icon alarm-clock
              :text " {{t('inbox.snooze')}}"
              :onClick snoozeNotification
              :tooltip H)
            (node #inboxRead
              :style propValue
              :icon "{{computed.notification.read ? 'mail' : 'mail-check'}}"
              :text " {{t(computed.notification.read ? 'inbox.markUnread' : 'inbox.markRead')}}"
              :onClick toggleRead
              :tooltip U))))}
    :cyclesPage {:note "The current cycle with its burn-up (scope and completed points), then the upcoming and past cycles"
      :params {}
      :node (node #panel-cycles-page :color @color.surface :role region :label "{{t('nav.cycles')}}"
        (node #cyclesHeader :use pageHeader :with {:icon timer :text "{{t('nav.cycles')}}"})
        (node #cycleCurrent
          :anchors {:left 24 :right 24 :top 72}
          :height 236
          :radius @radius.card
          :borderWidth 1
          :borderColor @color.border
          :color transparent
          (node #cycleName
            :type label
            :font small
            :anchors {:left 20 :top 14}
            :height 22
            :fitText true
            :textColor @color.text
            :runs [{:text "{{t('cycle.name', { n: state.cycle.number })}}" :bold true}
              "   "
              {:text "{{t('cycle.current')}}" :color @color.accent}])
          (node #cycleDates
            :style caption
            :anchors {:left 20 :top 36}
            :height 20
            :text "{{formatDate(dateMs(state.cycle.start), 'MMM d', 'utc')}} – {{formatDate(dateMs(state.cycle.end), 'MMM d', 'utc')}}  ·  {{t('cycle.left', { n: daysBetween(state.today, state.cycle.end) })}}")
          (node #cycleScope
            :use cycleStat
            :with {:text "{{t('cycle.scope')}}" :value "{{computed.cycleScope}}" :color @color.text}
            :anchors {:left 20 :top 70})
          (node #cycleStarted
            :use cycleStat
            :with {:text "{{t('cycle.started')}}"
              :value "{{sum(filter(computed.allCards, it.c > 0 && it.c < computed.lastColumn), it.points ?? 0)}}"
              :color "#d97706"}
            :anchors {:left 160 :top 70})
          (node #cycleCompleted
            :use cycleStat
            :with {:text "{{t('cycle.completed')}}"
              :value "{{computed.cycleDone}}  ·  {{formatPercent(computed.cycleDone / max(computed.cycleScope, 1))}}"
              :color @color.accent}
            :anchors {:left 300 :top 70})
          (node #cycleChartBase :anchors {:left 20 :right 20 :bottom 20} :height 1 :color @color.border)
          (node #cycleScopeArea
            :use areaChart
            :with {:values "{{concat(state.cycle.scopeHistory, [computed.cycleScope])}}"
              :top "{{computed.cycleTop}}"
              :color "#0d94881f"}
            :anchors {:left 20 :right 20 :bottom 21}
            :height 90)
          (node #cycleScopeLine
            :use lineChart
            :with {:values "{{concat(state.cycle.scopeHistory, [computed.cycleScope])}}"
              :top "{{computed.cycleTop}}"
              :color "#9ca3af"
              :lineWidth 1.5}
            :anchors {:left 20 :right 20 :bottom 21}
            :height 90
            :role img
            :label "{{t('cycle.scope')}}")
          (node #cycleDoneLine
            :use lineChart
            :with {:values "{{concat(state.cycle.doneHistory, [computed.cycleDone])}}"
              :top "{{computed.cycleTop}}"
              :color @color.accent
              :lineWidth 2}
            :anchors {:left 20 :right 20 :bottom 21}
            :height 90
            :role img
            :label "{{t('cycle.completed')}}"))
        (node #cycleRows :type column :anchors {:left 24 :right 24 :top 320 :bottom 0} :scroll vertical :role list
          (node #cycleIssuesTitle
            :style caption
            :text "{{t('cycle.issues', { n: length(computed.allCards) })}}"
            :height 32
            :align {:horizontal start :vertical center})
          (node #cycleIssue
            :repeat {:for card :in "sort(computed.allCards, it.c * 100 + it.at)"}
            :anchors {:left 0 :right 0}
            :height 36
            :color transparent
            :hover {:color @color.column}
            :cursor hand
            :focusable true
            :role listitem
            :label "{{card.ref}} {{card.title}}"
            :onClick [{:run selectCard :with {:c "{{card.c}}" :i "{{card.at}}"}}
              {:set state.page :to issues}
              {:set state.issueOpen :to true}]
            :keys {:enter [{:run selectCard :with {:c "{{card.c}}" :i "{{card.at}}"}}
                {:set state.page :to issues}
                {:set state.issueOpen :to true}]}
            (node #cycleIssueLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
            (node #cycleIssuePriority
              :type label
              :icon "{{state.priorities[card.priority ?? 0]}}"
              :text ""
              :fitText true
              :textColor "{{card.priority == 1 ? '@bug' : '@color.muted'}}"
              :anchors {:left 12 :top 0 :bottom 0}
              :align center)
            (node #cycleIssueRef
              :style caption
              :text "{{card.ref}}"
              :width 56
              :fitText false
              :anchors {:left 36 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #cycleIssueStatus
              :type label
              :icon "{{state.statusIcons[card.c] ?? 'circle'}}"
              :text ""
              :fitText true
              :textColor "{{computed.columns[card.c].color}}"
              :anchors {:left 100 :top 0 :bottom 0}
              :align center)
            (node #cycleIssueTitle
              :type label
              :font small
              :text "{{card.title}}"
              :textColor @color.text
              :anchors {:left 124 :right 120 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #cycleIssuePoints
              :style caption
              :text "{{card.points ?? ''}}"
              :width 24
              :fitText false
              :anchors {:right 44 :top 0 :bottom 0}
              :align {:horizontal end :vertical center})
            (node #cycleIssueOwner
              :style badge
              :font tiny
              :text "{{card.who}}"
              :width 20
              :height 20
              :radius 999
              :anchors {:right 12 :top 8}
              :color "{{find(state.members, it.who == card.who).color ?? '@color.muted'}}"
              :textColor "#ffffff"))
          (node #cyclesOthers
            :style caption
            :text "{{t('cycle.others')}}"
            :height 44
            :align {:horizontal start :vertical center})
          (node #cycleRow
            :repeat {:for cycle :in state.cycles :key "{{cycle.number}}"}
            :anchors {:left 0 :right 0}
            :height 44
            :color transparent
            :role listitem
            :label "{{t('cycle.name', { n: cycle.number })}}"
            (node #cycleRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
            (node #cycleRowIcon
              :type label
              :icon "{{cycle.done >= cycle.scope || cycle.number < state.cycle.number ? 'check-circle-2' : cycle.done == 0 ? 'circle-dashed' : 'circle-dot'}}"
              :text ""
              :fitText true
              :textColor "{{cycle.done == 0 ? '@color.muted' : '@color.accent'}}"
              :anchors {:left 8 :top 0 :bottom 0}
              :align center)
            (node #cycleRowName
              :type label
              :font small
              :text "{{t('cycle.name', { n: cycle.number })}}"
              :textColor @color.text
              :width 120
              :anchors {:left 36 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #cycleRowDates
              :style caption
              :width 180
              :fitText false
              :anchors {:left 160 :top 0 :bottom 0}
              :align {:horizontal start :vertical center}
              :text "{{formatDate(dateMs(cycle.start), 'MMM d', 'utc')}} – {{formatDate(dateMs(cycle.end), 'MMM d', 'utc')}}")
            (node #cycleRowState
              :style caption
              :width 120
              :fitText false
              :anchors {:left 340 :top 0 :bottom 0}
              :align {:horizontal start :vertical center}
              :text "{{t(cycle.start > state.today ? 'cycle.upcoming' : 'cycle.past')}}")
            (node #cycleRowBar
              :use progressBar
              :with {:ratio "{{cycle.done / max(cycle.scope, 1)}}" :color @color.accent :width 120}
              :anchors {:right 72 :top 20})
            (node #cycleRowPercent
              :style caption
              :width 56
              :fitText false
              :anchors {:right 8 :top 0 :bottom 0}
              :align {:horizontal end :vertical center}
              :text "{{formatPercent(cycle.done / max(cycle.scope, 1))}}"))))}
    :projectsPage {:note "The projects as a table: name, health, lead, target date and progress"
      :params {}
      :node (node #panel-projects-page :color @color.surface :role region :label "{{t('nav.projects')}}"
        (node #projectsHeader :use pageHeader :with {:icon box :text "{{t('nav.projects')}}"})
        (node #projectTabs
          :type row
          :gap 2
          :anchors {:left 120 :top 10}
          :width 360
          :height 28
          :role tablist
          :arrows horizontal
          (node #projectTab
            :repeat {:for scope :in "['all', 'active', 'completed']"}
            :style "{{state.projectScope == scope ? 'toolButtonOn' : 'toolButton'}}"
            :borderWidth "{{state.projectScope == scope ? 1 : 0}}"
            :text "{{t('projects.' + scope)}}"
            :role tab
            :selected "{{state.projectScope == scope}}"
            :onClick {:set state.projectScope :to "{{scope}}"}))
        (node #newProject
          :style topButton
          :icon plus
          :text " {{t('projects.new')}}"
          :fitText true
          :height 28
          :anchors {:right 12 :top 10}
          :label "{{t('projects.new')}}"
          :onClick addProject)
        (node #projectsColumns :anchors {:left 0 :right 0 :top 48} :height 32 :color transparent
          (node #projectsColumnName
            :style caption
            :text "{{t('project.name')}}"
            :anchors {:left 52 :top 0 :bottom 0}
            :align {:horizontal start :vertical center})
          (node #projectsColumnHealth
            :style caption
            :text "{{t('project.health')}}"
            :anchors {:right 440 :top 0 :bottom 0}
            :width 110
            :fitText false
            :align {:horizontal start :vertical center})
          (node #projectsColumnLead
            :style caption
            :text "{{t('project.lead')}}"
            :anchors {:right 360 :top 0 :bottom 0}
            :width 60
            :fitText false
            :align {:horizontal start :vertical center})
          (node #projectsColumnTarget
            :style caption
            :text "{{t('project.target')}}"
            :anchors {:right 240 :top 0 :bottom 0}
            :width 100
            :fitText false
            :align {:horizontal start :vertical center})
          (node #projectsColumnProgress
            :style caption
            :text "{{t('project.progress')}}"
            :anchors {:right 24 :top 0 :bottom 0}
            :width 196
            :fitText false
            :align {:horizontal start :vertical center})
          (node #projectsColumnsLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border))
        (node #projectRows :type column :anchors {:left 0 :right 0 :top 80 :bottom 0} :scroll vertical :role list
          (node #projectRow
            :repeat {:for project
              :in "filter(data.projects, state.projectScope == 'all' || (state.projectScope == 'completed') == (it.health == 'completed'))"
              :key "{{project.name}}"}
            :anchors {:left 0 :right 0}
            :height 44
            :color transparent
            :hover {:color @color.column}
            :cursor hand
            :focusable true
            :role listitem
            :label "{{project.name}}, {{t('health.' + project.health)}}, {{formatPercent(project.progress)}}"
            :tooltip "{{t('projects.open')}}"
            :onClick {:run openProject :with {:at "{{indexOf(map(data.projects, it.name), project.name)}}"}}
            :keys {:enter {:run openProject :with {:at "{{indexOf(map(data.projects, it.name), project.name)}}"}}}
            (node #projectRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
            (node #projectIcon
              :type label
              :icon "{{project.icon}}"
              :text ""
              :width 22
              :height 22
              :radius 5
              :align center
              :color "{{project.color + '24'}}"
              :textColor "{{project.color}}"
              :anchors {:left 20 :top 11})
            (node #projectName
              :type label
              :font small
              :text "{{project.name}}"
              :textColor @color.text
              :anchors {:left 52 :right 560 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            (node #projectHealth
              :type label
              :font small
              :icon circle
              :text " {{t('health.' + project.health)}}"
              :textColor "{{state.healthColors[project.health]}}"
              :anchors {:right 440 :top 0 :bottom 0}
              :width 110
              :align {:horizontal start :vertical center})
            (node #projectLead
              :style badge
              :text "{{project.lead}}"
              :width 22
              :height 22
              :radius 999
              :anchors {:right 398 :top 11}
              :color "{{find(state.members, it.who == project.lead).color ?? '@color.muted'}}"
              :textColor "#ffffff"
              :tooltip "{{find(state.members, it.who == project.lead).name ?? project.lead}}")
            (node #projectTarget
              :style caption
              :width 100
              :fitText false
              :anchors {:right 240 :top 0 :bottom 0}
              :align {:horizontal start :vertical center}
              :icon calendar
              :text " {{formatDate(dateMs(project.target), 'MMM d', 'utc')}}"
              :textColor "{{daysBetween(state.today, project.target) < 0 && project.progress < 1 ? '@bug' : '@color.muted'}}")
            (node #projectBar
              :use progressBar
              :with {:ratio "{{project.progress}}" :color "{{project.color}}" :width 120}
              :anchors {:right 100 :top 20})
            (node #projectPercent
              :style caption
              :width 56
              :fitText false
              :anchors {:right 24 :top 0 :bottom 0}
              :align {:horizontal end :vertical center}
              :text "{{formatPercent(project.progress)}}  ·  {{project.issues}}"))))}}}
