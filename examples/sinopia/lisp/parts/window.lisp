;;; window.lisp --- the window around the slide: the command bar, the thumbnails, the canvas, the notes and the status bar
;;;
;;; The ribbon under the command bar is parts/ribbon.lisp and parts/tabs.lisp; the other views of the deck (the
;;; outline, the sorter) and the panes beside the canvas are parts/views.lisp and parts/review.lisp.

;;; What the window keeps

(defstate current 0 "The index of the slide shown.")

(defstate theme light "The window's look, light or dark (not the deck's theme).")

(defstate tab home "The ribbon's tab shown.")

(defstate ribbon true "Whether the ribbon is shown under the command bar.")

(defstate view normal "The view of the deck: normal, outline or sorter.")

(defstate notes true "Whether the speaker notes are shown under the slide.")

(defstate foldedSections [] "The ids of the sections folded in the thumbnails.")

(defstate ruler false "Whether the canvas shows its rulers.")

(defstate gridlines false "Whether the canvas shows its grid.")

(defstate guides false "Whether the canvas shows its guides.")

(defstate fit true "Whether the slide is fitted to the canvas, which follows the window's size.")

(defstate board {:camera {:x 0 :y 0 :zoom 1} :selection [] :guides {:vertical [] :horizontal []}}
  "The canvas: where it looks, what is selected and its guides.")

(deftokens assistant-colours "The assistant's colours, in Sinopia's greens."
  {:assistant.accent {:light "#0e7c6b" :dark "#3cc2aa"}
   :assistant.accentHover {:light "#0b6a5b" :dark "#62d6c1"}
   :assistant.onAccent {:light "#ffffff" :dark "#0b1f1b"}
   :assistant.surface {:light "#ffffff" :dark "#161b1a"}
   :assistant.raised {:light "#eef2f0" :dark "#1d2321"}
   :assistant.border {:light "#d9e1de" :dark "#343f3b"}
   :assistant.text {:light "#1d2926" :dark "#e3ebe8"}
   :assistant.dim {:light "#5a6a65" :dark "#9daea8"}
   :assistant.dot {:light "#a2afab" :dark "#6e7c77"}
   :assistant.warningBackground {:light "#e2e7e5" :dark "#131716"}
   :kit.border {:light "#c5d0cc" :dark "#42504b"}})

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "settings.language") :description ~(t "settings.language.about"))

;;; The ribbon's tabs

(defcomputed tabs "The ribbon's tabs, in their order."
  [{:id "file" :title "File"}
   {:id "home" :title "Home"}
   {:id "insert" :title "Insert"}
   {:id "draw" :title "Ink"}
   {:id "design" :title "Design"}
   {:id "transitions" :title "Transitions"}
   {:id "animations" :title "Motion"}
   {:id "slideShow" :title "Present"}
   {:id "record" :title "Record"}
   {:id "review" :title "Check"}
   {:id "view" :title "View"}
   {:id "help" :title "Help"}])

;;; The canvas and its zoom

(defcomputed canvasWidth "The canvas's width: the window's, less the thumbnails or the outline, and a pane."
  (max (- window.width
          (if (= state.view "outline") 300 201)
          (if (or state.formatPane (some? state.sidePane)) 260 0))
       100))

(defcomputed canvasHeight "The canvas's height: the window's, less the bars, the ribbon and the notes."
  (max (- window.height 45 (if state.ribbon 100 0) 27 (if state.notes 77 0)) 100))

(defcomputed fitZoom "The zoom that fits the 960 by 540 page in the canvas, with a margin."
  (min (/ (- computed.canvasWidth 56) 960) (/ (- computed.canvasHeight 40) 540)))

(defcomputed fitCamera "Where the canvas looks when the slide is fitted: the page in the middle."
  {:x (- 480 (/ computed.canvasWidth (* 2 computed.fitZoom)))
   :y (- 270 (/ computed.canvasHeight (* 2 computed.fitZoom)))
   :zoom computed.fitZoom})

(defcomputed view "The canvas's view: the board's, with the fitted camera while the slide is fitted."
  (if state.fit (merge state.board {:camera computed.fitCamera}) state.board))

(defcomputed zoom "The canvas's zoom." computed.view.camera.zoom)

(defn fit "Fits the slide to the canvas again." []
  (set "state.fit" true))

(defn zoomBy "Zooms the canvas by a factor, about its middle, between 10% and 400%." [by]
  (let [view computed.view
        zoom (clamp (* computed.zoom by) 0.1 4)
        halfWidth (/ computed.canvasWidth 2)
        halfHeight (/ computed.canvasHeight 2)]
    (set "state.board"
         (merge view {:camera {:x (- (+ view.camera.x (/ halfWidth computed.zoom)) (/ halfWidth zoom))
                               :y (- (+ view.camera.y (/ halfHeight computed.zoom)) (/ halfHeight zoom))
                               :zoom zoom}})))
  (set "state.fit" false))

(defn viewChanged "Keeps the canvas's new view; a pan or a zoom stops fitting the slide." [view]
  (set "state.board" view)
  (when (and (some? state.editing) (not (contains (or-else view.selection []) state.editing)))
    (set "state.editing" nil))
  (when (or (> (abs (- view.camera.zoom computed.fitCamera.zoom)) 0.0001)
            (> (abs (- view.camera.x computed.fitCamera.x)) 0.01)
            (> (abs (- view.camera.y computed.fitCamera.y)) 0.01))
    (set "state.fit" false)))

;;; Sections

(defcomputed slideTitle "The slide's title: the first line of its title placeholder's text."
  (trim (get (split (or-else (:text (find (fn [it] (or (= it.ph "title") (= it.ph "ctrTitle"))) computed.objects)) "")
                    "\n")
             0)))

(defcomputed sectionFirsts "The index of each section's first slide." (map (fn [it] it.first) (or-else state.deck.sections [])))

(defcomputed sectionAt "For each slide, the section that starts there, or nil."
  (map-indexed (fn [index it]
                 (if (< (indexOf computed.sectionFirsts index) 0)
                   nil
                   (get state.deck.sections (indexOf computed.sectionFirsts index))))
               state.deck.slides))

(defcomputed sectionOf "For each slide, the id of the section it is in, or nil."
  (scan state.deck.slides
        (fn [acc it] (if (some? (get computed.sectionAt index)) (get-in computed.sectionAt [index :id]) acc))))

(defcomputed folded "For each slide, whether its section is folded."
  (map (fn [it] (and (some? it) (contains state.foldedSections it))) computed.sectionOf))

(defn toggleSection "Folds a section in the thumbnails, or unfolds it." [id]
  (set "state.foldedSections"
       (if (contains state.foldedSections id)
         (filter (fn [it] (not= it id)) state.foldedSections)
         (concat state.foldedSections [id]))))

;;; What a click does

(defn showTab "Shows a tab of the ribbon, and the ribbon if it was hidden." [tab]
  (set "state.tab" tab)
  (set "state.ribbon" true))

(defn chooseSlide "Shows a slide chosen in a list of slides, which keeps the keys." [index list]
  (pickSlide index)
  (focus list))

(defn slideMenu "Shows a slide and opens its menu, at the pointer." [index x y]
  (pickSlide index)
  (openContext x y "slide"))

(defn canvasMenu "Opens the canvas's menu at the pointer; a click there, not a key, selects nothing first."
  [keyboard x y]
  (when (not (or-else keyboard false))
    (set "state.board.selection" []))
  (openContext x y "canvas"))

(defn objectMenu "Opens an object's menu at the pointer, selecting it unless it already is." [id x y]
  (when (not (contains state.board.selection id))
    (set "state.board.selection" [id]))
  (openContext x y "object"))

;;; The command bar

(defview command-bar "The bar at the top: the deck's name, the tabs, the search box and the quick commands." []
  `(row#command-bar :height 44 :color @rb :alignItems center :gap 2
     (node#command-pad :width 8 :height 1 :color transparent)
     ~(app-mark)
     ~(doc-title)
     (node#command-divider :width 1 :height 20 :color @rbLine)
     (node#command-pad-2 :width 6 :height 1 :color transparent)
     ~(repeat [t computed.tabs :key t.id]
       (tab-title t))
     (node#command-gap :grow 1 :height 1 :color transparent)
     ~(search)
     (node#command-gap-2 :width 8 :height 1 :color transparent)
     ~@(quick-commands)
     ~(ribbon-collapse)
     (node#command-pad-3 :width 6 :height 1 :color transparent)
     ~(present-button)
     (node#command-pad-end :width 8 :height 1 :color transparent)))

(defview app-mark "Sinopia's mark, at the bar's start." []
  `(label#app-mark :width 28 :height 28 :radius 8 :align center :color @brand :textColor @onBrand
     :runs [{:icon presentation :size 16}]
     :tooltip "Sinopia"))

(defview doc-title "The deck's name: a click opens the File tab." []
  `(label#doc-title :height 28
     :fitText {:maxWidth 220}
     :padding 8
     :radius 14
     :textColor @rbInk
     :cursor hand
     :hover {:color @rbHover}
     :runs [{:text ~state.deck.title :size 13 :bold true}]
     :tooltip "Open, save or export the deck"
     :onClick ~(handler showTab "file")))

(defview tab-title "A tab's title in the command bar: `t` is the repeat's tab." [t]
  `(label :id ~(str "tab-" t.id "-label")
     :style ~(if (and (= state.tab t.id) state.ribbon) "tabOn" "tab")
     :selected ~(= state.tab t.id)
     :runs [{:text ~t.title :size 12}]
     :onClick ~(handler showTab t.id)))

(defview search "The search box: commands whose names hold its text, and a search of the slides." []
  `(textarea#search :width 200
     :height 28
     :padding 6
     :radius 14
     :color @rbFieldBg
     :borderWidth 1
     :borderColor @rbFieldLine
     :textColor @rbInk
     :bind state.search
     :placeholder "Search commands (Alt+Q)"
     :placeholderColor @rbMuted
     :label "Search commands"
     :tooltip "Type what you want to do, then Enter"
     :onSubmit ~(handler openDrop "search")))

(defview quick-commands "Undo, Redo, Save and Present from the start, as icons in the command bar." []
  [`(label#qat-undo :style barIcon
      :runs [{:icon undo-2 :size 16}]
      :textColor ~(if computed.canUndo "@rbInk" "@inkFaded")
      :tooltip "Undo (Ctrl+Z)"
      :onClick undo)
   `(label#qat-redo :style barIcon
      :runs [{:icon redo-2 :size 16}]
      :textColor ~(if computed.canRedo "@rbInk" "@inkFaded")
      :tooltip "Redo (Ctrl+Y)"
      :onClick redo)
   `(label#qat-save :style barIcon :runs [{:icon hard-drive-download :size 16}] :tooltip "Save (Ctrl+S)" :onClick saveDeck)
   `(label#qat-present :style barIcon
      :runs [{:icon skip-back :size 16}]
      :tooltip "Present from the start (F5)"
      :onClick ~(handler present 0))])

(defview ribbon-collapse "The button that hides the ribbon, or shows it again." []
  `(label#ribbon-collapse :style barIcon
     :runs [{:icon ~(if state.ribbon "panel-top-close" "panel-top-open") :size 16}]
     :tooltip ~(if state.ribbon "Hide the commands (Ctrl+F1)" "Show the commands (Ctrl+F1)")
     :onClick [{:toggle state.ribbon}]))

(defview present-button "Present, from the slide shown." []
  `(label#present-button :style pillButton
     :runs [{:icon play :size 13} {:text " Present" :size 12 :bold true}]
     :tooltip "Present from this slide (Shift+F5)"
     :onClick ~(handler present state.current)))

;;; The ribbon

(defview ribbon "The ribbon: the commands of the tab chosen." []
  `(node#ribbon :height 100 :color @rb :when ~state.ribbon
     (node#ribbon-body :y 2 :anchors {:left 8 :right 8} :height 92 :color transparent
       (node :use tabHome :when ~(= state.tab "home"))
       (node :use tabInsert :when ~(= state.tab "insert"))
       (node :use tabDesign :when ~(= state.tab "design"))
       (node :use tabTransitions :when ~(= state.tab "transitions"))
       (node :use tabView :when ~(= state.tab "view"))
       (node :use tabFile :when ~(= state.tab "file"))
       (node :use tabMotion :when ~(= state.tab "animations"))
       (node :use tabPresent :when ~(= state.tab "slideShow"))
       (node :use tabInk :when ~(= state.tab "draw"))
       (node :use tabRecord :when ~(= state.tab "record"))
       (node :use tabCheck :when ~(= state.tab "review"))
       (node :use tabHelp :when ~(= state.tab "help")))))

;;; The work area

(defview work-area "Between the ribbon and the status bar: the thumbnails or a view of the deck, the slide, a pane."
  []
  `(row#main :grow 1 :color @work :alignItems stretch
     ~(thumbnails)
     (node#thumbs-line :when ~(= state.view "normal") :width 1 :color @barLine)
     (node :use outlineView :when ~(= state.view "outline"))
     (node :use sorterView :when ~(= state.view "sorter"))
     ~(centre)
     (node :use formatPane :when ~(and state.formatPane (not= state.view "sorter")))
     (node :use sidePane :when ~(and (not state.formatPane) (some? state.sidePane) (not= state.view "sorter")))))

;;; The thumbnails

(defview thumbnails "The slides down the side, by section; the arrows choose one, Delete deletes it." []
  `(column#thumbnails :when ~(= state.view "normal")
     :width 200
     :color @rb
     :scroll vertical
     :gap 0
     :focusable true
     :role list
     :label "Slides"
     :onContextMenu ~(handler openContext event.x event.y "slide")
     :keys {:up ~(handler pickSlide (- state.current 1))
            :down ~(handler pickSlide (+ state.current 1))
            :pageup ~(handler pickSlide (- state.current 1))
            :pagedown ~(handler pickSlide (+ state.current 1))
            :home ~(handler pickSlide 0)
            :end ~(handler pickSlide (- (count state.deck.slides) 1))
            :enter ~(handler newSlide "content")
            :delete deleteSlide
            :backspace deleteSlide}
     ~(thumbnails-top)
     ~(repeat [s state.deck.slides :index i :key s.id]
       `(column :id ~(str "thumb-" s.id) :fitContent height :width 200 :color transparent
          ~(section-header s i)
          ~(thumbnail s i)))
     (node#thumbs-bottom :height 20 :width 200 :color transparent)))

(defview thumbnails-top "The thumbnails' heading: how many slides, and New Slide." []
  `(row#thumbs-top :height 36 :width 200 :color transparent :alignItems center
     (node#thumbs-top-pad :width 12 :height 1 :color transparent)
     (label#thumbs-title :fitText true
       :height 20
       :textColor @rbInk
       :runs [{:text "Slides" :size 12 :bold true}
              {:text ~(spaced "  " (count state.deck.slides)) :size 11 :color @rbMuted}])
     (node#thumbs-top-gap :grow 1 :height 1 :color transparent)
     (label#thumbs-add :style barIcon
       :width 26
       :height 26
       :runs [{:icon plus :size 15}]
       :tooltip "New Slide (Ctrl+M)"
       :onClick ~(handler newSlide "content"))
     (node#thumbs-top-pad-end :width 8 :height 1 :color transparent)))

(defview section-header "The name of the section that starts at slide `s`, if one does: a click folds it." [s i]
  `(label :id ~(str "thumb-" s.id "-section")
     :when ~(some? (get computed.sectionAt i))
     :width 200
     :height 24
     :padding 8
     :textColor @rbMuted
     :cursor hand
     :role button
     :label ~(get-in computed.sectionAt [i :name])
     :tooltip ~(if (get computed.folded i) "Expand the section" "Collapse the section")
     :runs [{:icon ~(if (get computed.folded i) "chevron-right" "chevron-down") :size 11}
            {:text ~(spaced " " (upper-case (get-in computed.sectionAt [i :name]))) :size 9 :bold true}
            {:text ~(if (get computed.folded i)
                      (spaced "  " (plural (count (filter (fn [it] (= it (get-in computed.sectionAt [i :id])))
                                                          computed.sectionOf))
                                           "slide"))
                      "")
             :size 9}]
     :onClick ~(handler toggleSection (get-in computed.sectionAt [i :id]))))

(defview thumbnail "Slide `s`, at index `i`: its picture and its number; drag it to move it." [s i]
  `(column :id ~(str "thumb-" s.id "-row")
     :width 200
     :when ~(not (get computed.folded i))
     :height 126
     :color transparent
     :cursor hand
     :role listitem
     :alignItems center
     :selected ~(= i state.current)
     :label ~(str "Slide " (+ i 1))
     :drag {:data {:slide ~i}}
     :onDrop ~(handler moveSlide (or-else event.data.slide (- 1)) i)
     :onClick ~(handler chooseSlide i "#thumbnails")
     :onContextMenu ~(handler slideMenu i event.x event.y)
     ~(thumbnail-picture s i)
     ~(thumbnail-gutter s i)))

(defview thumbnail-picture "A slide's picture in the thumbnails, framed when it is the one shown." [s i]
  `(node :id ~(str "thumb-" s.id "-frame")
     :width 178
     :height 104
     :radius 10
     :color transparent
     :borderWidth 2
     :borderColor ~(if (= i state.current) "@brandLine" "transparent")
     :hover {:borderColor ~(if (= i state.current) "@brandLine" "@thumbHover")}
     :dragOver {:borderColor @brandLine}
     (node :id ~(str "thumb-" s.id "-paper")
       :x 5 :y 5 :width 168 :height 94 :radius 4 :color "#ffffff" :borderWidth 1 :borderColor @thumbLine
       (image :id ~(str "thumb-" s.id "-page") :x 0 :y 0 :width 168 :height 94 :source ~(str "slide-" s.id) :fit fill)
       (node :id ~(str "thumb-" s.id "-veil") :when ~(or-else s.hidden false) :fill 0 :color "#ffffff99"))))

(defview thumbnail-gutter "Under a thumbnail: the slide's number, and marks for its motion and for a hidden slide."
  [s i]
  `(row :id ~(str "thumb-" s.id "-gutter") :width 170 :height 18 :gap 4 :color transparent :alignItems center
     (label :id ~(str "thumb-" s.id "-number")
       :fitText true
       :height 16
       :textColor ~(if (= i state.current) "@brandLine" "@rbMuted")
       :runs [{:text ~(text (+ i 1)) :size 11 :bold ~(= i state.current)}])
     (label :id ~(str "thumb-" s.id "-star")
       :when ~(or (not= (or-else s.transition.kind "none") "none") (> (count (or-else s.animations [])) 0))
       :width 14 :height 16 :align center :textColor @rbMuted
       :runs [{:icon wand-sparkles :size 10}]
       :tooltip "Has a transition or animations")
     (label :id ~(str "thumb-" s.id "-hidden")
       :when ~(or-else s.hidden false)
       :width 14 :height 16 :align center :textColor @rbMuted
       :runs [{:icon eye-off :size 10}]
       :tooltip "Hidden in the show")))

;;; The slide and its notes

(defview centre "The slide on its canvas, the ink over it, and the speaker notes under it." []
  `(column#centre :when ~(not= state.view "sorter") :grow 1 :color @work :alignItems stretch
     (node#board-area :grow 1 :color @work
       ~(board)
       (node :use inkLayer :when ~(and (some? state.inkTool) (not state.show.on))))
     (node#notes-line :height 1 :color @barLine :when ~state.notes)
     ~(notes-pane)))

(defview board "The canvas: the page, its background and its objects, which a drag moves, sizes or turns." []
  `(canvas#board :fill 0
     :dir ltr
     :focusable true
     :onContextMenu ~(handler canvasMenu event.keyboard event.x event.y)
     :color @work
     :accentColor @handles
     :view ~computed.view
     :scaleChildren true
     :snap true
     :rulers ~state.ruler
     :grid ~(if state.gridlines 12 0)
     :label ~(str "Slide " (+ state.current 1) (if (= computed.slideTitle "") "" (str ", " computed.slideTitle)))
     :onChange ~(handler viewChanged event.value)
     :onTransform ~(handler placeObjects event.items)
     (node :x 0 :y 0 :width 960 :height 540 :color "#ffffff" :shadow {:x 0 :y 1 :blur 4 :color "#00000033"}
       (node :use slideBackground :with {:slide ~computed.slide :prefix e-}))
     ~(repeat [o computed.objects :key o.id]
       `(node :use slideObject
          :with {:o ~o :prefix "" :layout ~computed.layout :edit true}
          :onContextMenu ~(handler objectMenu o.id event.x event.y)
          :onDoubleClick ~(handler editText o.id)))))

(defview notes-pane "The speaker notes of the slide shown, edited in place." []
  `(node#notes-pane :height 76 :color @notes :when ~state.notes
     (label#notes-placeholder :x 14
       :y 8
       :width 400
       :height 20
       :textColor @faint
       :runs [{:text ~(if (= (or-else computed.slide.notes "") "") "Click to add notes" "") :size 13}])
     (editor#notes :fill 4
       :color transparent
       :wrap true
       :textColor @rbInk
       :lineColor transparent
       :label "Notes"
       :bind ~(str "state.deck.slides[" state.current "].notes"))))

;;; The status bar

(defview status-bar "The bar at the bottom: the views, the slide's number, the notes and comments, and the zoom." []
  `(row#status-bar :height 26 :color @rb :alignItems center :gap 2
     (node#status-pad :width 8 :height 1 :color transparent)
     ~(status-views)
     (node#status-gap-1 :width 6 :height 1 :color transparent)
     (label#status-slide :style barButton
       :runs [{:text ~(str "Slide " (+ state.current 1) " of " (count state.deck.slides)) :size 11}])
     (label#status-notes :style ~(if state.notes "barButtonOn" "barButton")
       :runs [{:icon notebook-pen :size 13} {:text " Notes" :size 11}]
       :onClick [{:toggle state.notes}])
     ~(status-comments)
     (node#status-gap :grow 1 :height 1 :color transparent)
     ~@(zoom-controls)
     (node#status-pad-end :width 8 :height 1 :color transparent)))

(defview view-button "A button of the status bar that shows a view of the deck." [id view icon tooltip]
  `(label :id ~id
     :style ~(if (= state.view view) "barButtonOn" "barButton")
     :height 20
     :runs [{:icon ~icon :size 13}]
     :tooltip ~tooltip
     :onClick [{:set state.view :to ~view}]))

(defview status-views "The views of the deck, and Present from this slide." []
  `(row#status-views :fitContent width :height 22 :radius 11 :color @card :borderWidth 1 :borderColor @rbLine
     :alignItems center
     ~(view-button "status-normal" "normal" "panel-left" "Normal")
     ~(view-button "status-outline" "outline" "list-tree" "Outline")
     ~(view-button "status-sorter" "sorter" "layout-grid" "Slide Sorter")
     (label#status-show :style barButton
       :height 20
       :runs [{:icon play :size 13}]
       :tooltip "Present from this slide"
       :onClick ~(handler present state.current))))

(defview status-comments "Comments, and how many the slide has: a click opens them beside the slide." []
  `(label#status-comments :style ~(if (and (= state.sidePane "comments") (not state.formatPane)) "barButtonOn" "barButton")
     :runs [{:icon message-square :size 13}
            {:text ~(if (> (count computed.comments) 0) (str " Comments (" (count computed.comments) ")") " Comments")
             :size 11}]
     :onClick ~(handler openSidePane "comments")))

(defview zoom-controls "Fit, the zoom slider between its buttons, and the zoom in percent, in the status bar." []
  [`(label#zoom-fit :style ~(if state.fit "barButtonOn" "barButton")
      :runs [{:icon scan :size 13} {:text " Fit" :size 11}]
      :tooltip "Fit slide to current window"
      :onClick fit)
   `(label#zoom-out :style barButton :runs [{:icon zoom-out :size 13}] :tooltip "Zoom Out" :onClick ~(handler zoomBy 0.9))
   `(slider#zoom-slider :width 110
      :height 16
      :trackColor @rbFieldLine
      :activeColor @brand
      :knobColor @rbFieldBg
      :min 10
      :max 400
      :value ~(round (* computed.zoom 100))
      :label "Zoom"
      :onChange ~(handler zoomBy (/ (/ event.value 100) computed.zoom)))
   `(label#zoom-in :style barButton :runs [{:icon zoom-in :size 13}] :tooltip "Zoom In" :onClick ~(handler zoomBy 1.1))
   `(label#zoom-percent :style barButton
      :width 42
      :align center
      :runs [{:text ~(str (round (* computed.zoom 100)) "%") :size 11}])])
