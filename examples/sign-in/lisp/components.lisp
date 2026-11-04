;;; components.lisp --- Sign in's own components: the checked field, the strength meter, the password rules and the
;;; account page's rows
;;;
;;; A component is used by name (`(node#email :use checkedField :with {...})`); its parameters are names of the running
;;; app inside it, so `~(= state.focus key)` reads the key it was given.

;;; The checked field

(defcomponent checkedField
  "A field with its icon and accessible name, and its error under it once its form was sent."
  ;; The error is also the field's description, read out as an alert; a secret field shows dots and, with eye, a
  ;; button to show them. submit is the function Enter runs, change the one a change runs.
  {:bind ""
   :content ""
   :key ""
   :icon ""
   :fieldName ""
   :placeholder ""
   :valid true
   :message ""
   :submitted false
   :secret false
   :eye false
   :maxLength 0
   :submit ""
   :change ""}
  `(node#panel-checked-field :check {:value ~(or (not submitted) valid) :rule ~value :message ~message}
     :let {:on ~(= state.focus key)}
     :anchors {:left 0 :right 10}
     :height ~(if error 68 44)
     :color transparent
     (node#field-box :style ~(cond on "fieldBoxOn" error "fieldBoxError" :else "fieldBox")
       (label#field-icon :font icons
         :icon ~icon
         :text ""
         :anchors {:left 12 :top 0 :bottom 0}
         :width 20
         :align center
         :textColor ~(if on "@color.accent" "@color.muted"))
       ~(field-input)
       ~(eye-button))
     (node#label-error :when ~error :style inlineError :text ~error :anchors {:left 2 :right 0 :top 50})))

(defview field-input "The checked field's text: it marks the field focused, and runs its submit and change functions."
  []
  `(node#textarea-placeholder :style input
     :anchors {:left 34 :right ~(if eye 44 8) :top 1 :bottom 1}
     :placeholder ~placeholder
     :bind ~bind
     :secret ~(and secret (not state.showPassword))
     :maxLength ~maxLength
     :label ~fieldName
     :description ~(or error "")
     :onSubmit ~(handler runIfAny submit)
     :onFocus ~(handler focusField key)
     :onBlur ~(handler blurField key)
     :onChange ~(handler runIfAny change)))

(defview eye-button "The checked field's eye, which shows or hides every password." []
  `(node#field-eye :when ~eye
     :style iconButton
     :font iconsSmall
     :icon ~(if state.showPassword "eye-off" "eye")
     :anchors {:right 6 :top 6}
     :label ~(if state.showPassword (t "hidePassword") (t "showPassword"))
     :tooltip ~(if state.showPassword (t "hidePassword") (t "showPassword"))
     :onClick togglePassword))

;;; A new password

(defcomponent strengthMeter
  "The passwordMeter with the screen's words and colours, and a hint of what would make the password stronger."
  {:password ""}
  `(node#strength :let {:s ~(strength-of password)} :anchors {:left 0 :right 10} :height 52 :color transparent
     (node#strength-meter :color transparent
       :use passwordMeter
       :with {:password ~password
              :width ~(- (min 420 (- window.width 32)) 72)
              :labels ~(meter-words)
              :track @color.border
              :textColor @color.muted
              :colors [@color.weak @color.weak @color.fair @color.good @color.strong]})
     (node#strength-name :style hint
       :anchors {:left 0 :right 0 :top 32}
       :text ~(cond (= (count password) 0) (t "strengthHint")
                    (= (count s.missing) 0) (t "strengthDone")
                    :else (t "strengthAdd"
                             {:list (formatList (map (fn [it] (t "missingItem" {:what it})) s.missing) "and")})))))

(defview meter-words "The words a password meter shows for each strength." []
  `{:empty ~(t "strengthEmpty") :weak ~(t "weak") :fair ~(t "fair") :good ~(t "good") :strong ~(t "strong")})

(defcomponent passwordRules "What a new password needs, each ticked off as it is met."
  {:password ""}
  `(grid#rules :columns 2
     :rowGap 4
     :columnGap 8
     :height 40
     :color transparent
     :anchors {:left 0 :right 10}
     :let {:m ~(:missing (strength-of password)) :n ~(count password)}
     ~(password-rule "rule-length" "rule8" 8 [])
     ~(password-rule "rule-case" "ruleCase" 1 ["upper" "lower"])
     ~(password-rule "rule-digit" "ruleDigit" 1 ["digit"])
     ~(password-rule "rule-symbol" "ruleSymbol" 1 ["symbol"])))

(defmacro rule-met "A rule is met: the password `n` long has at least `least` characters and misses none of `kinds`."
  [least kinds]
  `(and (>= n ~least) (not (some (fn [kind] (contains m kind)) ~kinds))))

(defview password-rule "One rule of passwordRules: its tick, its text and, for screen readers, whether it is met."
  [id text least kinds]
  `(label :id ~id :font small
     :height 18
     :color transparent
     :icon ~(if (rule-met least kinds) "check-circle" "circle")
     :text ~(t text)
     :textColor ~(if (rule-met least kinds) "@color.text" "@color.muted")
     :label ~(str (t text) ": " (t "ruleState" {:ok (rule-met least kinds)}))))

;;; The account page's rows

(defcomponent sectionHeader "A section's heading and caption."
  {:section ""}
  `(column#column-section-header :fitContent height :gap 2 :anchors {:left 0 :right 10}
     (node#section-title :style heading :text ~(t (str "section_" section)))
     (node#section-caption :style caption :text ~(t (str "caption_" section)))))

(defcomponent itemRow
  "A row of a list: an icon, a title and a hint, then the row's own children (badges, links, buttons)."
  {:icon "" :title "" :hint "" :label ""}
  `(node#row-row :style itemRow :role group :label ~(or label title)
     (node#item-start :width 0 :color transparent)
     (node#item-icon :style itemIcon :icon ~icon)
     (node#item-text :style itemText
       (node#item-title :style itemTitle :text ~title)
       (node#item-hint :style itemHint :text ~hint))
     {:slot true}
     (node#item-end :width 0 :color transparent)))

(defcomponent navItem "One tab of the account's sections: a full-width row when wide, else a chip."
  {:section "" :icon "" :wide true}
  `(node#panel-t-section-name :style ~(str (if (= state.section section) "navItemOn" "navItem") (if wide "" "Chip"))
     :icon ~icon
     :text ~(t (str "section_" section))
     :selected ~(= state.section section)
     :onClick ~(handler setSection section)
     :keys {:enter ~(handler setSection section) :space ~(handler setSection section)}))
