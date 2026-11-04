;;; bars.lisp --- the bar at the top of the frame: the menu bar, then the tool bar at its end
;;;
;;; The menu bar is drawn by the document, on macOS too: File, Edit, Options, Buffers (the buffer list), Tools, a menu
;;; for the selected buffer's major mode, and Help (menus.lisp). F10 opens the first menu, also with the bar hidden;
;;; there are no Alt mnemonics, since M-f is a motion here.

;;; The top bar

(defview top-bar "The bar across the top: the menu bar at its start and the tool bar at its end, each if it is on." []
  `(row#topBar :height 40 :alignSelf stretch :alignItems center :color @bar
     (node#menuBar :use frameMenuBar
       :when ~(or (get settings "menu-bar-mode") (some (fn [it] (= it true)) (vals (or-else state.menuBar {})))))
     (node#topBarGap :grow 1 :height 1 :color transparent)
     (node#tool-bar-root :when ~(get settings "tool-bar-mode") :use toolBar)
     (node#topBarEnd :width 6 :height 1 :color transparent)))

;;; The menu bar

(defcomputed modeMenu
  "The selected buffer's major mode's menu, by name." (get-in state.modes [computed.buf.mode :menu]))

(defcomputed modeTitle "The title of the major mode's menu."
  (or-else (get {:lisp (t "menu.lisp") :text (t "menu.text") :python (t "menu.python") :c (t "menu.c")
                 :outline (t "menu.outline")}
                computed.modeMenu)
           (get-in state.modes [computed.buf.mode :title])))

(defcomputed buffersMenu "The Buffers menu: a buffer an item, the most recent first."
  (map (fn [it] {:title (get-in state.bufs [it :name]) :run "show" :with {:b it}}) state.order))

(defcomputed menuList "The menu bar's menus, in their order: the major mode's menu only while its buffer is selected."
  (concat [{:name "file" :title (t "menu.file") :items menus.file}
           {:name "edit" :title (t "menu.edit") :items menus.edit}
           {:name "options" :title (t "menu.options") :items menus.options}
           {:name "buffers" :title (t "menu.buffers") :items computed.buffersMenu}
           {:name "tools" :title (t "menu.tools") :items menus.tools}]
          (if (and (some? computed.modeMenu) (some? (get menus computed.modeMenu)))
            [{:name "mode" :title computed.modeTitle :items (get menus computed.modeMenu)}]
            [])
          [{:name "help" :title (t "menu.help") :items menus.help}]))

(defcomputed menuBar "The menus, each as wide as its title."
  (map (fn [it] (merge it {:width (+ (round (* (count it.title) 7.2)) 14)})) computed.menuList))

(defcomponent frameMenuBar
  "The menu bar: titles as wide as their text, no Alt mnemonics (M-f is a motion here); F10 opens the first menu."
  {}
  `(row :height 30 :fitContent width :gap 2 :alignItems center :color @bar :role menubar :arrows horizontal
     (node :width 2)
     ~(repeat [m computed.menuBar] (menu-title m))))

(defview menu-title "A menu's title on the bar; a click opens its menu under it." [m]
  `(label :text ~m.title :textColor @barFg :padding 9 :fitText true :height 28 :radius 6 :color @bar
     :focusable true :role menuitem :label ~m.title
     :isOpen ~(= (get state.menuBar m.name) true)
     :hover {:color @barHover} :focused {:color @barHover}
     :onClick {:toggle ~(str "state.menuBar." m.name)}
     (node :use menu :with {:items ~m.items :open ~(str "state.menuBar." m.name) :width 260})))

(defcommand menu-bar-open "Opens the menu bar's first menu."
  :title ~(t "command.menu-bar-open") :category ~(t "category.windows")
  :description ~(t "command.menu-bar-open.about")
  []
  (set "state.menuBar" {:file true}))

;;; The tool bar

(def tools "The tool bar's buttons, a command each, in groups; nil is a separator."
  [{:icon "folder-open" :run "find-file" :tip "openFile"}
   {:icon "folder" :run "dired-home" :tip "listHome"}
   {:icon "x" :run "kill-buffer" :tip "closeBuffer"}
   {:icon "save" :run "save-buffer" :tip "save"}
   nil
   {:icon "undo-2" :run "undo" :tip "undo"}
   nil
   {:icon "scissors" :run "kill-region" :tip "cut"}
   {:icon "copy" :run "kill-ring-save" :tip "copy"}
   {:icon "clipboard-paste" :run "yank" :tip "paste"}
   nil
   {:icon "search" :run "isearch-forward" :tip "search"}])

(defcomponent toolBar "The tool bar: a button a command, with a tip naming it and its key." {}
  `(row :height 34 :fitContent width :color @bar :role toolbar :label ~(t "toolBar.label") :gap 2 :alignItems center
     (node :width 4 :height 1 :color transparent)
     ~@(map (fn [tool] (if tool (tool-button tool) (tool-separator))) tools)))

(defview tool-button "A button of the tool bar: its icon, its tip, and the command a click runs." [tool]
  (let [tip (str "toolBar." (:tip tool))]
    `(node :width 32 :height 30 :radius 6 :color @bar :role button :label ~(t tip) :tooltip ~(t tip)
       :focusable true :disabled false :hover {:color @barHover}
       :onClick ~(:run tool)
       (label :font icons :icon ~(:icon tool) :align center :fill 0 :textColor @icon))))

(defview tool-separator "A thin line between two groups of the tool bar's buttons." []
  `(node :width 9 :height 30 :radius 6 :color @bar :role separator :label "" :tooltip ""
     :focusable false :disabled true :hover {:color @bar}
     (node :x 4 :width 1 :anchors {:top 6 :bottom 6} :color @barLine)))

;;; The frame

(defcommand suspend-frame "Would hide the frame; here it only says so."
  :title ~(t "command.suspend-frame") :category ~(t "category.windows")
  :description ~(t "command.suspend-frame.about")
  []
  (message (t "echo.frameStays")))
