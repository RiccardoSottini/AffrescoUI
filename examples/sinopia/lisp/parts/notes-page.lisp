;;; notes-page.lisp --- The notes page: the slide over its speaker notes, as a page to read, edit and print
;;;
;;; View > Notes Page shows the slide shown on a portrait page, its notes under it, edited in place. While the view is
;;; open, each slide is also drawn as such a page into its own image, 'notes-<id>', by the nodes 'notes-page-<id>':
;;; Export Notes Pages draws those nodes as a PDF, a page each.

(ns sinopia.parts.notes-page)

;;; Definitions

(defcomponent notesPageImage "Slide `s` and its notes as a printed page: what Export Notes Pages draws."
  {:s nil :n 0}
  `(node :id ~(str "notes-page-" s.id)
     :snapshot {:image ~(str "notes-" s.id) :scale 1}
     :x 0
     :y 0
     :width 540
     :height 720
     :color "#ffffff"
     (node :id ~(str "notes-page-" s.id "-slide")
       :x 30
       :y 40
       :width 960
       :height 540
       :color transparent
       :scale 0.5
       :transformOrigin topLeft
       (node :use slidePage :with {:slide ~s :prefix ~(str "n" s.id "-")}))
     (node :id ~(str "notes-page-" s.id "-frame")
       :x 30
       :y 40
       :width 480
       :height 270
       :color transparent
       :borderWidth 1
       :borderColor "#b8c4c0")
     (label :id ~(str "notes-page-" s.id "-notes")
       :x 40
       :y 340
       :width 460
       :height 330
       :wrap true
       :align start
       :runs [{:text ~(or-else s.notes "") :size 13 :color "#1d2926"}])
     (label :id ~(str "notes-page-" s.id "-number")
       :x 40
       :y 684
       :width 460
       :height 18
       :align end
       :runs [{:text ~(str (inc n)) :size 11 :color "#5a6a65"}])))

(defcomponent notesPageView "The notes page view: the slide shown over its notes, on a page, and its buttons."
  {}
  `(column#notes-page-view :grow 1 :color @work :alignItems center :gap 12 :scroll vertical
     (node#notes-page-top :height 4 :width 1 :color transparent)
     ~(notes-page-bar)
     (node#notes-page :width 540
       :height 720
       :color "#ffffff"
       :radius 2
       :shadow {:y 2 :blur 10 :color "#00000030"}
       :role document
       :label ~(t "notes.pageN" {:n (inc state.current)})
       (node#notes-page-slide :x 30
         :y 40
         :width 960
         :height 540
         :color transparent
         :scale 0.5
         :transformOrigin topLeft
         (node :use slidePage :with {:slide ~computed.slide :prefix np-}))
       (node#notes-page-frame :x 30 :y 40 :width 480 :height 270 :color transparent :borderWidth 1 :borderColor "#b8c4c0")
       (label#notes-page-placeholder :x 40
         :y 340
         :width 460
         :height 20
         :textColor "#7a8884"
         :runs [{:text ~(if (= (or-else computed.slide.notes "") "") (t "bar.clickToAddNotes") "") :size 13}])
       (editor#notes-page-notes :x 36
         :y 336
         :width 468
         :height 330
         :color transparent
         :wrap true
         :textColor "#1d2926"
         :lineColor transparent
         :label ~(t "bar.notes")
         :bind ~(str "state.deck.slides[" state.current "].notes"))
       (label#notes-page-number :x 40
         :y 684
         :width 460
         :height 18
         :align end
         :runs [{:text ~(str (inc state.current)) :size 11 :color "#5a6a65"}]))
     (node#notes-page-bottom :height 12 :width 1 :color transparent)))

;;; Views

(defview notes-page-bar "Over the notes page: the slide before and after, where it is, and Export Notes Pages." []
  `(row#notes-page-bar :width 540 :height 30 :gap 6 :alignItems center :color transparent
     (node#notes-page-previous :use rbSmall
       :with {:icon chevron-left :enabled ~(pos? state.current)}
       :tooltip ~(t "notes.previousSlide")
       :onClick ~(handler pick-slide (dec state.current)))
     (label#notes-page-where :fitText true
       :height 24
       :textColor @rbInk
       :runs [{:text ~(t "slide.nOfCount" {:n (inc state.current) :count (count state.deck.slides)}) :size 12}])
     (node#notes-page-next :use rbSmall
       :with {:icon chevron-right :enabled ~(< state.current (dec (count state.deck.slides)))}
       :tooltip ~(t "notes.nextSlide")
       :onClick ~(handler pick-slide (inc state.current)))
     (node#notes-page-gap :grow 1 :height 1 :color transparent)
     (label#notes-page-export :style rbButton
       :height 26
       :padding 6
       :fitText true
       :runs [{:icon file-text :size 14} {:text ~(t "notes.exportNotesPages") :size 12}]
       :tooltip ~(t "notes.everySlideWithItsNotes")
       :onClick export-notes-pages)))

;;; What the buttons do

(defn export-notes-pages "Exports every slide with its notes as a PDF, a portrait page each." []
  (export "pdf"
          :pages (map #(str "notes-page-" (:id %)) state.deck.slides)
          :size [540 720]
          :title (t "notes.deckNotes" {:deck computed.deckName})
          :onDone pdf-exported
          :onError not-exported))
