;;; menu.lisp --- Tempera's pop-up menu: its rows, its submenus, and the menu itself
;;;
;;; tMenu draws a list of items (the menu bar's menus, a picker's choices, a tool group's other tools) over the
;;; window; an item is a map with any of title, icon, keyLabel, checked, enabled, separator, submenu, run and with. A
;;; row chosen runs its item (tempera.lisp, chooseMenuItem); one with a submenu opens it to the right, a tSubmenu.

;;; The rows

(defmacro menu-rows "The items with every key a row reads, those an item leaves out at their default." [items]
  `(map (fn [it]
          (merge {:separator false :checked nil :enabled true :icon nil :keyLabel nil :submenu nil :run nil :with nil
                  :title ""}
                 it))
        ~items))

(defcomponent tMenuRow "One row of a menu: a separator, or an item with its mark, icon, title and key."
  {:item {} :path "" :mark check :indent 26 :close ""}
  `(node :anchors {:left 4 :right 4 :top 0 :bottom 0}
     :radius @kit.radius
     :role ~(if item.separator "separator" "menuitem")
     :label ~(or-else item.title "")
     :focusable ~(not item.separator)
     :disabled ~(or item.separator (= item.enabled false))
     :mnemonic ~(or-else item.title "")
     :color transparent
     :tooltip ~(if (= item.run "unavailable") (t "command.unavailable") "")
     :selected ~(if (nil? item.checked) nil (= item.checked true))
     :hover {:color ~(if (or item.separator (= item.enabled false)) "transparent" "@menuHover")}
     :focused {:color @menuHover}
     :onClick ~(handler chooseMenuItem item path close)
     (node :when ~item.separator :anchors {:left 0 :right 0 :top 3} :height 1 :color @border)
     (node :when ~(and (= item.checked true) (= mark "square")) :x 8 :y 8 :width 5 :height 5 :color @text)
     ~@(menu-row-labels)))

(defview menu-row-labels "A row's icon or check mark, its title, and its key or a submenu's chevron." []
  [`(label :when ~(and (not item.separator)
                        (or (and (= item.checked true) (= mark "check")) (and (> indent 26) (some? item.icon))))
       :icon ~(if (and (= item.checked true) (= mark "check")) "check" item.icon)
       :textColor ~(if (= item.enabled false) "@muted" "@text")
       :anchors {:left ~(if (= mark "square") 20 7) :top 4}
       :fitText true)
   `(label :when ~(not item.separator)
       :text ~(or-else item.title "")
       :textColor ~(if (= item.enabled false) "@muted" "@text")
       :anchors {:left ~indent :top 4}
       :fitText true)
   `(label :when ~(not item.separator)
       :icon ~(if (some? item.submenu) "chevron-right" "")
       :text ~(if (some? item.submenu) "" (join "↑" (split (join "↓" (split (or-else item.keyLabel "") "Down")) "Up")))
       :textColor @muted
       :anchors {:right 10 :top 4}
       :fitText true)])

;;; Menus

(defview menu-box "What a menu and a submenu share: a card over the window, as tall as its rows, and the rows." [placement content]
  `(column :let {:rows ~(menu-rows items)}
     :overlay {:placement ~placement :open ~open :dismiss outside :modal true}
     :role menu
     :arrows vertical
     :width ~width
     :height ~(+ (sum rows (fn [it] (if it.separator 7 22))) 10)
     :color @panel
     :radius @kit.cardRadius
     :borderWidth 1
     :borderColor @border
     :shadow {:y 4 :blur 14 :color "#00000060"}
     (node :height 5 :width 1 :style clear)
     ~content))

(defcomponent tSubmenu "A submenu, opened to the right of its row." {:items [] :open "" :width 220}
  (menu-box "right"
    (repeat [item rows]
      `(node :use tMenuRow :width "100%" :height ~(if item.separator 7 22) :with {:item ~item}))))

(defcomponent tMenu "A menu of items; mark is check or square for a checked item, and icons leaves room for icons."
  {:items [] :open state.menuOpen :placement below :width 240 :mark check :icons false :close ""}
  (menu-box placement
    (repeat [item rows :index i]
      `(node :style clear :width "100%" :height ~(if item.separator 7 22)
         (node :use tMenuRow
           :with {:item ~item
                  :path ~(str open "Sub.m" i)
                  :mark ~mark
                  :indent ~(if (or icons (= mark "square")) 40 26)
                  :close ~close})
         (node :use tSubmenu
           :when ~(some? item.submenu)
           :with {:items ~item.submenu :open ~(str open "Sub.m" i) :width ~width})))))
