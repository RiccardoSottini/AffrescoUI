;;; drawer.lisp --- Flow's card drawer, included by index.screen.lisp: the selected card's description (rich text, edited in place), due date with a date picker, labels, assignees, checklist, comments and attachments.

;;; Definitions

(defstyle section (label :font small :textColor @color.muted :height 18 :align {:horizontal start :vertical center}))

(defstyle drawerInput
  (textarea :height 30
    :padding 8
    :radius @radius.chip
    :borderWidth 1
    :borderColor @color.border
    :color @color.input
    :focused {:borderColor @color.accent}
    :textColor @color.text
    :placeholderColor @color.muted))

(defstyle pill
  (label :font small :height 26 :padding 8 :radius 999 :fitText true :focusable true :role checkbox :cursor hand))

(defstyle removeButton
  (label :font small
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
    :cursor hand))

(defcomponent cardDrawer "The selected card, in a scrolling column"
  {}
  `(column#cardDetail :gap 10 :scroll vertical :color transparent
     (node#detailHead :anchors {:left 12 :right 12} :height 24 :color transparent
       (label#detailTag :text ~(str " " (t (str "tag." computed.card.tag)))
         :icon ~(get state.tagIcons computed.card.tag)
         :font small
         :anchors {:left 0 :top 0}
         :padding 4
         :radius 4
         :fitText true
         :color ~(str "@" computed.card.tag "Soft")
         :textColor ~(str "@" computed.card.tag))
       {:id detailColumn
        :style caption
        :text ~(str (get-in computed.columns [state.selected.c :name]) " · " (t "card.points" {:n computed.card.points}))
        :anchors {:right 0 :top 2}})
     (label#detailTitle :font heading
       :text ~computed.card.title
       :textColor @color.text
       :wrap true
       :anchors {:left 12 :right 12}
       :height 44)
     {:id detailOwner
      :style caption
      :icon user
      :text ~(str " " (or-else (:name (find (fn [it] (= it.who computed.card.who)) state.members)) computed.card.who))
      :anchors {:left 12}
      :height 18}
     (label#detailNotes :when ~(not state.draft.editing)
       :anchors {:left 12 :right 12}
       :height 64
       :wrap true
       :textColor @color.text
       :runs ~(concat (if (or (nil? computed.card.notes) (= computed.card.notes ""))
                          [(t "detail.noNotes")]
                          (map-indexed (fn [index it] {:text it :bold (= (mod index 2) 1)})
                                       (split computed.card.notes "*")))
                      [" " {:text (or-else computed.card.linkText "") :link (or-else computed.card.link "")}])
       :onLink [{:copy ~event.link} {:run notify :with {:message ~(t "toast.linkCopied" {:link event.link})}}])
     (editor#notesEditor :when ~state.draft.editing
       :anchors {:left 12 :right 12}
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
     (row#notesButtons :when ~state.draft.editing :gap 8 :anchors {:left 12 :right 12} :height 30
       {:id notesSave :style detailButton :text ~(t "detail.save") :focusable true :role button :onClick saveNotes}
       {:id notesCancel :style detailButton :text ~(t "detail.cancel") :focusable true :role button :onClick cancelNotes})
     ~(detail-actions)
     {:id dueTitle :style section :text ~(t "drawer.due") :anchors {:left 12 :right 12}}
     ~(due-row)
     {:id labelsTitle :style section :text ~(t "drawer.labels") :anchors {:left 12 :right 12}}
     (row#labels :gap 6 :anchors {:left 12 :right 12} :height 26 :arrows horizontal
       ~(repeat [name state.labels]
         `{:id labelChip
           :style pill
           :let {:on ~(contains (or-else computed.card.labels []) name)}
           :text ~(str " " (t (str "label." name)))
           :selected ~on
           :icon ~(if on "check" "plus")
           :color transparent
           :borderWidth 1
           :borderColor ~(if on "@color.accent" "@color.border")
           :textColor ~(if on "@color.text" "@color.muted")
           :onClick {:run toggleListItem :with {:field labels :value ~name}}}))
     {:id assigneesTitle :style section :text ~(t "drawer.assignees") :anchors {:left 12 :right 12}}
     (row#assignees :gap 6 :anchors {:left 12 :right 12} :height 30 :arrows horizontal
       ~(repeat [member state.members]
         (assignee member)))
     {:id checklistTitle
      :style section
      :anchors {:left 12 :right 12}
      :text ~(t "drawer.checklist" {:done computed.checklistDone :total (count (or-else computed.card.checklist []))})}
     (row#checklistBar :anchors {:left 12 :right 12} :height 4 :radius 2 :color @color.field
       {:id checklistDone :grow ~computed.checklistDone :anchors {:top 0 :bottom 0} :radius 2 :color @chore}
       {:id checklistLeft
        :grow ~(- (count (or-else computed.card.checklist [])) computed.checklistDone)
        :anchors {:top 0 :bottom 0}
        :color transparent})
     (column#checklist :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.checklist [])) 28)
       ~(repeat [item computed.card.checklist :index k]
         (check-row item k)))
     {:id newItem
      :style drawerInput
      :anchors {:left 12 :right 12}
      :placeholder ~(t "drawer.addItem")
      :label ~(t "drawer.addItem")
      :bind state.newItem
      :onSubmit [{:run addToCard
                  :with {:field checklist :input ~state.newItem :value {:text ~(trim state.newItem) :done false}}}
                 {:set state.newItem :to ""}]}
     {:id commentsTitle
      :style section
      :text ~(t "drawer.comments" {:n (count (or-else computed.card.comments []))})
      :anchors {:left 12 :right 12}}
     (column#comments :gap 6 :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.comments [])) 46)
       ~(repeat [note computed.card.comments]
         `(label#comment :anchors {:left 0 :right 0}
            :height 40
            :wrap true
            :padding 6
            :radius @radius.chip
            :color @color.field
            :textColor @color.text
            :font small
            :runs [{:text ~(or-else (:name (find (fn [it] (= it.who note.who)) state.members)) note.who) :bold true}
                   ~(str "  " note.text)])))
     {:id newComment
      :style drawerInput
      :anchors {:left 12 :right 12}
      :placeholder ~(t "drawer.addComment")
      :label ~(t "drawer.addComment")
      :bind state.newComment
      :onSubmit [{:run addToCard
                  :with {:field comments :input ~state.newComment :value {:who "RS" :text ~(trim state.newComment)}}}
                 {:set state.newComment :to ""}]}
     {:id attachmentsTitle :style section :text ~(t "drawer.attachments") :anchors {:left 12 :right 12}}
     (column#attachments :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.attachments [])) 28)
       ~(repeat [file computed.card.attachments :index k]
         `(node#attachment :anchors {:left 0 :right 0} :height 28 :color transparent
            (label#attachmentName :icon paperclip
              :text ~(str " " file)
              :textColor @color.text
              :font small
              :anchors {:left 0 :right 30 :top 0 :bottom 0}
              :align {:horizontal start :vertical center})
            {:id attachmentRemove
             :style removeButton
             :anchors {:right 0 :top 2}
             :label ~(t "drawer.remove" {:name file})
             :onClick {:run removeFromCard :with {:field attachments :at ~k}}})))
     {:id newAttachment
      :style drawerInput
      :anchors {:left 12 :right 12}
      :placeholder ~(t "drawer.addAttachment")
      :label ~(t "drawer.addAttachment")
      :bind state.newAttachment
      :onSubmit [{:run addToCard :with {:field attachments :input ~state.newAttachment :value ~(trim state.newAttachment)}}
                 {:set state.newAttachment :to ""}]}))

;;; Views

(defview detail-actions "#detailActions." []
  `(row#detailActions :gap 8 :anchors {:left 12 :right 12} :height 30 :arrows horizontal :role toolbar
     {:id detailEdit
      :style detailButton
      :text ~(if (< window.width 1440) "" (t "detail.edit"))
      :label ~(t "detail.edit")
      :tooltip ~(t "detail.edit")
      :icon pencil
      :focusable true
      :role button
      :onClick editCard}
     {:id detailNotesEdit
      :style detailButton
      :text ""
      :icon file-pen-line
      :focusable true
      :role button
      :tooltip ~(t "detail.editNotes")
      :label ~(t "detail.editNotes")
      :onClick editNotes}
     {:id detailLeft
      :style detailButton
      :text ""
      :label ~(t "detail.left")
      :tooltip ~(t "detail.left")
      :icon arrow-left
      :mirror true
      :focusable true
      :role button
      :when ~(> state.selected.c 0)
      :onClick moveLeft}
     {:id detailRight
      :style detailButton
      :text ~(if (< window.width 1440) "" (t "detail.right"))
      :label ~(t "detail.right")
      :tooltip ~(t "detail.right")
      :icon arrow-right
      :mirror true
      :focusable true
      :role button
      :when ~(< state.selected.c computed.lastColumn)
      :onClick moveRight}
     {:id detailDelete
      :style detailButton
      :text ~(if (< window.width 1440) "" (t "detail.delete"))
      :label ~(t "detail.delete")
      :tooltip ~(t "detail.delete")
      :icon trash-2
      :textColor @bug
      :focusable true
      :role button
      :onClick deleteCard}
     {:id detailMore
      :style detailButton
      :text ""
      :icon ellipsis
      :focusable true
      :role button
      :tooltip ~(t "detail.more")
      :label ~(t "detail.more")
      :onClick {:run openCardMenu :with {:c ~state.selected.c :i ~state.selected.i}}}))

(defview due-picker "#duePicker." []
  `(node#duePicker :overlay {:placement below :open state.dueOpen :dismiss outside :modal true}
     :y 6
     :width 280
     :height 292
     :radius @radius.card
     :shadow {:y 6 :blur 16 :color "#0000002e"}
     :color @color.menu
     (calendar#dueCalendar :fill 10
       :focusable true
       :label ~(t "drawer.due")
       :value ~(or-else computed.card.due state.today)
       :today ~state.today
       :marked ~(filter (fn [it] (some? it)) (map (fn [it] it.due) computed.allCards))
       :firstDayOfWeek ~settings.board.weekStart
       :monthNames ~(split (t "calendar.months") ",")
       :dayNames ~(split (t "calendar.days") ",")
       :textColor @color.text
       :accentColor @color.accent
       :pickedTextColor @color.chipOnText
       :keys {:escape {:set state.dueOpen :to false}}
       :onChange {:run setDue :with {:date ~event.value}})))

(defview due-row "#dueRow." []
  `(row#dueRow :gap 8 :anchors {:left 12 :right 12} :height 30
     {:id dueButton
      :style detailButton
      :icon calendar
      :focusable true
      :role button
      :isOpen ~state.dueOpen
      :text ~(str " "
                  (if (nil? computed.card.due) (t "drawer.noDue") (formatDate (dateMs computed.card.due) "MMM d" "utc")))
      :onClick {:toggle state.dueOpen}}
     (label#dueState :font small
       :when ~(some? computed.dueDays)
       :fitText true
       :anchors {:top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :textColor ~(if (and (< computed.dueDays 0) (not= state.selected.c computed.lastColumn)) "@bug" "@color.muted")
       :text ~(if (< computed.dueDays 0)
                  (t "drawer.late" {:n (- computed.dueDays)})
                  (t "drawer.dueIn" {:n computed.dueDays})))
     {:id dueClear
      :style removeButton
      :when ~(some? computed.card.due)
      :anchors {:top 3}
      :tooltip ~(t "drawer.clearDue")
      :label ~(t "drawer.clearDue")
      :onClick {:run setDue :with {:date nil}}}
     ~(due-picker)))

(defview assignee "#assignee." [member]
  `{:id assignee
    :style badge
    :let {:on ~(contains (or-else computed.card.assignees []) member.who)}
    :text ~member.who
    :width 30
    :height 30
    :radius 999
    :focusable true
    :role checkbox
    :selected ~on
    :label ~member.name
    :tooltip ~member.name
    :cursor hand
    :color ~(if on member.color "@color.field")
    :textColor ~(if on "#ffffff" "@color.muted")
    :onClick {:run toggleListItem :with {:field assignees :value ~member.who}}})

(defview check-row "#checkRow." [item k]
  `(node#checkRow :anchors {:left 0 :right 0} :height 28 :color transparent
     (checkbox#checkBox :checked ~item.done
       :label ~item.text
       :anchors {:left 0 :top 5}
       :width 18
       :height 18
       :activeColor @color.accent
       :trackColor @color.border
       :onChange {:run checkItem :with {:at ~k :value ~event.value}})
     (label#checkText :text ~item.text
       :textColor ~(if item.done "@color.muted" "@color.text")
       :anchors {:left 28 :right 30 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:id checkRemove
      :style removeButton
      :anchors {:right 0 :top 2}
      :label ~(t "drawer.remove" {:name item.text})
      :onClick {:run removeFromCard :with {:field checklist :at ~k}}}))
