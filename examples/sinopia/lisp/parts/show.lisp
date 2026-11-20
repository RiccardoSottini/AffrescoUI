;;; show.lisp --- The slide show
;;;
;;; The node audience draws the slide being shown into the image 'audience' (a snapshot), with its transition from the
;;; slide before and its builds: the audience window shows that image, and so does this window, full screen, or the
;;; presenter view beside the next slide, the notes and a timer. state.show.t runs from 0 to 1 through a transition,
;;; and state.show.clock through the builds of the step shown (milliseconds), both animated. Each slide is also drawn
;;; once into its own image, 'slide-<id>', by the page nodes: the thumbnails, the sorter and the presenter view show
;;; those images, and the PDF export draws the page nodes themselves, as vectors.

(ns sinopia.parts.show)

;;; Definitions

(defstate presenterView false "Whether the show opens in a window of its own, with the presenter view here.")

(defstate show
  {:on false
   :index 0
   :from -1
   :step 0
   :clock 0
   :t 1
   :black false
   :white false
   :ended false
   :seconds 0
   :paused false
   :notesSize 18
   :onSlide 0
   :elapsed 0
   :rehearsing false
   :navigator false
   :typed ""
   :preview false})

(defcomputed showOrder "The indexes of the slides the show shows: those not hidden."
  (remove #(or-else (get-in state.deck.slides [% :hidden]) false) (range (count state.deck.slides))))

(defcomputed showSlide "The slide the show shows."
  (or-else (get state.deck.slides state.show.index) state.deck.slides.0))

(defcomputed showPh "The placeholders of the shown slide's layout."
  (or-else (:ph (or-else (find #(= (:id %) computed.showSlide.layout) computed.layouts) computed.layouts.1))
           []))

(defcomputed showPhIds "Their names." (map :ph computed.showPh))

(defcomputed showUnbuilt "The objects whose builds haven't come yet, or have gone (an emphasis keeps its object)."
  (->> computed.showTimings
       (filter #(case (:kind %)
                  :exit (< (:step %) state.show.step)
                  :emphasis false
                  :path false
                  (> (:step %) state.show.step)))
       (map :target)))

(defmacro shown-placeholder "The placeholder of the shown slide's layout that object `o` fills, or an empty map." [o]
  `(or-else (get computed.showPh (indexOf computed.showPhIds (:ph ~o))) {}))

(defcomputed showLinks "The shown slide's linked objects, each with its box and the slide it goes to."
  (map (fn [it]
         {:link it.link
          :x (or-else it.x (:x (shown-placeholder it)) 0)
          :y (or-else it.y (:y (shown-placeholder it)) 0)
          :w (or-else it.w (:w (shown-placeholder it)) 100)
          :h (or-else it.h (:h (shown-placeholder it)) 100)})
       (filter (fn [it] (and (some? it.link)
                             (not (or-else it.hidden false))
                             (not (contains computed.showUnbuilt it.id))))
               (or-else computed.showSlide.objects []))))

(defcomputed showFrom "The slide the transition comes from, or nil."
  (if (>= state.show.from 0) (get state.deck.slides state.show.from) nil))

(defcomputed showKind "The transition playing, or none once it is done."
  (if (>= state.show.t 1) "none" (or-else computed.showSlide.transition.kind "none")))

(defcomputed showStepEnd "When the builds of the step shown end, in milliseconds."
  (->> computed.showTimings (filter #(= (:step %) state.show.step)) (map :end) (concat [0]) max))

(defcomputed showFades "Whether the transition fades through black: a fade, unless it fades smoothly."
  (and (= computed.showKind :fade) (not= computed.showSlide.transition.option "smoothly")))

(defcomputed showTimings "Each build of the shown slide with its step, its start and its end, its repeats included."
  (scan (or-else computed.showSlide.animations [])
        (fn [acc it]
          (merge it
                 {:step (+ acc.step (if (= it.start :click) 1 0))
                  :start (+ (case it.start :click 0 :with acc.start acc.end)
                            (or-else it.delay 0))
                  :end (+ (case it.start :click 0 :with acc.start acc.end)
                          (or-else it.delay 0)
                          (* (or-else it.ms 500) (max (or-else it.repeat 1) 1)))}))
        {:step 0 :start 0 :end 0}))

(defcomputed showSteps "How many clicks the shown slide's builds take."
  (count (filter #(= (:start %) :click) (or-else computed.showSlide.animations []))))

(defcomputed showNext "The next slide the show shows, or nil."
  (find #(> % state.show.index) computed.showOrder))

(defcomputed showPrevious "The slide before, or nil."
  (find #(< % state.show.index) (reverse computed.showOrder)))

(defcomputed showPosition "The shown slide's place in the show." (inc (indexOf computed.showOrder state.show.index)))

(defcomputed showTimer "The presenter's timer, as hours, minutes and seconds."
  (str (pad (floor (/ state.show.seconds 3600)) 2 "0")
       ":"
       (pad (mod (floor (/ state.show.seconds 60)) 60) 2 "0")
       ":"
       (pad (mod state.show.seconds 60) 2 "0")))

(defstyle presenterButton "A button of the presenter view."
  (label :height 32
    :fitText true
    :padding 10
    :radius 16
    :color "#2a3230"
    :textColor "#eef3f1"
    :cursor hand
    :focusable true
    :hover {:color "#38423f"}))

(defstyle presenterButtonOn "A presenter view's button that is on."
  {:style presenterButton :color "#0e7c6b" :hover {:color "#139080"}})

(defcomponent showSlide
  "A slide as the show draws it: each object hidden until its build, then built; in a glide or a smooth fade, the
  objects of the slide before that it hasn't fade out, those it shares move to their places, and its others fade in."
  {:slide nil :step 0 :clock 0 :timings [] :prefix "" :glide [] :leaving [] :fromLayout nil :fade 1 :moved 1}
  `(node :id ~(str prefix "show")
     :color transparent
     :x 0
     :y 0
     :width 960
     :height 540
     :let {:layout ~(or-else (find #(= (:id %) slide.layout) computed.layouts) computed.layouts.1)
           :ground ~(slice (or-else (get computed.palette (or-else slide.background computed.theme.background))
                                    slide.background
                                    "#ffffff")
                           0
                           7)}
     (node :use slideBackground :with {:slide ~slide :prefix ~prefix})
     ~(repeat [o leaving :key o.id]
       (leaving-object prefix o))
     ~(repeat [o slide.objects :key o.id]
       (build step clock timings prefix o))))

(defcomponent showStage
  "What the audience sees: the slide before and the slide shown, as its transition goes."
  {}
  `(node#audience :snapshot {:image audience :scale 2}
     :x 0
     :y 0
     :width 960
     :height 540
     :color "#000000"
     :let {:k ~computed.showKind
           :t ~state.show.t
           :u ~(- 1 state.show.t)
           :d ~(or-else computed.showSlide.transition.direction "left")
           :dx ~(case d :left 960 :right -960 0)
           :dy ~(case d :up 540 :down -540 0)
           :band ~(case k
                    :wipe (case d
                            :left [(* 960 u) 0 (* 960 t) 540]
                            :right [0 0 (* 960 t) 540]
                            :up [0 (* 540 u) 960 (* 540 t)]
                            [0 0 960 (* 540 t)])
                    :split [(* 480 u) 0 (* 960 t) 540]
                    [0 0 960 540])}
     ~(show-from)
     ~(show-to)
     ~(show-marks)
     (node#show-from-over :when ~(and (some? computed.showFrom) (= k :reveal))
       :color transparent
       :x 0
       :y 0
       :width 960
       :height 540
       :translate ~[(* (- dx) t) (* (- dy) t)]
       (node :use slidePage :with {:slide ~computed.showFrom :prefix r-}))
     (node#show-fade :when ~computed.showFades :x 0 :y 0 :width 960 :height 540
       :color ~(gradientColor {:stops [{:at 0 :color "#00000000"} {:at 1 :color "#000000"}]}
                              (if (< t 0.5) (* 2 t) (* 2 u))))
     ~(show-blank)))

(defview show-blank "A black or white screen, or the end of the show." []
  `(node#show-blank :when ~(or state.show.black state.show.white state.show.ended)
     :x 0
     :y 0
     :width 960
     :height 540
     :color ~(if state.show.white "#ffffff" "#000000")
     (label#show-ended :when ~state.show.ended
       :x 0
       :y 16
       :width 960
       :height 24
       :align center
       :runs [{:text ~(t "show.endOfTheSlideShow") :size 14 :color "#ccd6d2"}])))

(defview show-from "The slide before, under the slide shown, pushed away or faded out." []
  `(node#show-from :when ~(and (some? computed.showFrom)
                               (not (#{:none :reveal} k))
                               (not computed.showDissolve)
                               (not (and computed.showFades (>= t 0.5))))
     :color transparent
     :x 0
     :y 0
     :width 960
     :height 540
     :translate ~(if (= k :push) [(* (- dx) t) (* (- dy) t)] [0 0])
     (node :use slidePage :with {:slide ~computed.showFrom :prefix f-})))

(defview show-to "The slide shown, coming in by its transition." []
  `(node#show-to :when ~(not (and computed.showFades (< t 0.5)))
     :color transparent
     :x ~band.0
     :y ~band.1
     :width ~(max band.2 0)
     :height ~(max band.3 0)
     :translate ~(if (#{:push :cover} k) [(* dx u) (* dy u)] [0 0])
     (node#show-to-page :color transparent :x ~(- band.0) :y ~(- band.1) :width 960 :height 540
       (node :use showSlide
         :with {:slide ~computed.showSlide
                :step ~state.show.step
                :clock ~state.show.clock
                :timings ~computed.showTimings
                :prefix a-
                :glide ~computed.showGlide
                :leaving ~computed.showLeaving
                :fromLayout ~computed.showFromLayout
                :fade ~(if computed.showDissolve t 1)
                :moved ~t}))))

(defcomponent slideImage
  "A slide drawn once into the image slide-<id>, for the thumbnails and the presenter; the PDF's pages."
  {:s nil}
  `(node :id ~(str "page-" s.id)
     :snapshot {:image ~(str "slide-" s.id) :scale ~(clamp (/ window.scale 4) 0.25 1)}
     :x 0
     :y 0
     :width 960
     :height 540
     :color "#ffffff"
     (node :use slidePage :with {:slide ~s :prefix ~(str "p" s.id "-")})))

(defcomponent showView "This window while the show runs: the show itself, full screen, or the presenter view."
  {}
  `(node#show-view :color "#000000"
     :focusable true
     :role dialog
     :label ~(t "show.slideShow")
     :every {:ms 1000 :do show-second}
     :onClick ~(handler show-view-click event.left event.top event.width event.height)
     (image#show-image :when ~(or (not state.presenterView) state.show.preview) :fill 0 :source audience :fit contain)
     ~(pen-layer "show-pen-layer" (or (not state.presenterView) state.show.preview))
     (node :use presenter :when ~(and state.presenterView (not state.show.preview)))
     ~(rehearsal)))

(defcomponent presenter "The presenter view: the slide shown, the next, the notes, a timer and the controls."
  {}
  `(node#presenter :fill 0 :color "#141a19" :every {:ms 1000 :do show-tick}
     ~(presenter-top)
     (node#presenter-current :anchors {:left 24 :top 60 :right 384 :bottom 76} :color "#000000" :radius 4
       (image#presenter-current-image :fill 0 :source audience :fit contain)
       ~(pen-layer "presenter-pen-layer" true))
     (grid#presenter-navigator :when ~state.show.navigator
       :anchors {:left 24 :top 60 :right 384 :bottom 76}
       :columns 4
       :gap 12
       :scroll vertical
       :color "#141a19"
       :role list
       :label ~(t "show.allSlides")
       ~(repeat [s state.deck.slides :index i :key s.id]
         (navigator-slide s i)))
     ~(presenter-side)
     ~(presenter-controls)))

(defkey "right" {:when ~state.show.on :do showNext})

(defkey "down" {:when ~state.show.on :do showNext})

(defkey "pagedown" {:when ~state.show.on :do showNext})

(defkey "space" {:when ~state.show.on :do showNext})

(defkey "enter"
  [{:when ~(and state.show.on (not= state.show.typed "")) :do show-typed} {:when ~state.show.on :do showNext}])

(defkey "n" {:when ~state.show.on :do showNext})

(defkey "left" {:when ~state.show.on :do show-previous})

(defkey "up" {:when ~state.show.on :do show-previous})

(defkey "pageup" {:when ~state.show.on :do show-previous})

(defkey "p" {:when ~state.show.on :do show-previous})

(defkey "home" {:when ~state.show.on :do show-go :with {:index ~(or-else computed.showOrder.0 0) :built false}})

(defkey "end"
  [{:when ~state.show.on
    :do show-go
    :with {:index ~(or-else (get computed.showOrder (dec (count computed.showOrder))) 0) :built true}}])

(defkey "0" {:when ~state.show.on :do show-type :with {:digit "0"}})

(defkey "1" {:when ~state.show.on :do show-type :with {:digit "1"}})

(defkey "2" {:when ~state.show.on :do show-type :with {:digit "2"}})

(defkey "3" {:when ~state.show.on :do show-type :with {:digit "3"}})

(defkey "4" {:when ~state.show.on :do show-type :with {:digit "4"}})

(defkey "5" {:when ~state.show.on :do show-type :with {:digit "5"}})

(defkey "6" {:when ~state.show.on :do show-type :with {:digit "6"}})

(defkey "7" {:when ~state.show.on :do show-type :with {:digit "7"}})

(defkey "8" {:when ~state.show.on :do show-type :with {:digit "8"}})

(defkey "9" {:when ~state.show.on :do show-type :with {:digit "9"}})

(defkey "b" {:when ~state.show.on :do show-black})

(defkey "period" {:when ~state.show.on :do show-black})

(defkey "w" {:when ~state.show.on :do show-white})

(defkey "comma" {:when ~state.show.on :do show-white})

(defkey "f5" {:when ~(not state.show.on) :do present :with {:from 0}})

(defkey "shift+f5" {:when ~(not state.show.on) :do present :with {:from ~state.current}})

(defpart show
         "What show adds."
         :windows {:audience {:title ~(t "show.slideShow")
                     :image audience
                     :background "#000000"
                     :fullscreen true
                     :onClick showNext
                     :onClose show-end}}
         :overlays {:zShow (node :use showView :when ~state.show.on :anchors {:left 0 :right 0 :top 0 :bottom 0})})

;;; Views

(defview build
  "Object `o` as the show draws it: hidden until its build, then faded, wiped, flown in or grown; pulsed, spun or
  teetered by an emphasis, each build repeated as often as it says; in a glide, moved from its place on the slide
  before."
  [step clock timings prefix o]
  `(node :id ~(str prefix "build-" o.id)
     :let {:a ~(find #(= (:target %) o.id) timings)
           :reps ~(max (or-else a.repeat 1) 1)
           :r ~(cond (or (nil? a) (< a.step step)) reps
                     (> a.step step) 0
                     :else (clamp (/ (- clock a.start) (max (or-else a.ms 500) 1)) 0 reps))
           :p ~(if (>= r reps) 1 (- r (floor r)))
           :q ~(cond (nil? a) 1 (#{:emphasis :path} a.kind) 1 (= a.kind :exit) (- 1 p) :else p)
           :g ~(find #(= (:id %) o.id) glide)
           :v ~(if (some? g) q (* q fade))
           :ph ~(or-else (find #(= (:ph %) o.ph) layout.ph) {})
           :box ~{:x (or-else o.x ph.x 0) :y (or-else o.y ph.y 0) :w (or-else o.w ph.w 100) :h (or-else o.h ph.h 100)}
           :effect ~(cond (= a.kind :emphasis) (if (and (pos? r) (< r reps)) a.effect "none")
                          (= a.kind :path) (if (pos? r) a.effect "none")
                          (< q 1) (or-else a.effect "appear")
                          (< v 1) "fade"
                          :else "none")
           :away ~(- 1 q)
           :wave ~(sin (* 3.14159 p))
           :swing ~(sin (* 12.566 p))
           :clip ~(wipe-clip effect (build-side a) box q)}
     :when ~(pos? v)
     :color transparent
     :x ~clip.0
     :y ~clip.1
     :width ~clip.2
     :height ~clip.3
     :translate ~(cond (= effect :fly) (fly-offset (or-else a.direction "bottom") box away)
                       (= a.kind :path) (path-offset effect p)
                       (some? g) (glide-offset g box moved)
                       :else [0 0])
     :scale ~(cond (= effect :zoom) (max q 0.01)
                   (= effect :pulse) (inc (* 0.15 wave))
                   (some? g) (glide-scale g box moved)
                   :else 1)
     :rotate ~(case effect :spin (* 360 p) :teeter (* 8 swing) 0)
     :transformOrigin ~[(/ (+ box.x (/ box.w 2)) 9.6) (/ (+ box.y (/ box.h 2)) 5.4)]
     (node :x ~(- clip.0) :y ~(- clip.1) :width 960 :height 540 :color transparent
       (node :use slideObject :with {:o ~o :prefix ~prefix :layout ~layout :edit false})
       (node :id ~(str prefix "veil-" o.id)
         :when ~(= effect :fade)
         :x ~(- box.x 4)
         :y ~(- box.y 4)
         :width ~(+ box.w 8)
         :height ~(+ box.h 8)
         :rotate ~(or-else o.rotate 0)
         :color ~(gradientColor {:stops [{:at 0 :color ground} {:at 1 :color (str ground "00")}]} v)))))

(defview rehearsal "The rehearsal's bar over the show: Next, Pause, this slide's time and the total." []
  `(row#rehearsal :when ~state.show.rehearsing
     :x 16
     :y 16
     :fitContent width
     :height 40
     :gap 4
     :radius 20
     :color "#141a19ee"
     :alignItems center
     :role toolbar
     :label ~(t "show.rehearsal")
     :onClick [{:note "a click on the bar isn't a click on the show"}]
     (node#rehearsal-start :width 0 :height 1 :color transparent)
     ~(rehearsal-next)
     (label#rehearsal-pause :style presenterButton
       :runs [{:icon ~(if state.show.paused "play" "pause") :size 15}]
       :tooltip ~(if state.show.paused (t "show.resume") (t "show.pause"))
       :onClick [{:toggle state.show.paused}])
     (label#rehearsal-slide :fitText true
       :height 32
       :padding 6
       :textColor "#eef3f1"
       :runs [{:text ~(str (floor (/ state.show.onSlide 60)) ":" (pad (mod state.show.onSlide 60) 2 "0"))
               :size 16
               :bold true}])
     (label#rehearsal-repeat :style presenterButton
       :runs [{:icon rotate-ccw :size 15}]
       :tooltip ~(t "show.timeThisSlideAgain")
       :onClick retime-slide)
     (label#rehearsal-total :fitText true
       :height 32
       :padding 8
       :textColor "#a3b1ac"
       :runs [{:text ~(str (floor (/ state.show.elapsed 60)) ":" (pad (mod state.show.elapsed 60) 2 "0")) :size 14}])
     (node#rehearsal-end :width 0 :height 1 :color transparent)))

(defview rehearsal-next "The rehearsal's Next." []
  `(label#rehearsal-next :style presenterButton :runs [{:icon arrow-right :size 15}] :tooltip ~(t "pane.next")
     :onClick showNext))

(defview presenter-top "The presenter view's top: the timer, its buttons, and the time of day." []
  `(row#presenter-top :anchors {:left 24 :right 24 :top 12} :height 36 :gap 8 :alignItems center :color transparent
     (label#presenter-timer :fitText true :height 32 :textColor "#eef3f1" :runs [{:text ~computed.showTimer :size 22}])
     (label#presenter-pause :style presenterButton
       :runs [{:icon ~(if state.show.paused "play" "pause") :size 15}]
       :tooltip ~(if state.show.paused (t "show.resumeTheTimer") (t "show.pauseTheTimer"))
       :onClick [{:toggle state.show.paused}])
     (label#presenter-reset :style presenterButton
       :runs [{:icon rotate-ccw :size 15}]
       :tooltip ~(t "show.restartTheTimer")
       :onClick [{:set state.show.seconds :to 0}])
     (node#presenter-top-gap :grow 1 :height 1 :color transparent)
     (label#presenter-clock :fitText true
                            :height 32
                            :textColor "#a3b1ac"
                            :runs [{:text ~(time (now) "HH:mm") :size 16}])))

(defview navigator-picture "Slide `s`'s picture in the presenter's grid, framed when it is shown." [s i]
  `(node :id ~(str "navigator-" s.id "-frame")
     :width 200
     :height 112
     :radius 4
     :color "#000000"
     :borderWidth 2
     :borderColor ~(if (= i state.show.index) "#5fd3bc" "transparent")
     :hover {:borderColor "#a3b1ac"}
     (image :id ~(str "navigator-" s.id "-image")
            :x 2
            :y 2
            :width 196
            :height 108
            :source ~(str "slide-" s.id)
            :fit fill)))

(defview navigator-slide "Slide `s` in the presenter's grid of every slide: a click goes to it." [s i]
  `(column :id ~(str "navigator-" s.id)
     :width 200
     :height 136
     :gap 4
     :alignItems center
     :color transparent
     :cursor hand
     :role listitem
     :label ~(t "slide.n" {:n (inc i)})
     :onClick ~(handler show-jump i)
     ~(navigator-picture s i)
     (label :id ~(str "navigator-" s.id "-number")
       :width 200
       :height 18
       :align center
       :textColor "#a3b1ac"
       :runs [{:text ~(if (or-else s.hidden false) (t "show.hiddenN" {:n (inc i)}) (str (inc i))) :size 12}])))

(defview presenter-side "The presenter view's side: the next slide, and the notes, larger or smaller." []
  `(column#presenter-side :width 336 :anchors {:right 24 :top 60 :bottom 76} :gap 8 :color transparent
     :alignItems stretch
     (label#presenter-next-title :height 20
       :textColor "#a3b1ac"
       :runs [{:text ~(if (some? computed.showNext) (t "pane.next") (t "show.endOfTheSlideShow2")) :size 13}])
     (node#presenter-next :height 189 :color "#000000" :radius 4
       (image#presenter-next-image :fill 0
         :when ~(some? computed.showNext)
         :source ~(str "slide-" (get-in state.deck.slides [computed.showNext :id]))
         :fit contain))
     (row#presenter-notes-bar :height 24 :gap 4 :alignItems center :color transparent
       (label#presenter-notes-title :grow 1 :height 20 :textColor "#a3b1ac" :runs [{:text ~(t "bar.notes") :size 13}])
       (label#presenter-notes-smaller :style presenterButton
         :height 24
         :padding 6
         :runs [{:icon a-arrow-down :size 14}]
         :tooltip ~(t "show.smallerNotes")
         :onClick ~(handler notes-by -2))
       (label#presenter-notes-larger :style presenterButton
         :height 24
         :padding 6
         :runs [{:icon a-arrow-up :size 14}]
         :tooltip ~(t "show.largerNotes")
         :onClick ~(handler notes-by 2)))
     (column#presenter-notes-box :grow 1 :scroll vertical :color transparent
       (label#presenter-notes :width 336
         :wrap true
         :fitText {:maxWidth 336}
         :align start
         :runs [{:text ~(if (= (or-else computed.showSlide.notes "") "")
                            (t "show.noNotesForThisSlide")
                            computed.showSlide.notes)
                 :size ~state.show.notesSize
                 :color ~(if (= (or-else computed.showSlide.notes "") "") "#7a8884" "#eef3f1")}]))))

(defview presenter-controls "The presenter view's bottom: back, where the show is, on, black, every slide, end." []
  `(row#presenter-controls :anchors {:left 24 :right 24 :bottom 22} :height 36 :gap 8 :alignItems center
     :color transparent
     (label#presenter-previous :style presenterButton
       :runs [{:icon chevron-left :size 16}]
       :tooltip ~(t "show.previousLeft")
       :onClick show-previous)
     (label#presenter-where :fitText true
       :height 32
       :padding 6
       :textColor "#eef3f1"
       :runs [{:text ~(t "slide.nOfCount" {:n computed.showPosition :count (count computed.showOrder)}) :size 14}])
     (label#presenter-forward :style presenterButton
       :runs [{:icon chevron-right :size 16}]
       :tooltip ~(t "show.nextRight")
       :onClick showNext)
     (label#presenter-black :style ~(if state.show.black "presenterButtonOn" "presenterButton")
       :runs [{:icon square :size 14} {:text ~(t "show.blackB") :size 13}]
       :onClick show-black)
     (label#presenter-all :style ~(if state.show.navigator "presenterButtonOn" "presenterButton")
       :runs [{:icon layout-grid :size 14} {:text ~(t "show.seeAllSlides") :size 13}]
       :onClick [{:toggle state.show.navigator}])
     ~(presenter-tools)
     (node#presenter-gap :grow 1 :height 1 :color transparent)
     (label#presenter-end :style presenterButton :runs [{:text ~(t "show.endSlideShow") :size 13}] :onClick show-end)))

;;; Starting and ending

(def show-reset "The show's state when it starts or goes to a slide."
  {:on true :from -1 :black false :white false :ended false :seconds 0 :paused false :elapsed 0})

(defn present "Starts the show at a slide: full screen, or in the audience's window with the presenter view here."
  [from]
  (show-start from)
  (if state.presenterView (openWindow "audience") (fullscreen true)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn readingView "Starts the show in this window, at a slide." [from]
  (show-start from))

(defn show-start "Starts the show at the first slide shown from an index." [from]
  (set "state.show" (merge state.show show-reset))
  (set "state.showInk" {})
  (pen-tool nil)
  (show-enter -1 (or-else (find #(>= % from) computed.showOrder) (get computed.showOrder 0) 0))
  (focus "#show-view"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn previewSlide
  "Plays the slide shown here as the show would: its transition, a build each second, then the end."
  []
  (set "state.show" (merge state.show show-reset {:preview true}))
  (show-enter (if (pos? state.current) (dec state.current) -1) state.current)
  (focus "#show-view"))

(defn show-end "Ends the show, keeping a rehearsal's timings, and shows the slide it ended on." []
  (when state.show.on
    (when state.show.rehearsing
      (set "state.deck.slides[state.show.index].advanceAfter" state.show.onSlide)
      (set "state.deck.useTimings" true)
      (set "state.show.rehearsing" false))
    (set "state.current" state.show.index)
    (set "state.show" (merge state.show {:on false :t 1 :from -1 :black false :white false :ended false
                                         :preview false}))
    (set "state.board.selection" [])
    (when windows.audience.open (closeWindow "audience"))
    (when windows.main.fullscreen (fullscreen false))
    (focus "#thumbnails")))

(defn toggle-presenter-view "Shows the next show in the audience's window, with the presenter view here, or not." []
  (toggle "state.presenterView"))

;;; Going through the slides

(defn show-enter "Goes to a slide from another, by its transition, keeping a rehearsal's time on the one left."
  [previous index]
  (when (and state.show.rehearsing (>= previous 0))
    (set (str "state.deck.slides[" previous "].advanceAfter") state.show.onSlide))
  (set "state.show" (merge state.show {:index index :from previous :step 0 :clock 0 :t 0 :ended false :onSlide 0}))
  (if (= (or-else computed.showSlide.transition.kind :none) :none)
    (set "state.show.t" 1)
    (animate "state.show.t" :to 1 :ms (or-else computed.showSlide.transition.ms 500) :easing "inOut"))
  (show-play))

(defn show-play "Plays the builds of the step shown, if it has any." []
  (if (pos? (->> computed.showTimings (filter #(= (:step %) state.show.step)) (map :end) (apply max 0)))
    (animate "state.show.clock" :to computed.showStepEnd :ms computed.showStepEnd :easing "linear")
    (set "state.show.clock" 0)))

; style: keep, show-next is already a definition
(defn showNext "The next build, else the next slide, else the end; after the end, leaves." []
  (cond state.show.ended (show-end)
        (< state.show.step computed.showSteps) (do (set "state.show.clock" 0)
                                                    (increment "state.show.step")
                                                    (set "state.show.t" 1)
                                                    (show-play))
        (some? computed.showNext) (show-enter state.show.index computed.showNext)
        :else (set "state.show.ended" true)))

(defn show-previous "Back a build, else to the slide before, built." []
  (cond state.show.ended (set "state.show.ended" false)
        (pos? state.show.step) (do (set "state.show.clock" 1000000) (increment "state.show.step" :by -1))
        (some? computed.showPrevious) (show-go computed.showPrevious true)))

(defn show-go "Goes straight to a slide, without its transition, built or not." [index built]
  (set "state.show"
       (merge state.show {:index (clamp index 0 (dec (count state.deck.slides))) :from -1 :t 1 :clock 1000000
                          :ended false :black false :white false :typed ""}))
  (set "state.show.step" (if built computed.showSteps 0)))

(defn show-jump "Goes to a slide chosen in the presenter's grid, and closes the grid." [index]
  (show-go index false)
  (set "state.show.navigator" false))

(defn inside? "Whether a point of the slide is in a box." [x y box]
  (and (>= x box.x) (<= x (+ box.x box.w)) (>= y box.y) (<= y (+ box.y box.h))))

(defn show-click "A click on the 960 by 540 slide: on linked text it goes to that slide, elsewhere it moves on." [x y]
  (if-some [link (find #(inside? x y %) (reverse computed.showLinks))]
      (show-go (indexOf (map :id state.deck.slides) (:link link)) false)
      (showNext)))

(defn show-view-click "A click on the show in this window, at its place on the slide drawn to fit."
  [left top width height]
  (when (or (not state.presenterView) state.show.preview)
    (let [scale (min (/ width 960) (/ height 540))]
      (show-click (/ (- left (/ (- width (* 960 scale)) 2)) scale) (/ (- top (/ (- height (* 540 scale)) 2)) scale)))))

(defn show-type "A digit typed in the show, of the slide number Enter goes to." [digit]
  (set "state.show.typed" (str state.show.typed digit)))

(defn show-typed "Goes to the slide whose number was typed." []
  (show-go (dec (number state.show.typed)) false)
  (set "state.show.typed" ""))

(defn show-black "Shows a black screen, or the show again." []
  (set "state.show.white" false)
  (toggle "state.show.black"))

(defn show-white "Shows a white screen, or the show again." []
  (set "state.show.black" false)
  (toggle "state.show.white"))

;;; Time

(defn show-second
  "Each second of the show: the slide's time, and the next step when the deck's timing for it is up."
  []
  (when-not (or state.show.paused state.show.ended)
    (increment "state.show.onSlide")
    (increment "state.show.elapsed")
    (when (and state.show.preview (>= state.show.t 1) (>= state.show.clock computed.showStepEnd))
      (if (< state.show.step computed.showSteps) (showNext) (show-end)))
    (when (and (not state.show.rehearsing)
               (or-else state.deck.useTimings false)
               (some? computed.showSlide.advanceAfter)
               (>= state.show.onSlide computed.showSlide.advanceAfter))
      (showNext))))

(defn show-tick "The presenter's timer, each second it isn't paused." []
  (when-not state.show.paused
    (increment "state.show.seconds")))

(defn retime-slide "Times the slide shown again, in a rehearsal." []
  (set "state.show.elapsed" (- state.show.elapsed state.show.onSlide))
  (set "state.show.onSlide" 0))

(defn notes-by "Makes the presenter's notes larger or smaller, between 10 and 48." [by]
  (set "state.show.notesSize" (clamp (+ state.show.notesSize by) 10 48)))
