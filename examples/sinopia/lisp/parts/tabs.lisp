;;; tabs.lisp --- The ribbon's tabs, each a row of groups built from parts/ribbon.lisp
;;;
;;; Commands act on the current slide and its selected objects (parts/edit.lisp).

;;; Definitions

(defcomponent tabHome "The Home tab: the clipboard, the slides, the font, the paragraph, drawing, editing, voice."
  {}
  `(row#tab-home :fitContent width :height 92 :gap 4 :color transparent
     ~(g-clipboard)
     ~(g-slides)
     (node#g-font :use rbGroup :with {:title ~(t "ribbon.font") :launcher "launch-font"}
       (node#s-font :use rbStack
         ~(r-font-1)
         ~(r-font-2)))
     (node#g-paragraph :use rbGroup :with {:title ~(t "ribbon.paragraph") :launcher "launch-paragraph"}
       (node#s-paragraph :use rbStack
         ~(r-para-1)
         ~(r-para-2)))
     ~(g-drawing)
     (node#g-editing :use rbGroup :with {:title ~(t "ribbon.editing")}
       (node#s-editing :use rbStack
         (node#b-find :use rbSmall
           :with {:icon search :title ~(t "ribbon.find")}
           :tooltip ~(t "ribbon.findCtrlF")
           :onClick ~(handler openFind false))
         (node#b-replace :use rbSmall
           :with {:icon replace :title ~(t "ribbon.replace")}
           :tooltip ~(t "ribbon.replaceCtrlH")
           :onClick ~(handler openFind true))
         (node#b-select :use rbSmall
           :with {:icon mouse-pointer-2 :title ~(t "ribbon.select") :menu true}
           :tooltip ~(t "ribbon.select")
           :onClick ~(handler openDrop "b-select"))))
     (node#g-voice :use rbGroup :with {:title ~(t "ribbon.voice")}
       (node#b-dictate :use rbBig
         :with {:icon audio-lines :title ~(t "ribbon.speak") :menu true :width 50 :enabled false}
         :tooltip ~(t "ribbon.speakToTypeIsnT")))))

(defview g-slides "New Slide, Layout, Reset and Section." []
  `(node#g-slides :use rbGroup :with {:title ~(t "bar.slides")}
     (node#b-new-slide :use rbBig
       :with {:icon layout-panel-top :title ~(t "ribbon.newSlide") :menu true :width 50 :drop b-new-slide}
       :tooltip ~(t "bar.newSlideCtrlM")
       :onClick ~(handler newSlide "content"))
     (node#s-slides :use rbStack
       (node#b-layout :use rbSmall
         :with {:icon layout-template :title ~(t "ribbon.layout") :menu true}
         :tooltip ~(t "ribbon.layout")
         :onClick ~(handler openDrop "b-layout"))
       (node#b-reset :use rbSmall :with {:icon rotate-ccw :title ~(t "ribbon.reset")} :tooltip ~(t "ribbon.reset") :onClick resetSlide)
       (node#b-section :use rbSmall
         :with {:icon layout-list :title ~(t "ribbon.section") :menu true}
         :tooltip ~(t "ribbon.section")
         :onClick ~(handler openDrop "b-section")))))

(defview g-clipboard "Paste, Cut, Copy and Copy Style." []
  `(node#g-clipboard :use rbGroup :with {:title ~(t "ribbon.clipboard")}
     (node#b-paste :use rbBig
       :with {:icon clipboard-paste :title ~(t "ribbon.paste") :menu true :width 46 :drop b-paste}
       :tooltip ~(t "ribbon.pasteCtrlV")
       :onClick pasteObjects)
     (node#s-clipboard :use rbStack
       (node#b-cut :use rbSmall :with {:icon scissors :title ~(t "ribbon.cut")} :tooltip ~(t "ribbon.cutCtrlX") :onClick cutObjects)
       (node#b-copy :use rbSmall
         :with {:icon copy :title ~(t "ribbon.copy") :menu true :drop b-copy}
         :tooltip ~(t "ribbon.copyCtrlC")
         :onClick copyObjects)
       (node#b-painter :use rbSmall
         :with {:icon paint-roller :title ~(t "ribbon.copyStyle") :on ~(some? state.painter)}
         :tooltip ~(t "ribbon.copyStyleSelectAnObject")
         :onClick copyStyle))))

(defcomponent tabInsert "The Insert tab: slides, tables, pictures, shapes and icons, links, comments, text, symbols."
  {}
  `(row#tab-insert :fitContent width :height 92 :gap 4 :color transparent
     (node#g-ins-slides :use rbGroup :with {:title ~(t "bar.slides")}
       (node#b-ins-new-slide :use rbBig
         :with {:icon layout-panel-top :title ~(t "ribbon.newSlide") :menu true :width 50}
         :onClick ~(handler newSlide "content")))
     (node#g-ins-tables :use rbGroup :with {:title ~(t "ribbon.tables")}
       (node#b-ins-table :use rbBig
         :with {:icon table :title ~(t "ribbon.table") :width 46 :enabled false}
         :tooltip ~(t "ribbon.insertingATableIsnT")))
     (node#g-ins-images :use rbGroup :with {:title ~(t "ribbon.images")}
       (node#b-ins-pictures :use rbBig
         :with {:icon image :title ~(t "ribbon.pictures") :width 52 :enabled false}
         :tooltip ~(t "ribbon.insertingPicturesIsnTAvailable"))
       (node#b-ins-album :use rbBig
         :with {:icon images :title ~(t "ribbon.photoAlbum") :width 50 :enabled false}
         :tooltip ~(t "ribbon.aPhotoAlbumIsnT")))
     ~(g-ins-illustrations)
     ~(g-ins-links)
     (node#g-ins-comments :use rbGroup :with {:title ~(t "ribbon.comments")}
       (node#b-ins-comment :use rbBig
         :with {:icon message-square-plus :title ~(t "ribbon.comment") :width 56}
         :tooltip ~(t "ribbon.commentOnThisSlide")
         :onClick newComment))
     ~(g-ins-text)
     (node#g-ins-symbols :use rbGroup :with {:title ~(t "ribbon.symbols")}
       (node#b-ins-equation :use rbBig
         :with {:icon sigma :title ~(t "ribbon.equation") :width 56 :enabled false}
         :tooltip ~(t "ribbon.insertingAnEquationIsnT"))
       (node#b-ins-symbol :use rbBig
         :with {:icon omega :title ~(t "ribbon.symbol") :menu true :width 48}
         :onClick ~(handler openDrop "b-ins-symbol")))
     (node#g-ins-media :use rbGroup :with {:title ~(t "ribbon.media")}
       (node#b-ins-video :use rbBig
         :with {:icon film :title ~(t "ribbon.video") :width 44 :enabled false}
         :tooltip ~(t "ribbon.insertingVideoIsnTAvailable"))
       (node#b-ins-audio :use rbBig
         :with {:icon music :title ~(t "ribbon.audio") :width 44 :enabled false}
         :tooltip ~(t "ribbon.insertingAudioIsnTAvailable")))))

(defview g-ins-links "Link the selected text to a slide." []
  `(node#g-ins-links :use rbGroup :with {:title ~(t "ribbon.links")}
     (node#b-ins-link :use rbBig
       :with {:icon link
              :title ~(t "ribbon.link")
              :menu true
              :width 44
              :enabled ~(some (fn [it] (contains ["text" "shape"] (or-else it.kind "text"))) computed.selected)}
       :tooltip ~(t "ribbon.linkTheSelectedTextTo")
       :onClick ~(handler openDrop "b-ins-link"))))

(defview g-ins-illustrations "Shapes, icons and charts." []
  `(node#g-ins-illustrations :use rbGroup :with {:title ~(t "ribbon.illustrations")}
     (node#b-ins-shapes :use rbBig
       :with {:icon shapes :title ~(t "ribbon.shapes") :menu true :width 48}
       :onClick ~(handler openDrop "b-ins-shapes"))
     (node#b-ins-icons :use rbBig
       :with {:icon face-slightly-smiling :title ~(t "ribbon.icons") :menu true :width 44}
       :onClick ~(handler openDrop "b-ins-icons"))
     (node#b-ins-chart :use rbBig
       :with {:icon chart-column-stacked :title ~(t "ribbon.chart") :width 44 :enabled false}
       :tooltip ~(t "ribbon.insertingAChartIsnT"))))

(defview g-ins-text "A text box, a header and footer, word art and a symbol." []
  `(node#g-ins-text :use rbGroup :with {:title ~(t "object.newText")}
     (node#b-ins-textbox :use rbBig
                         :with {:icon text-cursor-input :title ~(t "ribbon.textBox") :width 44}
                         :onClick insertTextBox)
     (node#b-ins-header :use rbBig
       :with {:icon panel-top :title ~(t "ribbon.headerFooter") :menu true :width 52}
       :onClick ~(handler openDrop "b-ins-header"))
     (node#b-ins-art-text :use rbBig
       :with {:icon type :title ~(t "ribbon.artText") :menu true :width 44}
       :onClick ~(handler openDrop "b-ins-art-text"))))

(defcomponent tabDesign "The Design tab: the themes, their variants, the colours and fonts, and the background."
  {}
  `(row#tab-design :fitContent width :height 92 :gap 4 :color transparent
     (node#g-themes :use rbGroup :with {:title ~(t "ribbon.themes")}
       (node#gal-themes :use rbGallery :with {:width 490 :more gal-themes}
         ~(repeat [theme (slice computed.themes 0 6) :key theme.id]
           (theme-choice theme))))
     (node#g-variants :use rbGroup :with {:title ~(t "ribbon.variants")}
       (node#gal-variants :use rbGallery :with {:width 290}
         ~(repeat [v computed.theme.variants :index n]
           (variant-choice v n)))
       (column#variants-col :width 76 :height 72 :gap 2 :alignItems stretch :color transparent
         (node#b-colors :use rbSmall
           :with {:icon palette :title ~(t "ribbon.colors") :menu true}
           :tooltip ~(t "ribbon.anotherThemeSColoursFor")
           :onClick ~(handler openDrop "b-colors"))
         (node#b-fonts :use rbSmall
           :with {:icon type :title ~(t "ribbon.fonts") :menu true}
           :tooltip ~(t "ribbon.theFontsOfTheTitles")
           :onClick ~(handler openDrop "b-fonts"))))
     (node#g-customize :use rbGroup :with {:title ~(t "ribbon.customize")}
       (node#b-slide-size :use rbBig
         :with {:icon rectangle-horizontal :title ~(t "ribbon.slideSize") :width 46 :enabled false}
         :tooltip ~(t "ribbon.changingTheSlideSizeIsn"))
       (node#b-format-bg :use rbBig
                         :with {:icon paint-bucket :title ~(t "ribbon.formatBackground") :width 70}
                         :onClick formatBackground))))

(defcomponent tabTransitions "The Transitions tab: the preview, the transition to this slide, and its timing."
  {}
  `(row#tab-transitions :fitContent width :height 92 :gap 4 :color transparent
     (node#g-preview :use rbGroup :with {:title ~(t "ribbon.preview")}
       (node#b-preview :use rbBig
         :with {:icon play :title ~(t "ribbon.preview") :width 50}
         :tooltip ~(t "ribbon.playThisSlideSTransition")
         :onClick previewSlide))
     ~(g-transition-to)
     (node#g-timing :use rbGroup :with {:title ~(t "ribbon.timing")}
       ~(timing-col))))

(defcomponent tabView "The View tab: the views of the deck, what the canvas shows, and the panes."
  {}
  `(row#tab-view :fitContent width :height 92 :gap 4 :color transparent
     ~(g-views)
     (node#g-show :use rbGroup :with {:title ~(t "ribbon.show") :launcher "launch-show"}
       (column#show-col :width 90 :height 72 :gap 2 :alignItems stretch :color transparent
         (node#cb-ruler :use rbCheck :with {:title ~(t "ribbon.ruler") :at state.ruler :prefix cb-ruler})
         (node#cb-grid :use rbCheck :with {:title ~(t "ribbon.gridlines") :at state.gridlines :prefix cb-grid})
         (node#cb-guides :use rbCheck :with {:title ~(t "ribbon.guides") :at state.guides :prefix cb-guides})))
     (node#g-zoom :use rbGroup :with {:title ~(t "bar.zoom")}
       (node#b-zoom :use rbBig :with {:icon zoom-in :title ~(t "bar.zoom") :width 44} :onClick ~(handler zoomBy 1.25))
       (node#b-fit :use rbBig :with {:icon maximize :title ~(t "ribbon.fitToWindow") :width 50} :onClick fit))
     (node#g-notes-show :use rbGroup :with {:title ~(t "ribbon.panes")}
       (node#b-notes-toggle :use rbBig
         :with {:icon notebook-pen :title ~(t "bar.notes") :on ~state.notes :width 44}
         :onClick [{:toggle state.notes}])
       (node#b-format-pane :use rbBig
         :with {:icon paintbrush :title ~(t "ribbon.formatPane") :on ~state.formatPane :width 48}
         :onClick [{:toggle state.formatPane} {:set state.sidePane :to nil}]))
     (node#g-mode :use rbGroup :with {:title ~(t "ribbon.appearance")}
       (node#b-dark :use rbBig
         :with {:icon ~(if (= state.theme "dark") "sun" "moon")
                :title ~(if (= state.theme "dark") (t "ribbon.lightMode") (t "ribbon.darkMode"))
                :width 48}
         :onClick toggleTheme))))

(defcomponent tabFile "The File tab: open and save the deck, export it, and what the last of those did."
  {}
  `(row#tab-file :fitContent width :height 92 :gap 4 :color transparent
     (node#g-file :use rbGroup :with {:title ~(t "ribbon.deck")}
       (node#b-open :use rbBig
         :with {:icon folder-open :title ~(t "ribbon.open") :width 46}
         :tooltip ~(t "ribbon.openADeckCtrlO")
         :onClick openDeck)
       (node#b-save :use rbBig
                    :with {:icon save :title ~(t "ribbon.save") :width 46}
                    :tooltip ~(t "ribbon.saveTheDeckCtrlS")
                    :onClick saveDeck))
     (node#g-export :use rbGroup :with {:title ~(t "ribbon.export")}
       (node#b-export-pdf :use rbBig
         :with {:icon file-text :title "PDF" :width 46}
         :tooltip ~(t "ribbon.everySlideShownAPage")
         :onClick exportPdf)
       (node#b-export-png :use rbBig
         :with {:icon file-image :title ~(t "ribbon.picture") :width 50}
         :tooltip ~(t "ribbon.thisSlideAsAPng")
         :onClick exportPng))
     (node#g-file-info :use rbGroup :with {:title ~(t "ribbon.last")}
       (label#file-status :width 220
         :height 72
         :wrap true
         :padding 4
         :textColor @rbMuted
         :runs [{:text ~(if (= state.fileStatus "") (t "ribbon.nothingSavedOrExportedYet") state.fileStatus) :size 12}]))))

(defcomponent tabMotion "The Motion tab: the preview, the builds' effects, their timing, and the animation pane."
  {}
  `(row#tab-motion :fitContent width
     :height 92
     :gap 4
     :color transparent
     :let {:a ~computed.animation :on ~(> (count computed.selected) 0)}
     (node#g-motion-preview :use rbGroup :with {:title ~(t "ribbon.preview")}
       (node#b-motion-preview :use rbBig
         :with {:icon play :title ~(t "ribbon.preview") :width 50}
         :tooltip ~(t "ribbon.playThisSlideSTransition")
         :onClick previewSlide))
     ~(g-effects)
     (node#g-motion-timing :use rbGroup :with {:title ~(t "ribbon.timing")}
       ~(motion-timing-col))
     (node#g-motion-advanced :use rbGroup :with {:title ~(t "ribbon.advanced")}
       (node#b-motion-pane :use rbBig
         :with {:icon list-ordered
                :title ~(t "ribbon.animationPane")
                :on ~(and (= state.sidePane "motion") (not state.formatPane))
                :width 64}
         :tooltip ~(t "ribbon.theSlideSBuildsIn")
         :onClick ~(handler openSidePane "motion")))))

(defcomponent rbCheck "A checkbox bound to the state path at, and its title, which toggles it too."
  {:title "" :at "" :prefix ""}
  `(row :height 22 :gap 6 :alignItems center :color transparent
     (checkbox :id ~(str prefix "-box") :width 14 :height 14 :bind ~at :label ~title)
     (label :id ~(str prefix "-caption")
       :fitText true
       :height 20
       :cursor hand
       :textColor @rbInk
       :runs [{:text ~title :size 12}]
       :onClick ~(handler toggleAt at))))

(defcomponent tabPresent "The Present tab: start the show, hide a slide, the presenter view, the reading view."
  {}
  `(row#tab-present :fitContent width :height 92 :gap 4 :color transparent
     (node#g-start :use rbGroup :with {:title ~(t "ribbon.startSlideShow")}
       (node#b-from-start :use rbBig
         :with {:icon presentation :title ~(t "ribbon.fromBeginning") :width 60}
         :tooltip ~(t "ribbon.fromTheStartF5")
         :onClick ~(handler present 0))
       (node#b-from-current :use rbBig
         :with {:icon projector :title ~(t "ribbon.fromThisSlide") :width 60}
         :tooltip ~(t "ribbon.fromThisSlideShiftF5")
         :onClick ~(handler present state.current)))
     (node#g-set-up :use rbGroup :with {:title ~(t "ribbon.setUp")}
       (node#b-hide-slide :use rbBig
         :with {:icon eye-off :title ~(t "ribbon.hideSlide") :on ~(or-else computed.slide.hidden false) :width 46}
         :tooltip ~(t "ribbon.leaveThisSlideOutOf")
         :onClick hideSlide))
     (node#g-monitors :use rbGroup :with {:title ~(t "ribbon.monitors")}
       (column#monitors-col :width 150 :height 72 :gap 2 :alignItems stretch :color transparent
         (node#cb-presenter :use rbCheck
           :with {:title ~(t "ribbon.presenterView") :at state.presenterView :prefix cb-presenter}
           :tooltip ~(t "ribbon.theShowInAWindow"))))))

;;; Views

(defview r-font-1 "The font, its size, larger, smaller, and clear the formatting." []
  `(node#r-font-1 :use rbRow
     (node#f-font :use rbField :with {:text ~computed.fontTitle :width 150 :drop f-font} :tooltip ~(t "ribbon.font"))
     (node#f-size :use rbField :with {:text ~(text computed.textSize) :width 46 :drop f-size} :tooltip ~(t "ribbon.fontSize"))
     (node#b-grow :use rbSmall
       :with {:icon a-arrow-up}
       :tooltip ~(t "ribbon.increaseFontSizeCtrlShift")
       :onClick ~(handler growText 1))
     (node#b-shrink :use rbSmall
       :with {:icon a-arrow-down}
       :tooltip ~(t "ribbon.decreaseFontSizeCtrlShift")
       :onClick ~(handler growText -1))
     (node#b-clear :use rbSmall
                   :with {:icon remove-formatting}
                   :tooltip ~(t "ribbon.clearAllFormatting")
                   :onClick clearFormatting)))

(defview r-font-2 "Bold, italic, underline, shadow, strikethrough, spacing, case, highlight and colour." []
  `(node#r-font-2 :use rbRow
     (node#b-bold :use rbSmall
       :with {:icon bold :on ~(and (some? computed.first) computed.allOn.bold)}
       :tooltip ~(t "ribbon.boldCtrlB")
       :onClick ~(handler toggleText "bold"))
     (node#b-italic :use rbSmall
       :with {:icon italic :on ~(and (some? computed.first) computed.allOn.italic)}
       :tooltip ~(t "ribbon.italicCtrlI")
       :onClick ~(handler toggleText "italic"))
     (node#b-underline :use rbSmall
       :with {:icon underline :on ~(and (some? computed.first) computed.allOn.underline)}
       :tooltip ~(t "ribbon.underlineCtrlU")
       :onClick ~(handler toggleText "underline"))
     (node#b-shadow :use rbSmall
       :with {:icon type-outline :on ~(and (some? computed.first) computed.allOn.shadow)}
       :tooltip ~(t "ribbon.textShadow")
       :onClick ~(handler toggleText "shadow"))
     (node#b-strike :use rbSmall
       :with {:icon strikethrough :on ~(and (some? computed.first) computed.allOn.strike)}
       :tooltip ~(t "ribbon.strikethrough")
       :onClick ~(handler toggleText "strike"))
     (node#b-spacing :use rbSmall
       :with {:icon move-horizontal :menu true :enabled false}
       :tooltip ~(t "ribbon.characterSpacingIsnTAvailable"))
     (node#b-case :use rbSmall
       :with {:icon case-sensitive :menu true}
       :tooltip ~(t "ribbon.changeCase")
       :onClick ~(handler openDrop "b-case"))
     (node#b-highlight :use rbSmall
       :with {:icon highlighter :menu true :tint ~(get state.lastColor "b-highlight") :drop b-highlight :colors true}
       :tooltip ~(t "ribbon.textHighlightColor")
       :onClick ~(handler applyColor "b-highlight" (get state.lastColor "b-highlight")))
     (node#b-color :use rbSmall
       :with {:icon baseline :menu true :tint ~(get state.lastColor "b-color") :drop b-color :colors true}
       :tooltip ~(t "ribbon.fontColor")
       :onClick ~(handler applyColor "b-color" (get state.lastColor "b-color")))))

(defview r-para-1 "Bullets, numbering, list levels, line spacing and columns." []
  `(node#r-para-1 :use rbRow
     (node#b-bullets :use rbSmall
       :with {:icon list
              :menu true
              :drop b-bullets
              :on ~(and (some? computed.first) computed.allOn.bullets (nil? computed.first.numbering))}
       :tooltip ~(t "ribbon.bullets")
       :onClick ~(handler toggleText "bullets"))
     (node#b-numbering :use rbSmall
       :with {:icon list-ordered :menu true :drop b-numbering :on ~(some? computed.first.numbering)}
       :tooltip ~(t "ribbon.numbering")
       :onClick toggleNumbering)
     (node#b-outdent :use rbSmall
       :with {:icon indent-decrease}
       :tooltip ~(t "ribbon.decreaseListLevel")
       :onClick ~(handler indentText -1))
     (node#b-indent :use rbSmall
       :with {:icon indent-increase}
       :tooltip ~(t "ribbon.increaseListLevel")
       :onClick ~(handler indentText 1))
     (node#b-line-spacing :use rbSmall
       :with {:icon list-chevrons-up-down :menu true}
       :tooltip ~(t "ribbon.lineSpacing")
       :onClick ~(handler openDrop "b-line-spacing"))
     (node#b-columns :use rbSmall
       :with {:icon columns-2 :menu true}
       :tooltip ~(t "ribbon.addOrRemoveColumns")
       :onClick ~(handler openDrop "b-columns"))))

(defview r-para-2 "The text's alignment, its direction, its place in the box." []
  `(node#r-para-2 :use rbRow
     ~(repeat [a [["left" "align-left" (t "ribbon.alignLeftCtrlL")] ["center" "align-center" (t "ribbon.centerCtrlE")] ["right" "align-right" (t "ribbon.alignRightCtrlR")] ["justify" "align-justify" (t "ribbon.justifyCtrlJ")]]]
       `(node :id ~(str "b-align-" a.0)
          :use rbSmall
          :with {:icon ~a.1
                 :on ~(and (some? computed.first)
                           (= (or-else computed.first.align computed.firstPlaceholder.align "left") a.0))}
          :tooltip ~a.2
          :onClick ~(handler setAlign a.0)))
     (node#b-direction :use rbSmall
       :with {:icon arrow-down-up :menu true}
       :tooltip ~(t "ribbon.textDirection")
       :onClick ~(handler openDrop "b-direction"))
     (node#b-anchor :use rbSmall
       :with {:icon align-vertical-justify-center :menu true}
       :tooltip ~(t "ribbon.alignText")
       :onClick ~(handler openDrop "b-anchor"))
     (node#b-smart :use rbSmall
       :with {:icon network :menu true :enabled false}
       :tooltip ~(t "ribbon.convertToADiagramIsn"))))

(defview gallery-shape-picture "Shape `k` drawn small, in the gallery." [k]
  `(node :id ~(str "gal-shape-" k "-box")
     :color transparent
     :when ~(not= k "textBox")
     :x 5
     :y 4
     :width 14
     :height 13
     :overflow visible
     (node :use shapePath
       :with {:o ~{:shape k
                   :w 14
                   :h (if (get-in computed.shapes [k :line]) 0 13)
                   :fill "none"
                   :line "#444444"
                   :lineWidth 1
                   :flipH false
                   :flipV false
                   :gradient nil
                   :dash nil
                   :effect nil}
              :name ~(str "gal-shape-" k)})))

(defview gallery-shape "Shape `k` of the gallery, or a text box: a click adds one to the slide." [k]
  `(panel :id ~(str "gal-shape-" k)
     :width 24
     :height 21
     :radius 2
     :color transparent
     :cursor hand
     :hover {:color @rbHover}
     :tooltip ~(if (= k "textBox") (t "ribbon.textBox2") (t (str "shape." k)))
     :onClick ~(handler shapeChosen k)
     (label :id ~(str "gal-shape-" k "-tb")
       :when ~(= k "textBox")
       :fill 0
       :align center
       :textColor @rbInk
       :runs [{:icon text-cursor-input :size 14}])
     ~(gallery-shape-picture k)))

(defview g-drawing "The shape gallery, Arrange, Shape Styles, and the fill, outline and effects." []
  `(node#g-drawing :use rbGroup :with {:title ~(t "ribbon.drawing") :launcher "launch-drawing"}
     (node#gal-shapes :use rbGallery :with {:width 168 :more gal-shapes}
       (grid#gal-shapes-grid :columns 6 :width 148 :height 66 :gap 1 :color transparent
         ~(repeat [k ["textBox" "rect" "ellipse" "triangle" "rtTriangle" "line" "lineArrow" "roundRect" "rightArrow" "downArrow" "star5" "curvedRightArrow" "leftBrace" "rightBrace" "smileyFace" "heart" "cloud" "wedgeRoundRectCallout"] :index n]
           (gallery-shape k))))
     (node#b-arrange :use rbBig
       :with {:icon layers-2 :title ~(t "ribbon.arrange") :menu true :width 52}
       :tooltip ~(t "ribbon.arrange")
       :onClick ~(handler openDrop "b-arrange"))
     (node#b-quick-styles :use rbBig
       :with {:icon swatch-book :title ~(t "ribbon.shapeStyles") :menu true :width 52}
       :tooltip ~(t "ribbon.shapeStyles2")
       :onClick ~(handler openColors "b-quick-styles"))
     (node#s-drawing :use rbStack
       (node#b-fill :use rbSmall
         :with {:icon paint-bucket :title ~(t "ribbon.shapeFill") :menu true :drop b-fill :colors true}
         :tooltip ~(t "ribbon.shapeFill")
         :onClick ~(handler applyColor "b-fill" (get state.lastColor "b-fill")))
       (node#b-outline :use rbSmall
         :with {:icon pen-tool :title ~(t "ribbon.shapeOutline") :menu true :drop b-outline :colors true}
         :tooltip ~(t "ribbon.shapeOutline")
         :onClick ~(handler applyColor "b-outline" (get state.lastColor "b-outline")))
       (node#b-effects :use rbSmall
         :with {:icon sparkles :title ~(t "ribbon.shapeEffects") :menu true}
         :tooltip ~(t "ribbon.shapeEffects")
         :onClick ~(handler openDrop "b-effects")))))

(defview theme-choice "Theme `theme` in the gallery: its background, Aa in its text colour, and its accents." [theme]
  `(panel :id ~(str "theme-" theme.id)
     :width 76
     :height 60
     :radius 2
     :cursor hand
     :tooltip ~(t (str "theme." theme.id))
     :color ~(get theme.colors theme.background)
     :borderWidth ~(if (= state.deck.theme theme.id) 2 1)
     :borderColor ~(if (= state.deck.theme theme.id) "@brand" "@rbFieldLine")
     :hover {:borderColor @brand}
     :onClick ~(handler setTheme theme.id)
     (label :id ~(str "theme-" theme.id "-aa")
       :x 8
       :y 10
       :width 40
       :height 24
       :textColor ~theme.colors.dark1
       :runs [{:text "Aa" :size 18}])
     (row :id ~(str "theme-" theme.id "-bar") :x 8 :y 42 :width 60 :height 8 :color transparent
       ~(repeat [c ["accent1" "accent2" "accent3" "accent4"]]
         `{:id ~(str "theme-" theme.id "-" c) :width 15 :height 8 :color ~(get theme.colors c)}))))

(defview variant-choice "Variant `v` of the theme, at `n`: its background, an accent band and two lines." [v n]
  `(panel :id ~(str "variant-" n)
     :width 62
     :height 60
     :radius 2
     :cursor hand
     :color ~(get (merge computed.theme.colors v) computed.theme.background)
     :borderWidth ~(if (= (or-else state.deck.variant 0) n) 2 1)
     :borderColor ~(if (= (or-else state.deck.variant 0) n) "@brand" "@rbFieldLine")
     :hover {:borderColor @brand}
     :onClick ~(handler setVariant n)
     {:id ~(str "variant-" n "-band")
      :x 0
      :y 0
      :width 10
      :height 60
      :color ~(:accent1 (merge computed.theme.colors v))}
     {:id ~(str "variant-" n "-line") :x 18 :y 22 :width 34 :height 5 :color ~(:dark1 (merge computed.theme.colors v))}
     {:id ~(str "variant-" n "-line2")
      :x 18
      :y 32
      :width 24
      :height 3
      :color ~(:accent2 (merge computed.theme.colors v))}))

(defview g-transition-to "The transitions, and the side the transition comes from." []
  `(node#g-transition-to :use rbGroup :with {:title ~(t "ribbon.transitionToThisSlide")}
     (node#gal-transitions :use rbGallery :with {:width 520}
       ~(repeat [k [["none" "ban" (t "ribbon.none")] ["glide" "blend" (t "ribbon.glide")] ["fade" "sun-dim" (t "ribbon.fade")] ["push" "arrow-left-to-line" (t "ribbon.push")] ["wipe" "chevrons-left" (t "ribbon.wipe")] ["split" "columns-2" (t "ribbon.split")] ["reveal" "arrow-left-from-line" (t "ribbon.reveal")] ["cover" "square-stack" (t "ribbon.cover")]]]
         `(label :id ~(str "tr-" k.0)
            :width 60
            :height 64
            :wrap true
            :align {:horizontal center :vertical center}
            :style ~(if (= (or-else computed.slide.transition.kind "none") k.0) "rbButtonOn" "rbButton")
            :runs ~[{:icon k.1 :size 24} {:text (str "\n" k.2) :size 11}]
            :onClick ~(handler setTransitionKind k.0))))
     (node#b-effect-options :use rbBig
       :with {:icon ~(get computed.directionIcons (or-else computed.slide.transition.direction "left"))
              :title ~(t "ribbon.fromSide" {:side (or-else computed.slide.transition.direction "left")})
              :width 52
              :enabled ~(contains ["push" "wipe" "reveal" "cover"] (or-else computed.slide.transition.kind "none"))}
       :tooltip ~(t "ribbon.theSideTheTransitionComes")
       :onClick nextDirection)))

(defview timing-col "The transition's duration, Apply To All, and On Mouse Click." []
  `(column#timing-col :width 200 :height 72 :gap 2 :alignItems stretch :color transparent
     (row#timing-row :height 22 :gap 2 :alignItems center :color transparent
       (label#timing-duration :width 120
         :height 22
         :textColor @rbInk
         :runs ~[{:icon "clock-3" :size 14}
                 {:text (t "ribbon.duration" {:seconds (fixed (/ (or-else computed.slide.transition.ms 500) 1000) 2)}) :size 12}])
       (node#timing-shorter :use rbSmall
                            :with {:icon minus}
                            :tooltip ~(t "ribbon.shorter")
                            :onClick ~(handler durationBy -250))
       (node#timing-longer :use rbSmall :with {:icon plus} :tooltip ~(t "ribbon.longer") :onClick ~(handler durationBy 250)))
     (label#timing-apply :height 22
       :style rbButton
       :padding 2
       :fitText true
       :runs [{:icon copy-check :size 14} {:text ~(t "ribbon.applyToAll") :size 12}]
       :tooltip ~(t "ribbon.giveEverySlideThisTransition")
       :onClick transitionToAll)
     (label#timing-advance :height 22
       :textColor @rbInk
       :runs [{:icon check-square :size 14} {:text ~(t "ribbon.onMouseClick") :size 12}])))

(defview g-views "The views of the deck: Normal, Outline, Slide Sorter, and Notes Page." []
  `(node#g-views :use rbGroup :with {:title ~(t "ribbon.views")}
     (node#b-normal :use rbBig
       :with {:icon panel-left :title ~(t "bar.normal") :on ~(= state.view "normal") :width 46}
       :onClick [{:set state.view :to normal}])
     (node#b-outline-view :use rbBig
       :with {:icon list-tree :title ~(t "ribbon.outlineView") :on ~(= state.view "outline") :width 48}
       :onClick [{:set state.view :to outline}])
     (node#b-sorter :use rbBig
       :with {:icon layout-grid :title ~(t "ribbon.slideSorter") :on ~(= state.view "sorter") :width 46}
       :onClick [{:set state.view :to sorter}])
     (node#b-notes-page :use rbBig
       :with {:icon notebook-text :title ~(t "ribbon.notesPage") :width 46 :enabled false}
       :tooltip ~(t "ribbon.theNotesPageViewIsn"))
     (node#b-reading :use rbBig
       :with {:icon book-open :title ~(t "ribbon.readingView") :width 50}
       :onClick ~(handler readingView state.current))))

(defview g-effects "The build effects for the selected objects, and As Exit." []
  `(node#g-effects :use rbGroup :with {:title ~(if (= a.kind "exit") (t "ribbon.exit") (t "ribbon.entrance"))}
     (node#gal-effects :use rbGallery :with {:width 380}
       ~(repeat [e [["none" "ban" (t "ribbon.none")] ["appear" "square" (t "ribbon.appear")] ["fade" "sun-dim" (t "ribbon.fade")] ["wipe" "chevrons-right" (t "ribbon.wipe")] ["fly" "move-up" (t "ribbon.fly")] ["zoom" "zoom-in" (t "bar.zoom")]]]
         `(label :id ~(str "fx-" e.0)
            :width 60
            :height 64
            :wrap true
            :align {:horizontal center :vertical center}
            :style ~(cond (not on) "rbButtonOff" (= (or-else a.effect "none") e.0) "rbButtonOn" :else "rbButton")
            :runs ~[{:icon e.1 :size 24} {:text (str "\n" e.2) :size 11}]
            :onClick ~(handler setAnimation e.0 (or-else a.kind "entrance") (or-else a.start "click") (or-else a.ms 500)))))
     (node#b-exit :use rbBig
       :with {:icon log-out :title ~(t "ribbon.asExit") :on ~(= a.kind "exit") :enabled ~(some? a) :width 46}
       :tooltip ~(t "ribbon.leaveTheSlideInsteadOf")
       :onClick toggleExit)))

(defview motion-timing-col "When the build starts, its duration, and how many builds the slide has." []
  `(column#motion-timing-col :width 200 :height 72 :gap 2 :alignItems stretch :color transparent
     (row#motion-start-row :height 22 :gap 2 :alignItems center :color transparent
       (label#motion-start-title :width 44 :height 22 :textColor @rbInk :runs [{:text ~(t "ribbon.start") :size 12}])
       ~(repeat [w [["click" (t "ribbon.click")] ["with" (t "ribbon.with")] ["after" (t "ribbon.after")]]]
         `(label :id ~(str "motion-start-" w.0)
            :height 22
            :fitText true
            :padding 4
            :style ~(cond (nil? a) "rbButtonOff" (= a.start w.0) "rbButtonOn" :else "rbButton")
            :runs [{:text ~w.1 :size 12}]
            :onClick ~(handler animationStart w.0))))
     (row#motion-duration-row :height 22 :gap 2 :alignItems center :color transparent
       (label#motion-duration :width 120
         :height 22
         :textColor @rbInk
         :runs ~[{:icon "clock-3" :size 14} {:text (t "ribbon.duration" {:seconds (fixed (/ (or-else a.ms 500) 1000) 2)}) :size 12}])
       (node#motion-shorter :use rbSmall
         :with {:icon minus :enabled ~(some? a)}
         :tooltip ~(t "ribbon.shorter")
         :onClick ~(handler animationDurationBy -250))
       (node#motion-longer :use rbSmall
         :with {:icon plus :enabled ~(some? a)}
         :tooltip ~(t "ribbon.longer")
         :onClick ~(handler animationDurationBy 250)))
     (label#motion-count :height 22
       :textColor @rbMuted
       :runs [{:text ~(t "ribbon.buildCount" {:n (count (or-else computed.slide.animations []))
                                               :clicks (count (filter (fn [it] (= it.start "click"))
                                                                      (or-else computed.slide.animations [])))})
               :size 12}])))

;;; What the ribbon does

(defn toggleAt "Turns a setting of the state on or off, by its path." [at]
  (toggle at))

(defn setAlign "Aligns the selected text: left, center, right or justify." [align]
  (setObjects {:align align}))

(defn toggleNumbering "Numbers the selected text's lines, or stops numbering them." []
  (setObjects (if (nil? computed.first.numbering) {:bullets true :numbering "1."} {:numbering nil})))

(defn shapeChosen "Adds a shape of the gallery to the slide, or a text box." [k]
  (if (= k "textBox") (insertTextBox) (insertShape k)))

(defn setTransitionKind "Gives the slide a transition." [kind]
  (setTransition {:kind kind}))

(defn nextDirection "Turns the side the transition comes from to the next: left, up, right, down." []
  (let [now (or-else computed.slide.transition.direction "left")]
    (setTransition {:direction (get computed.directions (mod (inc (indexOf computed.directions now)) 4))})))

(defn toggleExit "Makes the selected objects' build an exit, or an entrance again." []
  (changeAnimation {:kind (if (= computed.animation.kind "exit") "entrance" "exit")}))

(defn animationStart "Starts the selected objects' build on a click, with the one before, or after it." [start]
  (changeAnimation {:start start}))

(defn animationDurationBy "Makes the selected objects' build longer or shorter, by milliseconds." [by]
  (changeAnimation {:ms (clamp (+ (or-else computed.animation.ms 500) by) 0 59000)}))
