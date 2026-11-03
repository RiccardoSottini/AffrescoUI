;;; views.lisp --- the views beside Normal, the format pane, and the deck as a file
;;;
;;; The sorter shows every slide's picture in a grid, dragged to reorder; the outline, the slides' titles and text. The
;;; format pane at the right changes the selected objects, else the slide. The deck is saved and opened as JSON through
;;; the system's dialogs, and exported as a PDF of the slides shown or a PNG of one.

;;; What the views keep

(defstate formatPane false "Whether the format pane is shown.")

(defstate fileStatus "" "What the last save, open or export did.")

(defstate opened nil "The deck a file held, until it is checked.")

(defcomputed directions "The sides a transition comes from." '[left up right down])

(defcomputed directionIcons "Each side's arrow." '{:left arrow-left :up arrow-up :right arrow-right :down arrow-down})

(defcomputed fillSlots "The swatches of the fill and line rows: none, then the theme's colours."
  '[none light1 dark1 light2 dark2 accent1 accent2 accent3 accent4 accent5 accent6])

(defcomputed deckName "The deck's title, or Untitled deck."
  (if (= (or-else state.deck.title "") "") (t "view.untitledDeck") state.deck.title))

(defcomputed outlineText "Each slide's text for the outline: its objects' text, a line each."
  (map (fn [it]
         (join "\n"
               (filter (fn [it] (not= it "")) (map (fn [it] (trim (or-else it.text ""))) (or-else it.objects [])))))
       state.deck.slides))

;;; Keys

(defkey "mod+s" {:when ~(not state.show.on) :do saveDeck})

(defkey "mod+o" {:when ~(not state.show.on) :do openDeck})

;;; The format pane

(defcomponent swatches
  "The theme's colours and none, as a row of swatches; a click sets the selection's `key`, or the slide's background."
  {:prefix "" :chosen "" :run "" :key ""}
  `(row :id ~(str prefix "-swatches") :height 22 :gap 3 :color transparent
     ~(repeat [slot (if (= key "background") (slice computed.fillSlots 1) computed.fillSlots)]
       `(node :id ~(str prefix "-" slot)
          :width 18
          :height 18
          :radius 3
          :cursor hand
          :color ~(if (= slot "none") "@rbFieldBg" (get computed.palette slot))
          :borderWidth ~(if (= slot chosen) 2 1)
          :borderColor ~(if (= slot chosen) "@brandLine" "@rbFieldLine")
          :tooltip ~(if (= slot "none") (t "ribbon.none") (get computed.slotTitles slot))
          :role button
          :label ~slot
          :onClick ~(handler pickSwatch run key slot)
          (label :when ~(= slot "none") :fill 0 :align center :textColor @rbMuted :runs [{:icon ban :size 12}])))))

(defcomponent paneStepper "A number of the selected objects, `key`, with buttons that step it between `low` and `high`."
  {:title "" :value 0 :unit "" :key "" :step 1 :low 0 :high 1000 :prefix ""}
  `(row :id ~prefix :height 24 :gap 2 :alignItems center :color transparent
     (label :id ~(str prefix "-title") :width 84 :height 22 :textColor @rbInk :runs [{:text ~title :size 12}])
     (label :id ~(str prefix "-value")
       :width 64
       :height 22
       :align end
       :padding 4
       :textColor @rbInk
       :runs [{:text ~(str (fixed value 1) unit) :size 12}])
     (node :id ~(str prefix "-less")
       :use rbSmall
       :with {:icon minus}
       :tooltip ~(t "view.less")
       :onClick ~(handler setProperty key (clamp (- value step) low high)))
     (node :id ~(str prefix "-more")
       :use rbSmall
       :with {:icon plus}
       :tooltip ~(t "view.more")
       :onClick ~(handler setProperty key (clamp (+ value step) low high)))))

(defcomponent formatPane
  "The format pane: the selected objects' fill, line, place, lock and alt text, or the slide's background."
  {}
  `(column#format-pane :width 260
     :color @rb
     :borderWidth 1
     :borderColor @rbLine
     :gap 6
     :alignItems center
     :scroll vertical
     :role region
     :label ~(t "view.format")
     :let {:o ~computed.first :box ~(if (nil? computed.first) {} (merge computed.firstPlaceholder computed.first))}
     (node#format-top :width 1 :height 4 :color transparent)
     (row#format-head :width 236 :height 24 :alignItems center :color transparent
       (label#format-title :grow 1
         :height 22
         :textColor @rbInk
         :runs [{:text ~(if (nil? 'o) (t "menu.formatBackground2") (t "view.formatShape")) :size 14 :bold true}])
       (node#format-close :use rbSmall :with {:icon x} :tooltip ~(t "pane.close")
         :onClick [{:set state.formatPane :to false}]))
     ~(format-background)
     ~(format-object)))

(defview format-background "The slide's background colour, when nothing is selected." []
  `(column#format-slide :when ~(nil? 'o) :width 236 :fitContent height :alignItems stretch :gap 6 :color transparent
     (label#format-background-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.fill") :size 12}])
     (node :use swatches
       :with {:prefix format-background
              :chosen ~(or-else computed.slide.background computed.theme.background)
              :run setBackground
              :key background})
     (label#format-background-reset :style rbButton
       :height 24
       :fitText true
       :padding 4
       :runs [{:icon rotate-ccw :size 13} {:text ~(t "view.theThemeSBackground") :size 12}]
       :onClick ~(handler setBackground nil))))

(defview format-object "The selected objects' fill, line, effects, size and place, lock and alt text." []
  `(column#format-object :when ~(some? 'o) :width 236 :fitContent height :alignItems stretch :gap 6 :color transparent
     (label#format-fill-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.fill") :size 12}])
     (node :use swatches :with {:prefix format-fill :chosen ~(or-else o.fill "none") :run setObjects :key fill})
     (label#format-line-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.line") :size 12}])
     (node :use swatches :with {:prefix format-line :chosen ~(or-else o.line "none") :run setObjects :key line})
     ~(stepper "format-line-width" "view.width" 'o.lineWidth 1 " pt" "lineWidth" 0.5 0 50)
     (label#format-effects-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.effects") :size 12}])
     ~(format-effects)
     (label#format-size-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.sizeAndPlace") :size 12}])
     ~(stepper "format-w" "view.width" 'box.w 100 "" "w" 10 1 4000)
     ~(stepper "format-h" "view.height" 'box.h 100 "" "h" 10 0 4000)
     ~(stepper "format-x" "view.left" 'box.x 0 "" "x" 10 -2000 4000)
     ~(stepper "format-y" "menu.top" 'box.y 0 "" "y" 10 -2000 4000)
     ~(stepper "format-rotate" "view.rotation" 'o.rotate 0 "°" "rotate" 15 -360 360)
     ~(format-lock)
     (label#format-alt-title :height 18 :textColor @rbMuted :runs [{:text ~(t "view.altText") :size 12}])
     ~(format-alt)))
(defview stepper "A stepper of the format pane: a number of the selection, `value` or else `fallback`, titled by a key."
  [prefix title value fallback unit key step low high]
  `(node :use paneStepper
     :with {:prefix ~prefix :title ~(t title) :value ~(or-else value fallback) :unit ~unit :key ~key :step ~step
            :low ~low
            :high ~high}))

(defview format-effects "No effect, a shadow or a glow, for the selected objects." []
  `(row#format-effects :width 236 :height 26 :gap 4 :color transparent
     ~(repeat [fx [["none" (t "view.noEffect")] ["shadow" (t "effect.shadow")] ["glow" (t "effect.glow")]] :key fx.0]
       `(label :id ~(str "format-effect-" fx.0)
          :style ~(if (= (or-else o.effect "none") fx.0) "rbButtonOn" "rbButton")
          :height 26
          :fitText true
          :padding 6
          :runs [{:text ~fx.1 :size 12}]
          :onClick ~(handler setEffect fx.0)))))

(defview format-lock "Lock: a locked object stays where it is and isn't deleted." []
  `(label#format-lock :style ~(if computed.locked "rbButtonOn" "rbButton")
     :height 26
     :fitText true
     :padding 6
     :runs [{:icon ~(if computed.locked "lock" "lock-open") :size 14}
            {:text ~(if computed.locked (t "view.locked") (t "view.lock")) :size 12}]
     :tooltip ~(t "view.aLockedObjectStaysWhere")
     :onClick toggleLock))

(defview format-alt "The first selected object's alt text, edited in place." []
  `(editor#format-alt :height 54
     :wrap true
     :color @rbFieldBg
     :lineColor transparent
     :textColor @rbInk
     :label ~(t "view.altTextWhatAScreen")
     :bind ~(str "state.deck.slides[" state.current "].objects[" (indexOf (map (fn [it] it.id) computed.objects) o.id)
                 "].alt")))

;;; The sorter

(defcomponent sorterView "Every slide's picture in a grid: a click picks one, a double-click opens it, a drag moves it."
  {}
  `(column#sorter :grow 1 :color @work :alignItems stretch
     (node#sorter-top :height 20 :color transparent)
     (grid#sorter-grid :grow 1
       :color @work
       :scroll vertical
       :columns ~(max (floor (/ (- window.width 40) 236)) 1)
       :gap 16
       :justifyItems center
       :focusable true
       :role list
       :label ~(t "bar.slides")
       :keys {:left ~(handler pickSlide (- state.current 1))
              :right ~(handler pickSlide (+ state.current 1))
              :enter [{:set state.view :to normal}]
              :delete deleteSlide}
       ~(repeat [s state.deck.slides :index i :key s.id]
         (sorter-slide s i)))))

(defview sorter-slide "Slide `s` in the sorter: its picture, then its number." [s i]
  `(column :id ~(str "sorter-" s.id)
     :width 220
     :height 148
     :gap 4
     :alignItems stretch
     :color transparent
     :cursor hand
     :role listitem
     :selected ~(= i state.current)
     :label ~(t "slide.n" {:n (+ i 1)})
     :drag {:data {:slide ~i}}
     :onDrop ~(handler moveSlide (or-else event.data.slide (- 1)) i)
     :onClick ~(handler chooseSlide i "#sorter-grid")
     :onDoubleClick ~(handler openSlide i)
     (node :id ~(str "sorter-" s.id "-frame")
       :width 220
       :height 128
       :radius 6
       :color ~(if (= i state.current) "@brandLine" "transparent")
       :hover {:color ~(if (= i state.current) "@brandLine" "@thumbHover")}
       :dragOver {:color @brandLine}
       (image :id ~(str "sorter-" s.id "-image") :x 3 :y 3 :width 214 :height 122 :source ~(str "slide-" s.id)
         :fit fill)
       (node :id ~(str "sorter-" s.id "-veil") :when ~(or-else s.hidden false) :x 3 :y 3 :width 214 :height 122
         :color "#ffffff99"))
     ~(sorter-caption s i)))

(defview sorter-caption "Under a slide in the sorter: its number, and marks for a hidden slide and its motion." [s i]
  `(row :id ~(str "sorter-" s.id "-caption") :height 16 :gap 4 :color transparent
     (label :id ~(str "sorter-" s.id "-number")
       :fitText true
       :height 16
       :textColor @rbInk
       :runs [{:text ~(text (+ i 1)) :size 12 :bold ~(= i state.current)}])
     (label :id ~(str "sorter-" s.id "-hidden")
       :when ~(or-else s.hidden false)
       :fitText true
       :height 16
       :textColor @rbMuted
       :runs [{:icon eye-off :size 11}]
       :tooltip ~(t "bar.hiddenInTheShow"))
     (label :id ~(str "sorter-" s.id "-star")
       :when ~(or (not= (or-else s.transition.kind "none") "none") (> (count (or-else s.animations [])) 0))
       :fitText true
       :height 16
       :textColor @rbMuted
       :runs [{:icon star :size 10}]
       :tooltip ~(t "bar.hasATransitionOrAnimations"))))

;;; The outline

(defcomponent outlineView "The slides' text in place of the thumbnails: each slide's number and its lines."
  {}
  `(column#outline :width 300
     :color @rbFieldBg
     :scroll vertical
     :gap 8
     :alignItems center
     :focusable true
     :role list
     :label ~(t "bar.outline")
     :keys {:up ~(handler pickSlide (- state.current 1))
            :down ~(handler pickSlide (+ state.current 1))}
     (node#outline-top :width 1 :height 4 :color transparent)
     ~(repeat [s state.deck.slides :index i :key s.id]
       (outline-slide s i))))

(defview outline-slide "Slide `s` in the outline: its number and its text; a click picks it." [s i]
  `(row :id ~(str "outline-" s.id)
     :fitContent height
     :width 276
     :gap 6
     :cursor hand
     :radius 4
     :role listitem
     :selected ~(= i state.current)
     :label ~(t "slide.n" {:n (+ i 1)})
     :color ~(if (= i state.current) "@rbChecked" "transparent")
     :hover {:color ~(if (= i state.current) "@rbChecked" "@rbHover")}
     :onClick ~(handler chooseSlide i "#outline")
     (label :id ~(str "outline-" s.id "-number")
       :width 34
       :height 18
       :padding 2
       :textColor @rbMuted
       :runs [{:text ~(text (+ i 1)) :size 12} {:icon presentation :size 11}])
     (label :id ~(str "outline-" s.id "-text")
       :width 230
       :wrap true
       :fitText {:maxWidth 230}
       :padding 2
       :textColor @rbInk
       :runs [{:text ~(if (= (get computed.outlineText i) "") (t "view.noText") (get computed.outlineText i))
               :size 12}])))

;;; What the format pane and the views do

(defn setProperty "Sets one property of the selected objects." [key value]
  (setObjects (assoc {} key value)))

(defn pickSwatch "A swatch chosen: the slide's background, or the selection's fill or line." [run key slot]
  (if (= run "setBackground") (setBackground slot) (setProperty key slot)))

(defn setEffect "Gives the selected objects an effect: none, shadow or glow." [effect]
  (setObjects {:effect (if (= effect "none") nil effect)}))

(defn openSlide "Opens a slide of the sorter in the Normal view." [index]
  (pickSlide index)
  (set "state.view" "normal"))

;;; Files

(defn fileStatus "Says what the last save, open or export did, on the File tab." [text]
  (set "state.fileStatus" text))

(defn saved "Says the deck was saved." [result]
  (fileStatus (t "file.saved" {:name result.name})))

(defn notSaved "Says why the deck wasn't saved." [error]
  (fileStatus (t "file.notSaved" {:why error.message})))

(defn saveDeck "Saves the deck as JSON, where the person chooses." []
  (saveFile state.deck :name (str computed.deckName ".json") :title (t "view.saveTheDeck") :onDone saved
    :onError notSaved))

(defn opened "Takes the deck just opened, by its file's name." [result]
  (deckOpened result.name))

(defn notOpened "Says why no deck was opened." [error]
  (fileStatus (t "file.notOpened" {:why error.message})))

(defn openDeck "Opens a deck the person chooses." []
  (openFile "state.opened" :extensions ["json"] :title (t "view.openADeck") :onDone opened :onError notOpened))

(defn deckOpened "Shows the deck opened from a file, if it is one, as a change Undo takes back." [name]
  (if (and (vector? state.opened.slides) (> (count state.opened.slides) 0) (number? state.opened.next))
    (do (commit)
        (set "state.deck" state.opened)
        (set "state.current" 0)
        (set "state.board.selection" [])
        (fileStatus (t "file.opened" {:name name})))
    (fileStatus (t "file.notADeck" {:name name})))
  (set "state.opened" nil))

(defn pdfExported "Says the PDF was exported, and how many pages it has." [result]
  (fileStatus (t "file.exportedPages" {:name result.name :pages result.pages})))

(defn pngExported "Says the picture was exported." [result]
  (fileStatus (t "file.exported" {:name result.name})))

(defn notExported "Says why nothing was exported." [error]
  (fileStatus (t "file.notExported" {:why error.message})))

(defn exportPdf "Exports the slides the show would show as a PDF, a page each." []
  (export "pdf"
          :pages (map (fn [it] (str "page-" (get-in state.deck.slides [it :id]))) computed.showOrder)
          :size [720 405]
          :title computed.deckName
          :onDone pdfExported
          :onError notExported))

(defn exportPng "Exports the slide shown as a PNG picture, at twice its size." []
  (export "png"
          :pages [(str "page-" computed.slide.id)]
          :scale 2
          :name (str computed.deckName " " (+ state.current 1))
          :onDone pngExported
          :onError notExported))

;;; The look

(defn toggleTheme "Switches the window between light and dark." []
  (set "state.theme" (if (= state.theme "dark") "light" "dark")))

(defn setBackground "Gives the slide a background colour, a theme slot." [slot]
  (commit)
  (set "state.deck.slides[state.current].background" slot))
