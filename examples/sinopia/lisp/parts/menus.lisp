;;; menus.lisp --- The ribbon's drop-downs
;;;
;;; A button's arrow (or a button that is only a menu) runs openDrop with its own id: one menu opens below it, its
;;; items from computed['menu-' + id]. The colour buttons open a grid instead (openColors, computed['colors-' + id]);
;;; their body applies the last colour picked. Find and Replace open a small box below their button; Copy Style keeps
;;; the selection's style and gives it to the next objects selected.

;;; Definitions

(defstate drop {:open false :id ""} "The drop-down open, and the button it belongs to.")

(defstate search "")

(defstate colors {:open false :id ""} "The colour grid open, and its button.")

(defstate lastColor {:b-color "#c00000" :b-highlight "#ffff00" :b-fill accent1
                     :b-outline dark1} "The colour each colour button applies when its body is clicked.")

(defstate painter nil "The look Copy Style keeps, or nil.")

(defstate find {:open false :replace false :text "" :with "" :status ""
                :ask ""}
  "The find box: open, replacing, its text and replacement, its status, and what else it asks for.")

(defstate ctx {:open false :x 0 :y 0
               :kind ""} "The context menu open, where, and for what: a slide, an object or the canvas.")

(defcomputed anyText "Whether something is selected, so a text command can act." (> (count computed.selected) 0))

(defcomputed menuSizes "The Font Size menu's sizes." [8 9 10 11 12 14 16 18 20 24 28 32 36 40 48 54 60 72 80 96])

(defcomputed fontNames "The fonts a text can take, by id." '[[default "Roboto"] [mono "JetBrains Mono"]])

(defcomputed fontTitle "The font box's text: the selection's font."
  (cond (= (or-else computed.first.font "default") "mono") "JetBrains Mono"
        (or computed.titleLike (= computed.first.ph "title") (= computed.first.ph "ctrTitle")) (t "menu.robotoHeadings")
        :else (t "menu.robotoBody")))

(defcomputed dropItems "The items of the drop-down open: computed['menu-' + its button]."
  (if state.drop.open (or-else (get computed (str "menu-" state.drop.id)) []) []))

(defcomputed menu-b-paste "The Paste menu."
  [{:title (t "ribbon.paste")
    :icon "clipboard-paste"
    :keyLabel "Ctrl+V"
    :run "pasteObjects"
    :enabled (> (count state.clip) 0)}
   {:title (t "menu.keepTextOnly") :icon "type" :run "pasteText" :enabled (> (count state.clip) 0)}
   {:title (t "menu.duplicate")
    :icon "copy-plus"
    :keyLabel "Ctrl+D"
    :run "duplicateObjects"
    :enabled computed.anyText}])

(defcomputed menu-b-copy "The Copy menu."
  [{:title (t "ribbon.copy") :icon "copy" :keyLabel "Ctrl+C" :run "copyObjects" :enabled computed.anyText}
   {:title (t "menu.duplicate")
    :icon "copy-plus"
    :keyLabel "Ctrl+D"
    :run "duplicateObjects"
    :enabled computed.anyText}])

(defcomputed menu-b-new-slide "The New Slide menu: a slide of each layout, or a copy of this one."
  (concat (map (fn [it]
                 {:title (t (str "layout." it.id)) :icon "layout-panel-top" :run "newSlide" :with {:layout it.id}})
               computed.layouts)
          [{:separator true} {:title (t "menu.duplicateSlide") :icon "copy" :run "duplicateSlide"}]))

(defcomputed menu-b-ins-new-slide "The Insert tab's New Slide menu: the Home tab's." (get computed "menu-b-new-slide"))

(defcomputed ctxItems "The context menu's items, by what it is for."
  (cond (= state.ctx.kind "slide") computed.slideCtxItems
        (> (count computed.selected) 0) computed.objectCtxItems
        :else computed.canvasCtxItems))

(defcomputed slideCtxItems "A slide's context menu."
  [{:title (t "menu.newSlide")
    :icon "layout-panel-top"
    :keyLabel "Ctrl+M"
    :run "newSlide"
    :with {:layout "content"}}
   {:title (t "menu.duplicateSlide") :icon "copy-plus" :run "duplicateSlide"}
   {:title (t "menu.deleteSlide") :icon "trash-2" :run "deleteSlide" :enabled (> (count state.deck.slides) 1)}
   {:separator true}
   {:title (t "menu.addSection") :icon "list-start" :run "addSection"}
   {:separator true}
   {:title (t "menu.resetSlide") :icon "rotate-ccw" :run "resetSlide"}
   {:title (t "menu.formatBackground") :icon "paint-bucket" :run "formatBackground"}
   {:title (if (or-else computed.slide.hidden false) (t "menu.showSlide") (t "menu.hideSlide"))
    :icon "eye-off"
    :checked (or-else computed.slide.hidden false)
    :run "hideSlide"}])

(defcomputed objectCtxItems "The selected objects' context menu."
  [{:title (t "ribbon.cut") :icon "scissors" :keyLabel "Ctrl+X" :run "cutObjects"}
   {:title (t "ribbon.copy") :icon "copy" :keyLabel "Ctrl+C" :run "copyObjects"}
   {:title (t "ribbon.paste")
    :icon "clipboard-paste"
    :keyLabel "Ctrl+V"
    :run "pasteObjects"
    :enabled (> (count state.clip) 0)}
   {:separator true}
   {:title (t "arrange.front") :icon "bring-to-front" :run "order" :with {:how "front"}}
   {:title (t "arrange.back") :icon "send-to-back" :run "order" :with {:how "back"}}
   {:separator true}
   {:title (t "menu.formatShape") :icon "paintbrush" :run "openFormatPane"}
   {:title (t "menu.delete") :icon "trash-2" :keyLabel "Del" :run "deleteObjects"}])

(defcomputed canvasCtxItems "The canvas's context menu."
  [{:title (t "ribbon.paste")
    :icon "clipboard-paste"
    :keyLabel "Ctrl+V"
    :run "pasteObjects"
    :enabled (> (count state.clip) 0)}
   {:separator true}
   {:title (t "menu.resetSlide") :icon "rotate-ccw" :run "resetSlide"}
   {:title (t "menu.formatBackground") :icon "paint-bucket" :run "formatBackground"}])

(defcomputed menu-b-ins-icons "The Icons menu: each adds an icon to the slide."
  (map (fn [it]
         {:title it.0
          :icon it.1
          :run "insertObject"
          :with {:object {:kind "icon" :icon it.1 :x 420 :y 210 :w 120 :h 120 :color "accent1"}}})
       [[(t "menu.star") "star"]
        [(t "menu.heart") "heart"]
        [(t "menu.lightbulb") "lightbulb"]
        [(t "menu.flag") "flag"]
        [(t "menu.calendar") "calendar"]
        [(t "menu.clock") "clock"]
        [(t "menu.mapPin") "map-pin"]
        [(t "menu.people") "users"]
        [(t "menu.chat") "message-circle"]
        [(t "bar.check") "circle-check"]
        [(t "menu.warning") "triangle-alert"]
        [(t "menu.music") "music"]]))

(defcomputed menu-b-ins-art-text "The Art Text menu: each adds a styled text box."
  (map (fn [it]
         {:title it.0
          :icon "type"
          :run "insertObject"
          :with {:object {:kind "text"
                          :x 180
                          :y 210
                          :w 600
                          :h 120
                          :text (t "menu.yourTextHere")
                          :size 54
                          :bold true
                          :align "center"
                          :anchor "middle"
                          :color it.1
                          :fill (or-else it.2 "none")
                          :shadow it.3}}})
       [[(t "menu.accentBold") "accent1" nil false]
        [(t "menu.accentBoldShadow") "accent1" nil true]
        [(t "menu.darkBoldShadow") "dark1" nil true]
        [(t "menu.lightOnAccent") "light1" "accent1" false]
        [(t "menu.accent2OnLight") "accent2" "light2" false]]))

(defcomputed menu-b-ins-symbol "The Symbol menu: each adds a symbol to the text."
  (map (fn [it] {:title it :run "insertSymbol" :with {:symbol it}})
       ["©" "®" "™" "€" "£" "¥" "°" "±" "×" "÷" "≈" "≠"
        "≤" "≥" "→" "←" "↑" "↓" "•" "…" "§" "¶" "π" "Ω"]))

(defcomputed menu-b-ins-header "The Header & Footer menu."
  [{:title (t "menu.slideNumbers")
    :icon "hash"
    :checked (or-else state.deck.slideNumbers false)
    :run "toggleSlideNumbers"}
   {:title (if (nil? (or-else state.deck.footer nil))
               (t "menu.footerText")
               (t "menu.footerIs" {:text state.deck.footer}))
    :icon "panel-bottom"
    :run "askFooter"}])

(defcomputed menu-b-layout "The Layout menu: the slide's layout checked."
  (map (fn [it]
         {:title (t (str "layout." it.id))
          :checked (= computed.slide.layout it.id)
          :run "setLayout"
          :with {:layout it.id}})
       computed.layouts))

(defcomputed menu-b-section "The Section menu."
  [{:title (t "menu.addSection") :icon "list-plus" :run "addSection"}
   {:title (t "menu.renameSection") :icon "pencil" :run "askSectionName" :enabled (some? computed.sectionHere)}
   {:title (t "menu.removeSection") :icon "list-minus" :run "removeSection" :enabled (some? computed.sectionHere)}
   {:separator true}
   {:title (t "menu.removeAllSections")
    :icon "list-x"
    :run "removeSections"
    :enabled (> (count (or-else state.deck.sections [])) 0)}])

(defcomputed menu-f-font "The Font menu: the selection's font checked."
  (map (fn [it]
         {:title it.1
          :checked (= (or-else computed.first.font "default") it.0)
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:font (if (= it.0 "default") nil it.0)}}})
       computed.fontNames))

(defcomputed menu-f-size "The Font Size menu: the selection's size checked."
  (map (fn [it]
         {:title (text it)
          :checked (= computed.textSize it)
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:size it}}})
       computed.menuSizes))

(defcomputed menu-b-case "The Change Case menu."
  (map (fn [it] {:title it.0 :enabled computed.anyText :run "changeCase" :with {:how it.1}})
       [[(t "menu.sentenceCase") "sentence"]
        ["lowercase" "lower"]
        ["UPPERCASE" "upper"]
        [(t "menu.capitalizeEachWord") "words"]
        [(t "menu.toggleCase") "toggle"]]))

(defcomputed menu-b-bullets "The Bullets menu: none, or a bullet's shape."
  (concat [{:title (t "ribbon.none")
            :icon "ban"
            :run "setObjects"
            :with {:props {:bullets false :numbering nil}}
            :enabled computed.anyText}]
          (map (fn [it]
                 {:title it.0
                  :icon it.1
                  :checked (and (some? computed.first)
                                (or-else computed.first.bullets computed.firstPlaceholder.bullets false)
                                (nil? computed.first.numbering)
                                (= (or-else computed.first.bulletIcon "dot") it.1))
                  :enabled computed.anyText
                  :run "setObjects"
                  :with {:props {:bullets true :numbering nil :bulletIcon (if (= it.1 "dot") nil it.1)}}})
               [[(t "menu.filledRound") "dot"]
                [(t "menu.hollowRound") "circle"]
                [(t "menu.filledSquare") "square"]
                [(t "menu.dash") "minus"]
                [(t "menu.arrow") "chevron-right"]
                [(t "bar.check") "check"]])))

(defcomputed menu-b-numbering "The Numbering menu: none, or a numbering's style."
  (concat [{:title (t "ribbon.none")
            :icon "ban"
            :run "setObjects"
            :with {:props {:numbering nil}}
            :enabled computed.anyText}]
          (map (fn [it]
                 {:title it.0
                  :checked (= computed.first.numbering it.1)
                  :enabled computed.anyText
                  :run "setObjects"
                  :with {:props {:bullets true :numbering it.1}}})
               [["1. 2. 3." "1."] ["1) 2) 3)" "1)"] ["A. B. C." "A."] ["a) b) c)" "a)"] [(t "menu.iIiIii") "I."]])))

(defcomputed menu-b-line-spacing "The Line Spacing menu."
  (map (fn [it]
         {:title (fixed it 1)
          :checked (= (or-else computed.first.spacing 1) it)
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:spacing (if (= it 1) nil it)}}})
       [1 1.5 2 2.5 3]))

(defcomputed menu-b-columns "The Columns menu."
  (map (fn [it]
         {:title it.0
          :icon it.2
          :checked (= (or-else computed.first.columns 1) it.1)
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:columns (if (= it.1 1) nil it.1)}}})
       [[(t "menu.oneColumn") 1 "square"]
        [(t "menu.twoColumns") 2 "columns-2"]
        [(t "menu.threeColumns") 3 "columns-3"]]))

(defcomputed menu-b-direction "The Text Direction menu."
  (map (fn [it]
         {:title it.0
          :icon it.2
          :checked (= (or-else computed.first.textRotate 0) it.1)
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:textRotate (if (= it.1 0) nil it.1)}}})
       [[(t "menu.horizontal") 0 "move-horizontal"]
        [(t "menu.rotateAllText90") 90 "redo-2"]
        [(t "menu.rotateAllText270") 270 "undo-2"]]))

(defcomputed menu-b-anchor "The Align Text menu: top, middle or bottom."
  (map (fn [it]
         {:title it.0
          :icon it.2
          :checked (and (some? computed.first)
                        (= (or-else computed.first.anchor computed.firstPlaceholder.anchor "top") it.1))
          :enabled computed.anyText
          :run "setObjects"
          :with {:props {:anchor it.1}}})
       [[(t "menu.top") "top" "align-vertical-justify-start"]
        [(t "menu.middle") "middle" "align-vertical-justify-center"]
        [(t "menu.bottom") "bottom" "align-vertical-justify-end"]]))

(defcomputed menu-b-select "The Select menu."
  [{:title (t "menu.selectAll") :icon "mouse-pointer-2" :keyLabel "Ctrl+A" :run "selectAll"}
   {:title (t "menu.selectText") :icon "type" :run "selectKind" :with {:kinds ["text"]}}
   {:title (t "menu.selectShapes") :icon "shapes" :run "selectKind" :with {:kinds ["shape" "line"]}}
   {:separator true}
   {:title (t "menu.selectNone") :icon "square-dashed" :run "selectKind" :with {:kinds []}}
   {:separator true}
   {:title (t "menu.selectionPane")
    :icon "layers"
    :checked (and (= state.sidePane "select") (not state.formatPane))
    :run "openSidePane"
    :with {:pane "select"}}])

(defcomputed menu-b-arrange "The Arrange menu." computed.arrangeItems)

(defcomputed commands "What the search box can run, each a command by its title."
  [{:title (t "menu.newSlide")
    :icon "plus"
    :run "runSearched"
    :with {:action "newSlide" :args {:layout "content"}}
    :keyLabel "Ctrl+M"}
   {:title (t "menu.duplicateSlide") :icon "copy-plus" :run "runSearched" :with {:action "duplicateSlide" :args {}}}
   {:title (t "menu.deleteSlide")
    :icon "trash-2"
    :run "runSearched"
    :with {:action "deleteSlide" :args {}}
    :enabled (> (count state.deck.slides) 1)}
   {:title (t "menu.hideSlide") :icon "eye-off" :run "runSearched" :with {:action "hideSlide" :args {}}}
   {:title (t "menu.resetSlide") :icon "rotate-ccw" :run "runSearched" :with {:action "resetSlide" :args {}}}
   {:title (t "menu.addSection") :icon "folder-plus" :run "runSearched" :with {:action "addSection" :args {}}}
   {:title (t "ribbon.textBox2")
    :icon "text-cursor-input"
    :run "runSearched"
    :with {:action "insertTextBox" :args {}}}
   {:title (t "ribbon.find")
    :icon "search"
    :run "runSearched"
    :with {:action "openFind" :args {:replace false}}
    :keyLabel "Ctrl+F"}
   {:title (t "ribbon.replace")
    :icon "replace"
    :run "runSearched"
    :with {:action "openFind" :args {:replace true}}
    :keyLabel "Ctrl+H"}
   {:title (t "menu.selectAll")
    :icon "mouse-pointer-2"
    :run "runSearched"
    :with {:action "selectAll" :args {}}
    :keyLabel "Ctrl+A"}
   {:title (t "menu.selectionPane")
    :icon "layers"
    :run "runSearched"
    :with {:action "openSidePane" :args {:pane "select"}}}
   {:title (t "menu.animationPane")
    :icon "list-ordered"
    :run "runSearched"
    :with {:action "openSidePane" :args {:pane "motion"}}}
   {:title (t "menu.showComments")
    :icon "messages-square"
    :run "runSearched"
    :with {:action "openSidePane" :args {:pane "comments"}}}
   {:title (t "menu.newComment")
    :icon "message-square-plus"
    :run "runSearched"
    :with {:action "newComment" :args {}}}
   {:title (t "menu.checkAccessibility")
    :icon "accessibility"
    :run "runSearched"
    :with {:action "openSidePane" :args {:pane "access"}}}
   {:title (t "menu.keyboardShortcuts")
    :icon "keyboard"
    :run "runSearched"
    :with {:action "openSidePane" :args {:pane "keys"}}}
   {:title (t "menu.formatBackground2")
    :icon "paint-bucket"
    :run "runSearched"
    :with {:action "formatBackground" :args {}}}
   {:title (t "menu.presentFromTheStart")
    :icon "play"
    :run "runSearched"
    :with {:action "present" :args {:from 0}}
    :keyLabel "F5"}
   {:title (t "menu.presentFromThisSlide")
    :icon "presentation"
    :run "runSearched"
    :with {:action "present" :args {:from state.current}}
    :keyLabel "Shift+F5"}
   {:title (t "menu.previewTransitionAndBuilds")
    :icon "play"
    :run "runSearched"
    :with {:action "previewSlide" :args {}}}
   {:title (t "menu.readingView")
    :icon "book-open"
    :run "runSearched"
    :with {:action "readingView" :args {:from state.current}}}
   {:title (t "menu.rehearseTimings") :icon "timer" :run "runSearched" :with {:action "rehearse" :args {}}}
   {:title (t "menu.clearInkOnTheSlide") :icon "eraser" :run "runSearched" :with {:action "clearInk" :args {}}}
   {:title (t "menu.applyTransitionToAll")
    :icon "copy-check"
    :run "runSearched"
    :with {:action "transitionToAll" :args {}}}
   {:title (t "menu.slideNumbers") :icon "hash" :run "runSearched" :with {:action "toggleSlideNumbers" :args {}}}
   {:title (t "menu.footer") :icon "panel-bottom" :run "runSearched" :with {:action "askFooter" :args {}}}
   {:title (t "ribbon.save") :icon "save" :run "runSearched" :with {:action "saveDeck" :args {}} :keyLabel "Ctrl+S"}
   {:title (t "ribbon.open")
    :icon "folder-open"
    :run "runSearched"
    :with {:action "openDeck" :args {}}
    :keyLabel "Ctrl+O"}
   {:title (t "menu.exportToPdf") :icon "file-down" :run "runSearched" :with {:action "exportPdf" :args {}}}
   {:title (t "menu.exportSlideAsPicture") :icon "image" :run "runSearched" :with {:action "exportPng" :args {}}}
   {:title (t "menu.darkMode") :icon "moon" :run "runSearched" :with {:action "toggleTheme" :args {}}}
   {:title (t "menu.undo")
    :icon "undo-2"
    :run "runSearched"
    :with {:action "undo" :args {}}
    :keyLabel "Ctrl+Z"
    :enabled computed.canUndo}
   {:title (t "menu.redo")
    :icon "redo-2"
    :run "runSearched"
    :with {:action "redo" :args {}}
    :keyLabel "Ctrl+Y"
    :enabled computed.canRedo}])

(defcomputed menu-search "The search box's menu: the commands whose titles hold its text, and a search of the slides."
  (if (= (trim state.search) "")
      []
      (concat (slice (filter (fn [it] (contains (lower-case it.title) (lower-case (trim state.search))))
                             computed.commands)
                     0
                     10)
              [{:separator true}
               {:title (t "menu.findInSlides" {:text (trim state.search)}) :icon "search" :run "searchSlides"}])))

(defcomputed menu-b-colors "The theme Colors menu: the theme's own, or another's."
  (concat [{:title (t "menu.fromTheTheme") :checked (nil? state.deck.colors) :run "setColorSet" :with {:id nil}}
           {:separator true}]
          (map (fn [it]
                 {:title (t (str "theme." it.id))
                  :checked (= state.deck.colors it.id)
                  :run "setColorSet"
                  :with {:id it.id}})
               computed.themes)))

(defcomputed menu-b-fonts "The Fonts menu: the heading and body fonts, Roboto or JetBrains Mono."
  (map (fn [it]
         {:title (if (= it.0 it.1)
                     (if (= it.0 "mono") "JetBrains Mono" "Roboto")
                     (t "menu.fontPair"
                        {:heading (if (= it.0 "mono") "JetBrains Mono" "Roboto")
                         :body (if (= it.1 "mono") "JetBrains Mono" "Roboto")}))
          :checked (and (= computed.headingFont it.0) (= computed.bodyFont it.1))
          :run "setFonts"
          :with {:heading it.0 :body it.1}})
       [["default" "default"] ["mono" "default"] ["mono" "mono"]]))

(defmacro slide-title "A slide's title, as a link to it shows it: the first line of its title placeholder's text."
  [slide]
  `(or-else (get (split (or-else (:text (or-else (find (fn [it] (contains ["title" "ctrTitle"] (or-else it.ph "")))
                                                       (or-else (:objects ~slide) []))
                                                 {}))
                                 "")
                        "\n")
                 0)
            ""))

(defcomputed menu-b-ins-link "The Link menu: every slide, by its number and title, and Remove Link."
  (concat (map-indexed (fn [index it]
                         {:title (t "menu.slideTitled" {:n (+ index 1) :title (slide-title it)})
                          :checked (= computed.linkOf it.id)
                          :run "setLink"
                          :with {:slide it.id}})
                       state.deck.slides)
          [{:separator true}
           {:title (t "menu.removeLink")
            :icon "unlink"
            :run "setLink"
            :with {:slide nil}
            :enabled (some (fn [it] (some? it.link)) computed.selected)}]))

(defcomputed linkOf "The slide the selection's text links to, if all of it links to one."
  (if (and (> (count computed.selected) 0)
           (every? (fn [it] (= it.link computed.selected.0.link)) computed.selected))
      computed.selected.0.link
      nil))

(defcomputed menu-gal-shapes "The shape gallery's whole list: the Shapes menu." (get computed "menu-b-ins-shapes"))

(defcomputed menu-b-delete-comment "The Delete menu of the comments."
  [{:title (t "menu.deleteAllCommentsOnThis")
    :icon "message-square-x"
    :run "deleteComments"
    :with {:all false}
    :enabled (> (count computed.comments) 0)}
   {:title (t "menu.deleteAllCommentsInThis") :icon "trash-2" :run "deleteComments" :with {:all true}}])

(defcomputed menu-gal-themes "The theme gallery's whole list."
  (map (fn [it]
         {:title (t (str "theme." it.id)) :checked (= state.deck.theme it.id) :run "setTheme" :with {:theme it.id}})
       computed.themes))

(defcomputed menu-b-effects "The Shape Effects menu." computed.effectItems)

(defcomputed menu-b-ins-shapes "The Shapes menu: the gallery's shapes."
  (map (fn [it] {:title (t (str "shape." it)) :run "insertShape" :with {:shape it}})
       ["rect"
        "roundRect"
        "ellipse"
        "triangle"
        "rtTriangle"
        "line"
        "lineArrow"
        "rightArrow"
        "downArrow"
        "curvedRightArrow"
        "star5"
        "heart"
        "cloud"
        "smileyFace"
        "leftBrace"
        "rightBrace"
        "wedgeRoundRectCallout"]))

(defcomputed themeGrid "The theme colours' grid: each slot, then its tints, ten a row."
  (map (fn [it] (+ (get computed.slotNames (mod it 10)) (if (< it 10) "" (str "." (floor (/ it 10)))))) (range 60)))

(defcomputed highlightColors "The text highlight's colours."
  ["#ffff00"
   "#00ff00"
   "#00ffff"
   "#ff00ff"
   "#0000ff"
   "#ff0000"
   "#000080"
   "#008080"
   "#008000"
   "#800080"
   "#800000"
   "#808000"
   "#808080"
   "#c0c0c0"
   "#000000"])

(defcomputed colors-b-color "The Font Color grid."
  {:key "color"
   :none (t "menu.automatic")
   :sections [{:title (t "menu.themeColors") :columns 10 :cells computed.themeGrid}
              {:title (t "menu.standardColors") :columns 10 :cells computed.standardColors}]})

(defcomputed colors-b-fill "The Shape Fill grid."
  {:key "fill"
   :none (t "menu.noFill")
   :sections [{:title (t "menu.themeColors") :columns 10 :cells computed.themeGrid}
              {:title (t "menu.standardColors") :columns 10 :cells computed.standardColors}]})

(defcomputed colors-b-outline "The Shape Outline grid."
  {:key "line"
   :none (t "menu.noOutline")
   :sections [{:title (t "menu.themeColors") :columns 10 :cells computed.themeGrid}
              {:title (t "menu.standardColors") :columns 10 :cells computed.standardColors}]})

(defcomputed colors-b-highlight "The Text Highlight Color grid."
  {:key "highlight"
   :none (t "menu.noColor")
   :sections [{:title (t "menu.highlightColors") :columns 5 :cells computed.highlightColors}]})

(defcomputed colors-b-quick-styles "The Shape Styles grid: filled, light and outlined."
  {:key "style"
   :none (t "menu.noStyle")
   :sections [{:title (t "menu.filled") :columns 6 :cells (map (fn [it] (str "accent" (+ it 1))) (range 6))}
              {:title (t "menu.light") :columns 6 :cells (map (fn [it] (str "accent" (+ it 1) ".1")) (range 6))}
              {:title (t "menu.outlined") :columns 6 :cells (map (fn [it] (str "line:accent" (+ it 1))) (range 6))}]})

(defcomputed colorDrop "The colour grid open: computed['colors-' + its button], or nil."
  (if state.colors.open (get computed (str "colors-" state.colors.id)) nil))

(defcomputed sectionHere "The section the slide shown is in, or nil."
  (find (fn [it] true)
        (sort-by (fn [it] it.first)
                 >
                 (filter (fn [it] (<= it.first state.current)) (or-else state.deck.sections [])))))

(defcomputed findQuery "The text the find box looks for." state.find.text)

(defcomputed hitsBySlide "For each slide, the ids of its objects whose text holds the find box's text."
  (if (= computed.findQuery "")
      (map (fn [it] []) state.deck.slides)
      (map (fn [it]
             (map (fn [it] it.id)
                  (filter (fn [it] (contains (or-else it.text "") computed.findQuery)) (or-else it.objects []))))
           state.deck.slides)))

(defcomputed hitCount "How many objects hold the find box's text." (sum computed.hitsBySlide (fn [it] (count it))))

(defcomponent colorGrid
  "The colour drop-down: its sections of swatches, and the none row; a swatch is a slot, a tint, or line:slot."
  {}
  `(column#colors :width 232
     :height ~(+ 40
                 (sum (or-else computed.colorDrop.sections [])
                      (fn [it] (- (+ 24 (* (ceil (/ (count it.cells) it.columns)) 22)) 4))))
     :gap 4
     :radius 10
     :color @card
     :borderWidth 1
     :borderColor @rbLine
     :shadow {:y 4 :blur 14 :color "#00000033"}
     :role menu
     :label ~(t "menu.colours")
     :overlay {:anchor ~(str "#" state.colors.id) :placement below :open state.colors.open :dismiss outside :modal true}
     :let {:drop ~(or-else computed.colorDrop {:sections [] :none ""})}
     (node#colors-top :height 4 :color transparent)
     (label#colors-none :height 24
       :padding 4
       :style rbButton
       :anchors {:left 8 :right 8}
       :runs [{:icon ban :size 14} {:text ~(str " " drop.none) :size 12}]
       :onClick ~(handler pickColor state.colors.id nil))
     ~(repeat [sec drop.sections :index s]
       `(column :id ~(str "colors-s" s)
          :anchors {:left 8 :right 8}
          :height ~(- (+ 20 (* (ceil (/ (count sec.cells) sec.columns)) 22)) 4)
          :gap 4
          :color transparent
          :alignItems stretch
          (label :id ~(str "colors-s" s "-title")
                 :height 16
                 :textColor @rbMuted
                 :runs [{:text ~sec.title :size 11 :bold true}])
          ~(color-cells sec s)))
     (node#colors-bottom :height 4 :color transparent)))

(defcomponent findBox
  "Find, Replace, a section's name or the footer: a field or two, buttons and a status, below its button."
  {}
  `(column#find-box :width 300
     :height ~(+ 94
                 (if (and state.find.replace (= (or-else state.find.ask "") "")) 32 0)
                 (if (not= state.find.status "") 24 0))
     :gap 6
     :radius 10
     :color @card
     :borderWidth 1
     :borderColor @rbLine
     :shadow {:y 4 :blur 14 :color "#00000033"}
     :role dialog
     :label ~(cond (= state.find.ask "section") (t "menu.renameSection")
                   (= state.find.ask "footer") (t "menu.footer")
                   state.find.replace (t "ribbon.replace")
                   :else (t "ribbon.find"))
     :overlay {:anchor ~(cond (= state.find.ask "section") "#b-section"
                              (= state.find.ask "footer") "#b-ins-header"
                              state.find.replace "#b-replace"
                              :else "#b-find")
               :placement below
               :open state.find.open
               :dismiss outside
               :modal true}
     :keys {:escape [{:set state.find.open :to false}]}
     (node#find-top :height 4 :color transparent)
     ~(find-title)
     ~(find-text)
     ~(find-with)
     ~(find-buttons)
     (label#find-status :anchors {:left 10 :right 10}
       :when ~(not= state.find.status "")
       :height 18
       :textColor @rbMuted
       :runs [{:text ~state.find.status :size 11}])
     (node#find-bottom :height 4 :color transparent)))

(defview find-title "What the find box asks for." []
  `(label#find-title :anchors {:left 10 :right 10}
     :height 18
     :textColor @rbInk
     :runs [{:text ~(cond (= state.find.ask "section") (t "menu.sectionName")
                          (= state.find.ask "footer") (t "menu.footerTextEmptyNone")
                          state.find.replace (t "ribbon.replace")
                          :else (t "menu.findWhat"))
             :size 12
             :bold true}]))

(defview find-with "The replacement, for Replace." []
  `(textarea#find-with :anchors {:left 10 :right 10}
     :when ~(and state.find.replace (= (or-else state.find.ask "") ""))
     :height 26
     :padding 6
     :radius 6
     :color @rbFieldBg
     :borderWidth 1
     :borderColor @rbFieldLine
     :textColor @rbInk
     :bind state.find.with
     :placeholder ~(t "menu.replaceWith")
     :placeholderColor @rbMuted
     :onSubmit replaceOne))

(defview find-text "The text to find, or the section's name, or the footer." []
  `(textarea#find-text :anchors {:left 10 :right 10}
     :height 26
     :padding 6
     :radius 6
     :color @rbFieldBg
     :borderWidth 1
     :borderColor @rbFieldLine
     :textColor @rbInk
     :bind state.find.text
     :autofocus true
     :placeholder ~(cond (= state.find.ask "section") (or-else computed.sectionHere.name (t "section.untitled"))
                         (= state.find.ask "footer") (or-else state.deck.footer (t "menu.footer"))
                         :else (t "menu.textInTheSlides"))
     :placeholderColor @rbMuted
     :onSubmit submitFind))

(defpart menus
         "The drop-downs, the colour grid and the find box over the window, and Copy Style's watch."
         :watch {:state.board.selection [~(handler paintSelection event.value)]}
         :overlays {:zDrop (node :use
                       contextMenu
                       :shadow {:x 0 :y 4 :blur 14 :color "#0000003a"}
                       :with {:open state.drop.open
                        :items ~computed.dropItems
                        :anchor ~(str "#" state.drop.id)
                        :placement below
                        :width 220
                        :icons ~(> (count (filter (fn [it] (or (some? it.icon) (some? it.checked)))
                                                  computed.dropItems))
                                   0)
                        :color @card
                        :textColor @rbInk
                        :dimColor @rbMuted
                        :highlight @rbHover})
          :zColors (node :use colorGrid)
          :zContext (node :use contextMenu
                          :shadow {:x 0 :y 4 :blur 14 :color "#0000003a"}
                          :with {:open state.ctx.open
                           :items ~(if state.ctx.open computed.ctxItems [])
                           :x ~state.ctx.x
                           :y ~state.ctx.y
                           :width 220
                           :icons true
                           :color @card
                           :textColor @rbInk
                           :dimColor @rbMuted
                           :highlight @rbHover})
          :zFind (node :use findBox :when ~state.find.open)})

;;; Views

(defview color-cells "A section of the colour grid: its swatches; a click picks one." [sec s]
  `(grid :id ~(str "colors-s" s "-grid")
     :columns ~sec.columns
     :gap 4
     :height ~(- (* (ceil (/ (count sec.cells) sec.columns)) 22) 4)
     :color transparent
     ~(repeat [c sec.cells :index k]
       `{:id ~(str "colors-s" s "-" k)
         :width ~(if (> sec.columns 6) 18 32)
         :height 18
         :radius 3
         :cursor hand
         :role menuitem
         :let {:line ~(= (slice c 0 5) "line:") :slot ~(if (= (slice c 0 5) "line:") (slice c 5) c)}
         :color ~(if line "@card" (or-else (get computed.palette slot) slot))
         :borderWidth ~(if line 2 1)
         :borderColor ~(if line (get computed.palette slot) "@rbFieldLine")
         :hover {:borderColor @brand}
         :tooltip ~(or-else (get computed.slotTitles (get (split slot ".") 0)) slot)
         :label ~slot
         :onClick ~(handler pickColor state.colors.id c)})))

(defview find-buttons "Find Next, Replace and Replace All, or OK for a name." []
  `(row#find-buttons :anchors {:left 10 :right 10} :height 26 :gap 4 :color transparent :justify end
     (label#find-rename :when ~(not= (or-else state.find.ask "") "")
       :style rbButtonOn
       :height 26
       :fitText true
       :padding 10
       :runs [{:text ~(if (= state.find.ask "section") (t "menu.rename") (t "menu.apply")) :size 12}]
       :onClick answer)
     (label#find-next :when ~(= (or-else state.find.ask "") "")
       :style rbButton
       :height 26
       :fitText true
       :padding 8
       :runs [{:text ~(t "menu.findNext") :size 12}]
       :onClick findNext)
     (label#find-replace :when ~(and state.find.replace (= (or-else state.find.ask "") ""))
       :style rbButton
       :height 26
       :fitText true
       :padding 8
       :runs [{:text ~(t "ribbon.replace") :size 12}]
       :onClick replaceOne)
     (label#find-replace-all :when ~(and state.find.replace (= (or-else state.find.ask "") ""))
       :style rbButtonOn
       :height 26
       :fitText true
       :padding 10
       :runs [{:text ~(t "menu.replaceAll") :size 12}]
       :onClick replaceAll)))

;;; Opening the menus and panes

(defn openDrop "Opens a button's drop-down, below it." [id]
  (set "state.colors.open" false)
  (set "state.drop" {:open true :id id}))

(defn openColors "Opens a colour button's grid, below it." [id]
  (set "state.drop.open" false)
  (set "state.colors" {:open true :id id}))

(defn openContext "Opens the menu of a slide, an object or the canvas, at a point." [x y kind]
  (set "state.drop.open" false)
  (set "state.ctx" {:open true :x x :y y :kind kind}))

(defn openFormatPane "Opens the format pane, leaving the sorter for the slide." []
  (when (= state.view "sorter")
    (set "state.view" "normal"))
  (set "state.formatPane" true)
  (set "state.sidePane" nil))

(defn formatBackground "Opens the format pane on the slide's background." []
  (set "state.board.selection" [])
  (openFormatPane))

;;; The search box

(defn runSearched "Runs a command the search box found, and empties the box." [action args]
  (set "state.search" "")
  (run action :with args))

(defn searchSlides "Finds the search box's text in the slides, and empties the box." []
  (set "state.find.text" (trim state.search))
  (openFind false)
  (set "state.find.text" (trim state.search))
  (set "state.search" ""))

;;; Colours

(def color-keys "The property each colour button sets." {"b-color" "color" "b-fill" "fill" "b-outline" "line"
                                                          "b-highlight" "highlight"})

(defn pickColor "A colour picked in a button's grid (nil for none): the button keeps it, and the selection takes it."
  [id value]
  (set "state.colors.open" false)
  (when (some? value)
    (set "state.lastColor" (assoc state.lastColor id value)))
  (applyColor id value))

(defn quickStyle "The fill, line and text colour of a quick style: a slot, a tint, line:slot, or nil for none." [value]
  (cond (nil? value) {:fill nil :line nil :color nil}
        (= (slice value 0 5) "line:") {:fill "light1" :line (slice value 5) :lineWidth 2 :color (slice value 5)}
        (contains value ".") {:fill value :line (get (split value ".") 0) :color "dark1"}
        :else {:fill value :line value :color "light1"}))

(defn applyColor "Gives the selected objects a colour button's colour, or a quick style." [id value]
  (if (= id "b-quick-styles")
    (setObjects (quickStyle value))
    (setObjects (assoc {} (get color-keys id) value))))

;;; Text

(defn pasteText "Pastes the copied objects' text only, as text boxes a little down and right." []
  (when (> (count state.clip) 0)
    (commit)
    (let [next state.deck.next]
      (set-objects (concat computed.objects
                           (map-indexed (fn [index it]
                                          {:id (str "o" (+ next index)) :kind "text"
                                           :x (+ (or-else it.x 330) 18) :y (+ (or-else it.y 240) 18)
                                           :w (or-else it.w 300) :h (or-else it.h 40)
                                           :text (or-else it.text "") :size 18})
                                        state.clip)))
      (set "state.board.selection" (map-indexed (fn [index it] (str "o" (+ next index))) state.clip)))
    (increment "state.deck.next" :by (count state.clip))))

(defn setLink
  "Links the selected objects' text to a slide of the deck, which a click in the show goes to (nil: no link)."
  [slide]
  (setObjects {:link slide}))

(defn selectKind "Selects the slide's objects of some kinds: text, shape, line..." [kinds]
  (set "state.board.selection"
       (map (fn [it] it.id) (filter (fn [it] (contains kinds (or-else it.kind "text"))) computed.objects))))

(defn indentLine "A line moved a list level in (by 1, four spaces, up to 8 levels) or out (by -1)." [by line]
  (let [at (indexOf line (trim line))]
    (if (> by 0)
      (if (< at 32) (str "    " line) line)
      (slice line (min at 4)))))

(defn indentText "Moves every line of the selected text a list level in (by 1) or out (by -1)." [by]
  (when computed.anyText
    (commit)
    (set-objects (map (fn [o]
                        (if (not (selected? o))
                          o
                          (merge o {:text (join "\n" (map (fn [line] (indentLine by line))
                                                         (split (or-else o.text "") "\n")))})))
                      computed.objects))))

(defn insertSymbol "Adds a symbol to the selected text, or a text box of it in the middle of the slide." [symbol]
  (if computed.anyText
    (do (commit)
        (set-objects (map (fn [o]
                            (if (and (selected? o) (not= o.kind "line"))
                              (merge o {:text (str (or-else o.text "") symbol)})
                              o))
                          computed.objects)))
    (insertObject {:kind "text" :x 430 :y 230 :w 100 :h 80 :text symbol :size 48 :align "center"})))

(def text-style-keys "What Copy Style copies." ["bold" "italic" "underline" "strike" "shadow" "size" "color" "highlight"
                                                 "font" "fill" "line" "lineWidth" "dash" "align" "anchor" "bullets"
                                                 "bulletIcon" "numbering" "spacing"])

(defn copyStyle "Keeps the selection's look, which the next objects selected take; or forgets it." []
  (if (or (some? state.painter) (not computed.anyText))
    (set "state.painter" nil)
    (set "state.painter" (zipmap text-style-keys (map (fn [key] (get computed.first key)) text-style-keys)))))

;;; Sections and the footer

(def closed-box "The find box closed and empty." {:open false :replace false :text "" :with "" :status "" :ask ""})

(defn removeSection "Removes the section the slide shown is in; its slides join the one before." []
  (when (some? computed.sectionHere)
    (commit)
    (set "state.deck.sections" (filter (fn [it] (not= it.id computed.sectionHere.id)) state.deck.sections))))

(defn removeSections "Removes every section." []
  (commit)
  (set "state.deck.sections" []))

(defn askSectionName "Asks for the section's new name, in the find box." []
  (set "state.find" (merge closed-box {:open true :ask "section"})))

(defn askFooter "Asks for the footer's text, in the find box." []
  (set "state.find" (merge closed-box {:open true :ask "footer"})))

(defn answer "Takes what the find box was asked for: the section's name or the footer." []
  (if (= state.find.ask "section") (renameSection) (setFooter)))

(defn renameSection "Renames the section the slide shown is in." []
  (let [name (trim state.find.text)]
    (when (and (some? computed.sectionHere) (not= name ""))
      (commit)
      (set "state.deck.sections"
           (map (fn [it] (if (= it.id computed.sectionHere.id) (merge it {:name name}) it)) state.deck.sections))))
  (set "state.find" closed-box))

(defn setFooter "Sets the footer's text on every slide (empty: none)." []
  (commit)
  (set "state.deck.footer" (if (= (trim state.find.text) "") nil (trim state.find.text)))
  (set "state.find" closed-box))

(defn toggleSlideNumbers "Shows the slides' numbers, or hides them." []
  (commit)
  (set "state.deck.slideNumbers" (not (or-else state.deck.slideNumbers false))))

;;; Find and Replace

(defn submitFind "Enter in the find box: takes what it asked for, or finds the next." []
  (if (not= (or-else state.find.ask "") "") (answer) (findNext)))

(defn paintSelection "Gives the objects selected next the look Copy Style kept." [selection]
  (when (and (some? state.painter) (> (count selection) 0))
    (setObjects state.painter)
    (set "state.painter" nil)))

(defn openFind "Opens the find box, with Replace or without, keeping the text unless it was asking for something."
  [replace]
  (set "state.find" {:open true :replace replace :text (if (not= (or-else state.find.ask "") "") "" state.find.text)
                     :with state.find.with :status "" :ask ""}))

(defn findNext "Selects the next object that holds the text, on this slide or the next that has one." []
  (if (= computed.hitCount 0)
    (set "state.find.status" (if (= computed.findQuery "") "" (t "menu.notFound")))
    (let [here (get computed.hitsBySlide state.current)]
      (findAfter (indexOf here (or-else (get state.board.selection 0) "")) here))))

(defn findAfter "Selects the hit after the one at `at` of this slide's hits, `here`, else the next slide's first."
  [at here]
  (if (or (and (>= at 0) (< (inc at) (count here)))
          (and (< at 0) (> (count here) 0) (= (count state.board.selection) 0)))
    (set "state.board.selection" [(get here (inc at))])
    (let [slides (count state.deck.slides)]
      (findOn (find (fn [it] (> (count (get computed.hitsBySlide it)) 0))
                    (map (fn [it] (mod (+ state.current 1 it) slides)) (range slides))))))
  (set "state.find.status" (t "menu.matches" {:n computed.hitCount})))

(defn findOn "Shows a slide and selects its first hit." [slide]
  (pickSlide slide)
  (set "state.board.selection" [(get-in computed.hitsBySlide [slide 0])]))

(defn replaced "A text with every hit replaced." [text]
  (join state.find.with (split text computed.findQuery)))

(defn replaceOne "Replaces the text in the selected object, then finds the next." []
  (when (and (not= computed.findQuery "")
             (> (count computed.selected) 0)
             (contains (or-else computed.first.text "") computed.findQuery))
    (setObjects {:text (replaced computed.first.text)}))
  (findNext))

(defn replaceAll "Replaces the text everywhere in the deck, as one change." []
  (if (> computed.hitCount 0)
    (do (set "state.find.status" (t "menu.replaced" {:n computed.hitCount}))
        (commit)
        (set "state.deck.slides"
             (map (fn [slide]
                    (merge slide {:objects (map (fn [o] (if (contains (or-else o.text "") computed.findQuery)
                                                          (merge o {:text (replaced o.text)})
                                                          o))
                                                (or-else slide.objects []))}))
                  state.deck.slides)))
    (set "state.find.status" (t "menu.notFound"))))
