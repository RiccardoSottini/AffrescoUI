;;; mine.lisp --- the My apps page: the apps the person made, a row each
;;;
;;; A row opens its app; its buttons open it too, duplicate it, show it in the folder, or delete it after a question.
;;; Delete on a row asks the same. With no app yet, the page says how to make one.

;;; The page

(defview mine-page "My apps: its title, its buttons, a note if the folder can't be read, and the rows." []
  `(node#minePage :when ~(= state.page "mine") :fill true :color transparent :role tabpanel :label ~(t "nav.mine")
     (node#mineTitle :style pageTitle :text ~(t "mine.title") :role heading)
     (node#mineSub :style pageSub :text ~(t "mine.sub") :anchors {:left 36 :right 360 :top 72})
     (row#mineButtons :gap 8 :anchors {:right 36 :top 38} :height 38 :width 330 :justify end
       (node#mineFolder :style secondary
         :icon folder-open
         :text ~(t "mine.showFolder")
         :width 150
         :height 38
         :onClick ~(handler revealMine ""))
       (node#mineNew :style primary :icon plus :text ~(t "nav.newApp") :width 130 :height 38 :onClick showCreate))
     (label#mineUnavailable :when ~(not= state.mineError "")
       :font small
       :wrap true
       :text ~(t "mine.unavailable")
       :textColor @c.muted
       :color @c.raised
       :radius 10
       :padding 10
       :anchors {:left 36 :right 36 :top 108}
       :height 40
       :role status)
     (column#mineList :when ~(> (count state.mine) 0)
       :anchors {:left 36 :right 36 :top ~(if (not= state.mineError "") 160 116) :bottom 0}
       :gap 10
       :scroll vertical
       :role list
       :label ~(t "nav.mine")
       :arrows vertical
       ~(repeat [app state.mine :index i :key app.id]
         `(node#mineRow :use mineRow :with {:app ~app :index ~i}))
       (node#mineListEnd :height 12 :color transparent :anchors {:left 0 :right 0}))
     ~(mine-empty)))

(defview mine-empty "With no app yet: a picture, what the page is for, and a button that makes one." []
  `(node#mineEmpty :when ~(= (count state.mine) 0)
     :anchors {:top ~(if (not= state.mineError "") 168 124)}
     :x ~(max 36 (/ (- (- window.width computed.sideWidth) 460) 2))
     :width 460
     :height 300
     :color transparent
     (label#mineEmptyArt :icon sparkles
       :font heroIcons
       :align center
       :textColor @c.accent
       :color @c.accentSoft
       :radius 999
       :anchors {:top 20}
       :x 190
       :width 80
       :height 80)
     (label#mineEmptyTitle :font heading :text ~(t "mine.emptyTitle") :textColor @c.text :align center
       :anchors {:left 0 :right 0 :top 120} :height 26)
     (label#mineEmptyText :text ~(t "mine.emptyText") :textColor @c.muted :align center :wrap true
       :anchors {:left 0 :right 0 :top 152} :height 44)
     (node#mineEmptyCreate :style primary
       :icon plus
       :text ~(t "mine.emptyButton")
       :anchors {:top 212}
       :x 120
       :width 220
       :height 42
       :onClick showCreate)))

;;; A row

(defcomponent mineRow "An app of the person's: its initial in a colour, its name, its folder and date, and its buttons."
  {:app {} :index 0}
  `(node#mineRowPanel :style card
     :height 76
     :anchors {:left 0 :right 0}
     :focusable true
     :role button
     :cursor hand
     :label ~(t "mine.openLabel" {:name app.name})
     :hover {:borderColor @c.accent}
     :focused {:borderColor @c.accent :borderWidth 2}
     :onClick ~(handler openApp app)
     :keys {:delete ~(handler askDelete app.id app.name)}
     (label#mineInitial :text ~(upper-case (slice app.name 0 1))
       :font initial
       :align center
       :textColor "#ffffff"
       :color ~(get state.tileColors (mod index 6))
       :radius 12
       :anchors {:left 16 :top 16}
       :width 44
       :height 44)
     (label#mineName :text ~app.name :font strong :textColor @c.text :anchors {:left 76 :right 220 :top 17} :height 22)
     (label#mineInfo :font small
       :textColor @c.muted
       :text ~(if (> app.modified 0) (t "mine.info" {:folder app.id :date (formatDate app.modified "medium")}) app.id)
       :anchors {:left 76 :right 220 :top 41}
       :height 18)
     ~(mine-actions app)))

(defview mine-actions "A row's buttons: open, duplicate, show in the folder, delete." [app]
  `(row#mineActions :gap 4 :anchors {:right 14 :top 21} :height 34 :width 210 :justify end
     (node#mineOpen :style primary :text ~(t "mine.open") :width 74 :height 34 :onClick ~(handler openApp app))
     (node#mineDuplicate :style iconButton
       :icon copy
       :tooltip ~(t "mine.duplicate")
       :label ~(t "mine.duplicateLabel" {:name app.name})
       :onClick ~(handler duplicateMine app.id app.name))
     (node#mineReveal :style iconButton
       :icon folder-open
       :tooltip ~(t "mine.reveal")
       :label ~(t "mine.revealLabel" {:name app.name})
       :onClick ~(handler revealMine app.id))
     (node#mineDelete :style iconButton
       :icon trash-2
       :tooltip ~(t "mine.delete")
       :label ~(t "mine.deleteLabel" {:name app.name})
       :hover {:color @c.dangerSoft :textColor @c.danger}
       :focused {:color @c.dangerSoft :textColor @c.danger}
       :onClick ~(handler askDelete app.id app.name))))

;;; The question before a delete

(defpart deleteQuestion "The question before an app is deleted, and the notes at the bottom: over the window."
  :overlays {:confirmDelete (node#confirmDeleteDialog :use dialog
                              :with {:open state.confirmOpen
                                     :title ~(t "delete.title" {:name state.confirm.name})
                                     :message ~(t "delete.message" {:folder state.confirm.id})
                                     :width 460
                                     :height 180
                                     :highlight @c.accent
                                     :buttons [{:title ~(t "delete.confirm") :run deleteMine}
                                               {:title ~(t "delete.cancel") :default true}]})
             :launcherToasts (node#launcherToastList :use toasts :with {:name toasts})})
