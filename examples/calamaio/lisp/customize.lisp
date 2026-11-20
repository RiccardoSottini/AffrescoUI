;;; customize.lisp --- the *Customize* buffer: every variable with its value, its state and a way back to its default
;;;
;;; A variable's row gives its title and name, what it does, a control for its value (a check box, a choice, a number
;;; or a text) and its state: STANDARD while it has its default, SAVED and set once it is changed, since Calamaio keeps
;;; a changed value for the next session. Reset gives it back its default. The search at the top narrows the rows.

(ns calamaio.customize)

;;; The rows shown

(defcomputed customItems "The variables Customize shows: those whose title, name or description holds the search."
  (filter #(str/includes? (str/lower-case (str (:title %) " " (:key %) " " (:description %)))
                          (str/lower-case state.customQuery))
          settingsList))

;;; The page

(defview customize-page "The *Customize* buffer: a title, the search, then a row for each variable." []
  `(column#customize :anchors {:left 14 :top 10 :right 14 :bottom 10} :color @bg :gap 10
     (row :alignSelf stretch :alignItems center :gap 12 :height 30 :color transparent
       (label :fitText true :textColor @fg :runs [{:text ~(t "customize.title") :bold true :size 16}])
       (node :grow 1 :height 1 :color transparent)
       (textarea#customSearch :width 240 :height 28 :padding 6 :radius 4 :bind state.customQuery
         :placeholder ~(t "customize.search") :color @field :textColor @fg :placeholderColor @kit.dim))
     (node :alignSelf stretch :height 1 :color @barLine)
     (column :grow 1 :width "100%" :scroll vertical :gap 0 :color transparent
       ~(repeat [s computed.customItems :key s.key] (custom-row s))
       (label :when ~(empty? computed.customItems) :fitText true :textColor @kit.dim :text ~(t "customize.none")))))

(defview custom-row
  "A variable's row: its title and name, what it does, its state and Reset, and the control for its value." [s]
  `(node :width "100%" :height 78 :color transparent
     (node :anchors {:left 0 :top 8 :bottom 8} :width 3 :radius 2 :color ~(if s.modified "@link" "transparent"))
     (label :anchors {:left 14 :top 6} :fitText true :textColor @fg
       :runs [{:text ~s.title :bold true} {:text ~(str "   " s.key) :font code :color @kit.dim}])
     (label :anchors {:left 14 :right 300 :top 30} :height 18 :textColor @kit.dim :text ~s.description)
     (row :anchors {:left 14 :top 52} :height 18 :gap 12 :alignItems center :fitContent true :color transparent
       (label :fitText true :textColor ~(if s.modified "@link" "@kit.dim")
         :text ~(if s.modified (t "customize.saved") (t "customize.standard")))
       (label :when ~s.modified :fitText true :textColor @link :cursor hand :role button
         :label ~(t "customize.reset") :runs [{:text ~(t "customize.reset") :underline true}]
         :onClick ~(handler reset-setting s.key)))
     (node :anchors {:right 8 :top 22} :width 280 :height 30 :color transparent ~(custom-control s))
     (node :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @line)))

(defview custom-control "The control for a variable's value: a check box, a choice of its values, a number or a text." [s]
  `(row :anchors {:left 0 :top 0 :right 0 :bottom 0} :alignItems center :justify end :gap 6 :color transparent
     (checkbox :when ~(= s.type "boolean") :width 18 :height 18 :bind ~(str "settings." s.key) :label ~s.title
       :activeColor @link)
     (row :when ~(some? s.enum) :gap 4 :fitContent true :color transparent
       ~(repeat [o s.enum]
         `(label :fitText true :padding 6 :radius 4 :cursor hand :role button :label ~o :text ~o
            :color ~(if (= s.value o) "@link" "@button")
            :textColor ~(if (= s.value o) "@bg" "@barFg")
            :hover {:color ~(if (= s.value o) "@link" "@barHover")}
            :onClick ~(handler set-setting s.key o))))
     (slider :when ~(and (= s.type "integer") (some? s.maximum)) :width 160 :height 20
       :bind ~(str "settings." s.key) :min ~s.minimum :max ~s.maximum :step 1 :activeColor @link)
     (label :when ~(= s.type "integer") :fitText true :font code :textColor @fg :text ~(str s.value))
     (textarea :when ~(and (= s.type "string") (nil? s.enum)) :width 160 :height 26 :padding 4 :radius 4
       :bind ~(str "settings." s.key) :color @field :textColor @fg)))

;;; What its controls do

(defn set-setting "Gives a variable a value." [k value]
  (set (str "settings." k) value))

(defn reset-setting "Gives a variable back its default." [k]
  (remove-state (str "settings." k))
  (message (t "echo.reset" {:name k})))
