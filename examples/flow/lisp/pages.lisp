;;; pages.lisp --- Flow's team pages, included by index.screen.lisp: the inbox of notifications, the cycles with their
;;; burn-up, and the projects
;;;
;;; The sidebar and the G chords switch between them and the issues.

(ns flow.pages)

;;; State

(defstate page issues)

(defstate inboxAt 0)

(defstate cycle
  {:number 12 :start "2026-03-09" :end "2026-03-22" :scopeHistory [22 24 24 27 30 30 31] :doneHistory [0 2 5 5 6 8 8]})

(defstate cycles
  [{:number 13 :start "2026-03-23" :end "2026-04-05" :scope 9 :done 0}
   {:number 11 :start "2026-02-23" :end "2026-03-08" :scope 26 :done 24}
   {:number 10 :start "2026-02-09" :end "2026-02-22" :scope 21 :done 21}])

(defstate projectScope all)

(defstate projectOpen -1)

(defstate projectLayout list)

(defstate projectDraft {:text "" :health onTrack})

(defstate milestoneDraft "")

(defstate projectName "")

(defstate projectSummary "")

(defstate roadmapStart "2026-02-01")

(defstate projectOf {:feature 0 :bug 0 :design 1 :chore 2})

(defstate healthColors {:onTrack "#16a34a" :atRisk "#d97706" :offTrack "#dc2626" :completed "#0d9488"})

;;; What it works out

(defcomputed inboxShown (filter #(not (:snoozed %)) data.inbox))

(defcomputed unread (count (filter #(and (not (:read %)) (not (:snoozed %))) data.inbox)))

(defcomputed notification (get computed.inboxShown state.inboxAt))

(defcomputed notificationCard
  (when-some [n computed.notification] (find #(= (:ref %) n.ref) computed.allCards)))

; style: keep, sum is the expression's own function: the function it takes names its item it
(defcomputed cycleScope (sum computed.allCards (fn [it] (or-else it.points 0))))

(defcomputed cycleDone
  ; style: keep, sum is the expression's own function: the function it takes names its item it
  (sum (filter #(= (:c %) computed.lastColumn) computed.allCards) (fn [it] (or-else it.points 0))))

(defcomputed cycleTop (+ (max (concat state.cycle.scopeHistory [computed.cycleScope])) 4))

(defcomputed project (when (>= state.projectOpen 0) (get data.projects state.projectOpen)))

(defcomputed projectsShown
  (filter (fn [it]
            (or (= state.projectScope :all) (= (= state.projectScope :completed) (= it.health "completed"))))
          data.projects))

(defcomputed roadmapTracks
  (map-indexed (fn [i p]
                 {:id (str i)
                  :label p.name
                  :clips [{:id (str i)
                           :label p.name
                           :start (max 0 (daysBetween state.roadmapStart (or-else p.start p.target)))
                           :duration (max 1 (daysBetween (or-else p.start p.target) p.target))
                           :color (str p.color "38")}]})
               data.projects))

(defcomputed milestones (or-else computed.project.milestones []))

(defcomputed milestonesDone (count (filter :done computed.milestones)))

(defcomputed milestoneCount (str computed.milestonesDone " / " (count computed.milestones)))

(defcomputed projectTags (filter #(= (get state.projectOf %) state.projectOpen) (keys state.projectOf)))

(defcomputed projectIssues (filter #(contains computed.projectTags (:tag %)) computed.allCards))

(defcomputed projectIssueCount (count computed.projectIssues))

(defcomputed updateCount (count (or-else computed.project.updates [])))

;;; Components

(defcomponent pageHeader "A page's top bar: an icon, a title and room for buttons on the end side"
  {:icon circle :text ""}
  `(node#pageHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
     (label#pageTitle :font small
       :icon ~icon
       :text ~text
       :textColor @color.text
       :fitText true
       :anchors {:left 20 :top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :role heading)
     (node#pageLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)))

(defcomponent progressBar "A thin bar filled to a ratio from 0 to 1"
  {:ratio 0 :color "#0d9488" :width 120}
  `(node#progressTrack :width ~width :height 4 :radius 2 :color @color.border :role progressbar :value ~ratio
     (node#progressFill :anchors {:left 0 :top 0 :bottom 0}
       :width ~(round (* width (clamp ratio 0 1)))
       :radius 2
       :color ~color)))

(defcomponent cycleStat "One figure of the current cycle: a caption over a number"
  {:text "" :value "" :color "#111827"}
  `(node#cycleStat :width 130 :height 44 :color transparent
     (node#cycleStatName :style caption
       :text ~text
       :anchors {:left 0 :right 0 :top 0}
       :height 20
       :align {:horizontal start :vertical center})
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
     (node#inboxAllRead :style topButton
       :icon check-check
       :text ""
       :width 28
       :height 28
       :padding 0
       :align center
       :anchors {:right 12 :top 10}
       :tooltip ~(t "inbox.allRead")
       :label ~(t "inbox.allRead")
       :onClick markAllRead)
     (column#inboxList :anchors {:left 0 :top 48 :bottom 0} :width 360 :scroll vertical :role list
       ~(repeat [n computed.inboxShown :index k :key n.id]
         (inbox-item n k)))
     (node#inboxDivider :anchors {:left 360 :top 48 :bottom 0} :width 1 :color @color.border)
     (label#inboxEmpty :when ~(nil? computed.notification)
       :icon inbox
       :text ~(t "inbox.empty")
       :textColor @color.muted
       :anchors {:left 361 :right 0 :top 48 :bottom 0}
       :align center)
     (node#inboxDetail :when ~(some? computed.notification)
       :anchors {:left 361 :right 0 :top 48 :bottom 0}
       :color transparent
       (node#inboxDetailRef :style caption :text ~computed.notification.ref :anchors {:left 40 :top 32} :height 20)
       (label#inboxDetailTitle :font title
         :text ~(or-else computed.notificationCard.title "")
         :textColor @color.text
         :anchors {:left 40 :right 40 :top 54}
         :height 36
         :align {:horizontal start :vertical center})
       ~(inbox-detail-event)
       ~(inbox-props)
       ~(inbox-actions))))

(defcomponent cyclesPage
  "The current cycle with its burn-up (scope and completed points), then the upcoming and past cycles"
  {}
  `(node#panel-cycles-page :color @color.surface :role region :label ~(t "nav.cycles")
     (node#cyclesHeader :use pageHeader :with {:icon timer :text ~(t "nav.cycles")})
     ~(cycle-current)
     (column#cycleRows :anchors {:left 24 :right 24 :top 320 :bottom 0} :scroll vertical :role list
       (node#cycleIssuesTitle :style caption
         :text ~(t "cycle.issues" {:n (count computed.allCards)})
         :height 32
         :align {:horizontal start :vertical center})
       ~(repeat [card (sort-by #(+ (* (:c %) 100) (:at %)) computed.allCards)]
         (cycle-issue card))
       (node#cyclesOthers :style caption
         :text ~(t "cycle.others")
         :height 44
         :align {:horizontal start :vertical center})
       ~(repeat [cycle state.cycles :key cycle.number]
         (cycle-row cycle)))))

(defcomponent projectsPage
  "The projects as a table or on a roadmap, and a project's own page with its milestones and updates"
  {}
  `(node#panel-projects-page :color @color.surface :role region :label ~(t "nav.projects")
     ~(projects-list)
     ~(project-page)))

;;; Keys

(defkey "g i" {:when ~(not context.textInputFocus) :do goToInbox})

(defkey "g a" {:when ~(not context.textInputFocus) :do goToIssues})

(defkey "g v" {:when ~(not context.textInputFocus) :do goToCycles})

(defkey "g p" {:when ~(not context.textInputFocus) :do goToProjects})

(defkey "u" {:when ~(and (not context.textInputFocus) (= state.page :inbox)) :do toggleRead})

(defkey "h" {:when ~(and (not context.textInputFocus) (= state.page :inbox)) :do snoozeNotification})

(defkey "enter" {:when ~(and (not context.textInputFocus) (= state.page :inbox)) :do openNotification})

(defkey "g r" {:when ~(not context.textInputFocus) :do showRoadmap})

;;; Views

(defview inbox-item "A notification in the inbox's list." [n k]
  `(node#inboxItem :anchors {:left 0 :right 0}
     :height 60
     :color ~(if (= k state.inboxAt) "@color.accentSoft" "transparent")
     :hover {:color ~(if (= k state.inboxAt) "@color.accentSoft" "@color.column")}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str n.ref
                  " "
                  (or-else (:title (find #(= (:ref %) n.ref) computed.allCards)) "")
                  ": "
                  (t (str "inbox." n.kind)
                     {:who (or-else (:name (find #(= (:who %) n.who) state.members)) n.who)
                      :to (or-else n.to "")}))
     :onClick ~(handler pick-notification k)
     :onDoubleClick openNotification
     (node#inboxUnread :when ~(not n.read)
       :width 6
       :height 6
       :radius 3
       :color @color.accent
       :anchors {:left 8 :top 27})
     ~(inbox-who n)
     (label#inboxRef :font small
       :width 44
       :textColor @color.muted
       :text ~n.ref
       :padding 0
       :anchors {:left 54 :top 8}
       :height 22
       :align {:horizontal start :vertical center})
     ~(inbox-title n)
     ~(inbox-what n)
     ~(inbox-when n)))

(defview inbox-when "When a notification came." [n]
  `(node#inboxWhen :style caption
     :anchors {:right 16 :top 8}
     :height 22
     :width 48
     :fitText false
     :align {:horizontal end :vertical center}
     :text ~(formatDate (dateMs n.when) "MMM d" "utc")))

(defview inbox-who "Who a notification is from." [n]
  `(node#inboxWho :style badge
     :text ~n.who
     :width 24
     :height 24
     :radius 999
     :anchors {:left 20 :top 10}
     :color ~(or-else (:color (find #(= (:who %) n.who) state.members)) "@color.muted")
     :textColor "#ffffff"))

(defview inbox-what "What a notification says happened." [n]
  `(node#inboxWhat :style caption
     :anchors {:left 54 :right 16 :top 30}
     :height 20
     :align {:horizontal start :vertical center}
     :text ~(t (str "inbox." n.kind)
               {:who (or-else (:name (find #(= (:who %) n.who) state.members)) n.who) :to (or-else n.to "")})))

(defview inbox-title "The title of a notification's issue." [n]
  `(label#inboxTitle :font small
     :anchors {:left 98 :right 64 :top 8}
     :height 22
     :padding 0
     :textColor ~(if n.read "@color.muted" "@color.text")
     :align {:horizontal start :vertical center}
     :text ~(or-else (:title (find #(= (:ref %) n.ref) computed.allCards)) "")))

(defview inbox-detail-event "What happened, in the inbox's detail: who did what." []
  `(node#inboxDetailEvent :anchors {:left 40 :right 40 :top 144} :height 64 :radius @radius.card :color @color.column
     (node#inboxDetailWho :style badge
       :text ~computed.notification.who
       :width 24
       :height 24
       :radius 999
       :anchors {:left 14 :top 10}
       :color ~(or-else (:color (find #(= (:who %) computed.notification.who) state.members)) "@color.muted")
       :textColor "#ffffff")
     (label#inboxDetailWhat :font small
       :textColor @color.text
       :anchors {:left 48 :right 14 :top 10}
       :height 24
       :align {:horizontal start :vertical center}
       :text ~(t (str "inbox." computed.notification.kind)
                 {:who (or-else (:name (find #(= (:who %) computed.notification.who) state.members))
                                computed.notification.who)
                  :to (or-else computed.notification.to "")}))
     (node#inboxDetailText :style caption
       :anchors {:left 48 :right 14 :top 34}
       :height 20
       :align {:horizontal start :vertical center}
       :text ~(or-else computed.notification.text ""))))

(defview inbox-props "The issue's properties, in the inbox's detail." []
  `(row#inboxProps :gap 6
     :anchors {:left 40 :right 40 :top 102}
     :height 24
     :alignItems center
     :let {:card ~computed.notificationCard :col ~(get computed.columns computed.notificationCard.c)}
     (node#inboxStatus :style cardChip
       :textColor @color.text
       :runs [{:icon ~(or-else (get state.statusIcons card.c) "circle") :color ~col.color} ~(str " " col.name)])
     (node#inboxPriority :style cardChip
       :textColor @color.text
       :runs [{:icon ~(get state.priorities (or-else card.priority 0)) :color ~(if (= card.priority 1)
                                                                                   "@bug"
                                                                                   "@color.muted")}
              ~(str " " (t (str "priority." (or-else card.priority 0))))])
     (node#inboxAssignee :style cardChip
       :textColor @color.text
       :icon circle-user
       :text ~(str " " (or-else (:name (find #(= (:who %) card.who) state.members)) card.who)))
     (node#inboxTag :style cardChip
       :textColor @color.text
       :runs [{:text "●" :font symbols :size 10 :color ~(str "@" card.tag)} ~(str " " (t (str "tag." card.tag)))])))

(defview inbox-actions "The inbox's detail buttons: open the issue, mark it read, snooze it." []
  `(row#inboxActions :gap 8 :anchors {:left 40 :right 40 :top 224} :height 28
     (node#inboxOpen :style propValue
       :icon maximize-2
       :text ~(t "inbox.open")
       :onClick openNotification
       :tooltip ~(t "key.enter"))
     (node#inboxSnooze :style propValue
       :icon alarm-clock
       :text ~(t "inbox.snooze")
       :onClick snoozeNotification
       :tooltip ~(t "key.h"))
     (node#inboxRead :style propValue
       :icon ~(if computed.notification.read "mail" "mail-check")
       :text ~(t (if computed.notification.read "inbox.markUnread" "inbox.markRead"))
       :onClick toggleRead
       :tooltip ~(t "key.u"))))

(defview cycle-current "The current sprint: its dates, its figures and its burn-up." []
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
     ~(cycle-dates)
     (node#cycleScope :use cycleStat
       :with {:text ~(t "cycle.scope") :value ~computed.cycleScope :color @color.text}
       :anchors {:left 20 :top 70})
     (node#cycleStarted :use cycleStat
       :with {:text ~(t "cycle.started")
              :value ~(sum (filter #(and (pos? (:c %)) (< (:c %) computed.lastColumn)) computed.allCards)
                           ; style: keep, sum is the expression's own function: the function it takes names its item it
                           (fn [it] (or-else it.points 0)))
              :color "#d97706"}
       :anchors {:left 160 :top 70})
     ~(cycle-completed)
     (node#cycleChartBase :anchors {:left 20 :right 20 :bottom 20} :height 1 :color @color.border)
     (node#cycleScopeArea :use areaChart
       :with {:values ~(concat state.cycle.scopeHistory [computed.cycleScope])
              :top ~computed.cycleTop
              :color "#0d94881f"}
       :anchors {:left 20 :right 20 :bottom 21}
       :height 90)
     ~(cycle-scope-line)
     ~(cycle-done-line)))

(defview cycle-completed "The sprint's completed points." []
  `(node#cycleCompleted :use cycleStat
     :with {:text ~(t "cycle.completed")
            :value ~(str computed.cycleDone
                         "  ·  "
                         (formatPercent (/ computed.cycleDone (max computed.cycleScope 1))))
            :color @color.accent}
     :anchors {:left 300 :top 70}))

(defview cycle-scope-line "The burn-up's scope line." []
  `(node#cycleScopeLine :use lineChart
     :with {:values ~(concat state.cycle.scopeHistory [computed.cycleScope])
            :top ~computed.cycleTop
            :color "#9ca3af"
            :lineWidth 1.5}
     :anchors {:left 20 :right 20 :bottom 21}
     :height 90
     :role img
     :label ~(t "cycle.scope")))

(defview cycle-done-line "The burn-up's completed line." []
  `(node#cycleDoneLine :use lineChart
     :with {:values ~(concat state.cycle.doneHistory [computed.cycleDone])
            :top ~computed.cycleTop
            :color @color.accent
            :lineWidth 2}
     :anchors {:left 20 :right 20 :bottom 21}
     :height 90
     :role img
     :label ~(t "cycle.completed")))

(defview cycle-dates "The sprint's dates." []
  `(node#cycleDates :style caption
     :anchors {:left 20 :top 36}
     :height 20
     :text ~(str (formatDate (dateMs state.cycle.start) "MMM d" "utc")
                 " – "
                 (formatDate (dateMs state.cycle.end) "MMM d" "utc")
                 "  ·  "
                 (t "cycle.left" {:n (daysBetween state.today state.cycle.end)}))))

(defview cycle-issue "An issue of the current sprint." [card]
  `(node#cycleIssue :anchors {:left 0 :right 0}
     :height 36
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str card.ref " " card.title)
     :onClick [~(handler select-card card.c card.at)
               {:set state.page :to issues}
               {:set state.issueOpen :to true}]
     :keys {:enter [~(handler select-card card.c card.at)
                    {:set state.page :to issues}
                    {:set state.issueOpen :to true}]}
     (node#cycleIssueLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
     ~(cycle-issue-priority card)
     (node#cycleIssueRef :style caption
       :text ~card.ref
       :width 56
       :fitText false
       :anchors {:left 36 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     ~(cycle-issue-status card)
     (label#cycleIssueTitle :font small
       :text ~card.title
       :textColor @color.text
       :anchors {:left 124 :right 120 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#cycleIssuePoints :style caption
       :text ~(or-else card.points "")
       :width 24
       :fitText false
       :anchors {:right 44 :top 0 :bottom 0}
       :align {:horizontal end :vertical center})
     ~(cycle-issue-owner card)))

(defview cycle-issue-status "An issue of the sprint's status icon." [card]
  `(label#cycleIssueStatus :icon ~(or-else (get state.statusIcons card.c) "circle")
     :text ""
     :fitText true
     :textColor ~(get-in computed.columns [card.c :color])
     :anchors {:left 100 :top 0 :bottom 0}
     :align center))

(defview cycle-issue-priority "An issue of the sprint's priority icon." [card]
  `(label#cycleIssuePriority :icon ~(get state.priorities (or-else card.priority 0))
     :text ""
     :fitText true
     :textColor ~(if (= card.priority 1) "@bug" "@color.muted")
     :anchors {:left 12 :top 0 :bottom 0}
     :align center))

(defview cycle-issue-owner "An issue of the sprint's owner." [card]
  `(node#cycleIssueOwner :style badge
     :font tiny
     :text ~card.who
     :width 20
     :height 20
     :radius 999
     :anchors {:right 12 :top 8}
     :color ~(or-else (:color (find #(= (:who %) card.who) state.members)) "@color.muted")
     :textColor "#ffffff"))

(defview cycle-row "An upcoming or past sprint, as a row." [cycle]
  `(node#cycleRow :anchors {:left 0 :right 0}
     :height 44
     :color transparent
     :role listitem
     :label ~(t "cycle.name" {:n cycle.number})
     (node#cycleRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
     ~(cycle-row-icon cycle)
     (label#cycleRowName :font small
       :text ~(t "cycle.name" {:n cycle.number})
       :textColor @color.text
       :width 120
       :anchors {:left 36 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#cycleRowDates :style caption
       :width 180
       :fitText false
       :anchors {:left 160 :top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :text ~(str (formatDate (dateMs cycle.start) "MMM d" "utc")
                   " – "
                   (formatDate (dateMs cycle.end) "MMM d" "utc")))
     (node#cycleRowState :style caption
       :width 120
       :fitText false
       :anchors {:left 340 :top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :text ~(t (if (> cycle.start state.today) "cycle.upcoming" "cycle.past")))
     (node#cycleRowBar :use progressBar
       :with {:ratio ~(/ cycle.done (max cycle.scope 1)) :color @color.accent :width 120}
       :anchors {:right 72 :top 20})
     (node#cycleRowPercent :style caption
       :width 56
       :fitText false
       :anchors {:right 8 :top 0 :bottom 0}
       :align {:horizontal end :vertical center}
       :text ~(formatPercent (/ cycle.done (max cycle.scope 1))))))

(defview cycle-row-icon "A sprint's icon: done, started or not." [cycle]
  `(label#cycleRowIcon :icon ~(cond (or (>= cycle.done cycle.scope) (< cycle.number state.cycle.number))
                                    "check-circle-2"
                                    (= cycle.done 0) "circle-dashed"
                                    :else "circle-dot")
     :text ""
     :fitText true
     :textColor ~(if (= cycle.done 0) "@color.muted" "@color.accent")
     :anchors {:left 8 :top 0 :bottom 0}
     :align center))

(defview projects-columns "The projects table's header." []
  `(node#projectsColumns :anchors {:left 0 :right 0 :top 48} :height 32 :color transparent
     (node#projectsColumnName :style caption
       :text ~(t "project.name")
       :anchors {:left 52 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#projectsColumnHealth :style caption
       :text ~(t "project.health")
       :anchors {:right 440 :top 0 :bottom 0}
       :width 110
       :fitText false
       :align {:horizontal start :vertical center})
     (node#projectsColumnLead :style caption
       :text ~(t "project.lead")
       :anchors {:right 360 :top 0 :bottom 0}
       :width 60
       :fitText false
       :align {:horizontal start :vertical center})
     (node#projectsColumnTarget :style caption
       :text ~(t "project.target")
       :anchors {:right 240 :top 0 :bottom 0}
       :width 100
       :fitText false
       :align {:horizontal start :vertical center})
     (node#projectsColumnProgress :style caption
       :text ~(t "project.progress")
       :anchors {:right 24 :top 0 :bottom 0}
       :width 196
       :fitText false
       :align {:horizontal start :vertical center})
     (node#projectsColumnsLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)))

(defview project-row "A project as a row of the table." [project]
  `(node#projectRow :anchors {:left 0 :right 0}
     :height 44
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str project.name ", " (t (str "health." project.health)) ", " (formatPercent project.progress))
     :tooltip ~(t "projects.openPage")
     :onClick ~(handler open-project-page (indexOf (map :name data.projects) project.name))
     :keys {:enter ~(handler open-project-page (indexOf (map :name data.projects) project.name))}
     (node#projectRowLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)
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
     ~(project-health project)
     ~(project-lead project)
     ~(project-target project)
     (node#projectBar :use progressBar
       :with {:ratio ~project.progress :color ~project.color :width 120}
       :anchors {:right 100 :top 20})
     (node#projectPercent :style caption
       :width 56
       :fitText false
       :anchors {:right 24 :top 0 :bottom 0}
       :align {:horizontal end :vertical center}
       :text ~(str (formatPercent project.progress) "  ·  " project.issues))))

(defview project-health "A project's health, on track or at risk." [project]
  `(label#projectHealth :font small
     :icon circle
     :text ~(t (str "health." project.health))
     :textColor ~(get state.healthColors project.health)
     :anchors {:right 440 :top 0 :bottom 0}
     :width 110
     :align {:horizontal start :vertical center}))

(defview project-lead "A project's lead." [project]
  `(node#projectLead :style badge
     :text ~project.lead
     :width 22
     :height 22
     :radius 999
     :anchors {:right 398 :top 11}
     :color ~(or-else (:color (find #(= (:who %) project.lead) state.members)) "@color.muted")
     :textColor "#ffffff"
     :tooltip ~(or-else (:name (find #(= (:who %) project.lead) state.members)) project.lead)))

(defview project-target "A project's target date." [project]
  `(node#projectTarget :style caption
     :width 100
     :fitText false
     :anchors {:right 240 :top 0 :bottom 0}
     :align {:horizontal start :vertical center}
     :icon calendar
     :text ~(formatDate (dateMs project.target) "MMM d" "utc")
     :textColor ~(if (and (< (daysBetween state.today project.target) 0) (< project.progress 1))
                     "@bug"
                     "@color.muted")))

(defview projects-list "The projects page: its tabs, the layout switch, New project, then the table or the roadmap." []
  `(node#projectsList :when ~(neg? state.projectOpen) :anchors {:left 0 :right 0 :top 0 :bottom 0} :color transparent
     (node#projectsHeader :use pageHeader :with {:icon box :text ~(t "nav.projects")})
     (row#projectTabs :gap 2 :anchors {:left 120 :top 10} :width 360 :height 28 :role tablist :arrows horizontal
       ~(repeat [scope ["all" "active" "completed"]]
         `(node#projectTab :style ~(if (= state.projectScope scope) "toolButtonOn" "toolButton")
            :borderWidth ~(if (= state.projectScope scope) 1 0)
            :text ~(t (str "projects." scope))
            :role tab
            :selected ~(= state.projectScope scope)
            :onClick ~(handler choose-project-scope scope))))
     (row#projectLayouts :gap 2 :anchors {:right 140 :top 10} :fitContent true :height 28 :role radiogroup
       ~(repeat [layout ["list" "roadmap"]]
         `(label#projectLayout :style ~(if (= state.projectLayout layout) "toolButtonOn" "toolButton")
            :borderWidth ~(if (= state.projectLayout layout) 1 0)
            :icon ~(if (= layout "list") "list" "chart-gantt")
            :text ~(t (str "projects.layout." layout))
            :role radio
            :selected ~(= state.projectLayout layout)
            :tooltip ~(t (str "projects.layout." layout))
            :onClick {:set state.projectLayout :to ~layout})))
     (node#newProject :style topButton
       :icon plus
       :text ~(t "projects.new")
       :fitText true
       :height 28
       :anchors {:right 12 :top 10}
       :label ~(t "projects.new")
       :onClick addProject)
     ~(projects-table)
     ~(roadmap)))

(defview projects-table "The projects as rows: name, health, lead, target date and progress." []
  `(node#projectsTable :when ~(not= state.projectLayout :roadmap) :anchors {:left 0 :right 0 :top 48 :bottom 0} :color transparent
     ~(projects-columns)
     (column#projectRows :anchors {:left 0 :right 0 :top 32 :bottom 0} :scroll vertical :role list
       ~(repeat [project computed.projectsShown :key project.name]
         (project-row project)))))

(defview roadmap "The projects on a timeline: a bar from each one's start to its target; drag one to move its dates." []
  `(node#roadmapView :when ~(= state.projectLayout :roadmap) :anchors {:left 0 :right 0 :top 48 :bottom 0} :color transparent
     (timeline#roadmapTimeline :anchors {:left 0 :right 0 :top 0 :bottom 0}
       :tracks ~computed.roadmapTracks
       :duration 120
       :fps 1
       :timeFormat none
       :trackButtons false
       :pixelsPerSecond 9
       :offset 0
       :time ~(daysBetween state.roadmapStart state.today)
       :markers ~(map (fn [m] {:time (daysBetween state.roadmapStart (str "2026-0" m "-01"))
                               :label (formatDate (dateMs (str "2026-0" m "-01")) "MMMM" "utc")})
                      [2 3 4 5])
       :headerWidth 220
       :trackHeight 40
       :label ~(t "roadmap.title")
       :color @color.surface
       :clipColor @color.accent
       :headerColor @color.surface
       :textColor @color.text
       :accentColor @color.accent
       :onClipChange change-roadmap-clip
       ~(repeat [project data.projects]
         `(label#roadmapHeader :font small
            :icon ~project.icon
            :text ~project.name
            :textColor @color.text
            :color @color.surface
            :fill 0
            :padding 16
            :align {:horizontal start :vertical center})))))

(defview project-page "A project's own page: a crumb back, its summary, milestones and updates, its properties aside." []
  `(node#projectPage :when ~(some? computed.project) :anchors {:left 0 :right 0 :top 0 :bottom 0} :color transparent
     ~(project-header)
     (column#projectBody :anchors {:left 40 :right 340 :top 48 :bottom 0} :scroll vertical :gap 4
       ~(project-hero)
       ~(project-section "milestones" "milestone" 'computed.milestoneCount)
       ~(repeat [m (or-else computed.project.milestones []) :index k]
         (milestone-row m k))
       ~(milestone-field)
       ~(project-section "updates" "megaphone" 'computed.updateCount)
       ~(update-composer)
       ~(repeat [u (or-else computed.project.updates []) :index k]
         (update-item u))
       ~(project-section "issues" "copy" 'computed.projectIssueCount)
       ~(repeat [card computed.projectIssues :key card.ref]
         (project-issue card)))
     (node#projectAsideLine :anchors {:right 300 :top 48 :bottom 0} :width 1 :color @color.border)
     ~(project-aside)))

(defview project-header "The project page's bar: Projects › its name, and View issues." []
  `(node#projectHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
     (row#projectCrumbs :anchors {:left 12 :top 10} :height 28 :fitContent width :gap 2 :alignItems center
       (label#projectCrumb :style topButton
         :icon box
         :text ~(t "nav.projects")
         :height 28
         :textColor @color.muted
         :tooltip ~(t "projects.back")
         :label ~(t "projects.back")
         :onClick close-project-page)
       (label#projectCrumbSep :font small :icon chevron-right :text "" :fitText true :height 28 :textColor @color.muted)
       (label#projectCrumbName :font small
         :text ~computed.project.name
         :textColor @color.text
         :fitText true
         :padding 6
         :height 28
         :align {:horizontal start :vertical center}))
     (label#projectViewIssues :style toolButton
       :icon list-filter
       :text ~(t "project.viewIssues")
       :anchors {:right 12 :top 10}
       :onClick ~(handler open-project state.projectOpen))
     (node#projectHeaderLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)))

(defview project-hero "The project's icon, name and summary." []
  `(node#projectHero :anchors {:left 0 :right 0} :height 140 :color transparent
     (label#projectHeroIcon :icon ~computed.project.icon
       :font iconsLarge
       :text ""
       :width 36
       :height 36
       :radius 8
       :align center
       :color ~(str computed.project.color "24")
       :textColor ~computed.project.color
       :anchors {:left 0 :top 28})
     (textarea#projectHeroName :font display
       :anchors {:left -6 :right 0 :top 70}
       :height 38
       :padding 6
       :radius @radius.chip
       :color transparent
       :hover {:color @color.column}
       :focused {:color @color.field}
       :textColor @color.text
       :label ~(t "project.name")
       :tooltip ~(t "project.nameHint")
       :placeholder ~computed.project.name
       :placeholderColor @color.muted
       :autofocus ~(= state.projectName "")
       :bind state.projectName
       :onSubmit rename-project
       :keys {:escape {:set state.projectName :to ~computed.project.name}})
     (textarea#projectSummary :anchors {:left -6 :right 0 :top 108}
       :height 28
       :padding 6
       :radius @radius.chip
       :color transparent
       :hover {:color @color.column}
       :focused {:color @color.field}
       :textColor @color.muted
       :placeholder ~(t "project.noSummary")
       :placeholderColor @color.muted
       :label ~(t "project.summary")
       :bind state.projectSummary
       :onSubmit save-project-summary
       :keys {:escape {:set state.projectSummary :to ~(or-else computed.project.summary "")}})))

(defview project-section "A heading of the project page with an icon and a count." [key icon n]
  `(node :id ~(str key "Section") :anchors {:left 0 :right 0} :height 52 :color transparent
     (label :id ~(str key "SectionTitle")
       :font heading
       :icon ~icon
       :text ~(t (str "project." key))
       :textColor @color.text
       :fitText true
       :anchors {:left 0 :top 20 :bottom 0}
       :align {:horizontal start :vertical center}
       :role heading)
     (node :id ~(str key "SectionCount")
       :style caption
       :text ~n
       :anchors {:right 8 :top 20 :bottom 0}
       :align {:horizontal end :vertical center})
     (node :id ~(str key "SectionLine") :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)))

(defview milestone-row "A milestone: done or not (a click switches it), its name and date." [m k]
  `(node#milestoneRow :anchors {:left 0 :right 0}
     :height 36
     :radius @radius.chip
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role checkbox
     :selected ~m.done
     :label ~(str m.name ", " (formatDate (dateMs m.target) "MMM d" "utc"))
     :tooltip ~(t (if m.done "milestone.reopen" "milestone.done"))
     :onClick ~(handler toggle-milestone k)
     :keys {:space ~(handler toggle-milestone k) :enter ~(handler toggle-milestone k)}
     (label#milestoneIcon :icon ~(if m.done "check-circle-2" "diamond")
       :text ""
       :fitText true
       :textColor ~(if m.done computed.project.color "@color.muted")
       :anchors {:left 8 :top 0 :bottom 0}
       :align center)
     (label#milestoneName :font small
       :text ~m.name
       :textColor ~(if m.done "@color.muted" "@color.text")
       :anchors {:left 36 :right 120 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#milestoneDate :style caption
       :icon calendar
       :text ~(formatDate (dateMs m.target) "MMM d" "utc")
       :textColor ~(cond m.done "@color.muted" (< (daysBetween state.today m.target) 0) "@bug" :else "@color.muted")
       :anchors {:right 8 :top 0 :bottom 0}
       :align {:horizontal end :vertical center})))

(defview project-issue "One of the project's issues: a click opens it." [card]
  `(node#projectIssue :anchors {:left 0 :right 0}
     :height 34
     :radius @radius.chip
     :color transparent
     :hover {:color @color.column}
     :cursor hand
     :focusable true
     :role listitem
     :label ~(str card.ref " " card.title)
     :onClick [~(handler select-card card.c card.at) {:set state.page :to issues} {:set state.issueOpen :to true}]
     :keys {:enter [~(handler select-card card.c card.at) {:set state.page :to issues} {:set state.issueOpen :to true}]}
     (label#projectIssueStatus :icon ~(or-else (get state.statusIcons card.c) "circle")
       :text ""
       :fitText true
       :textColor ~(get-in computed.columns [card.c :color])
       :anchors {:left 8 :top 0 :bottom 0}
       :align center)
     (node#projectIssueRef :style caption
       :text ~card.ref
       :width 52
       :fitText false
       :anchors {:left 36 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label#projectIssueTitle :font small
       :text ~card.title
       :textColor @color.text
       :anchors {:left 92 :right 40 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#projectIssueOwner :style badge
       :font tiny
       :text ~card.who
       :width 20
       :height 20
       :radius 999
       :anchors {:right 8 :top 7}
       :color ~(or-else (:color (find #(= (:who %) card.who) state.members)) "@color.muted")
       :textColor "#ffffff")))

(defview milestone-field "The field under the milestones: Enter adds one by that name." []
  `(textarea#milestoneField :anchors {:left 0 :right 0}
     :height 32
     :padding 8
     :font small
     :radius @radius.chip
     :color transparent
     :focused {:color @color.field}
     :textColor @color.text
     :placeholder ~(t "milestone.add")
     :label ~(t "milestone.add")
     :placeholderColor @color.muted
     :bind state.milestoneDraft
     :onSubmit add-milestone
     :keys {:escape {:set state.milestoneDraft :to ""}}))

(defview update-composer "Writes a project update: the text, the health it reports, and Post." []
  `(node#updateComposer :anchors {:left 0 :right 0}
     :height 112
     :radius @radius.card
     :borderWidth 1
     :borderColor @color.border
     :color @color.surface
     (textarea#updateField :anchors {:left 8 :right 8 :top 6}
       :height 54
       :padding 6
       :wrap true
       :color transparent
       :focused {:color transparent}
       :textColor @color.text
       :placeholder ~(t "update.placeholder")
       :label ~(t "update.placeholder")
       :placeholderColor @color.muted
       :bind state.projectDraft.text
       :keys {"mod+enter" submit-project-update})
     (row#updateHealths :gap 6 :anchors {:left 12 :bottom 12} :fitContent true :height 26 :role radiogroup
       ~(repeat [h ["onTrack" "atRisk" "offTrack"]]
         `(label#updateHealth :style displayPill
            :height 26
            :icon circle
            :text ~(t (str "health." h))
            :textColor ~(get state.healthColors h)
            :borderColor ~(if (= state.projectDraft.health h) (get state.healthColors h) "@color.border")
            :color ~(if (= state.projectDraft.health h) "@color.chipHover" "transparent")
            :role radio
            :selected ~(= state.projectDraft.health h)
            :onClick {:set state.projectDraft.health :to ~h})))
     (label#postUpdate :style toolButtonOn
       :text ~(t "update.post")
       :tooltip ~(t "update.postKeys")
       :anchors {:right 12 :bottom 12}
       :onClick submit-project-update)))

(defview update-item "A project update: its health, who wrote it and when, then what it says." [u]
  `(node#updateItem :anchors {:left 0 :right 0} :height 76 :color transparent :role article
     :label ~(str (t (str "health." u.health)) ", " u.who ", " u.text)
     (node#updateWho :style badge
       :font tiny
       :text ~u.who
       :width 20
       :height 20
       :radius 999
       :anchors {:left 0 :top 16}
       :color ~(or-else (:color (find #(= (:who %) u.who) state.members)) "@color.muted")
       :textColor "#ffffff")
     (row#updateHead :anchors {:left 30 :top 16} :height 20 :fitContent width :gap 12 :alignItems center
       (label#updateHealthLabel :font small
         :icon circle
         :text ~(t (str "health." u.health))
         :textColor ~(or-else (get state.healthColors u.health) "@color.muted")
         :fitText true
         :height 20
         :align {:horizontal start :vertical center})
       (node#updateMeta :style caption
         :text ~(str (or-else (:name (find #(= (:who %) u.who) state.members)) u.who)
                     "  ·  "
                     (formatDate (dateMs u.when) "MMM d" "utc"))
         :height 20
         :align {:horizontal start :vertical center}))
     (label#updateText :font small
       :text ~u.text
       :textColor @color.text
       :wrap true
       :anchors {:left 30 :right 0 :top 40 :bottom 8}
       :align {:horizontal start :vertical start})
     (node#updateLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border)))

(defview project-aside "The project's properties: health, lead, dates, progress and issues." []
  `(column#projectAside :anchors {:right 24 :top 64 :bottom 0} :width 252 :gap 2
     (node#projectAsideTitle :style caption :text ~(t "project.properties") :height 32 :align {:horizontal start :vertical center})
     ~(project-prop "health"
                    `(label#projectAsideHealth :font small :icon circle
                       :text ~(t (str "health." computed.project.health))
                       :textColor ~(get state.healthColors computed.project.health)
                       :fitText true
                       :anchors {:left 100 :top 0 :bottom 0}
                       :align {:horizontal start :vertical center}))
     ~(project-prop "lead"
                    `(label#projectAsideLead :font small
                       :text ~(or-else (:name (find #(= (:who %) computed.project.lead) state.members)) computed.project.lead)
                       :textColor @color.text
                       :fitText true
                       :anchors {:left 100 :top 0 :bottom 0}
                       :align {:horizontal start :vertical center}))
     ~(project-prop "start"
                    `(label#projectAsideStart :font small
                       :text ~(if (some? computed.project.start) (formatDate (dateMs computed.project.start) "MMM d, yyyy" "utc") "–")
                       :textColor @color.text
                       :fitText true
                       :anchors {:left 100 :top 0 :bottom 0}
                       :align {:horizontal start :vertical center}))
     ~(project-prop "target"
                    `(label#projectAsideTarget :font small
                       :text ~(formatDate (dateMs computed.project.target) "MMM d, yyyy" "utc")
                       :textColor @color.text
                       :fitText true
                       :anchors {:left 100 :top 0 :bottom 0}
                       :align {:horizontal start :vertical center}))
     ~(project-prop "progress"
                    `(node#projectAsideProgress :anchors {:left 100 :right 0 :top 0 :bottom 0} :color transparent
                       (node#projectAsideBar :use progressBar
                         :with {:ratio ~computed.project.progress :color ~computed.project.color :width 100}
                         :anchors {:left 0 :top 14})
                       (node#projectAsidePercent :style caption
                         :text ~(formatPercent computed.project.progress)
                         :anchors {:left 112 :top 0 :bottom 0}
                         :align {:horizontal start :vertical center})))
     ~(project-prop "issues"
                    `(label#projectAsideIssues :font small
                       :text ~(str computed.project.issues)
                       :textColor @color.text
                       :fitText true
                       :anchors {:left 100 :top 0 :bottom 0}
                       :align {:horizontal start :vertical center}))))

(defview project-prop "A property row of the project's side panel: its name, then its value." [key value]
  `(node :id ~(str "prop-" key) :anchors {:left 0 :right 0} :height 32 :color transparent
     (node :id ~(str "prop-" key "-name")
       :style caption
       :text ~(t (str "project." key))
       :anchors {:left 0 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     ~value))
