;;; menubar.lisp --- the menu bar across the top of Tempera's window
;;;
;;; Tempera's mark, then one button per menu of menus.lisp; a click opens that menu under it, a tMenu (menu.lisp),
;;; and the arrow keys move along the bar.

;;; The menu bar

(def menuBarMenus "The menu bar's menus, in order: each button opens the menu of that name."
  ["file" "edit" "image" "layer" "type" "select" "filter" "view" "window" "help"])

(defview menu-bar "The menu bar: Tempera's mark, then File to Help." []
  `(row#menu-bar :style bar :color @background :height 24 :gap 2 :arrows horizontal
     (label#app-mark :icon palette :width 32 :height 24 :align center :textColor @accent :tooltip "Tempera")
     ~@(map menu-title menuBarMenus)))

(defview menu-title "A menu's button on the menu bar, outlined while its menu is open under it." [key]
  `(node :id ~(str "menu-" key)
     :role button
     :label ~(t (str "menubar." key))
     :mnemonic ~(t (str "menubar." key))
     :focusable true
     :width ~(+ (* (count (t (str "menubar." key))) 6.5) 18)
     :height 20
     :radius 999
     :color ~(if (get state.menuBar key) "@panel" "transparent")
     :hover {:color @panel}
     :focused {:color @panel}
     :onClick ~(handler toggleOpen (str "state.menuBar." key))
     (label :text ~(t (str "menubar." key)) :textColor @text :anchors {:left 9 :top 3} :fitText true)
     (node :use tMenu :with {:items ~(or-else (get menus key) []) :open ~(str "state.menuBar." key) :width 260})))
