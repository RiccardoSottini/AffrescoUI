;;; ribbon.lisp --- The ribbon's parts: large buttons (an icon over a one- or two-line title), small ones (an icon, a title, a menu arrow), icon-only ones, drop-down fields, galleries, and groups, each a card with its title along the top
;;;
;;; Each tab is a component of groups; the screen shows the chosen one. A button is a label, so the use gives it its
;;; onClick, tooltip and menu.

;;; Definitions

(defcomponent rbBig "With drop (the button's own id), a split button: the arrow under the title (id: drop + '-more') opens that menu, the rest runs the use's onClick."
  {:icon "" :title "" :menu false :on false :enabled true :width 48 :drop ""}
  `(column :width ~width :height 70 :color transparent :disabled ~(not enabled)
     (label :width ~width
       :height ~(if (= drop "") 70 52)
       :padding 2
       :wrap true
       :style ~(cond (not enabled) "rbButtonOff" on "rbButtonOn" :else "rbButton")
       :align {:horizontal center :vertical start}
       :runs ~(concat [{:icon icon :size 30}
                       {:text "\n" :size 6}
                       {:text (if (= drop "") title (join " " (split title "\n"))) :size (if (= drop "") 12 11)}]
                      (if (and menu (= drop "")) [{:text " " :size 12} {:icon "chevron-down" :size 10}] [])))
     (label :id ~(str drop "-more")
       :when ~(not= drop "")
       :width ~width
       :height 18
       :align center
       :disabled ~(not enabled)
       :tooltip "More options"
       :style ~(if (not enabled) "rbButtonOff" "rbButton")
       :runs [{:icon chevron-down :size 10}]
       :onClick ~(handler openDrop drop))))

(defcomponent rbSmall "With drop (the button's own id), a split button: its arrow (id: drop + '-arrow') opens that menu (or, with colors, the colour grid), the rest runs the use's onClick."
  {:icon "" :title "" :menu false :on false :enabled true :tint nil :drop "" :colors false}
  `(column :fitContent width :height 24 :color transparent :disabled ~(not enabled)
     ~(rb-small-row icon title menu on enabled tint drop colors)
     {:width 16 :height 3 :x 4 :color ~(or-else (get computed.palette tint) tint "transparent") :when ~(some? tint)}))

(defcomponent rbField "A drop-down field: a click anywhere on it opens the menu named drop (the field's own id)."
  {:text "" :width 100 :drop ""}
  `(row :width ~width
     :height 22
     :role button
     :label ~text
     :onClick ~(handler openDrop drop)
     :color @rbFieldBg
     :borderWidth 1
     :borderColor @rbFieldLine
     :radius 11
     :cursor hand
     :hover {:borderColor @rbFieldHover}
     :alignItems center
     (label :grow 1 :height 20 :padding 7 :textColor @rbInk :runs [{:text ~text :size 12}])
     (label :width 16 :height 20 :align center :textColor @rbInk :runs [{:icon chevron-down :size 11}])))

(defcomponent rbGroup "A group is a card: its title in small capitals along the top, the launcher at the title's end (it opens the format pane), its commands below."
  {:title "" :launcher false}
  `(column :fitContent width :height 92 :radius 10 :color @card :borderWidth 1 :borderColor @rbLine :alignItems stretch
     ~(rb-group-row title launcher)
     (row :fitContent width :height 72 :gap 2 :color transparent :alignItems start
       {:width 3 :height 1 :color transparent}
       (row :fitContent width :height 72 :gap 2 :color transparent :alignItems start
         {:slot true})
       {:width 3 :height 1 :color transparent})))

(defcomponent rbStack "Small buttons in a column, three to a group."
  {}
  `(column :fitContent width :height 72 :gap 0 :color transparent :justify space-evenly
     {:slot true}))

(defcomponent rbRow "Small buttons in a row."
  {}
  `(row :fitContent width :height 24 :gap 1 :color transparent :alignItems center
     {:slot true}))

(defcomponent rbGallery "A ribbon gallery: one row of cells (the use's children). Every cell is shown, so the arrows that scroll are off; the last opens the whole list as a menu (more, the gallery's own id), when there is one."
  {:width 200 :more ""}
  `(row :width ~width :height 70 :color @rbFieldBg :borderWidth 1 :borderColor @rbFieldLine :radius 10
     {:width 4 :height 68 :color transparent}
     (row :grow 1 :height 68 :gap 2 :color transparent :alignItems center :overflow clip
       {:slot true})
     {:width 2 :height 68 :color transparent}
     ~(rb-gallery-column width more)))

;;; Views

(defview rb-small-row "A small button's row: its icon and title, and the arrow of its menu." [icon title menu on enabled tint drop colors]
  `(row :fitContent width :height ~(if (nil? tint) 24 21) :color transparent
     (label :height ~(if (nil? tint) 24 21)
       :fitText true
       :padding 4
       :style ~(cond (not enabled) "rbButtonOff" on "rbButtonOn" :else "rbButton")
       :runs ~(concat [{:icon icon :size 16}]
                      (if (= title "") [] [{:text (str " " title) :size 12}])
                      (if (and menu (= drop "")) [{:text " " :size 12} {:icon "chevron-down" :size 10}] [])))
     (label :id ~(str drop "-arrow")
       :when ~(not= drop "")
       :width 13
       :height ~(if (nil? tint) 24 21)
       :align center
       :disabled ~(not enabled)
       :tooltip "More options"
       :style ~(if (not enabled) "rbButtonOff" "rbButton")
       :runs [{:icon chevron-down :size 10}]
       :onClick ~(handler openMenu colors drop))))

(defview rb-group-row "A group's caption and, if it has one, its launcher." [title launcher]
  `(row :fitContent width :height 18 :color transparent :alignItems center :gap 4
     {:width 6 :height 1 :color transparent}
     (label :style rbGroupTitle :fitText true :runs [{:text ~(upper-case title) :size 9 :bold true}])
     {:grow 1 :height 1 :color transparent}
     (label :id ~(str "launch-" (join "-" (split (lower-case title) " ")))
       :when ~launcher
       :width 16
       :height 16
       :style rbButton
       :align center
       :tooltip ~(str "More " (lower-case title) " options")
       :role button
       :label ~(str title " options")
       :runs [{:icon settings-2 :size 10}]
       :onClick openFormatPane)
     {:width 4 :height 1 :color transparent}))

(defview rb-gallery-column "A gallery's arrows: up, down, and every item." [width more]
  `(column :width 16 :height 68 :color transparent
     (label :width 16
       :height 22
       :style rbButtonOff
       :align center
       :tooltip "Every item is shown"
       :runs [{:icon chevron-up :size 10}])
     (label :width 16
       :height 22
       :style rbButtonOff
       :align center
       :tooltip "Every item is shown"
       :runs [{:icon chevron-down :size 10}])
     (label :id ~(str more "-all")
       :width 16
       :height 22
       :style ~(if (= more "") "rbButtonOff" "rbButton")
       :align center
       :disabled ~(= more "")
       :tooltip ~(if (= more "") "Every item is shown" "The whole list")
       :runs [{:icon chevrons-down :size 10}]
       :onClick ~(handler openDrop more))))

;;; What the ribbon's parts do

(defn openMenu "Opens a button's drop-down, or its colour grid." [colors id]
  (if colors (openColors id) (openDrop id)))
