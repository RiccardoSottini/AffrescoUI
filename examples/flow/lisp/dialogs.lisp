;;; dialogs.lisp --- what opens over the work area: the composer, the edit dialog and the selection bar

;;; The composer

(defview composer-box "The composer: a new card's title, notes and properties, and Create more to keep it open." []
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

(defview composer-chips "The composer's chips: the new card's status, priority, owner and label." []
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

;;; The edit dialog

(defview edit-dialog "The dialog that renames the chosen card (Cmd+E)." []
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

;;; The selection bar

(defview bulk-actions "The selection bar's buttons: the count, then what to do with every marked card." []
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
