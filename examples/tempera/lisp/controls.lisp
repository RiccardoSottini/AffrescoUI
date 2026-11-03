;;; controls.lisp --- Tempera's small controls: icon buttons, sliders, number fields, pills, checks, choices, pickers
;;;
;;; Each is a component the bars and panels use by name (`(node#option-flow :use tField :with {...})`); its parameters
;;; are names of the running app inside it. A control runs the function it is given (`run`, setOptions by default)
;;; with `args`, the value it changes set at `key` (tempera.lisp, "What the controls run"). A slider's range is
;;; `low` to `high`, since min and max are the language's.

;;; Buttons

(defcomponent tIcon "An icon button with its tooltip: on shows it chosen, off disabled; a click runs run with args."
  {:icon "" :tip "" :run "" :args {} :on false :off false}
  `(label :role button
     :label ~tip
     :tooltip ~tip
     :icon ~icon
     :width 24
     :height 22
     :align center
     :radius @kit.radius
     :cursor hand
     :selected ~on
     :disabled ~off
     :textColor ~(if off "@muted" "@text")
     :color ~(if on "@selected" "transparent")
     :hover {:color ~(if off "transparent" "@hover")}
     :onClick ~(handler runWith run args)))

(defcomponent tButton "A pill with a title; a click runs run with args."
  {:title "" :tip "" :run "" :args {} :off false}
  `(label :role button
     :label ~title
     :tooltip ~tip
     :text ~title
     :width ~(+ (* (count title) 6) 22)
     :height 22
     :align center
     :radius 999
     :color @selected
     :textColor ~(if off "@muted" "@text")
     :disabled ~off
     :cursor hand
     :borderWidth 1
     :borderColor @border
     :hover {:color ~(if off "@selected" "@hover")}
     :onClick ~(handler runWith run args)))

;;; Numbers

(defcomponent tRange "A slider for a number from low to high: dragged or arrowed, it runs run, value / scale at key."
  {:title "" :key "" :value 0 :low 0 :high 100 :scale 1 :run setOptions :args {} :width 120}
  `(node :let {:shown ~(round (* value scale)) :span ~(- high low)}
     :role slider
     :label ~title
     :tooltip ~(str title ": " shown)
     :focusable true
     :width ~width
     :height 14
     :color transparent
     :cursor hand
     :onPress ~(handler pressSlider run args key low high scale)
     :onDrag ~(handler dragSlider run args key low high scale)
     :keys {:left ~(handler stepSlider run args key shown -1 low high scale)
            :right ~(handler stepSlider run args key shown 1 low high scale)}
     :focused {:borderWidth 1 :borderColor @accent}
     (node :anchors {:left 4 :right 4 :top 4} :height 2 :color @border)
     (node :x 4 :y 4 :height 2 :width ~(* (- width 8) (clamp (/ (- shown low) (max 1 span)) 0 1)) :color @muted)
     (path :d "M 0 6 L 4 0 L 8 6 Z"
       :fillColor @text
       :width 8
       :height 6
       :y 7
       :x ~(* (- width 8) (clamp (/ (- shown low) (max 1 span)) 0 1)))))

(defcomponent tField
  "A number's field, its title before it and its unit after; with pop, a chevron opens a slider under it."
  {:title "" :key "" :value 0 :low 0 :high 100 :scale 1 :unit "%" :run setOptions :args {} :width 46 :pop true
   :off false :tip ""}
  `(row :let {:shown ~(round (* value scale)) :where ~(str "state.pop." key run)}
     :width ~(+ (if (empty? title) 0 (+ (* (count title) 6) 10)) width (if pop 14 0))
     :height 22
     :alignItems center
     :gap 4
     :tooltip ~tip
     (node :style dim :when ~(not (empty? title)) :text ~(str title ":"))
     (textarea :label ~title
       :text ~(str shown unit)
       :width ~width
       :height 20
       :padding 3
       :textColor ~(if off "@muted" "@text")
       :color @input
       :borderWidth 1
       :borderColor @border
       :radius @kit.radius
       :disabled ~off
       :onSubmit ~(handler submitField run args key shown low high scale))
     ~(field-slider)))

(defview field-slider "A field's chevron, and the slider it opens under the field." []
  `(label :when ~pop
     :role button
     :label ~(t "control.slider" {:title title})
     :icon chevron-down
     :font small
     :width 10
     :height 20
     :align center
     :textColor @muted
     :cursor hand
     :disabled ~off
     :hover {:textColor @text}
     :onClick ~(handler toggleOpen where)
     (node :overlay {:placement below-end :open ~where :dismiss outside}
       :width 150
       :height 26
       :color @panel
       :radius @kit.cardRadius
       :borderWidth 1
       :borderColor @border
       :shadow {:y 3 :blur 10 :color "#00000060"}
       (node :use tRange
         :x 8
         :y 6
         :with {:title ~title :key ~key :value ~value :low ~low :high ~high :scale ~scale :run ~run :args ~args
                :width 134}))))

;;; Choices

(defcomponent tCheck "A check box with its title: it sets the tool's option key."
  {:title "" :key "" :checked false :off false}
  `(row :width ~(+ (* (count title) 6) 26) :height 24 :alignItems center :gap 5
     (checkbox :label ~title
       :width 13
       :height 13
       :checked ~checked
       :disabled ~off
       :activeColor ~(if off "@muted" "@accent")
       :trackColor @border
       :knobColor @input
       :onChange ~(handler setOption key event.value))
     (label :text ~title :textColor ~(if off "@muted" "@text") :fitText true)))

(defcomponent tChoice "Icon buttons for one of a few values of the tool's option key: tabs are {id icon title}."
  {:tabs [] :value "" :key "" :tip ""}
  `(row :role group :label ~tip :width ~(* (count tabs) 25) :height 22 :gap 1
     ~(repeat [c tabs]
       `(node :use tIcon
          :with {:icon ~c.icon
                 :tip ~c.title
                 :on ~(= c.id value)
                 :run setOptions
                 :args ~(fromEntries [{:key key :value c.id}])}))))

(defcomponent tPicker "A button showing a choice, opening a menu of the others; in a dialog, a choice only closes it."
  {:title "" :tip "" :open "" :items [] :width 64 :inDialog false}
  `(label :role button
     :label ~tip
     :tooltip ~tip
     :text ~title
     :width ~width
     :height 22
     :padding 4
     :radius @kit.radius
     :color @input
     :textColor @text
     :borderWidth 1
     :borderColor @border
     :cursor hand
     :hover {:borderColor @muted}
     :onClick ~(handler toggleOpen open)
     (label :icon chevron-down :font small :textColor @muted :anchors {:right 3 :top 5} :fitText true)
     (node :use tMenu :with {:open ~open :width 200 :items ~items :close ~(if inDialog open "")})))
