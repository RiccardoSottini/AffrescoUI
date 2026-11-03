;;; pages.lisp --- Flow's team pages, included by index.screen.lisp: the inbox of notifications, the cycles with their burn-up, and the projects
;;;
;;; The sidebar and the G chords switch between them and the issues.

;;; Definitions

(defstate page issues)

(defstate inboxAt 0)

(defstate cycle
  {:number 12 :start "2026-03-09" :end "2026-03-22" :scopeHistory [22 24 24 27 30 30 31] :doneHistory [0 2 5 5 6 8 8]})

(defstate cycles
  [{:number 13 :start "2026-03-23" :end "2026-04-05" :scope 9 :done 0}
   {:number 11 :start "2026-02-23" :end "2026-03-08" :scope 26 :done 24}
   {:number 10 :start "2026-02-09" :end "2026-02-22" :scope 21 :done 21}])

(defstate projectScope all)

(defstate projectOf {:feature 0 :bug 0 :design 1 :chore 2})

(defstate healthColors {:onTrack "#16a34a" :atRisk "#d97706" :offTrack "#dc2626" :completed "#0d9488"})

(defcomputed inboxShown (filter (fn [it] (not it.snoozed)) data.inbox))

(defcomputed unread (count (filter (fn [it] (and (not it.read) (not it.snoozed))) data.inbox)))

(defcomputed notification (get computed.inboxShown state.inboxAt))

(defcomputed notificationCard
  (if (nil? computed.notification) nil (find (fn [it] (= it.ref computed.notification.ref)) computed.allCards)))

(defcomputed cycleScope (sum computed.allCards (fn [it] (or-else it.points 0))))

(defcomputed cycleDone
  (sum (filter (fn [it] (= it.c computed.lastColumn)) computed.allCards) (fn [it] (or-else it.points 0))))

(defcomputed cycleTop (+ (max (concat state.cycle.scopeHistory [computed.cycleScope])) 4))

(defcomponent pageHeader "A page's top bar: an icon, a title and room for buttons on the end side"
  {:icon circle :text ""}
  `(node#pageHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
     (label#pageTitle :font small
       :icon ~icon
       :text ~(str " " text)
       :textColor @color.text
       :fitText true
       :anchors {:left 20 :top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :role heading)
     {:id pageLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}))

(defcomponent progressBar "A thin bar filled to a ratio from 0 to 1"
  {:ratio 0 :color "#0d9488" :width 120}
  `(node#progressTrack :width ~width :height 4 :radius 2 :color @color.border :role progressbar :value ~ratio
     {:id progressFill
      :anchors {:left 0 :top 0 :bottom 0}
      :width ~(round (* width (clamp ratio 0 1)))
      :radius 2
      :color ~color}))

(defcomponent cycleStat "One figure of the current cycle: a caption over a number"
  {:text "" :value "" :color "#111827"}
  `(node#cycleStat :width 130 :height 44 :color transparent
     {:id cycleStatName
      :style caption
      :text ~text
      :anchors {:left 0 :right 0 :top 0}
      :height 20
      :align {:horizontal start :vertical center}}
     (label#cycleStatValue :font title
       :text ~value
       :textColor ~color
       :anchors {:left 0 :right 0 :top 20}
       :height 28
       :align {:horizontal start :vertical center})))

(defcomponent inboxPage "The notifications on the start side, the one picked on the end side with its issue"
  {}
  `(node#panel-inbox-page :color @color.surface :role region :label ~(t "nav.inbox")
     (node#inboxHeader :use pageHeader :with {:icon inbox :text ~(t "nav.inbox")})
     {:id inboxAllRead
      :style topButton
      :icon check-check
      :text ""
      :width 28
      :height 28
      :padding 0
      :align center
      :anchors {:right 12 :top 10}
      :tooltip ~(t "inbox.allRead")
      :label ~(t "inbox.allRead")
      :onClick markAllRead}
     (column#inboxList :anchors {:left 0 :top 48 :bottom 0} :width 360 :scroll vertical :role list
       ~(repeat [n computed.inboxShown :index k :key n.id]
         (inbox-item n k)))
     {:id inboxDivider :anchors {:left 360 :top 48 :bottom 0} :width 1 :color @color.border}
     (label#inboxEmpty :when ~(nil? computed.notification)
       :icon inbox
       :text ~(str " " (t "inbox.empty"))
       :textColor @color.muted
       :anchors {:left 361 :right 0 :top 48 :bottom 0}
       :align center)
     (node#inboxDetail :when ~(some? computed.notification)
       :anchors {:left 361 :right 0 :top 48 :bottom 0}
       :color transparent
       {:id inboxDetailRef :style caption :text ~computed.notification.ref :anchors {:left 40 :top 32} :height 20}
       (label#inboxDetailTitle :font title
         :text ~(or-else computed.notificationCard.title "")
         :textColor @color.text
         :anchors {:left 40 :right 40 :top 54}
         :height 36
         :align {:horizontal start :vertical center})
       ~(inbox-detail-event)
       ~(inbox-props)
       ~(inbox-actions))))

(defcomponent cyclesPage "The current cycle with its burn-up (scope and completed points), then the upcoming and past cycles"
  {}
  `(node#panel-cycles-page :color @color.surface :role region :label ~(t "nav.cycles")
     (node#cyclesHeader :use pageHeader :with {:icon timer :text ~(t "nav.cycles")})
     ~(cycle-current)
     (column#cycleRows :anchors {:left 24 :right 24 :top 320 :bottom 0} :scroll vertical :role list
       {:id cycleIssuesTitle
        :style caption
        :text ~(t "cycle.issues" {:n (count computed.allCards)})
        :height 32
        :align {:horizontal start :vertical center}}
       ~(repeat [card (sort-by (fn [it] (+ (* it.c 100) it.at)) computed.allCards)]
         (cycle-issue card))
       {:id cyclesOthers :style caption :text ~(t "cycle.others") :height 44 :align {:horizontal start :vertical center}}
       ~(repeat [cycle state.cycles :key cycle.number]
         (cycle-row cycle)))))

(defcomponent projectsPage "The projects as a table: name, health, lead, target date and progress"
  {}
  `(node#panel-projects-page :color @color.surface :role region :label ~(t "nav.projects")
     (node#projectsHeader :use pageHeader :with {:icon box :text ~(t "nav.projects")})
     (row#projectTabs :gap 2 :anchors {:left 120 :top 10} :width 360 :height 28 :role tablist :arrows horizontal
       ~(repeat [scope ["all" "active" "completed"]]
         `{:id projectTab
           :style ~(if (= state.projectScope scope) "toolButtonOn" "toolButton")
           :borderWidth ~(if (= state.projectScope scope) 1 0)
           :text ~(t (str "projects." scope))
           :role tab
           :selected ~(= state.projectScope scope)
           :onClick {:set state.projectScope :to ~scope}}))
     {:id newProject
      :style topButton
      :icon plus
      :text ~(str " " (t "projects.new"))
      :fitText true
      :height 28
      :anchors {:right 12 :top 10}
      :label ~(t "projects.new")
      :onClick addProject}
     ~(projects-columns)
     (column#projectRows :anchors {:left 0 :right 0 :top 80 :bottom 0} :scroll vertical :role list
       ~(repeat [project (filter (fn [it] (or (= state.projectScope "all") (= (= state.projectScope "completed") (= it.health "completed")))) data.projects) :key project.name]
         (project-row project)))))

(defkey "g i" {:when ~(not context.textInputFocus) :do goToInbox})

(defkey "g a" {:when ~(not context.textInputFocus) :do goToIssues})

(defkey "g v" {:when ~(not context.textInputFocus) :do goToCycles})

(defkey "g p" {:when ~(not context.textInputFocus) :do goToProjects})

(defkey "u" {:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do toggleRead})

(defkey "h" {:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do snoozeNotification})

(defkey "enter" {:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do openNotification})

;;; Views

(defview inbox-item "#inboxItem." [n k]
  `(node#inboxItem :anchors {:left 0 :right 0}
     :height 60
     :color ~(if (= k state.inboxAt) "@color.accentSoft" "transparent")
     :hover {:color ~(if (= k state.inboxAt) "@color.accentSoft" "@color.column")}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str n.ref
                  " "
                  (or-else (:title (find (fn [it] (= it.ref n.ref)) computed.allCards)) "")
                  ": "
                  (t (str "inbox." n.kind)
                     {:who (or-else (:name (find (fn [it] (= it.who n.who)) state.members)) n.who)
                      :to (or-else n.to "")}))
     :onClick {:run pickNotification :with {:at ~k}}
     :onDoubleClick openNotification
     {:id inboxUnread :when ~(not n.read) :width 6 :height 6 :radius 3 :color @color.accent :anchors {:left 8 :top 27}}
     {:id inboxWho
      :style badge
      :text ~n.who
      :width 24
      :height 24
      :radius 999
      :anchors {:left 20 :top 10}
      :color ~(or-else (:color (find (fn [it] (= it.who n.who)) state.members)) "@color.muted")
      :textColor "#ffffff"}
     (label#inboxRef :font small
       :width 44
       :textColor @color.muted
       :text ~n.ref
       :padding 0
       :anchors {:left 54 :top 8}
       :height 22
       :align {:horizontal start :vertical center})
     (label#inboxTitle :font small
       :anchors {:left 98 :right 64 :top 8}
       :height 22
       :padding 0
       :textColor ~(if n.read "@color.muted" "@color.text")
       :align {:horizontal start :vertical center}
       :text ~(or-else (:title (find (fn [it] (= it.ref n.ref)) computed.allCards)) ""))
     {:id inboxWhat
      :style caption
      :anchors {:left 54 :right 16 :top 30}
      :height 20
      :align {:horizontal start :vertical center}
      :text ~(t (str "inbox." n.kind)
                {:who (or-else (:name (find (fn [it] (= it.who n.who)) state.members)) n.who) :to (or-else n.to "")})}
     {:id inboxWhen
      :style caption
      :anchors {:right 16 :top 8}
      :height 22
      :width 48
      :fitText false
      :align {:horizontal end :vertical center}
      :text ~(formatDate (dateMs n.when) "MMM d" "utc")}))

(defview inbox-detail-event "#inboxDetailEvent." []
  `(node#inboxDetailEvent :anchors {:left 40 :right 40 :top 144} :height 64 :radius @radius.card :color @color.column
     {:id inboxDetailWho
      :style badge
      :text ~computed.notification.who
      :width 24
      :height 24
      :radius 999
      :anchors {:left 14 :top 10}
      :color ~(or-else (:color (find (fn [it] (= it.who computed.notification.who)) state.members)) "@color.muted")
      :textColor "#ffffff"}
     (label#inboxDetailWhat :font small
       :textColor @color.text
       :anchors {:left 48 :right 14 :top 10}
       :height 24
       :align {:horizontal start :vertical center}
       :text ~(t (str "inbox." computed.notification.kind)
                 {:who (or-else (:name (find (fn [it] (= it.who computed.notification.who)) state.members))
                                computed.notification.who)
                  :to (or-else computed.notification.to "")}))
     {:id inboxDetailText
      :style caption
      :anchors {:left 48 :right 14 :top 34}
      :height 20
      :align {:horizontal start :vertical center}
      :text ~(or-else computed.notification.text "")}))

(defview inbox-props "#inboxProps." []
  `(row#inboxProps :gap 6
     :anchors {:left 40 :right 40 :top 102}
     :height 24
     :alignItems center
     :let {:card ~computed.notificationCard :col ~(get computed.columns computed.notificationCard.c)}
     {:id inboxStatus
      :style cardChip
      :textColor @color.text
      :runs [{:icon ~(or-else (get state.statusIcons card.c) "circle") :color ~col.color} ~(str " " col.name)]}
     {:id inboxPriority
      :style cardChip
      :textColor @color.text
      :runs [{:icon ~(get state.priorities (or-else card.priority 0)) :color ~(if (= card.priority 1)
                                                                                  "@bug"
                                                                                  "@color.muted")}
             ~(str " " (t (str "priority." (or-else card.priority 0))))]}
     {:id inboxAssignee
      :style cardChip
      :textColor @color.text
      :icon circle-user
      :text ~(str " " (or-else (:name (find (fn [it] (= it.who card.who)) state.members)) card.who))}
     {:id inboxTag
      :style cardChip
      :textColor @color.text
      :runs [{:text "●" :font symbols :size 10 :color ~(str "@" card.tag)} ~(str " " (t (str "tag." card.tag)))]}))

(defview inbox-actions "#inboxActions." []
  `(row#inboxActions :gap 8 :anchors {:left 40 :right 40 :top 224} :height 28
     {:id inboxOpen
      :style propValue
      :icon maximize-2
      :text ~(str " " (t "inbox.open"))
      :onClick openNotification
      :tooltip "Enter"}
     {:id inboxSnooze
      :style propValue
      :icon alarm-clock
      :text ~(str " " (t "inbox.snooze"))
      :onClick snoozeNotification
      :tooltip "H"}
     {:id inboxRead
      :style propValue
      :icon ~(if computed.notification.read "mail" "mail-check")
      :text ~(str " " (t (if computed.notification.read "inbox.markUnread" "inbox.markRead")))
      :onClick toggleRead
      :tooltip "U"}))

(defview cycle-current "#cycleCurrent." []
  `(node#cycleCurrent :anchors {:left 24 :right 24 :top 72}
     :height 236
     :radius @radius.card
     :borderWidth 1
     :borderColor @color.border
     :color transparent
     (label#cycleName :font small
       :anchors {:left 20 :top 14}
       :height 22
       :fitText true
       :textColor @color.text
       :runs [{:text ~(t "cycle.name" {:n state.cycle.number}) :bold true}
              "   "
              {:text ~(t "cycle.current") :color @color.accent}])
     {:id cycleDates
      :style caption
      :anchors {:left 20 :top 36}
      :height 20
      :text ~(str (formatDate (dateMs state.cycle.start) "MMM d" "utc")
                  " – "
                  (formatDate (dateMs state.cycle.end) "MMM d" "utc")
                  "  ·  "
                  (t "cycle.left" {:n (daysBetween state.today state.cycle.end)}))}
     (node#cycleScope :use cycleStat
       :with {:text ~(t "cycle.scope") :value ~computed.cycleScope :color @color.text}
       :anchors {:left 20 :top 70})
     (node#cycleStarted :use cycleStat
       :with {:text ~(t "cycle.started")
              :value ~(sum (filter (fn [it] (and (> it.c 0) (< it.c computed.lastColumn))) computed.allCards)
                           (fn [it] (or-else it.points 0)))
              :color "#d97706"}
       :anchors {:left 160 :top 70})
     (node#cycleCompleted :use cycleStat
       :with {:text ~(t "cycle.completed")
              :value ~(str computed.cycleDone "  ·  " (formatPercent (/ computed.cycleDone (max computed.cycleScope 1))))
              :color @color.accent}
       :anchors {:left 300 :top 70})
     {:id cycleChartBase :anchors {:left 20 :right 20 :bottom 20} :height 1 :color @color.border}
     (node#cycleScopeArea :use areaChart
       :with {:values ~(concat state.cycle.scopeHistory [computed.cycleScope]) :top ~computed.cycleTop :color "#0d94881f"}
       :anchors {:left 20 :right 20 :bottom 21}
       :height 90)
     (node#cycleScopeLine :use lineChart
       :with {:values ~(concat state.cycle.scopeHistory [computed.cycleScope])
              :top ~computed.cycleTop
              :color "#9ca3af"
              :lineWidth 1.5}
       :anchors {:left 20 :right 20 :bottom 21}
       :height 90
       :role img
       :label ~(t "cycle.scope"))
     (node#cycleDoneLine :use lineChart
       :with {:values ~(concat state.cycle.doneHistory [computed.cycleDone])
              :top ~computed.cycleTop
              :color @color.accent
              :lineWidth 2}
       :anchors {:left 20 :right 20 :bottom 21}
       :height 90
       :role img
       :label ~(t "cycle.completed"))))

(defview cycle-issue "#cycleIssue." [card]
  `(node#cycleIssue :anchors {:left 0 :right 0}
     :height 36
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str card.ref " " card.title)
     :onClick [{:run selectCard :with {:c ~card.c :i ~card.at}}
               {:set state.page :to issues}
               {:set state.issueOpen :to true}]
     :keys {:enter [{:run selectCard :with {:c ~card.c :i ~card.at}}
                    {:set state.page :to issues}
                    {:set state.issueOpen :to true}]}
     {:id cycleIssueLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}
     (label#cycleIssuePriority :icon ~(get state.priorities (or-else card.priority 0))
       :text ""
       :fitText true
       :textColor ~(if (= card.priority 1) "@bug" "@color.muted")
       :anchors {:left 12 :top 0 :bottom 0}
       :align center)
     {:id cycleIssueRef
      :style caption
      :text ~card.ref
      :width 56
      :fitText false
      :anchors {:left 36 :top 0 :bottom 0}
      :align {:horizontal start :vertical center}}
     (label#cycleIssueStatus :icon ~(or-else (get state.statusIcons card.c) "circle")
       :text ""
       :fitText true
       :textColor ~(get-in computed.columns [card.c :color])
       :anchors {:left 100 :top 0 :bottom 0}
       :align center)
     (label#cycleIssueTitle :font small
       :text ~card.title
       :textColor @color.text
       :anchors {:left 124 :right 120 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:id cycleIssuePoints
      :style caption
      :text ~(or-else card.points "")
      :width 24
      :fitText false
      :anchors {:right 44 :top 0 :bottom 0}
      :align {:horizontal end :vertical center}}
     {:id cycleIssueOwner
      :style badge
      :font tiny
      :text ~card.who
      :width 20
      :height 20
      :radius 999
      :anchors {:right 12 :top 8}
      :color ~(or-else (:color (find (fn [it] (= it.who card.who)) state.members)) "@color.muted")
      :textColor "#ffffff"}))

(defview cycle-row "#cycleRow." [cycle]
  `(node#cycleRow :anchors {:left 0 :right 0}
     :height 44
     :color transparent
     :role listitem
     :label ~(t "cycle.name" {:n cycle.number})
     {:id cycleRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}
     (label#cycleRowIcon :icon ~(cond (or (>= cycle.done cycle.scope) (< cycle.number state.cycle.number)) "check-circle-2"
                                      (= cycle.done 0) "circle-dashed"
                                      :else "circle-dot")
       :text ""
       :fitText true
       :textColor ~(if (= cycle.done 0) "@color.muted" "@color.accent")
       :anchors {:left 8 :top 0 :bottom 0}
       :align center)
     (label#cycleRowName :font small
       :text ~(t "cycle.name" {:n cycle.number})
       :textColor @color.text
       :width 120
       :anchors {:left 36 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:id cycleRowDates
      :style caption
      :width 180
      :fitText false
      :anchors {:left 160 :top 0 :bottom 0}
      :align {:horizontal start :vertical center}
      :text ~(str (formatDate (dateMs cycle.start) "MMM d" "utc") " – " (formatDate (dateMs cycle.end) "MMM d" "utc"))}
     {:id cycleRowState
      :style caption
      :width 120
      :fitText false
      :anchors {:left 340 :top 0 :bottom 0}
      :align {:horizontal start :vertical center}
      :text ~(t (if (> cycle.start state.today) "cycle.upcoming" "cycle.past"))}
     (node#cycleRowBar :use progressBar
       :with {:ratio ~(/ cycle.done (max cycle.scope 1)) :color @color.accent :width 120}
       :anchors {:right 72 :top 20})
     {:id cycleRowPercent
      :style caption
      :width 56
      :fitText false
      :anchors {:right 8 :top 0 :bottom 0}
      :align {:horizontal end :vertical center}
      :text ~(formatPercent (/ cycle.done (max cycle.scope 1)))}))

(defview projects-columns "#projectsColumns." []
  `(node#projectsColumns :anchors {:left 0 :right 0 :top 48} :height 32 :color transparent
     {:id projectsColumnName
      :style caption
      :text ~(t "project.name")
      :anchors {:left 52 :top 0 :bottom 0}
      :align {:horizontal start :vertical center}}
     {:id projectsColumnHealth
      :style caption
      :text ~(t "project.health")
      :anchors {:right 440 :top 0 :bottom 0}
      :width 110
      :fitText false
      :align {:horizontal start :vertical center}}
     {:id projectsColumnLead
      :style caption
      :text ~(t "project.lead")
      :anchors {:right 360 :top 0 :bottom 0}
      :width 60
      :fitText false
      :align {:horizontal start :vertical center}}
     {:id projectsColumnTarget
      :style caption
      :text ~(t "project.target")
      :anchors {:right 240 :top 0 :bottom 0}
      :width 100
      :fitText false
      :align {:horizontal start :vertical center}}
     {:id projectsColumnProgress
      :style caption
      :text ~(t "project.progress")
      :anchors {:right 24 :top 0 :bottom 0}
      :width 196
      :fitText false
      :align {:horizontal start :vertical center}}
     {:id projectsColumnsLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}))

(defview project-row "#projectRow." [project]
  `(node#projectRow :anchors {:left 0 :right 0}
     :height 44
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str project.name ", " (t (str "health." project.health)) ", " (formatPercent project.progress))
     :tooltip ~(t "projects.open")
     :onClick {:run openProject :with {:at ~(indexOf (map (fn [it] it.name) data.projects) project.name)}}
     :keys {:enter {:run openProject :with {:at ~(indexOf (map (fn [it] it.name) data.projects) project.name)}}}
     {:id projectRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}
     (label#projectIcon :icon ~project.icon
       :text ""
       :width 22
       :height 22
       :radius 5
       :align center
       :color ~(str project.color "24")
       :textColor ~project.color
       :anchors {:left 20 :top 11})
     (label#projectName :font small
       :text ~project.name
       :textColor @color.text
       :anchors {:left 52 :right 560 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label#projectHealth :font small
       :icon circle
       :text ~(str " " (t (str "health." project.health)))
       :textColor ~(get state.healthColors project.health)
       :anchors {:right 440 :top 0 :bottom 0}
       :width 110
       :align {:horizontal start :vertical center})
     {:id projectLead
      :style badge
      :text ~project.lead
      :width 22
      :height 22
      :radius 999
      :anchors {:right 398 :top 11}
      :color ~(or-else (:color (find (fn [it] (= it.who project.lead)) state.members)) "@color.muted")
      :textColor "#ffffff"
      :tooltip ~(or-else (:name (find (fn [it] (= it.who project.lead)) state.members)) project.lead)}
     {:id projectTarget
      :style caption
      :width 100
      :fitText false
      :anchors {:right 240 :top 0 :bottom 0}
      :align {:horizontal start :vertical center}
      :icon calendar
      :text ~(str " " (formatDate (dateMs project.target) "MMM d" "utc"))
      :textColor ~(if (and (< (daysBetween state.today project.target) 0) (< project.progress 1)) "@bug" "@color.muted")}
     (node#projectBar :use progressBar
       :with {:ratio ~project.progress :color ~project.color :width 120}
       :anchors {:right 100 :top 20})
     {:id projectPercent
      :style caption
      :width 56
      :fitText false
      :anchors {:right 24 :top 0 :bottom 0}
      :align {:horizontal end :vertical center}
      :text ~(str (formatPercent project.progress) "  ·  " project.issues)}))
