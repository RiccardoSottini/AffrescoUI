;;; drawer.lisp --- Flow's card drawer, included by index.screen.lisp: the selected card's description (rich text,
;;; edited in place), due date with a date picker, labels, assignees, checklist, comments and attachments.

;;; Styles

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

;;; Components

(defcomponent cardDrawer "The selected card, in a scrolling column"
  {}
  `(column#cardDetail :gap 10 :scroll vertical :color transparent
     ~(detail-head)
     (label#detailTitle :font heading
       :text ~computed.card.title
       :textColor @color.text
       :wrap true
       :anchors {:left 12 :right 12}
       :height 44)
     ~(detail-owner)
     ~(detail-notes)
     ~(notes-editor)
     ~(notes-buttons)
     ~(detail-actions)
     (node#dueTitle :style section :text ~(t "drawer.due") :anchors {:left 12 :right 12})
     ~(due-row)
     (node#labelsTitle :style section :text ~(t "drawer.labels") :anchors {:left 12 :right 12})
     ~(labels)
     (node#assigneesTitle :style section :text ~(t "drawer.assignees") :anchors {:left 12 :right 12})
     (row#assignees :gap 6 :anchors {:left 12 :right 12} :height 30 :arrows horizontal
       ~(repeat [member state.members]
         (assignee member)))
     ~(checklist-title)
     ~(checklist-bar)
     (column#checklist :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.checklist [])) 28)
       ~(repeat [item computed.card.checklist :index k]
         (check-row item k)))
     ~(new-item)
     (node#commentsTitle :style section
       :text ~(t "drawer.comments" {:n (count (or-else computed.card.comments []))})
       :anchors {:left 12 :right 12})
     ~(comments)
     ~(new-comment)
     (node#attachmentsTitle :style section :text ~(t "drawer.attachments") :anchors {:left 12 :right 12})
     ~(attachments)
     ~(new-attachment)))

(defview checklist-title "The checklist's heading in the drawer, with how many items are done." []
  `(node#checklistTitle :style section
     :anchors {:left 12 :right 12}
     :text ~(t "drawer.checklist" {:done computed.checklistDone :total (count (or-else computed.card.checklist []))})))

(defview detail-owner "The card's owner, in the drawer." []
  `(node#detailOwner :style caption
     :icon user
     :text ~(or-else (:name (find (fn [it] (= it.who computed.card.who)) state.members)) computed.card.who)
     :anchors {:left 12}
     :height 18))

(defview new-attachment "The field that attaches a link to the card." []
  `(node#newAttachment :style drawerInput
     :anchors {:left 12 :right 12}
     :placeholder ~(t "drawer.addAttachment")
     :label ~(t "drawer.addAttachment")
     :bind state.newAttachment
     :onSubmit [~(handler addToCard "attachments" (trim state.newAttachment) state.newAttachment)
                {:set state.newAttachment :to ""}]))

(defview new-item "The field that adds an item to the checklist." []
  `(node#newItem :style drawerInput
     :anchors {:left 12 :right 12}
     :placeholder ~(t "drawer.addItem")
     :label ~(t "drawer.addItem")
     :bind state.newItem
     :onSubmit [{:run addToCard
                 :with {:field checklist :input ~state.newItem :value {:text ~(trim state.newItem) :done false}}}
                {:set state.newItem :to ""}]))

(defview notes-buttons "Save and Cancel, under the description being edited." []
  `(row#notesButtons :when ~state.draft.editing :gap 8 :anchors {:left 12 :right 12} :height 30
     (node#notesSave :style detailButton :text ~(t "detail.save") :focusable true :role button :onClick saveNotes)
     (node#notesCancel :style detailButton
       :text ~(t "detail.cancel")
       :focusable true
       :role button
       :onClick cancelNotes)))

(defview checklist-bar "The checklist's progress, as a bar." []
  `(row#checklistBar :anchors {:left 12 :right 12} :height 4 :radius 2 :color @color.field
     (node#checklistDone :grow ~computed.checklistDone :anchors {:top 0 :bottom 0} :radius 2 :color @chore)
     (node#checklistLeft :grow ~(- (count (or-else computed.card.checklist [])) computed.checklistDone)
       :anchors {:top 0 :bottom 0}
       :color transparent)))

(defview notes-editor "The description, being edited." []
  `(editor#notesEditor :when ~state.draft.editing
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
     :keys {"mod+enter" saveNotes :escape cancelNotes}))

(defview new-comment "The field that adds a comment." []
  `(node#newComment :style drawerInput
     :anchors {:left 12 :right 12}
     :placeholder ~(t "drawer.addComment")
     :label ~(t "drawer.addComment")
     :bind state.newComment
     :onSubmit [{:run addToCard
                 :with {:field comments :input ~state.newComment :value {:who "RS" :text ~(trim state.newComment)}}}
                {:set state.newComment :to ""}]))

(defview comments "The card's comments, newest last." []
  `(column#comments :gap 6 :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.comments [])) 46)
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
                 ~note.text]))))

(defview detail-notes "The description, its bold words and link drawn." []
  `(label#detailNotes :when ~(not state.draft.editing)
     :anchors {:left 12 :right 12}
     :height 64
     :wrap true
     :textColor @color.text
     :runs ~(concat (if (or (nil? computed.card.notes) (= computed.card.notes ""))
                        [(t "detail.noNotes")]
                        (map-indexed (fn [index it] {:text it :bold (= (mod index 2) 1)})
                                     (split computed.card.notes "*")))
                    [" " {:text (or-else computed.card.linkText "") :link (or-else computed.card.link "")}])
     :onLink copyLink))

(defview labels "The card's labels, as pills to turn on and off." []
  `(row#labels :gap 6 :anchors {:left 12 :right 12} :height 26 :arrows horizontal
     ~(repeat [name state.labels]
       `(node#labelChip :style pill
          :let {:on ~(contains (or-else computed.card.labels []) name)}
          :text ~(str " " (t (str "label." name)))
          :selected ~on
          :icon ~(if on "check" "plus")
          :color transparent
          :borderWidth 1
          :borderColor ~(if on "@color.accent" "@color.border")
          :textColor ~(if on "@color.text" "@color.muted")
          :onClick ~(handler toggleListItem "labels" name)))))

(defview detail-head "The drawer's top: the card's label, its column and points." []
  `(node#detailHead :anchors {:left 12 :right 12} :height 24 :color transparent
     (label#detailTag :text ~(t (str "tag." computed.card.tag))
       :icon ~(get state.tagIcons computed.card.tag)
       :font small
       :anchors {:left 0 :top 0}
       :padding 4
       :radius 4
       :fitText true
       :color ~(str "@" computed.card.tag "Soft")
       :textColor ~(str "@" computed.card.tag))
     (node#detailColumn :style caption
       :text ~(str (get-in computed.columns [state.selected.c :name])
                   " · "
                   (t "card.points" {:n computed.card.points}))
       :anchors {:right 0 :top 2})))

(defview attachments "The card's attachments, each with an x." []
  `(column#attachments :anchors {:left 12 :right 12} :height ~(* (count (or-else computed.card.attachments [])) 28)
     ~(repeat [file computed.card.attachments :index k]
       `(node#attachment :anchors {:left 0 :right 0} :height 28 :color transparent
          (label#attachmentName :icon paperclip
            :text ~file
            :textColor @color.text
            :font small
            :anchors {:left 0 :right 30 :top 0 :bottom 0}
            :align {:horizontal start :vertical center})
          (node#attachmentRemove :style removeButton
            :anchors {:right 0 :top 2}
            :label ~(t "drawer.remove" {:name file})
            :onClick ~(handler removeFromCard "attachments" k))))))

;;; Views

(defview detail-actions "The drawer's buttons: open the card as a page, move it, and its menu." []
  `(row#detailActions :gap 8 :anchors {:left 12 :right 12} :height 30 :arrows horizontal :role toolbar
     (node#detailEdit :style detailButton
       :text ~(if (< window.width 1440) "" (t "detail.edit"))
       :label ~(t "detail.edit")
       :tooltip ~(t "detail.edit")
       :icon pencil
       :focusable true
       :role button
       :onClick editCard)
     (node#detailNotesEdit :style detailButton
       :text ""
       :icon file-pen-line
       :focusable true
       :role button
       :tooltip ~(t "detail.editNotes")
       :label ~(t "detail.editNotes")
       :onClick editNotes)
     ~(detail-left)
     ~(detail-right)
     ~(detail-delete)
     (node#detailMore :style detailButton
       :text ""
       :icon ellipsis
       :focusable true
       :role button
       :tooltip ~(t "detail.more")
       :label ~(t "detail.more")
       :onClick ~(handler openCardMenu state.selected.c state.selected.i))))

(defview detail-left "The drawer's button that moves the card a column back." []
  `(node#detailLeft :style detailButton
     :text ""
     :label ~(t "detail.left")
     :tooltip ~(t "detail.left")
     :icon arrow-left
     :mirror true
     :focusable true
     :role button
     :when ~(> state.selected.c 0)
     :onClick moveLeft))

(defview detail-delete "The drawer's button that deletes the card." []
  `(node#detailDelete :style detailButton
     :text ~(if (< window.width 1440) "" (t "detail.delete"))
     :label ~(t "detail.delete")
     :tooltip ~(t "detail.delete")
     :icon trash-2
     :textColor @bug
     :focusable true
     :role button
     :onClick deleteCard))

(defview detail-right "The drawer's button that moves the card a column on." []
  `(node#detailRight :style detailButton
     :text ~(if (< window.width 1440) "" (t "detail.right"))
     :label ~(t "detail.right")
     :tooltip ~(t "detail.right")
     :icon arrow-right
     :mirror true
     :focusable true
     :role button
     :when ~(< state.selected.c computed.lastColumn)
     :onClick moveRight))

(defview due-picker "The drawer's date picker, below the due date." []
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
       :onChange ~(handler setDue event.value))))

(defview due-row "The drawer's due date, which opens the date picker." []
  `(row#dueRow :gap 8 :anchors {:left 12 :right 12} :height 30
     (node#dueButton :style detailButton
       :icon calendar
       :focusable true
       :role button
       :isOpen ~state.dueOpen
       :text ~(if (nil? computed.card.due)
                       (t "drawer.noDue")
                       (formatDate (dateMs computed.card.due) "MMM d" "utc"))
       :onClick {:toggle state.dueOpen})
     (label#dueState :font small
       :when ~(some? computed.dueDays)
       :fitText true
       :anchors {:top 0 :bottom 0}
       :align {:horizontal start :vertical center}
       :textColor ~(if (and (< computed.dueDays 0) (not= state.selected.c computed.lastColumn)) "@bug" "@color.muted")
       :text ~(if (< computed.dueDays 0)
                  (t "drawer.late" {:n (- computed.dueDays)})
                  (t "drawer.dueIn" {:n computed.dueDays})))
     (node#dueClear :style removeButton
       :when ~(some? computed.card.due)
       :anchors {:top 3}
       :tooltip ~(t "drawer.clearDue")
       :label ~(t "drawer.clearDue")
       :onClick ~(handler setDue nil))
     ~(due-picker)))

(defview assignee "A member as a pill in the drawer, on while they are an assignee of the card." [member]
  `(node#assignee :style badge
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
     :onClick ~(handler toggleListItem "assignees" member.who)))

(defview check-row "One item of the card's checklist: a check box, its text and an x." [item k]
  `(node#checkRow :anchors {:left 0 :right 0} :height 28 :color transparent
     (checkbox#checkBox :checked ~item.done
       :label ~item.text
       :anchors {:left 0 :top 5}
       :width 18
       :height 18
       :activeColor @color.accent
       :trackColor @color.border
       :onChange ~(handler checkItem k event.value))
     (label#checkText :text ~item.text
       :textColor ~(if item.done "@color.muted" "@color.text")
       :anchors {:left 28 :right 30 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#checkRemove :style removeButton
       :anchors {:right 0 :top 2}
       :label ~(t "drawer.remove" {:name item.text})
       :onClick ~(handler removeFromCard "checklist" k))))
