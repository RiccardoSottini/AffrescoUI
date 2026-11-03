;;; overlays.lisp --- the window's other parts: the toolbar, the status bar, the menus, the pickers and the pages
;;;
;;; index.screen.lisp lists these in the order they are drawn. A view takes what differs between its uses as
;;; arguments: a quoted name of the running app ('menus.card) is read while the app runs, as a binding.

;;; The toolbar and the status bar

(defview toolbar "The toolbar under the top bar: the filters and the search, and the Display button with its menu." []
  `(node#toolbar :dir auto :anchors {:left 220 :right 0 :top "#topBar.bottom"} :height 44 :color @color.surface
     ~(filter-bar)
     (node#displayButton :style ~(if state.displayOpen "toolButtonOn" "toolButton")
       :anchors {:right 12 :top 8}
       :icon sliders-horizontal
       :text ~(str " " (t "display.button"))
       :label ~(t "display.button")
       :tooltip ~(t "display.button")
       :isOpen ~state.displayOpen
       :keys {:enter {:toggle state.displayOpen} :space {:toggle state.displayOpen}}
       :onClick {:toggle state.displayOpen}
       ~(display-menu))))

(defview status-bar "The status bar: the revision shown, and the window's size." []
  `(node#statusBar :use statusBar
     :anchors {:left 220 :right 0 :bottom 0}
     :with {:height 32
            :color @color.surface
            :textColor @color.muted
            :highlight @color.chipHover
            :items [{:text ~(t "status.revision"
                               {:number revision.number :author revision.author :message revision.message})}
                    {:text ~(str window.width " × " window.height) :side end}]}))

;;; Menus and pickers

(defview context-menu "A menu at the pointer: open while the state at the path `open` is true, its items `items`."
  [id open items]
  `(node :id ~id :use contextMenu
     :with {:open ~open
            :items ~items
            :x ~(min state.menuAt.x (- window.width 228))
            :y ~state.menuAt.y
            :width 220
            :color @color.menu
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accentSoft}))

(defview command-palette "The palette (Cmd+Shift+P), with the cards after # (Cmd+P)." []
  `(node#quickPick :use quickPick
     :with {:color @color.menu
            :textColor @color.text
            :dimColor @color.muted
            :accent @color.accent
            :highlight @color.accentSoft
            :field @color.field
            :providers [{:items ~(concat computed.palette computed.picks) :placeholder ~(t "quick.commands")}
                        {:prefix "#" :items ~computed.picks :placeholder ~(t "quick.cards")}]}))

(defview issue-picker "The cards, to go to one." []
  `(node#issuePick :use quickPick
     :with {:name issuePick
            :color @color.menu
            :textColor @color.text
            :dimColor @color.muted
            :accent @color.accent
            :highlight @color.accentSoft
            :field @color.field
            :providers [{:items ~computed.picks :placeholder ~(t "quick.cards")}]}))

(defview property-picker "The status, priority or assignee of the chosen card, or of every marked one (S, P, A)." []
  `(node#propPick :use quickPick
     :with {:name propPick
            :width 360
            :recentLimit 0
            :color @color.menu
            :textColor @color.text
            :dimColor @color.muted
            :accent @color.accent
            :highlight @color.accentSoft
            :field @color.field
            :providers [{:items ~computed.propItems :placeholder ~(t (str "quick." (or-else state.propKind "status")))}]}))

(defview toast "A notice in the corner, with Undo when what sent it can be taken back." []
  `(node#toast :use toast
     :with {:open state.toast.open
            :message ~state.toast.message
            :width 320
            :actions ~(if state.toast.undo [{:title (t "toast.undo") :run "undoChange"}] [])
            :closeLabel ~(t "toast.close")
            :color @color.menu
            :textColor @color.text
            :accent @color.accent}))

;;; Over the work area

(defview composer "The composer over the window, behind a backdrop: Cmd+N or a column's +." []
  `(node#composer :overlay {:placement window :open state.composer.open :dismiss outside :modal true}
     :fill true
     :color @color.backdrop
     ~(composer-box)))

(defview selection-bar "The bar at the bottom while cards are marked: their count and what to do with them." []
  `(node#bulkBar :when ~(and (= state.page "issues") (> (count state.marked) 0))
     :x ~(+ 228 (round (/ (- (- window.width 236) 340) 2)))
     :anchors {:bottom 56}
     :width 340
     :height 40
     :radius 8
     :color @color.menu
     :borderWidth 1
     :borderColor @color.border
     :shadow {:y 6 :blur 18 :color "#00000030"}
     :role toolbar
     :label ~(t "bulk.count" {:n (count state.marked)})
     ~(bulk-actions)))

;;; The pages

(defview page "A page beside the sidebar, while it is the one chosen: the component of pages.lisp named as its id."
  [id page]
  `(node :id ~id :use ~id
     :when ~(= state.page page)
     :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"}))

(defview issue-page "The chosen card as a page over the work area (Cmd+Enter, or a double-click)." []
  `(node#issuePage :use issuePage
     :when ~(and state.issueOpen (some? computed.card))
     :anchors {:left 220 :right 0 :top 0 :bottom "#statusBar.top"}))
