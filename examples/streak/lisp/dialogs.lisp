;;; dialogs.lisp --- what opens over the app: the day's popup, and the dialog that adds or edits a habit

;;; The day's popup
(defview entry-popup "The day's popup: the value or yes, skip, no, and a note." []
  `(node#entryPopup :overlay {:placement window :open state.popup.open :dismiss outside :modal true}
     :let {:ph ~(get data.ordered state.popup.h)
           :pcol ~(get computed.pal ph.color)
           :meas ~(= ph.kind "measurable")
           :pd ~(if (= state.popup.date "") state.today state.popup.date)}
     :x ~(clamp (- state.popup.x 140) 8 (- window.width 288))
     :y ~(clamp (+ state.popup.y 14) 8 (- window.height 208))
     :width 280
     :height ~(if meas 196 152)
     :style card
     :shadow {:y 4 :blur 16 :color "#00000050"}
     :role dialog
     :label ~(str ph.name ", " (formatDate (dateMs pd) "full" "utc"))
     (label#label-format-date-date-8 :text ~(formatDate (dateMs pd) "EEEE d MMMM" "utc")
       :font heading
       :textColor ~pcol
       :anchors {:left 16 :top 14}
       :fitText true)
     (node#popupValue
     :when ~meas
     :style field
     :bind state.popup.value
     :autofocus true
     :label ~(t "popup.value")
     :placeholder ~(t "popup.value")
     :anchors {:left 16 :right 80 :top 44}
     :onSubmit popupSave)
     (label#label-ph-unit :text
                          ~ph.unit
                          :when
                          ~meas
                          :textColor
                          @c.muted
                          :anchors
                          {:right 16 :top 56}
                          :width
                          56
                          :height
                          18)
     (node#popupNote
     :when ~(not meas)
     :style field
     :bind state.popup.note
     :autofocus true
     :label ~(t "popup.note")
     :placeholder ~(t "popup.note")
     :anchors {:left 16 :right 16 :top 44}
     :onSubmit popupNoteOnly)
     (node#popupMeasNote
     :when ~meas
     :style field
     :bind state.popup.note
     :label ~(t "popup.note")
     :placeholder ~(t "popup.note")
     :anchors {:left 16 :right 16 :top 92}
     :onSubmit popupSave)
     ~(popup-buttons)
     (node#popupSave
     :style textButton
     :when ~meas
     :text ~(upper-case (t "popup.save"))
     :anchors {:right 12 :bottom 10}
     :onClick popupSave)))

(defview popup-buttons "The popup's yes, skip and no." []
  `(row#popupButtons :when ~(not meas)
     :anchors {:left 8 :right 8 :bottom 10}
     :height 40
     :justify space-around
     :arrows horizontal
     (node#popupYes
     :style textButton
     :icon check
     :text ~(t "popup.yes")
     :textColor ~pcol
     :mnemonic y
     :onClick ~(handler popupSet "y"))
     (node#popupSkip
     :style textButton
     :icon chevrons-right
     :text ~(t "popup.skip")
     :textColor ~pcol
     :mnemonic s
     :onClick ~(handler popupSet "s"))
     (node#popupNo
     :style textButton
     :icon x
     :text ~(t "popup.no")
     :textColor @c.muted
     :mnemonic n
     :onClick ~(handler popupSet "-"))))

(defview edit-fields "The dialog's fields: name, colour, question, unit, target, frequency and reminder." []
  `(column#editFields :anchors {:left 20 :right 20 :top 48 :bottom 56} :gap 6
     (node#editName :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-name :style caption :text ~(t "edit.name") :anchors {:left 2 :top 0})
       (node#nameField
       :style field
       :bind state.edit.name
       :autofocus true
       :label ~(t "edit.name")
       :placeholder ~(t "edit.namePh")
       :anchors {:left 0 :right 52 :top 18}
       :borderColor ~(if (not= state.edit.error "") "@c.danger" "@c.divider"))
       ~(color-button))
     (node#editQuestion :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-question :style caption :text ~(t "edit.question") :anchors {:left 2 :top 0})
       (node#questionField
       :style field
       :bind state.edit.question
       :label ~(t "edit.question")
       :placeholder ~(t "edit.questionPh")
       :anchors {:left 0 :right 0 :top 18}))
     (node#editTarget :anchors {:left 0 :right 0}
       :height 58
       :color transparent
       :children [{:id label-t-edit-target :style caption :text ~(t "edit.target") :anchors {:left 2 :top 0}}
                  ~(row-edit-target)]
       :when ~(= state.edit.kind "measurable"))
     (node#editFrequency :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-frequency :style caption :text ~(t "edit.frequency") :anchors {:left 2 :top 0})
       ~(row-edit-frequency))
     (node#editReminder :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-reminder :style caption :text ~(t "edit.reminder") :anchors {:left 2 :top 0})
       ~(row-edit-reminder))
     (node#editNotes :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-notes :style caption :text ~(t "edit.notes") :anchors {:left 2 :top 0})
       (node#notesField
       :style field
       :bind state.edit.description
       :label ~(t "edit.notes")
       :placeholder ~(t "edit.notesPh")
       :anchors {:left 0 :right 0 :top 18}))
     (label#label-edit-error :text ~state.edit.error
       :when ~(not= state.edit.error "")
       :font small
       :textColor @c.danger
       :height 16
       :width 400
       :live assertive)))

(defview swatch "A colour of the palette." [sw]
  `(node#swatch :width 32
     :height 32
     :radius 16
     :color ~(get computed.pal sw)
     :focusable true
     :role radio
     :selected ~(= state.edit.color sw)
     :label ~(str (t "edit.color") " " (+ sw 1))
     :cursor hand
     :borderWidth ~(if (= state.edit.color sw) 3 0)
     :borderColor @c.card
     :focused {:borderWidth 3 :borderColor @c.text}
     :onClick ~(handler pickColor sw)
     (label#label-swatch :text ""
       :font iconsSmall
       :icon ~(if (= state.edit.color sw) "check" "")
       :textColor "#ffffff"
       :fill 0
       :align center)))

(defview color-button "The colour's button, and its palette when open." []
  `(node#colorButton :anchors {:right 4 :top 20}
     :width 36
     :height 36
     :radius 18
     :color ~(get computed.pal state.edit.color)
     :focusable true
     :role button
     :cursor hand
     :label ~(t "edit.color")
     :tooltip ~(t "edit.color")
     :isOpen ~state.colorOpen
     :focused {:borderWidth 2 :borderColor @c.text}
     :onClick {:set state.colorOpen :to true}
     (node#popover-color-button :use popover
       :with {:open state.colorOpen
              :anchor "#colorButton"
              :title ~(t "edit.color")
              :width 232
              :height 236
              :placement below-end}
       (grid#swatches :columns 5 :gap 8 :anchors {:left 12 :right 12 :top 38} :height 176 :arrows both
         ~(repeat [sw (range 20)]
           (swatch sw))))))

(defview row-edit-target "A measurable habit's target: at least or at most, and how much." []
  `(row#row-edit-target :anchors {:left 0 :right 0 :top 18} :height 40 :gap 8 :alignItems center
     (node#atLeast
     :style ~(if (= state.edit.targetType "atLeast") "chipOn" "chip")
     :text ~(t "chip.atLeast")
     :selected ~(= state.edit.targetType "atLeast")
     :onClick {:set state.edit.targetType :to atLeast})
     (node#atMost
     :style ~(if (= state.edit.targetType "atMost") "chipOn" "chip")
     :text ~(t "chip.atMost")
     :selected ~(= state.edit.targetType "atMost")
     :onClick {:set state.edit.targetType :to atMost})
     (node#targetField :style field :bind state.edit.target :label ~(t "edit.target") :width 96 :placeholder "10")
     (node#unitField
     :style field
     :bind state.edit.unit
     :label ~(t "edit.unit")
     :width 140
     :placeholder ~(t "edit.unitPh"))))

(defview row-edit-frequency "How often: every day, every n days, n times a week or a month." []
  `(row#row-edit-frequency :anchors {:left 0 :right 0 :top 20}
     :height 32
     :gap 6
     :alignItems center
     :arrows horizontal
     :role radiogroup
     :label ~(t "edit.frequency")
     (node#freq_daily
     :style ~(if (= state.edit.freqMode "daily") "chipOn" "chip")
     :text ~(t "chip.daily")
     :selected ~(= state.edit.freqMode "daily")
     :onClick {:set state.edit.freqMode :to daily}
     :when ~(not= state.edit.kind "measurable"))
     (node#freq_everyN
     :style ~(if (= state.edit.freqMode "everyN") "chipOn" "chip")
     :text ~(t "chip.everyN")
     :selected ~(= state.edit.freqMode "everyN")
     :onClick {:set state.edit.freqMode :to everyN}
     :when ~(not= state.edit.kind "measurable"))
     (node#freq_week
     :style ~(if (= state.edit.freqMode "week") "chipOn" "chip")
     :text ~(t "chip.week")
     :selected ~(= state.edit.freqMode "week")
     :onClick {:set state.edit.freqMode :to week}
     :when ~(not= state.edit.kind "measurable"))
     (node#freq_month
     :style ~(if (= state.edit.freqMode "month") "chipOn" "chip")
     :text ~(t "chip.month")
     :selected ~(= state.edit.freqMode "month")
     :onClick {:set state.edit.freqMode :to month}
     :when ~(not= state.edit.kind "measurable"))
     (node#per_daily
     :style ~(if (= state.edit.freqMode "daily") "chipOn" "chip")
     :text ~(t "chip.perDay")
     :selected ~(= state.edit.freqMode "daily")
     :onClick {:set state.edit.freqMode :to daily}
     :when ~(= state.edit.kind "measurable"))
     (node#per_week
     :style ~(if (= state.edit.freqMode "week") "chipOn" "chip")
     :text ~(t "chip.week")
     :selected ~(= state.edit.freqMode "week")
     :onClick {:set state.edit.freqMode :to week}
     :when ~(= state.edit.kind "measurable"))
     (node#per_month
     :style ~(if (= state.edit.freqMode "month") "chipOn" "chip")
     :text ~(t "chip.month")
     :selected ~(= state.edit.freqMode "month")
     :onClick {:set state.edit.freqMode :to month}
     :when ~(= state.edit.kind "measurable"))
     ~(freq-stepper)))

(defview freq-stepper "The frequency's times, with minus and plus." []
  `(row#freqStepper :when ~(and (not= state.edit.kind "measurable") (not= state.edit.freqMode "daily"))
     :width 112
     :height 32
     :alignItems center
     (node#freqLess
     :style iconButton
     :font iconsSmall
     :icon minus
     :width 32
     :height 32
     :label "−"
     :onClick ~(handler stepTimes -1))
     (label#label-text-edit-n :text
                              ~(text state.edit.n)
                              :textColor
                              @c.text
                              :width
                              40
                              :height
                              20
                              :align
                              center
                              :role
                              status)
     (node#freqMore
     :style iconButton
     :font iconsSmall
     :icon plus
     :width 32
     :height 32
     :label "+"
     :onClick ~(handler stepTimes 1))))

(defview row-edit-reminder "The reminder's time, if any." []
  `(row#row-edit-reminder :anchors {:left 0 :right 0 :top 20} :height 32 :gap 6 :alignItems center
     (node#reminderToggle :use switch
       :with {:on ~state.edit.reminderOn :path state.edit.reminderOn :label ~(t "edit.reminder")})
     (node#timeField
     :style field
     :bind state.edit.time
     :label ~(t "edit.reminder")
     :width 72
     :height 32
     :padding 6
     :disabled ~(not state.edit.reminderOn)
     :placeholder "08:00")
     ~(repeat [wd computed.weekdays]
       `{:id dayChip
         :style ~(if (and (get state.edit.days wd) state.edit.reminderOn) "chipOn" "chip")
         :role checkbox
         :padding 6
         :text ~(formatDate (dateMs (addDays "2000-01-02" wd)) "EEE" "utc")
         :selected ~(get state.edit.days wd)
         :disabled ~(not state.edit.reminderOn)
         :onClick {:toggle ~(str "state.edit.days[" wd "]")}})))

