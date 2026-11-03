;;; issue.lisp --- Flow's issue page, included by index.screen.lisp: the selected card on a page of its own, its description and activity on the left, its properties on the right.

;;; Definitions

(defstate issueOpen false)

(defstate subOpen false)

(defstate issueStatusOpen false)

(defstate issuePriorityOpen false)

(defstate issueLabelsOpen false)

(defstate issueWhoOpen false)

(defstate issueDueOpen false)

(defstate issueEstimateOpen false)

(defstate issueTagOpen false)

(defstate estimates [0 1 2 3 5 8])

(defcomputed issueEvents
  (if (nil? computed.card) [] (filter (fn [it] (= it.title computed.card.title)) state.activity)))

(defstyle propName
  (label :font small :textColor @color.muted :width 84 :height 32 :align {:horizontal start :vertical center}))

(defstyle propValue
  (label :font small
    :height 28
    :padding 8
    :fitText true
    :radius @radius.chip
    :color transparent
    :hover {:color @color.sidebarHover}
    :textColor @color.text
    :cursor hand
    :focusable true
    :role button))

(defstyle pickRow
  (label :font small
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
    :role menuitem))

(defstyle pickMenu
  (column :y 6
    :width 200
    :justify center
    :radius @radius.card
    :shadow {:y 6 :blur 16 :color "#0000002e"}
    :color @color.menu))

(defcomponent issuePage "The selected card as a page: a header with the way back, the title, description, sub-issues and activity, and a properties panel"
  {}
  `(node#panel-issue-page :color @color.surface :role dialog :label ~(str computed.card.ref " " computed.card.title)
     ~(issue-header)
     ~(issue-main)
     {:id issueSideLine :anchors {:right 300 :top 48 :bottom 0} :width 1 :color @color.border}
     ~(issue-side)))

;;; Views

(defview issue-nav "#issueNav." []
  `(row#issueNav :gap 2 :alignItems center :justify end :anchors {:right 12 :top 10} :width 140 :height 28
     (label#issuePosition :font small
       :fitText true
       :padding 8
       :textColor @color.muted
       :text ~(str (+ (indexOf computed.visibleRefs computed.card.ref) 1) " / " (count computed.visibleRefs)))
     {:id issueUp
      :style propValue
      :icon chevron-up
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align center
      :tooltip ~(str (t "issue.previous") " (K)")
      :label ~(t "issue.previous")
      :disabled ~(nil? computed.prev)
      :onClick selectPrev}
     {:id issueDown
      :style propValue
      :icon chevron-down
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align center
      :tooltip ~(str (t "issue.next") " (J)")
      :label ~(t "issue.next")
      :disabled ~(nil? computed.next)
      :onClick selectNext}))

(defview issue-header "#issueHeader." []
  `(node#issueHeader :anchors {:left 0 :right 0 :top 0} :height 48 :color transparent
     {:id issueBack
      :style propValue
      :icon arrow-left
      :text ""
      :width 28
      :fitText false
      :padding 0
      :align {:horizontal center :vertical center}
      :anchors {:left 12 :top 10}
      :mirror true
      :tooltip ~(t "issue.back")
      :label ~(t "issue.back")
      :onClick closeIssue}
     (label#issueCrumb :font small
       :anchors {:left 48 :top 10}
       :height 28
       :width 400
       :textColor @color.muted
       :align {:horizontal start :vertical center}
       :runs ["Flow  ›  " {:text ~(or-else computed.card.ref "") :bold true}])
     ~(issue-nav)
     {:id issueHeaderLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}))

(defview issue-sub "#issueSub." [item k]
  `(node#issueSub :anchors {:left 0 :right 0} :height 34 :color transparent :hover {:color @color.column}
     {:id issueSubLine :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @color.border}
     (checkbox#issueSubBox :checked ~item.done
       :label ~item.text
       :anchors {:left 8 :top 8}
       :width 18
       :height 18
       :activeColor @color.accent
       :trackColor @color.border
       :onChange {:run checkItem :with {:at ~k :value ~event.value}})
     (label#issueSubText :text ~item.text
       :textColor ~(if item.done "@color.muted" "@color.text")
       :anchors {:left 36 :right 30 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:id issueSubRemove
      :style removeButton
      :anchors {:right 4 :top 5}
      :label ~(t "drawer.remove" {:name item.text})
      :onClick {:run removeFromCard :with {:field checklist :at ~k}}}))

(defview issue-comment "#issueComment." [note]
  `(node#issueComment :anchors {:left 40 :right 40}
     :height 64
     :radius @radius.card
     :borderWidth 1
     :borderColor @color.border
     :color @color.card
     {:id issueCommentWho
      :style badge
      :text ~note.who
      :width 20
      :height 20
      :radius 999
      :anchors {:left 12 :top 10}
      :color ~(or-else (:color (find (fn [it] (= it.who note.who)) state.members)) "@color.muted")
      :textColor "#ffffff"}
     (label#issueCommentName :font small
       :text ~(or-else (:name (find (fn [it] (= it.who note.who)) state.members)) note.who)
       :textColor @color.text
       :anchors {:left 40 :top 10}
       :height 20
       :fitText true
       :align {:horizontal start :vertical center})
     (label#issueCommentText :text ~note.text
       :textColor @color.text
       :wrap true
       :anchors {:left 40 :right 12 :top 32 :bottom 6})))

(defview issue-main "#issueMain." []
  `(column#issueMain :anchors {:left 0 :right 301 :top 49 :bottom 0} :scroll vertical :gap 12 :color transparent
     {:id issueTop :height 28 :width 1 :color transparent}
     (label#issueTitle :font display
       :text ~computed.card.title
       :textColor @color.text
       :wrap true
       :anchors {:left 40 :right 40}
       :height 44
       :onDoubleClick editCard)
     (label#issueNotes :when ~(not state.draft.editing)
       :anchors {:left 40 :right 40}
       :height 64
       :wrap true
       :cursor ibeam
       :textColor ~(if (or (nil? computed.card.notes) (= computed.card.notes "")) "@color.muted" "@color.text")
       :runs ~(concat (if (or (nil? computed.card.notes) (= computed.card.notes ""))
                          [(t "detail.noNotes")]
                          (map-indexed (fn [index it] {:text it :bold (= (mod index 2) 1)})
                                       (split computed.card.notes "*")))
                      [" " {:text (or-else computed.card.linkText "") :link (or-else computed.card.link "")}])
       :onClick editNotes
       :onLink [{:copy ~event.link} {:run notify :with {:message ~(t "toast.linkCopied" {:link event.link})}}])
     (editor#issueNotesEditor :when ~state.draft.editing
       :anchors {:left 40 :right 40}
       :height 110
       :radius @radius.chip
       :color @color.input
       :textColor @color.text
       :wrap true
       :grammar markdown
       :bind state.draft.notes
       :autofocus true
       :label ~(t "detail.editNotes")
       :keys {"mod+enter" saveNotes :escape cancelNotes})
     (row#issueNotesButtons :when ~state.draft.editing :gap 8 :anchors {:left 40 :right 40} :height 30
       {:id issueNotesSave :style detailButton :text ~(t "detail.save") :focusable true :role button :onClick saveNotes}
       {:id issueNotesCancel
        :style detailButton
        :text ~(t "detail.cancel")
        :focusable true
        :role button
        :onClick cancelNotes})
     (row#issueAddSub :when ~(and (= (count (or-else computed.card.checklist [])) 0) (not state.subOpen))
       :anchors {:left 40 :right 40}
       :height 28
       {:id issueAddSubButton
        :style propValue
        :icon plus
        :text ~(str " " (t "issue.addSubs"))
        :textColor @color.muted
        :onClick {:set state.subOpen :to true}})
     {:id issueSubTitle
      :when ~(> (count (or-else computed.card.checklist [])) 0)
      :style section
      :anchors {:left 40 :right 40}
      :height 28
      :text ~(t "issue.subIssues" {:done computed.checklistDone :total (count (or-else computed.card.checklist []))})}
     (column#issueSubs :anchors {:left 40 :right 40} :height ~(* (count (or-else computed.card.checklist [])) 34)
       ~(repeat [item computed.card.checklist :index k]
         (issue-sub item k)))
     {:id issueNewSub
      :when ~(or (> (count (or-else computed.card.checklist [])) 0) state.subOpen)
      :autofocus ~state.subOpen
      :style drawerInput
      :anchors {:left 40 :right 40}
      :placeholder ~(t "issue.addSub")
      :label ~(t "issue.addSub")
      :bind state.newItem
      :onSubmit [{:run addToCard
                  :with {:field checklist :input ~state.newItem :value {:text ~(trim state.newItem) :done false}}}
                 {:set state.newItem :to ""}]}
     (label#issueActivityTitle :font heading
       :text ~(t "issue.activity")
       :textColor @color.text
       :anchors {:left 40 :right 40}
       :height 40
       :align {:horizontal start :vertical end})
     (label#issueCreated :font small
       :icon circle-plus
       :textColor @color.muted
       :anchors {:left 40 :right 40}
       :height 22
       :align {:horizontal start :vertical center}
       :text ~(str "  "
                   (t "issue.created"
                      {:who (or-else (:name (find (fn [it] (= it.who computed.card.who)) state.members))
                                     computed.card.who)})))
     ~(repeat [entry computed.issueEvents]
       `(label#issueEvent :font small
          :icon history
          :textColor @color.muted
          :anchors {:left 40 :right 40}
          :height 22
          :align {:horizontal start :vertical center}
          :text ~(str "  "
                      (t (str "activity." entry.key)
                         {:who (or-else (:name (find (fn [it] (= it.who entry.who)) state.members)) entry.who)
                          :title entry.title
                          :column entry.column}))))
     ~(repeat [note computed.card.comments]
       (issue-comment note))
     {:id issueNewComment
      :style drawerInput
      :anchors {:left 40 :right 40}
      :height 44
      :placeholder ~(t "issue.comment")
      :label ~(t "issue.comment")
      :bind state.newComment
      :onSubmit [{:run addToCard
                  :with {:field comments :input ~state.newComment :value {:who ~state.me :text ~(trim state.newComment)}}}
                 {:set state.newComment :to ""}]}))

(defview status-picker "#statusPicker." []
  `(node#statusPicker :style pickMenu
     :overlay {:placement below :open state.issueStatusOpen :dismiss outside :modal true}
     :height ~(+ 8 (* 30 (count computed.columns)))
     :role menu
     ~(repeat [column computed.columns :index k]
       `(node#statusPick :style pickRow
          :icon ~(or-else (get state.statusIcons k) "circle")
          :text ~(str "  " column.name)
          :selected ~(= k state.selected.c)
          :children [(label#statusCheck :when ~(= k state.selected.c)
                       :icon check
                       :text ""
                       :width 28
                       :align center
                       :textColor @color.text
                       :anchors {:right 0 :top 0 :bottom 0})]
          :onClick {:run setStatus :with {:to ~k}}))))

(defview priority-picker "#priorityPicker." []
  `(node#priorityPicker :style pickMenu
     :overlay {:placement below :open state.issuePriorityOpen :dismiss outside :modal true}
     :height 158
     :role menu
     ~(repeat [icon state.priorities :index p]
       `(node#priorityPick :style pickRow
          :icon ~icon
          :text ~(str "  " (t (str "priority." p)))
          :selected ~(= p (or-else computed.card.priority 0))
          :children [(label#priorityCheck :when ~(= p (or-else computed.card.priority 0))
                       :icon check
                       :text ""
                       :width 28
                       :align center
                       :textColor @color.text
                       :anchors {:right 0 :top 0 :bottom 0})]
          :onClick {:run setPriority :with {:p ~p}}))))

(defview who-pick "#whoPick." [member]
  `(node#whoPick :style pickRow
     :text ~member.name
     :padding 36
     :selected ~(= member.who computed.card.who)
     :onClick {:run setAssignee :with {:who ~member.who}}
     (label#whoCheck :when ~(= member.who computed.card.who)
       :icon check
       :text ""
       :width 28
       :align center
       :textColor @color.text
       :anchors {:right 0 :top 0 :bottom 0})
     {:id whoPickBadge
      :style badge
      :text ~member.who
      :width 18
      :height 18
      :radius 999
      :anchors {:left 10 :top 6}
      :color ~member.color
      :textColor "#ffffff"}))

(defview issue-due-picker "#issueDuePicker." []
  `(node#issueDuePicker :overlay {:placement below :open state.issueDueOpen :dismiss outside :modal true}
     :y 6
     :width 280
     :height 292
     :radius @radius.card
     :shadow {:y 6 :blur 16 :color "#0000002e"}
     :color @color.menu
     (calendar#issueCalendar :fill 10
       :focusable true
       :label ~(t "issue.due")
       :value ~(or-else computed.card.due state.today)
       :today ~state.today
       :firstDayOfWeek ~settings.board.weekStart
       :monthNames ~(split (t "calendar.months") ",")
       :dayNames ~(split (t "calendar.days") ",")
       :textColor @color.text
       :accentColor @color.accent
       :pickedTextColor @color.chipOnText
       :onChange [{:run setDue :with {:date ~event.value}} {:set state.issueDueOpen :to false}])))

(defview prop-estimate "#propEstimate." []
  `(row#propEstimate :anchors {:left 8 :right 16} :height 32 :alignItems center
     {:id propEstimateValue
      :style propValue
      :icon triangle
      :isOpen ~state.issueEstimateOpen
      :label ~(t "card.points" {:n (or-else computed.card.points 0)})
      :text ~(str "  " (t "card.points" {:n (or-else computed.card.points 0)}))
      :onClick {:toggle state.issueEstimateOpen}}
     (node#estimatePicker :style pickMenu
       :overlay {:placement below :open state.issueEstimateOpen :dismiss outside :modal true}
       :height ~(+ 8 (* 30 (count state.estimates)))
       :role menu
       ~(repeat [n state.estimates]
         `{:id estimatePick
           :style pickRow
           :icon triangle
           :text ~(str "  " (t "card.points" {:n n}))
           :selected ~(= n (or-else computed.card.points 0))
           :onClick {:run setEstimate :with {:n ~n}}}))))

(defview prop-tag "#propTag." []
  `(node#propTag :style cardChip
     :height 24
     :textColor @color.text
     :tooltip ~(t "issue.changeTag")
     :label ~(t (str "tag." computed.card.tag))
     :cursor hand
     :focusable true
     :role button
     :isOpen ~state.issueTagOpen
     :onClick {:toggle state.issueTagOpen}
     :runs [{:text "●" :font symbols :size 10 :color ~(str "@" computed.card.tag)}
            ~(str " " (t (str "tag." computed.card.tag)))]
     (node#tagPicker :style pickMenu
       :overlay {:placement below :open state.issueTagOpen :dismiss outside :modal true}
       :height ~(+ 8 (* 30 (count (keys state.tagIcons))))
       :role menu
       ~(repeat [tag (keys state.tagIcons)]
         `{:id tagPick
           :style pickRow
           :icon ~(get state.tagIcons tag)
           :text ~(str "  " (t (str "tag." tag)))
           :selected ~(= tag computed.card.tag)
           :onClick {:run setTag :with {:tag ~tag}}}))))

(defview prop-label-add "#propLabelAdd." []
  `(node#propLabelAdd :style cardChip
     :height 24
     :width 24
     :fitText false
     :padding 0
     :align center
     :icon plus
     :text ""
     :label ~(t "issue.addLabel")
     :tooltip ~(t "issue.addLabel")
     :cursor hand
     :focusable true
     :role button
     :isOpen ~state.issueLabelsOpen
     :onClick {:toggle state.issueLabelsOpen}
     (node#labelPicker :style pickMenu
       :overlay {:placement below :open state.issueLabelsOpen :dismiss outside :modal true}
       :height ~(+ 8 (* 30 (count state.labels)))
       :role menu
       ~(repeat [name state.labels]
         `{:id labelPick
           :style pickRow
           :icon ~(if (contains (or-else computed.card.labels []) name) "square-check" "square")
           :text ~(str "  " (t (str "label." name)))
           :selected ~(contains (or-else computed.card.labels []) name)
           :onClick {:run toggleListItem :with {:field labels :value ~name}}}))))

(defview prop-labels "#propLabels." []
  `(column#propLabels :anchors {:left 8 :right 16}
     :height ~(if (> (count (or-else computed.card.labels [])) 2) 84 56)
     :alignItems start
     {:id propLabelsName :style propName :height 24 :padding 8 :width 200 :text ~(t "issue.labels")}
     (row#propLabelChips :wrap true :gap 4 :x 8 :width 268 :height 60
       ~(prop-tag)
       ~(repeat [name (or-else computed.card.labels [])]
         `{:id propLabel
           :style cardChip
           :height 24
           :runs [{:text "●" :font symbols :size 10 :color @color.accent} ~(str " " (t (str "label." name)))]
           :textColor @color.text
           :tooltip ~(t "issue.removeLabel")
           :cursor hand
           :focusable true
           :role button
           :label ~(t (str "label." name))
           :onClick {:run toggleListItem :with {:field labels :value ~name}}})
       ~(prop-label-add))))

(defview issue-side "#issueSide." []
  `(column#issueSide :anchors {:right 0 :top 49 :bottom 0}
     :width 300
     :gap 2
     :color @color.sidebar
     :role region
     :label ~(t "issue.properties")
     {:id issueSideTop :height 12 :width 1 :color transparent}
     {:id issueSideTitle :style section :text ~(t "issue.properties") :anchors {:left 16 :right 16} :height 28}
     (row#propStatus :anchors {:left 8 :right 16} :height 32 :alignItems center
       {:id propStatusValue
        :style propValue
        :isOpen ~state.issueStatusOpen
        :label ~(get-in computed.columns [state.selected.c :name])
        :runs [{:icon ~(or-else (get state.statusIcons state.selected.c) "circle")
                :color ~(get-in computed.columns [state.selected.c :color])}
               ~(str "  " (get-in computed.columns [state.selected.c :name]))]
        :onClick {:toggle state.issueStatusOpen}}
       ~(status-picker))
     (row#propPriority :anchors {:left 8 :right 16} :height 32 :alignItems center
       {:id propPriorityValue
        :style propValue
        :isOpen ~state.issuePriorityOpen
        :label ~(t (str "priority." (or-else computed.card.priority 0)))
        :runs [{:icon ~(get state.priorities (or-else computed.card.priority 0))
                :color ~(if (= computed.card.priority 1) "@bug" "@color.muted")}
               ~(str "  " (t (str "priority." (or-else computed.card.priority 0))))]
        :onClick {:toggle state.issuePriorityOpen}}
       ~(priority-picker))
     (row#propWho :anchors {:left 8 :right 16} :height 32 :alignItems center
       {:id propWhoValue
        :style propValue
        :icon circle-user
        :isOpen ~state.issueWhoOpen
        :text ~(str "  "
                    (or-else (:name (find (fn [it] (= it.who computed.card.who)) state.members)) (t "issue.noAssignee")))
        :onClick {:toggle state.issueWhoOpen}}
       (node#whoPicker :style pickMenu
         :overlay {:placement below :open state.issueWhoOpen :dismiss outside :modal true}
         :height ~(+ 8 (* 30 (count state.members)))
         :role menu
         ~(repeat [member state.members]
           (who-pick member))))
     (row#propDue :anchors {:left 8 :right 16} :height 32 :alignItems center
       {:id propDueValue
        :style propValue
        :icon calendar
        :isOpen ~state.issueDueOpen
        :textColor ~(if (and (some? computed.dueDays) (< computed.dueDays 0) (not= state.selected.c computed.lastColumn))
                        "@bug"
                        "@color.text")
        :text ~(str "  "
                    (if (nil? computed.card.due) (t "drawer.noDue") (formatDate (dateMs computed.card.due) "MMM d" "utc")))
        :onClick {:toggle state.issueDueOpen}}
       ~(issue-due-picker))
     ~(prop-estimate)
     {:id issueSideGap :height 10 :width 1 :color transparent}
     ~(prop-labels)
     (column#propProject :anchors {:left 8 :right 16} :height 56 :alignItems start
       {:id propProjectName :style propName :height 24 :padding 8 :width 200 :text ~(t "issue.project")}
       {:id propProjectValue
        :style propValue
        :focusable false
        :role text
        :cursor arrow
        :hover {:color transparent}
        :tooltip ~(t "issue.projectFollows")
        :let {:project ~(get data.projects (or-else (get state.projectOf computed.card.tag) 0))}
        :runs [{:icon ~project.icon :color ~project.color} ~(str "  " project.name)]})
     (column#propCycle :anchors {:left 8 :right 16} :height 56 :alignItems start
       {:id propCycleName :style propName :height 24 :padding 8 :width 200 :text ~(t "issue.cycle")}
       {:id propCycleValue
        :style propValue
        :focusable false
        :role text
        :cursor arrow
        :hover {:color transparent}
        :icon refresh-cw
        :textColor @color.text
        :text ~(str "  " (t "cycle.name" {:n state.cycle.number}))})))
