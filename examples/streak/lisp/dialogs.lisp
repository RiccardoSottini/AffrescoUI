;;; dialogs.lisp --- what opens over the app: the day's popup, and the dialog that adds or edits a habit

(ns streak.dialogs)

;;; The day's popup
(defview entry-popup "The day's popup: the value or yes, skip, no, and a note." []
  `(node#entryPopup :overlay {:placement window :open state.popup.open :dismiss outside :modal true}
     :let {:ph ~(get data.ordered state.popup.h)
           :pcol ~(get computed.pal ph.color)
           :meas ~(= ph.kind :measurable)
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
     ~@(popup-fields)
     ~(popup-buttons)
     (node#popupSave
       :style textButton
       :when ~meas
       :text ~(upper-case (t "popup.save"))
       :anchors {:right 12 :bottom 10}
       :onClick popup-save)))

(defview popup-fields "The value and its unit, or the note, and a measurable habit's note: four fields." []
  [`(node#popupValue
      :when ~meas
      :style field
      :bind state.popup.value
      :autofocus true
      :label ~(t "popup.value")
      :placeholder ~(t "popup.value")
      :anchors {:left 16 :right 80 :top 44}
      :onSubmit popup-save)
   `(label#label-ph-unit :text ~ph.unit
      :when ~meas
      :textColor @c.muted
      :anchors {:right 16 :top 56}
      :width 56
      :height 18)
   `(node#popupNote
      :when ~(not meas)
      :style field
      :bind state.popup.note
      :autofocus true
      :label ~(t "popup.note")
      :placeholder ~(t "popup.note")
      :anchors {:left 16 :right 16 :top 44}
      :onSubmit popup-note-only)
   `(node#popupMeasNote
      :when ~meas
      :style field
      :bind state.popup.note
      :label ~(t "popup.note")
      :placeholder ~(t "popup.note")
      :anchors {:left 16 :right 16 :top 92}
      :onSubmit popup-save)])

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
       :onClick ~(handler popup-set "y"))
     (node#popupSkip
       :style textButton
       :icon chevrons-right
       :text ~(t "popup.skip")
       :textColor ~pcol
       :mnemonic s
       :onClick ~(handler popup-set "s"))
     (node#popupNo
       :style textButton
       :icon x
       :text ~(t "popup.no")
       :textColor @c.muted
       :mnemonic n
       :onClick ~(handler popup-set "-"))))

(defview edit-fields "The dialog's fields: name, colour, question, unit, target, frequency and reminder." []
  `(column#editFields :anchors {:left 20 :right 20 :top 48 :bottom 56} :gap 6
     ~(edit-name)
     ~(edit-question)
     (node#editTarget :anchors {:left 0 :right 0}
       :height 58
       :color transparent
       :children [(node#label-t-edit-target :style caption :text ~(t "edit.target") :anchors {:left 2 :top 0})
                  ~(row-edit-target)]
       :when ~(= state.edit.kind :measurable))
     (node#editFrequency :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-frequency :style caption :text ~(t "edit.frequency") :anchors {:left 2 :top 0})
       ~(row-edit-frequency))
     (node#editReminder :anchors {:left 0 :right 0} :height 58 :color transparent
       (node#label-t-edit-reminder :style caption :text ~(t "edit.reminder") :anchors {:left 2 :top 0})
       ~(row-edit-reminder))
     ~(edit-notes)
     (label#label-edit-error :text ~state.edit.error
       :when ~(not= state.edit.error "")
       :font small
       :textColor @c.danger
       :height 16
       :width 400
       :live assertive)))

(defview edit-name "The habit's name, and the button choosing its colour." []
  `(node#editName :anchors {:left 0 :right 0} :height 58 :color transparent
     (node#label-t-edit-name :style caption :text ~(t "edit.name") :anchors {:left 2 :top 0})
     (node#nameField
       :style field
       :bind state.edit.name
       :autofocus true
       :label ~(t "edit.name")
       :placeholder ~(t "edit.namePh")
       :anchors {:left 0 :right 52 :top 18}
       :borderColor ~(if (not= state.edit.error "") "@c.danger" "@c.divider"))
     ~(color-button)))

(defview edit-question "The question asked each day." []
  `(node#editQuestion :anchors {:left 0 :right 0} :height 58 :color transparent
     (node#label-t-edit-question :style caption :text ~(t "edit.question") :anchors {:left 2 :top 0})
     (node#questionField
       :style field
       :bind state.edit.question
       :label ~(t "edit.question")
       :placeholder ~(t "edit.questionPh")
       :anchors {:left 0 :right 0 :top 18})))

(defview edit-notes "The habit's notes." []
  `(node#editNotes :anchors {:left 0 :right 0} :height 58 :color transparent
     (node#label-t-edit-notes :style caption :text ~(t "edit.notes") :anchors {:left 2 :top 0})
     (node#notesField
       :style field
       :bind state.edit.description
       :label ~(t "edit.notes")
       :placeholder ~(t "edit.notesPh")
       :anchors {:left 0 :right 0 :top 18})))

(defview swatch "A colour of the palette." [sw]
  `(node#swatch :width 32
     :height 32
     :radius 16
     :color ~(get computed.pal sw)
     :focusable true
     :role radio
     :selected ~(= state.edit.color sw)
     :label ~(str (t "edit.color") " " (inc sw))
     :cursor hand
     :borderWidth ~(if (= state.edit.color sw) 3 0)
     :borderColor @c.card
     :focused {:borderWidth 3 :borderColor @c.text}
     :onClick ~(handler pick-color sw)
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
       :style ~(if (= state.edit.targetType :atLeast) "chipOn" "chip")
       :text ~(t "chip.atLeast")
       :selected ~(= state.edit.targetType :atLeast)
       :onClick {:set state.edit.targetType :to atLeast})
     (node#atMost
       :style ~(if (= state.edit.targetType :atMost) "chipOn" "chip")
       :text ~(t "chip.atMost")
       :selected ~(= state.edit.targetType :atMost)
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
     ~(freq-chip "daily" "chip.daily")
     ~(freq-chip "everyN" "chip.everyN")
     ~(freq-chip "week" "chip.week")
     ~(freq-chip "month" "chip.month")
     ~(per-chip "daily" "chip.perDay")
     ~(per-chip "week" "chip.week")
     ~(per-chip "month" "chip.month")
     ~(freq-stepper)))

(defview freq-chip "A yes-or-no habit's frequency `mode` to choose, its text `key`." [mode key]
  `(node :id ~(str "freq_" mode)
     :style ~(if (= state.edit.freqMode mode) "chipOn" "chip")
     :text ~(t key)
     :selected ~(= state.edit.freqMode mode)
     :onClick {:set state.edit.freqMode :to ~mode}
     :when ~(not= state.edit.kind :measurable)))

(defview per-chip "A measurable habit's period `mode` to choose (a target per day, week or month), its text `key`." [mode key]
  `(node :id ~(str "per_" mode)
     :style ~(if (= state.edit.freqMode mode) "chipOn" "chip")
     :text ~(t key)
     :selected ~(= state.edit.freqMode mode)
     :onClick {:set state.edit.freqMode :to ~mode}
     :when ~(= state.edit.kind :measurable)))

(defview freq-stepper "The frequency's times, with minus and plus." []
  `(row#freqStepper :when ~(and (not= state.edit.kind :measurable) (not= state.edit.freqMode :daily))
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
       :onClick ~(handler step-times -1))
     (label#label-text-edit-n :text ~(text state.edit.n)
       :textColor @c.text
       :width 40
       :height 20
       :align center
       :role status)
     (node#freqMore
       :style iconButton
       :font iconsSmall
       :icon plus
       :width 32
       :height 32
       :label "+"
       :onClick ~(handler step-times 1))))

(defn clock-chip-keys "The size of an hour's or a minute's chip on the reminder's clock." []
  [:width 34 :height 28 :padding 0 :fitText false :align :center])

(defview hour-chip "An hour of the reminder's clock." [hour]
  `(node#hourChip
     :style ~(if (= computed.reminderHour hour) "chipOn" "chip")
     :text ~(pad hour 2)
     :label ~(str (t "edit.hour") " " (pad hour 2))
     :selected ~(= computed.reminderHour hour)
     :onClick ~(handler pick-time :hour hour)
     ~@(clock-chip-keys)))

(defview minute-chip "A minute of the reminder's clock." [minute]
  `(node#minuteChip
     :style ~(if (= computed.reminderMinute minute) "chipOn" "chip")
     :text ~(pad minute 2)
     :label ~(str (t "edit.minute") " " (pad minute 2))
     :selected ~(= computed.reminderMinute minute)
     :onClick ~(handler pick-time :minute minute)
     ~@(clock-chip-keys)))

(defview time-button "The reminder's time, and a clock of hours and minutes to pick it from when open." []
  `(label#timeButton
     :text ~state.edit.time
     :icon clock-3
     :textColor ~(if state.edit.reminderOn "@c.text" "@c.muted")
     :width 84
     :height 32
     :padding 8
     :radius 4
     :color @c.card
     :borderWidth 1
     :borderColor ~(if state.timeOpen computed.accent "@c.divider")
     :focusable true
     :role button
     :cursor hand
     :label ~(str (t "edit.time") " " state.edit.time)
     :tooltip ~(t "edit.time")
     :isOpen ~state.timeOpen
     :focused {:borderWidth 2 :borderColor ~computed.accent}
     :onClick open-clock
     (node#popover-time-button :use popover
       :with {:open state.timeOpen
              :anchor "#timeButton"
              :title ~(t "edit.time")
              :width 476
              :height 172
              :placement below}
       (label#label-t-edit-hour :style caption :text ~(t "edit.hour") :anchors {:left 14 :top 38})
       (grid#hours :columns 12 :gap 4 :anchors {:left 12 :right 12 :top 56} :height 60 :arrows both
         ~(repeat [hour (range 24)]
           (hour-chip hour)))
       (label#label-t-edit-minute :style caption :text ~(t "edit.minute") :anchors {:left 14 :top 120})
       (grid#minutes :columns 12 :gap 4 :anchors {:left 12 :right 12 :top 138} :height 28 :arrows both
         ~(repeat [minute (range 0 60 5)]
           (minute-chip minute))))))

(defview row-edit-reminder "The reminder's time, if any." []
  `(row#row-edit-reminder :anchors {:left 0 :right 0 :top 20} :height 32 :gap 6 :alignItems center
     (node#reminderToggle :use switch
       :with {:on ~state.edit.reminderOn :path state.edit.reminderOn :label ~(t "edit.reminder")})
     ~(time-button)
     ~(repeat [wd computed.weekdays]
       `(node#dayChip
          :style ~(if (and (get state.edit.days wd) state.edit.reminderOn) "chipOn" "chip")
          :role checkbox
          :padding 6
          :text ~(formatDate (dateMs (addDays "2000-01-02" wd)) "EEE" "utc")
          :selected ~(get state.edit.days wd)
          :onClick ~(handler tap-reminder-day wd)))))

