;;; sorter.lisp --- the slide sorter: thumbnails of an ordered list of pages, drag to reorder, click to select
;;;
;;; Written once, for Sinopia's slides, and installed into any app that offers its needs (feature.lisp): it reads
;;; the app only as app.pages, app.current and app.title, and changes it only through the actions app.select, app.reorder and
;;; app.duplicate. Its own state is the page picked last, the selection of an app that keeps none.

;;; Its state

(defstate slideSorter {:picked ""} "The id of the page picked last: the selection when the app has no current page.")

;;; What it does

; style: keep, a feature's names start with its id, as its composition requires
(defn slideSorter-selected
  "The selected page: the app's current page, else the one picked last."
  [current]
  (or-else current state.slideSorter.picked))

(defcommand slideSorter.pick
  "Selects a page: the sorter remembers it, and the app selects it."
  :title "Select Page"
  :category "Slide Sorter"
  [id]
  (set "state.slideSorter.picked" id)
  (run "app.select" :with {:id id}))

(defcommand slideSorter.move
  "Moves page id to the place of page onto, unless it was dropped on itself."
  :title "Move Page"
  :category "Slide Sorter"
  [id onto]
  (when (not= id onto) (run "app.reorder" :with {:id id :onto onto})))

(defcommand slideSorter.duplicate
  "Duplicates page id, the selected one, which the view passes."
  :title "Duplicate Page"
  :category "Slide Sorter"
  [id]
  (run "app.duplicate" :with {:id id}))

;;; What it shows

; style: keep, a feature's names start with its id, as its composition requires
(defview slideSorter-header
  "The strip's header: what the app calls it (Pages unless it says) and how many pages there are, and the button that duplicates the selected one."
  []
  `(row#slideSorter-header :width "100%" :height 24 :gap 6 :alignItems center
     (label#slideSorter-title :grow 1 :height 24 :padding 6 :textColor @text
       :text ~(str (or-else app.title "Pages") " (" (count app.pages) ")"))
     (label#slideSorter-duplicate :width 24 :height 24 :icon copy :cursor hand :tooltip "Duplicate page"
       :textColor @text :onClick ~(handler slideSorter.duplicate (slideSorter-selected app.current)))))

; style: keep, a feature's names start with its id, as its composition requires
(defview slideSorter-page
  "One page: its thumbnail, framed in the accent when it is the selected one, and its name. Dragged onto another page,
  it takes that page's place; clicked, it is selected."
  [page]
  `(column :id ~(str "slideSorter-page-" page.id) :width 84 :height 96 :gap 2 :color transparent :cursor hand
     :drag {:data {:id ~page.id}}
     :onDrop ~(handler slideSorter.move event.data.id page.id)
     :onClick ~(handler slideSorter.pick page.id)
     (image :id ~(str "slideSorter-image-" page.id) :width 84 :height 70 :source ~page.image :fit contain
       :borderWidth ~(if (= page.id (slideSorter-selected app.current)) 2 1)
       :borderColor ~(if (= page.id (slideSorter-selected app.current)) "@accent" "@border")
       :dragOver {:borderColor @accent :borderWidth 3})
     (label :id ~(str "slideSorter-name-" page.id) :width 84 :height 18 :align center :fitText true
       :textColor @muted :text ~page.name)))

; style: keep, a feature's names start with its id, as its composition requires
(defview slideSorter-strip
  "The sorter: its header over the pages, in a row that scrolls across."
  []
  `(column :width "100%" :height 132 :gap 4 :color transparent :borderWidth 1 :borderColor @border
     ~(slideSorter-header)
     (row#slideSorter-pages :width "100%" :grow 1 :gap 8 :scroll horizontal :color transparent
       ~(repeat [page app.pages :key page.id] (slideSorter-page page)))))

;;; Where it goes

(defpart slideSorter-views
  "The strip, in the slot the app binds to the dock need."
  :views {:dock {:slideSorter-view ~(slideSorter-strip)}})
