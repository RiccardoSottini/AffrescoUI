;;; review.lisp --- The Ink, Record, Check and Help tabs
;;;
;;; Ink: a pen, a highlighter and an eraser draw on the slide through a layer over it; a stroke becomes an object of
;;; kind 'ink' (its points as a path in its own box), a highlighter's in a pale colour behind the other objects.
;;; Record: rehearsing a show keeps each slide's seconds as its advanceAfter, which the show uses when the deck's
;;; useTimings is on. Check: comments on each slide (slide.comments) and an accessibility check. Help: the keyboard
;;; shortcuts. Comments, the check, the shortcuts and About open in the side pane (state.sidePane), which takes the
;;; format pane's place.

;;; Definitions

(defstate inkTool nil "The ink tool chosen: pen, highlighter, eraser, or nil to select.")

(defstate inkColor "#1f2933")

(defstate inkWidth 3 "The pen's width.")

(defstate stroke [] "The points of the stroke being drawn.")

(defstate strokeFrom [0 0] "Where the stroke started, in slide points.")

(defstate sidePane nil "The side pane shown: comments, select, motion, access, keys, about, or nil.")

(defstate commentFocus nil "The comment that takes the focus when shown.")

(defcomputed inkPens "The pens' colours, each with its name."
  '[["#1f2933" "black"] ["#e03131" "red"] ["#1c7ed6" "blue"] ["#2f9e44" "green"] ["#f08c00" "orange"]])

(defcomputed inkHighlights "The highlighter's colours, each with its name."
  '[["#ffe680" "yellow"] ["#b2f2bb" "green"] ["#a5d8ff" "blue"] ["#fcc2d7" "pink"]])

(defcomputed inkSwatches "The colours shown: the highlighter's or the pens'."
  (if computed.inkHighlight computed.inkHighlights computed.inkPens))

(defcomputed inkWidths "The pen's widths, each with its name." '[[1.5 "thin"] [3 "medium"] [6 "thick"]])

(defcomputed inkHighlight "Whether the highlighter is chosen." (= state.inkTool "highlighter"))

(defcomputed strokeColor "The stroke's colour." state.inkColor)

(defcomputed strokeWidth "The stroke's width: wide for the highlighter." (if computed.inkHighlight 14 state.inkWidth))

(defcomputed liveStroke "The stroke being drawn, as a path on the canvas."
  (str "M "
       (join " L "
             (map (fn [it]
                    (str (fixed (* (- it.0 computed.view.camera.x) computed.view.camera.zoom) 1)
                         " "
                         (fixed (* (- it.1 computed.view.camera.y) computed.view.camera.zoom) 1)))
                  state.stroke))))

(defcomputed slideInk "How many ink strokes the slide has."
  (count (filter (fn [it] (= it.kind "ink")) computed.objects)))

(defcomputed timed "How many slides have a timing."
  (count (filter (fn [it] (some? it.advanceAfter)) state.deck.slides)))

(defmacro object-name "An object's name: its own, else its kind (or its placeholder's) and a number." [o number]
  `(or-else (:name ~o)
            (t "object.name" {:kind (cond (contains ["title" "ctrTitle"] (or-else (:ph ~o) "")) "title"
                                          (= (:ph ~o) "subTitle") "subTitle"
                                          (some? (:ph ~o)) "content"
                                          :else (or-else (:kind ~o) "text"))
                              :n ~number})))

(defcomputed objectNames "Each object's name, as the selection pane shows it."
  (map-indexed (fn [index it] (object-name it (+ index 1))) computed.objects))

(defcomputed motionRows "The slide's builds in the order they play, each with its object's name."
  (map (fn [it]
         (merge it
                {:name (or-else (get computed.objectNames
                                     (indexOf (map (fn [it] it.id) computed.objects) it.target))
                                "")}))
       (scan (or-else computed.slide.animations [])
             (fn [acc it] (merge it {:step (+ acc.step (if (= it.start "click") 1 0))}))
             {:step 0})))

(defcomputed comments "The slide's comments." (or-else computed.slide.comments []))

(defcomputed commented "The indexes of the slides with comments."
  (filter (fn [it] (> (count (or-else (get-in state.deck.slides [it :comments]) [])) 0))
          (range (count state.deck.slides))))

(defcomputed untitled "How many slides have no title."
  (count (filter (fn [it]
                   (not (some (fn [it]
                                (and (contains ["title" "ctrTitle"] (or-else it.ph ""))
                                     (not= (trim (or-else it.text "")) "")))
                              (or-else it.objects []))))
                 state.deck.slides)))

(defcomputed accessIssues "How many findings the accessibility check has."
  (+ computed.untitled
     (count (filter (fn [it]
                      (or (and (some? it.ph) (= (trim (or-else it.text "")) ""))
                          (and (or (= it.kind "icon") (= it.kind "ink")) (= (trim (or-else it.alt "")) ""))))
                    (flat (map (fn [it] (or-else it.objects [])) state.deck.slides))))))

(defcomputed shortcuts "The keyboard shortcuts the Help pane lists: keys and what they do."
  [["F5" (t "pane.presentFromTheBeginning")]
   [(t "pane.shiftF5") (t "bar.presentFromThisSlide")]
   [(t "pane.ctrlM") (t "pane.newSlide")]
   ["F2" (t "pane.editTheSelectedText")]
   [(t "pane.escape") (t "pane.stopEditingOrDropThe")]
   [(t "pane.tabShiftTab") (t "pane.whileEditingTextAList")]
   [(t "pane.ctrlZCtrlY") (t "pane.undoRedo")]
   [(t "pane.ctrlCCtrlXCtrl") (t "pane.copyCutPaste")]
   [(t "pane.ctrlD") (t "menu.duplicate")]
   [(t "pane.ctrlA") (t "pane.selectAll")]
   [(t "menu.delete") (t "pane.deleteTheSelection")]
   [(t "pane.ctrlFCtrlH") (t "pane.findReplace")]
   [(t "pane.ctrlBCtrlICtrl") (t "pane.boldItalicUnderline")]
   [(t "pane.ctrlLERJ") (t "pane.alignLeftCentreRightJustify")]
   [(t "pane.ctrlShift") (t "pane.largerOrSmallerText")]
   [(t "pane.ctrlF1") (t "pane.showOrHideTheRibbon")]
   [(t "pane.shiftF10") (t "pane.theContextMenu")]
   [(t "pane.rightSpaceN") (t "pane.nextInTheShow")]
   [(t "pane.leftP") (t "pane.previousInTheShow")]
   ["B, W" (t "pane.aBlackOrWhiteScreen")]])

(defcomponent tabInk "The Ink tab: the drawing tools, the pens, Clear Ink, and what isn't there yet."
  {}
  `(row#tab-ink :fitContent width :height 92 :gap 4 :color transparent
     ~(g-ink-tools)
     (node#g-ink-pens :use rbGroup :with {:title ~(t "pane.pens")}
       ~(ink-pens))
     (node#g-ink-edit :use rbGroup :with {:title ~(t "pane.edit")}
       (node#b-ink-clear :use rbBig
         :with {:icon brush-cleaning :title ~(t "pane.clearInk") :width 46 :enabled ~(> computed.slideInk 0)}
         :tooltip ~(t "pane.deleteEveryStrokeOnThis")
         :onClick clearInk))
     (node#g-ink-convert :use rbGroup :with {:title ~(t "pane.convert")}
       (node#b-ink-shape :use rbBig
         :with {:icon shapes :title ~(t "pane.inkToShape") :width 46 :enabled false}
         :tooltip ~(t "pane.turningInkIntoShapesIsn"))
       (node#b-ink-replay :use rbBig
         :with {:icon history :title ~(t "pane.inkReplay") :width 46 :enabled false}
         :tooltip ~(t "pane.replayingInkIsnTAvailable")))))

(defcomponent inkLayer
  "Over the slide while a pen is chosen: a press, a drag and a release draw, or erase, in slide points."
  {}
  `(node#ink-layer :fill 0
     :color transparent
     :role region
     :label ~(t "bar.ink")
     :onPress ~(handler inkPress (+ computed.view.camera.x (/ event.left computed.view.camera.zoom))
                                 (+ computed.view.camera.y (/ event.top computed.view.camera.zoom)))
     :onDrag ~(handler inkDrag (/ event.dx computed.view.camera.zoom) (/ event.dy computed.view.camera.zoom))
     :onRelease inkRelease
     (path#ink-live :when ~(> (count state.stroke) 1)
       :fill 0
       :d ~computed.liveStroke
       :stroke ~computed.strokeColor
       :strokeWidth ~(* computed.strokeWidth computed.view.camera.zoom)
       :strokeCap round
       :strokeJoin round)))

(defcomponent tabRecord "The Record tab: rehearsing the timings, and recording, which isn't there yet."
  {}
  `(row#tab-record :fitContent width :height 92 :gap 4 :color transparent
     ~(g-rehearse)
     (node#g-record :use rbGroup :with {:title ~(t "bar.record")}
       (node#b-record :use rbBig
         :with {:icon circle-dot :title ~(t "bar.record") :width 46 :enabled false}
         :tooltip ~(t "pane.recordingAudioAndVideoIsn"))
       (node#b-export-video :use rbBig
         :with {:icon film :title ~(t "pane.exportToVideo") :width 54 :enabled false}
         :tooltip ~(t "pane.exportingToVideoIsnT")))))

(defcomponent tabCheck "The Check tab: the accessibility check and the comments."
  {}
  `(row#tab-check :fitContent width :height 92 :gap 4 :color transparent
     (node#g-proofing :use rbGroup :with {:title ~(t "pane.proofing")}
       (node#b-spelling :use rbBig
         :with {:icon spell-check :title ~(t "pane.spelling") :width 50 :enabled false}
         :tooltip ~(t "pane.checkingSpellingIsnTAvailable"))
       (node#b-access :use rbBig
         :with {:icon accessibility
                :title ~(t "pane.checkAccessibility")
                :on ~(and (= state.sidePane "access") (not state.formatPane))
                :width 70}
         :tooltip ~(t "pane.slidesWithoutTitlesEmptyPlaceholders")
         :onClick ~(handler openSidePane "access")))
     ~(g-comments)))

(defcomponent tabHelp "The Help tab: the keyboard shortcuts and About."
  {}
  `(row#tab-help :fitContent width :height 92 :gap 4 :color transparent
     (node#g-help :use rbGroup :with {:title ~(t "bar.help")}
       (node#b-shortcuts :use rbBig
         :with {:icon keyboard
                :title ~(t "pane.keyboardShortcuts")
                :on ~(and (= state.sidePane "keys") (not state.formatPane))
                :width 62}
         :onClick ~(handler openSidePane "keys"))
       (node#b-about :use rbBig
         :with {:icon info :title ~(t "pane.about") :on ~(and (= state.sidePane "about") (not state.formatPane))
                :width 46}
         :onClick ~(handler openSidePane "about"))
       (node#b-feedback :use rbBig
         :with {:icon message-circle-heart :title ~(t "pane.feedback") :width 54 :enabled false}
         :tooltip ~(t "pane.sendingFeedbackIsnTAvailable")))))

(defcomponent sidePane
  "The comments, the accessibility check, the shortcuts or About, by state.sidePane, where the format pane goes."
  {}
  `(column#side-pane :width 260
     :color @rb
     :borderWidth 1
     :borderColor @rbLine
     :gap 6
     :alignItems center
     :scroll vertical
     :role region
     :let {:pane ~state.sidePane}
     :label ~(cond (= pane "comments") (t "ribbon.comments")
                   (= pane "access") (t "pane.accessibility")
                   (= pane "select") (t "pane.selection")
                   (= pane "motion") (t "menu.animationPane")
                   (= pane "keys") (t "menu.keyboardShortcuts")
                   :else (t "pane.about"))
     (node#side-top :width 1 :height 4 :color transparent)
     ~(side-head)
     (node :use commentsList :when ~(= pane "comments"))
     (node :use accessList :when ~(= pane "access"))
     (node :use selectionList :when ~(= pane "select"))
     (node :use motionList :when ~(= pane "motion"))
     ~(shortcuts)
     (label#about-text :when ~(= pane "about")
       :width 236
       :height 150
       :wrap true
       :textColor @rbInk
       :runs [{:text "Sinopia" :size 13 :bold true}
              {:text ~(t "pane.isAPresentationEditorWritten")
               :size 12}])))

(defview side-head "The side pane's title, and Close." []
  `(row#side-head :width 236 :height 24 :alignItems center :color transparent
     (label#side-title :grow 1
       :height 22
       :textColor @rbInk
       :runs [{:text ~(cond (= pane "comments") (t "ribbon.comments")
                            (= pane "access") (t "pane.accessibility")
                            (= pane "select") (t "pane.selection")
                            (= pane "motion") (t "menu.animationPane")
                            (= pane "keys") (t "menu.keyboardShortcuts")
                            :else (t "pane.aboutSinopia"))
               :size 14
               :bold true}])
     (node#side-close :use rbSmall :with {:icon x} :tooltip ~(t "pane.close") :onClick closeSidePane)))

(defcomponent commentsList "The slide's comments, each edited in place, resolved or deleted; and New comment."
  {}
  `(column#comments-list :width 236 :fitContent height :gap 8 :alignItems stretch :color transparent
     (label#comment-new :style rbButton
       :height 26
       :fitText true
       :padding 4
       :runs [{:icon message-square-plus :size 14} {:text ~(t "pane.newComment") :size 12}]
       :onClick newComment)
     (label#comments-none :when ~(= (count computed.comments) 0)
       :height 40
       :wrap true
       :textColor @rbMuted
       :runs [{:text ~(t "pane.noComments" {:n (+ state.current 1)}) :size 12}])
     ~(repeat [c computed.comments :index n :key c.id]
       (comment c n))))

(defcomponent selectionList
  "The slide's objects, front first, by their names: a click selects one, the eye shows or hides it."
  {}
  `(column#selection-list :width 236 :fitContent height :gap 2 :alignItems stretch :color transparent
     (row#selection-all :height 26 :gap 6 :color transparent
       (label#selection-show-all :style rbButton
         :height 24
         :fitText true
         :padding 4
         :disabled ~(not (some (fn [it] (or-else it.hidden false)) computed.objects))
         :runs [{:text ~(t "pane.showAll") :size 12}]
         :onClick ~(handler hideObjects (map (fn [it] it.id) computed.objects) false))
       (label#selection-hide-all :style rbButton
         :height 24
         :fitText true
         :padding 4
         :disabled ~(not (some (fn [it] (not (or-else it.hidden false))) computed.objects))
         :runs [{:text ~(t "pane.hideAll") :size 12}]
         :onClick ~(handler hideObjects (map (fn [it] it.id) computed.objects) true)))
     (label#selection-empty :when ~(= (count computed.objects) 0)
       :height 24
       :textColor @rbMuted
       :runs [{:text ~(t "pane.thisSlideHasNoObjects") :size 12}])
     ~(repeat [o (reverse computed.objects) :index n :key o.id]
       (selection-row o n))))

(defcomponent motionList
  "The slide's builds in the order they play: a click selects one's object, the arrows move it, the cross removes it."
  {}
  `(column#motion-list :width 236 :fitContent height :gap 2 :alignItems stretch :color transparent
     (row#motion-all :height 26 :gap 6 :color transparent
       (label#motion-play :style rbButton
         :height 24
         :fitText true
         :padding 4
         :disabled ~(= (count computed.motionRows) 0)
         :runs [{:icon play :size 12} {:text ~(t "pane.playAll") :size 12}]
         :onClick previewSlide))
     (label#motion-empty :when ~(= (count computed.motionRows) 0)
       :width 236
       :height 36
       :wrap true
       :textColor @rbMuted
       :runs [{:text ~(t "pane.noBuildsOnThisSlide") :size 12}])
     ~(repeat [a computed.motionRows :index n :key a.id]
       (motion-row a n))))

(defcomponent accessList
  "Each slide's findings: no title, an empty placeholder, an icon or ink without alt text; a click goes there."
  {}
  `(column#access-list :width 236 :fitContent height :gap 2 :alignItems stretch :color transparent
     (label#access-summary :height 36
       :wrap true
       :textColor @rbMuted
       :runs [{:text ~(if (= computed.accessIssues 0)
                          (t "pane.noProblemsFoundPeopleWho")
                          (t "pane.findings" {:n computed.accessIssues}))
               :size 12}])
     ~(repeat [s state.deck.slides :index n :key s.id]
       `(column :id ~(str "access-" s.id) :fitContent height :gap 2 :alignItems stretch :color transparent
          (label :id ~(str "access-" s.id "-title")
            :when ~(not (some (fn [it]
                                (and (contains ["title" "ctrTitle"] (or-else it.ph ""))
                                     (not= (trim (or-else it.text "")) "")))
                              (or-else s.objects [])))
            :style rbButton
            :height 24
            :padding 4
            :tooltip ~(t "pane.giveEverySlideATitle")
            :runs [{:icon heading :size 13} {:text ~(t "pane.slideNoTitle" {:n (+ n 1)}) :size 12}]
            :onClick ~(handler goToIssue n nil))
          ~(repeat [o (or-else s.objects []) :key o.id]
            (access-issue n o))))))

;;; Views

(defview g-ink-tools "The drawing tools: select, pen, highlighter and eraser." []
  `(node#g-ink-tools :use rbGroup :with {:title ~(t "pane.drawingTools")}
     (node#b-ink-select :use rbBig
       :with {:icon mouse-pointer-2 :title ~(t "ribbon.select") :on ~(nil? state.inkTool) :width 46}
       :tooltip ~(t "pane.selectAndMoveObjects")
       :onClick ~(handler inkTool nil))
     (node#b-ink-pen :use rbBig
       :with {:icon pencil-line :title ~(t "pane.pen") :on ~(= state.inkTool "pen") :width 46}
       :tooltip ~(t "pane.drawOnTheSlideEscape")
       :onClick ~(handler inkTool "pen"))
     (node#b-ink-highlighter :use rbBig
       :with {:icon highlighter :title ~(t "pane.highlighter") :on ~(= state.inkTool "highlighter") :width 62}
       :tooltip ~(t "pane.highlightOnTheSlideEscape")
       :onClick ~(handler inkTool "highlighter"))
     (node#b-ink-eraser :use rbBig
       :with {:icon eraser :title ~(t "pane.eraser") :on ~(= state.inkTool "eraser") :width 46}
       :tooltip ~(t "pane.eraseTheStrokesYouDrag")
       :onClick ~(handler inkTool "eraser"))))

(defview ink-pens "The pens' colours (or the highlighter's), and their widths." []
  `(column#ink-pens :fitContent width :height 72 :gap 8 :justify center :color transparent
     (row#ink-colors :fitContent width :height 22 :gap 4 :color transparent
       ~(repeat [c computed.inkSwatches :key c.0]
         (ink-swatch c)))
     (node#ink-widths :use rbRow
       ~(repeat [w computed.inkWidths :key w.1]
         `(node :id ~(str "ink-width-" w.1)
            :use rbSmall
            :with {:icon minus :title ~(t (str "ink.width." w.1)) :on ~(= state.inkWidth w.0)
                   :enabled ~(not computed.inkHighlight)}
            :tooltip ~(t (str "ink.pen." w.1))
            :onClick ~(handler inkWidth w.0))))))

(defview g-rehearse "Rehearse and clear the timings, and whether the show uses them." []
  `(node#g-rehearse :use rbGroup :with {:title ~(t "pane.timings")}
     (node#b-rehearse :use rbBig
       :with {:icon timer :title ~(t "pane.rehearseTimings") :width 60}
       :tooltip ~(t "pane.presentFromTheBeginningKeeping")
       :onClick rehearse)
     (node#b-clear-timings :use rbBig
       :with {:icon timer-off :title ~(t "pane.clearTimings") :width 52 :enabled ~(> computed.timed 0)}
       :tooltip ~(t "pane.forgetEverySlideSTiming")
       :onClick clearTimings)
     (column#timings-col :width 130 :height 72 :gap 2 :justify center :alignItems stretch :color transparent
       (node#cb-use-timings :use rbCheck
         :with {:title ~(t "pane.useTimings") :at state.deck.useTimings :prefix cb-use-timings}
         :tooltip ~(t "pane.inTheShowEachTimed"))
       (label#timings-count :height 20
         :textColor @rbMuted
         :runs [{:text ~(if (= computed.timed 0) (t "pane.noSlideIsTimed") (t "pane.slidesTimed" {:n computed.timed}))
                 :size 11}]))))

(defview g-comments "New, delete and show the comments, and go through the slides that have some." []
  `(node#g-comments :use rbGroup :with {:title ~(t "ribbon.comments")}
     (node#b-new-comment :use rbBig
       :with {:icon message-square-plus :title ~(t "pane.newComment2") :width 56}
       :tooltip ~(t "ribbon.commentOnThisSlide")
       :onClick newComment)
     (node#b-delete-comment :use rbBig
       :with {:icon message-square-x :title ~(t "menu.delete") :menu true :width 46
              :enabled ~(> (count computed.commented) 0)}
       :onClick ~(handler openDrop "b-delete-comment"))
     (node#comments-nav :use rbStack
       (node#b-comment-previous :use rbSmall
         :with {:icon arrow-left :title ~(t "pane.previous") :enabled ~(> (count computed.commented) 0)}
         :tooltip ~(t "pane.theSlideWithCommentsBefore")
         :onClick ~(handler goToComment false))
       (node#b-comment-next :use rbSmall
         :with {:icon arrow-right :title ~(t "pane.next") :enabled ~(> (count computed.commented) 0)}
         :tooltip ~(t "pane.theSlideWithCommentsAfter")
         :onClick ~(handler goToComment true)))
     (node#b-show-comments :use rbBig
       :with {:icon messages-square
              :title ~(t "pane.showComments")
              :on ~(and (= state.sidePane "comments") (not state.formatPane))
              :width 60}
       :onClick ~(handler openSidePane "comments"))))

(defview shortcuts "The keyboard shortcuts, in the side pane." []
  `(column#shortcuts :when ~(= pane "keys") :width 236 :fitContent height :gap 2 :alignItems stretch :color transparent
     ~(repeat [k computed.shortcuts :index n]
       `(row :id ~(str "shortcut-" n) :height 34 :gap 8 :alignItems center :color transparent
          (label :id ~(str "shortcut-" n "-keys")
            :width 96
            :height 32
            :wrap true
            :align "start center"
            :textColor @rbInk
            :runs [{:text ~k.0 :size 11 :font mono}])
          (label :id ~(str "shortcut-" n "-what")
            :grow 1
            :height 32
            :wrap true
            :align "start center"
            :textColor @rbMuted
            :runs [{:text ~k.1 :size 12}])))))

(defview comment-head "A comment's heading: its number, or Resolved, with Resolve and Delete." [c n]
  `(row :id ~(str "comment-" c.id "-head") :width 220 :height 22 :gap 2 :alignItems center :color transparent
     (label :id ~(str "comment-" c.id "-who")
       :grow 1
       :height 20
       :textColor ~(if c.done "@rbMuted" "@rbInk")
       :runs [{:text ~(if c.done (t "pane.resolved") (t "pane.commentN" {:n (+ n 1)})) :size 12 :bold true}])
     (node :id ~(str "comment-" c.id "-resolve")
       :use rbSmall
       :with {:icon ~(if c.done "rotate-ccw" "check")}
       :tooltip ~(if c.done (t "pane.reopen") (t "pane.resolve"))
       :onClick ~(handler resolveComment n))
     (node :id ~(str "comment-" c.id "-delete")
       :use rbSmall
       :with {:icon trash-2}
       :tooltip ~(t "menu.delete")
       :onClick ~(handler deleteComment c.id))))

(defview comment "Comment `c` of the slide, at `n`: its heading and its text, edited in place." [c n]
  `(column :id ~(str "comment-" c.id)
     :height 104
     :gap 4
     :radius 6
     :alignItems center
     :justify center
     :color @card
     :borderWidth 1
     :borderColor @rbLine
     ~(comment-head c n)
     (editor :id ~(str "comment-" c.id "-text")
       :width 220
       :height 62
       :wrap true
       :autofocus ~(= state.commentFocus c.id)
       :color @rbFieldBg
       :lineColor transparent
       :textColor @rbInk
       :label ~(t "pane.commentN" {:n (+ n 1)})
       :bind ~(str "state.deck.slides[" state.current "].comments[" n "].text"))))

(defview selection-row "Object `o` in the selection pane: its name, which selects it, and the eye that hides it." [o n]
  `(row :id ~(str "selection-" o.id)
     :height 26
     :gap 4
     :alignItems center
     :let {:title ~(object-name o (- (count computed.objects) n))}
     :color ~(if (contains state.board.selection o.id) "@rbChecked" "transparent")
     (label :id ~(str "selection-" o.id "-name")
       :grow 1
       :height 24
       :align "start center"
       :cursor hand
       :role button
       :textColor ~(if (or-else o.hidden false) "@rbMuted" "@rbInk")
       :label ~title
       :runs [{:text ~(str "  " title) :size 12 :italic ~(or-else o.hidden false)}]
       :onClick ~(handler selectObject o.id o.hidden))
     (node :id ~(str "selection-" o.id "-eye")
       :use rbSmall
       :with {:icon ~(if (or-else o.hidden false) "eye-off" "eye")}
       :tooltip ~(if (or-else o.hidden false) (t "pane.showIt") (t "pane.hideIt"))
       :onClick ~(handler hideObjects [o.id] (not (or-else o.hidden false))))))

(defview motion-row "Build `a` in the animation pane: its click, its object and effect, and its buttons." [a n]
  `(row :id ~(str "motion-" a.id)
     :height 26
     :gap 2
     :alignItems center
     :color ~(if (contains state.board.selection a.target) "@rbChecked" "transparent")
     (label :id ~(str "motion-" a.id "-step")
       :width 20
       :height 24
       :align center
       :textColor @rbMuted
       :runs [{:text ~(if (= a.start "click") (str "" a.step) "") :size 11}])
     (label :id ~(str "motion-" a.id "-name")
       :grow 1
       :height 24
       :align "start center"
       :cursor hand
       :role button
       :textColor @rbInk
       :label ~(t "pane.buildLabel" {:name a.name :effect a.effect :exit (= a.kind "exit") :start a.start})
       :runs ~[{:icon (if (= a.kind "exit") "log-out" "log-in") :size 13}
               {:text (str " " a.name) :size 12}
               {:text (str "  " a.effect (if (= a.start "click") "" (str ", " a.start))) :size 11 :color "@rbMuted"}]
       :onClick ~(handler selectTarget a.target))
     (node :id ~(str "motion-" a.id "-up")
       :use rbSmall
       :with {:icon chevron-up :enabled ~(> n 0)}
       :tooltip ~(t "pane.playItEarlier")
       :onClick ~(handler moveAnimation a.id -1))
     (node :id ~(str "motion-" a.id "-down")
       :use rbSmall
       :with {:icon chevron-down :enabled ~(< n (- (count computed.motionRows) 1))}
       :tooltip ~(t "pane.playItLater")
       :onClick ~(handler moveAnimation a.id 1))
     (node :id ~(str "motion-" a.id "-remove")
       :use rbSmall
       :with {:icon x}
       :tooltip ~(t "pane.removeIt")
       :onClick ~(handler removeAnimation a.id))))

(defview access-issue "What the check found on object `o` of slide `n`: an empty placeholder, or no alt text." [n o]
  `(label :id ~(str "access-" o.id)
     :when ~(or (and (some? o.ph) (= (trim (or-else o.text "")) ""))
                (and (or (= o.kind "icon") (= o.kind "ink")) (= (trim (or-else o.alt "")) "")))
     :style rbButton
     :height 24
     :padding 4
     :tooltip ~(if (some? o.ph) (t "pane.fillItInOrDelete") (t "pane.describeItInAltText"))
     :runs [{:icon ~(if (some? o.ph) "square-dashed-bottom" "image-off") :size 13}
            {:text ~(t "pane.slideIssue"
                       {:n (+ n 1) :issue (cond (some? o.ph) "placeholder" (= o.kind "ink") "ink" :else "icon")})
             :size 12}]
     :onClick ~(handler goToIssue n o.id)))

;;; Ink

(defview ink-swatch "A colour of the pens, or of the highlighter: a click takes it, and a pen if none is chosen." [c]
  `(node :id ~(str (if computed.inkHighlight "ink-highlight-" "ink-color-") c.1)
     :width 22
     :height 22
     :radius 11
     :color ~c.0
     :cursor hand
     :tooltip ~(t (str "ink.color." c.1))
     :borderWidth ~(if (= state.inkColor c.0) 2 1)
     :borderColor ~(if (= state.inkColor c.0) "@brand" "@rbFieldLine")
     :onClick ~(handler inkColor c.0)))

(defn inkColor "Takes a colour for the ink, and the pen if no pen is chosen." [color]
  (set "state.inkColor" color)
  (when (or (nil? state.inkTool) (= state.inkTool "eraser"))
    (inkTool "pen")))

(defn inkWidth "Takes a width for the pen." [width]
  (set "state.inkWidth" width))

(defn inkTool "Chooses a pen, the highlighter or the eraser (nil: select), with a colour that suits it." [tool]
  (set "state.inkTool" tool)
  (when (and (= tool "highlighter") (not (contains (map first computed.inkHighlights) state.inkColor)))
    (set "state.inkColor" "#ffe680"))
  (when (and (= tool "pen") (not (contains (map first computed.inkPens) state.inkColor)))
    (set "state.inkColor" "#1f2933"))
  (when (some? tool)
    (set "state.board.selection" [])
    (set "state.editing" nil)))

(defn inkPress "Starts a stroke at a point of the slide, or erases there." [x y]
  (set "state.strokeFrom" [x y])
  (if (= state.inkTool "eraser") (eraseAt x y) (set "state.stroke" [[x y]])))

(defn inkDrag "Goes on with the stroke, or erases, at the press moved by dx and dy, in slide points." [dx dy]
  (let [x (+ (first state.strokeFrom) dx)
        y (+ (second state.strokeFrom) dy)]
    (if (= state.inkTool "eraser")
      (eraseAt x y)
      (when (and (> (count state.stroke) 0) (< (count state.stroke) 2000))
        (push "state.stroke" [x y])))))

(defn inkRelease "Ends the stroke: two points or more become an ink object." []
  (when (and (not= state.inkTool "eraser") (> (count state.stroke) 1))
    (let [xs (map first state.stroke)
          ys (map second state.stroke)]
      (addInk (apply min xs) (apply min ys) (apply max xs) (apply max ys))))
  (set "state.stroke" []))

(defn addInk
  "Adds the stroke as an ink object, a path in a box around its points; a highlighter's goes behind the others."
  [left top right bottom]
  (let [pad (+ (/ computed.strokeWidth 2) 1)
        w (+ (- right left) (* 2 pad))
        h (+ (- bottom top) (* 2 pad))
        point (fn [p] (str (fixed (+ (- (first p) left) pad) 1) " " (fixed (+ (- (second p) top) pad) 1)))]
    (insertObject {:kind "ink" :x (- left pad) :y (- top pad) :w w :h h :vw w :vh h
                   :color computed.strokeColor :width computed.strokeWidth
                   :d (str "M " (join " L " (map point state.stroke)))}))
  (when computed.inkHighlight
    (set-objects (concat [(last computed.objects)] (slice computed.objects 0 (dec (count computed.objects))))))
  (set "state.board.selection" []))

(defn eraseAt "Erases the topmost ink stroke whose box is under a point." [x y]
  (eraseInk (:id (find (fn [o] (and (= o.kind "ink") (inside? x y o))) (reverse computed.objects)))))

(defn eraseInk "Takes an ink stroke off the slide, by its id (nil: none)." [hit]
  (when (some? hit)
    (commit)
    (set-objects (filter (fn [it] (not= it.id hit)) computed.objects))))

(defn clearInk "Takes every ink stroke off the slide." []
  (when (> computed.slideInk 0)
    (commit)
    (set-objects (filter (fn [it] (not= it.kind "ink")) computed.objects))))

;;; Rehearsing

(defn rehearse "Starts the show from the beginning, keeping how long each slide is shown." []
  (commit)
  (set "state.show.rehearsing" true)
  (present 0))

(defn clearTimings "Forgets every slide's timing." []
  (when (> computed.timed 0)
    (commit)
    (set "state.deck.slides" (map (fn [it] (merge it {:advanceAfter nil})) state.deck.slides))))

;;; The side pane

(defn openSidePane "Opens a side pane (comments, select, motion, access, keys or about); the same again closes it."
  [pane]
  (set "state.sidePane" (if (and (= state.sidePane pane) (not state.formatPane)) nil pane))
  (set "state.formatPane" false))

(defn closeSidePane "Closes the side pane." []
  (set "state.sidePane" nil))

(defn selectObject "Selects an object of the selection pane, unless it is hidden." [id hidden]
  (when (not (or-else hidden false))
    (set "state.board.selection" [id])))

(defn selectTarget "Selects the object a build plays on." [id]
  (set "state.board.selection" [id]))

(defn hideObjects "Hides objects, by their ids, or shows them; hidden ones leave the selection." [ids hidden]
  (commit)
  (set-objects (map (fn [it] (if (contains ids it.id) (merge it {:hidden hidden}) it)) computed.objects))
  (set "state.board.selection"
       (if hidden (filter (fn [it] (not (contains ids it))) state.board.selection) state.board.selection)))

;;; Builds

(defn swapAnimations "Swaps the build at a place with its neighbour before (by -1) or after (by 1)." [i by]
  (let [animations (or-else computed.slide.animations [])
        j (+ i by)]
    (when (and (>= i 0) (>= j 0) (< j (count animations)))
      (commit)
      (set "state.deck.slides[state.current].animations"
           (map-indexed (fn [index it] (cond (= index i) (get animations j) (= index j) (get animations i) :else it))
                        animations)))))

(defn moveAnimation "Plays a build earlier (by -1) or later (by 1)." [id by]
  (swapAnimations (indexOf (map (fn [it] it.id) (or-else computed.slide.animations [])) id) by))

(defn removeAnimation "Takes a build off the slide." [id]
  (commit)
  (set "state.deck.slides[state.current].animations"
       (filter (fn [it] (not= it.id id)) (or-else computed.slide.animations []))))

;;; Comments

(defn resolveComment "Resolves a comment of the slide, by its place, or opens it again." [n]
  (commit)
  (toggle (str "state.deck.slides[" state.current "].comments[" n "].done")))

(defn newComment "Adds an empty comment to the slide, and shows the comments with it in focus." []
  (commit)
  (let [id (str "c" state.deck.next)]
    (set "state.deck.slides[state.current].comments" (concat computed.comments [{:id id :text "" :done false}]))
    (set "state.commentFocus" id))
  (increment "state.deck.next" :by 1)
  (set "state.sidePane" "comments")
  (set "state.formatPane" false))

(defn deleteComment "Deletes a comment of the slide." [id]
  (commit)
  (set "state.deck.slides[state.current].comments" (filter (fn [it] (not= it.id id)) computed.comments)))

(defn deleteComments "Deletes the slide's comments, or every slide's." [all]
  (commit)
  (set "state.deck.slides"
       (map-indexed (fn [index it] (if (or all (= index state.current)) (merge it {:comments []}) it))
                    state.deck.slides)))

(defn goToComment "Shows the next slide with comments (or the previous), round to the first (or the last)." [forward]
  (when (> (count computed.commented) 0)
    (set "state.current"
         (if forward
           (or-else (find (fn [it] (> it state.current)) computed.commented) (first computed.commented))
           (or-else (find (fn [it] (< it state.current)) (reverse computed.commented)) (last computed.commented))))
    (set "state.board.selection" [])
    (set "state.sidePane" "comments")
    (set "state.formatPane" false)))

;;; The accessibility check

(defn goToIssue "Shows a slide the check found, and selects its object (nil: the slide)." [slide id]
  (set "state.current" slide)
  (set "state.board.selection" (if (nil? id) [] [id])))
