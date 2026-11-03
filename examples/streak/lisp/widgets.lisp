;;; widgets.lisp --- the pieces the pages share: the score ring, a period's button, settings rows, a number of the
;;; overview, and switches

;;; Widgets
(defcomponent ring "A score ring: value from 0 to 1 drawn clockwise from the top over a full track."
  {:value 0 :color "#888888" :size 22 :stroke 3}
  `(node#panel-ring :width ~size :height ~size :color transparent :role img :label ~(formatPercent value)
     (path#path-ring :fill
                     0
                     :viewBox
                     "0 0 24 24"
                     :d
                     "M 12 3 A 9 9 0 1 1 11.99 3"
                     :stroke
                     @c.track
                     :strokeWidth
                     ~stroke)
     (path#path-ring-2 :fill 0
       :viewBox "0 0 24 24"
       :stroke ~color
       :strokeWidth ~stroke
       :d ~(cond (>= value 0.999) "M 12 3 A 9 9 0 1 1 11.99 3"
                 (<= value 0.005) "M 12 3"
                 :else (str "M 12 3 A 9 9 0 "
                      (if (> value 0.5) 1 0)
                      " 1 "
                      (fixed (+ 12 (* 9 (sin (* value 6.28319)))) 2)
                      " "
                      (fixed (- 12 (* 9 (cos (* value 6.28319)))) 2))))))

(defcomponent periodButton "A chart's period: a text button that opens a menu of the five periods, kept in state[key]."
  {:key scorePeriod :color "#888888"}
  `(label#label-text-button :style textButton
     :font small
     :textColor ~color
     :runs [~(str (upper-case (t (str "period." (get state key)))) " ") {:icon chevron-down}]
     :label ~(str (t "period") ": " (t (str "period." (get state key))))
     :isOpen ~(get state.menus (if (= key "scorePeriod") "score" "history"))
     :onClick ~(handler openPeriodMenu key)
     (node#menu-text-button :use menu
       :with {:open ~(str "state.menus." (if (= key "scorePeriod") "score" "history"))
              :width 160
              :placement below-end
              :items ~(map (fn [it]
                             {:title (t (str "period." it))
                              :checked (= (get state key) it)
                              :run "setPeriod"
                              :with {:key key :value it}})
                           ["day" "week" "month" "quarter" "year"])})))

(defcomponent prefRow "A settings row: a title, a summary under it, and its control (the children) at the end."
  {:title "" :summary ""}
  `(node#panel-pref-row :anchors {:left 0 :right 0}
     :height ~(if (= summary "") 48 64)
     :color transparent
     :focusable true
     :role button
     :cursor hand
     :label ~title
     :description ~summary
     :hover {:color @c.hover}
     :focused {:color @c.hover}
     (label#label-title :text
                        ~title
                        :textColor
                        @c.text
                        :anchors
                        {:left 16 :top ~(if (= summary "") 14 12)}
                        :fitText
                        true)
     (label#label-summary :when ~(not= summary "")
       :text ~summary
       :font small
       :textColor @c.muted
       :anchors {:left 16 :top 34}
       :fitText true)
     {:slot true}))

(defcomponent stat "One number of the overview with its caption under it."
  {:value "" :caption "" :color @c.text}
  `(node#panel-stat :width 120 :height 56 :color transparent :role text :label ~(str caption ": " value)
     (label#label-value :text ~value
       :font title
       :textColor ~color
       :anchors {:left 0 :right 0 :top 4}
       :height 26
       :align center)
     (label#label-caption :text ~caption
       :font small
       :textColor @c.muted
       :anchors {:left 0 :right 0 :top 32}
       :height 16
       :align center)))

(defcomponent switch "A switch: a rounded track and a round knob that slides to the end when on; a click toggles path. Its colours are the habit's unless track and knob name others."
  {:on false :path state.reminders :label "" :enabled true :track "" :knob ""}
  `(node#panel-switch :width 40
     :height 24
     :color transparent
     :focusable true
     :role switch
     :label ~label
     :selected ~on
     :cursor hand
     :disabled ~(not enabled)
     :onClick {:toggle ~path}
     (node#panel-switch-2
     :anchors {:left 3 :right 3 :top 5}
     :height 14
     :radius 7
     :color ~(if (and on enabled) (if (not= track "") track (str computed.accent "80")) "@c.faint"))
     (node#panel-switch-3
     :anchors {:left ~(if on 18 0) :top 1}
     :width 22
     :height 22
     :radius 11
     :color ~(if (and on enabled) (if (not= knob "") knob computed.accent) "@c.knob")
     :shadow {:y 1 :blur 3 :color "#00000050"}
     :focused {:borderWidth 2 :borderColor @c.text})))

(defcomponent toggleRow "A settings row with a switch bound to a state path."
  {:title "" :summary "" :bind state.reminders :on false}
  `(node#pref-row-row :use prefRow :onClick {:toggle ~bind}
     (node#switch-pref-row-row :use switch
       :with {:on ~on :path ~bind :label ~title :track @c.switchTrack :knob @c.primary}
       :anchors {:right 16 :top ~(if (= summary "") 12 20)})))

