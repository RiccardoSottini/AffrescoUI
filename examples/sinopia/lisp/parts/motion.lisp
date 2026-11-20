;;; motion.lisp --- Glide, emphasis, the builds' timing, and the presenter's pen and laser pointer
;;;
;;; A glide matches the objects of two slides (by name, else the same object moved or resized) and moves each from
;;; its place on the slide before to its place on the slide shown, while the others fade out or in; a smooth fade is a
;;; glide that matches nothing. An emphasis pulses, spins or teeters an object that stays shown. A build waits its
;;; delay and plays as many times as it repeats. The presenter draws on the slide shown with a pen, strokes kept per
;;; slide while the show runs, or points at it with a laser dot the audience sees.

(ns sinopia.parts.motion)

;;; Definitions

(defstate showInk {} "The presenter's pen strokes, by slide id: each a list of strokes, each a list of points.")

(defstate showPen
  {:tool nil
   :stroke []
   :from [0 0]
   :laser nil}
  "The presenter's tool (pen, laser or nil), the stroke being drawn, where the press was, and the laser's point.")

(defcomputed showDissolve "Whether the transition dissolves the slide's objects: a glide, or a smooth fade."
  (or (= computed.showKind :glide)
      (and (= computed.showKind :fade) (= computed.showSlide.transition.option "smoothly"))))

(defcomputed showFromLayout "The layout of the slide the transition comes from."
  (or-else (find #(= (:id %) computed.showFrom.layout) computed.layouts) computed.layouts.1))

(defcomputed showGlide "In a glide, each object of the slide shown that the slide before has too, with its box there."
  (if (and (= computed.showKind :glide) (some? computed.showFrom))
    (glide-pairs computed.showSlide.objects computed.showFrom.objects computed.showFromLayout)
    []))

(defcomputed showLeaving "In a dissolve, the objects of the slide before that the slide shown hasn't: they fade out."
  (if (and computed.showDissolve (some? computed.showFrom))
    (leaving-objects computed.showFrom.objects (if (= computed.showKind :glide) computed.showSlide.objects []))
    []))

(defcomputed showStrokes "The presenter's pen strokes on the slide shown, as paths, the one being drawn last."
  (stroke-paths (or-else (get state.showInk computed.showSlide.id) []) state.showPen.stroke))

(defcomputed flySides "The sides a build flies in from, or out to, each with its arrow."
  '[["bottom" "move-down"] ["left" "move-left"] ["top" "move-up"] ["right" "arrow-right"]])

(defcomputed pathEffects "The motion paths of the gallery: each with its icon and its name."
  [["pathRight" "move-right" (t "motion.pathRight")]
   ["pathDown" "move-down" (t "motion.pathDown")]
   ["pathArc" "redo-2" (t "motion.pathArc")]
   ["pathCircle" "circle-dashed" (t "motion.pathCircle")]])

(defcomputed emphasisEffects "The emphasis effects of the gallery: each with its icon and its name."
  [["pulse" "heart-pulse" (t "motion.pulse")]
   ["spin" "rotate-cw" (t "motion.spin")]
   ["teeter" "vibrate" (t "motion.teeter")]])

;;; Matching and moving

(defn same-object?
  "Whether two objects of two slides are one, for a glide: by name, else the same kind, text and shape, moved or
  resized."
  [a b]
  (if (or (some? (:name a)) (some? (:name b)))
    (= (:name a) (:name b))
    (every? #(= (get a %) (get b %)) [:kind :ph :text :shape :icon :d])))

(defn glide-pairs "Each object of a slide that another has too, its id with its box there." [objects before layout]
  (for [o objects
        :let [was (find #(same-object? o %) before)]
        :when (some? was)]
    (merge (object-box was layout) {:id (:id o)})))

(defn shared? "Whether an object is on a slide of these objects too." [o objects]
  (some #(same-object? o %) objects))

(defn leaving-objects "The shown objects of the slide before that aren't on the slide shown." [before objects]
  (remove #(or (:hidden %) (shared? % objects)) before))

(defn point-text "A point of a path, as its text." [[x y]]
  (str (fixed x 1) " " (fixed y 1)))

(defn stroke-paths "The strokes kept and the one being drawn, those of two points or more, each as a path." [kept live]
  (for [stroke (conj kept live)
        :when (> (count stroke) 1)]
    (str "M " (join " L " (map point-text stroke)))))

(defn object-box "An object's box on a slide of a layout: its own, else its placeholder's." [o layout]
  (let [ph (or-else (find #(= (:ph %) (:ph o)) (:ph layout)) {})]
    {:x (or-else (:x o) (:x ph) 0) :y (or-else (:y o) (:y ph) 0)
     :w (or-else (:w o) (:w ph) 100) :h (or-else (:h o) (:h ph) 100)}))

(defn glide-offset "How far an object's centre is from where it was, the part `moved` of the way done." [g box moved]
  (let [away (- 1 moved)
        dx (- (+ g.x (/ g.w 2)) box.x (/ box.w 2))
        dy (- (+ g.y (/ g.h 2)) box.y (/ box.h 2))]
    [(* away dx) (* away dy)]))

(defn glide-scale "An object's scale from its size there to its size here, the part `moved` of the way done."
  [g box moved]
  (let [away (- 1 moved)
        sx (/ g.w (max box.w 1))
        sy (/ g.h (max box.h 1))]
    [(+ moved (* away sx)) (+ moved (* away sy))]))

(defn path-offset "Where an object is along a motion path, the part `p` of the way done, from where it starts." [path p]
  (let [turn (* 6.28318 p)
        rise (sin (* 3.14159 p))
        drop (- 1 (cos turn))]
    (case path
      "pathRight" [(* 240 p) 0]
      "pathDown" [0 (* 160 p)]
      "pathArc" [(* 240 p) (* -120 rise)]
      "pathCircle" [(* 80 (sin turn)) (* 80 drop)]
      [0 0])))

(defn fly-offset "Where a flying object is, the part `away` of the way off the slide, by the side it flies from."
  [side box away]
  (case side
    "left" [(* away (- 0 box.x box.w 20)) 0]
    "right" [(* away (- 980 box.x)) 0]
    "top" [0 (* away (- 0 box.y box.h 20))]
    [0 (* away (- 560 box.y))]))

;;; Views

(defview leaving-object "Object `o` of the slide before, fading out as the slide shown dissolves in." [prefix o]
  `(node :id ~(str prefix "leave-" o.id)
     :let {:ph ~(or-else (find #(= (:ph %) o.ph) fromLayout.ph) {})
           :box ~{:x (or-else o.x ph.x 0) :y (or-else o.y ph.y 0) :w (or-else o.w ph.w 100) :h (or-else o.h ph.h 100)}}
     :when ~(< moved 1)
     :color transparent
     :x 0
     :y 0
     :width 960
     :height 540
     (node :use slideObject :with {:o ~o :prefix ~(str prefix "leaving-") :layout ~fromLayout :edit false})
     (node :id ~(str prefix "leave-veil-" o.id)
       :x ~(- box.x 4)
       :y ~(- box.y 4)
       :width ~(+ box.w 8)
       :height ~(+ box.h 8)
       :rotate ~(or-else o.rotate 0)
       :color ~(gradientColor {:stops [{:at 0 :color ground} {:at 1 :color (str ground "00")}]} (- 1 moved)))))

(defview show-marks "Over the slide shown: the presenter's pen strokes, and the laser's dot." []
  `(node#show-marks :x 0 :y 0 :width 960 :height 540 :color transparent
     ~(repeat [d computed.showStrokes :index i]
       `(path :id ~(str "show-stroke-" i)
          :x 0
          :y 0
          :width 960
          :height 540
          :d ~d
          :stroke "#e03131"
          :strokeWidth 4
          :strokeCap round
          :strokeJoin round))
     (node#show-laser :when ~(some? state.showPen.laser)
       :x ~(- (or-else state.showPen.laser.0 0) 9)
       :y ~(- (or-else state.showPen.laser.1 0) 9)
       :width 18
       :height 18
       :radius 9
       :color "#ff2d2d"
       :borderWidth 3
       :borderColor "#ff9c9c"
       :shadow {:blur 12 :color "#ff2d2d99"})))

(defview presenter-tools "The presenter's pen, laser pointer and Erase All, under the slide shown." []
  `(row#presenter-tools :fitContent width :height 32 :gap 6 :alignItems center :color transparent
     (label#presenter-pen :style ~(if (= state.showPen.tool "pen") "presenterButtonOn" "presenterButton")
       :runs [{:icon pencil :size 14} {:text ~(t "motion.pen") :size 13}]
       :tooltip ~(t "motion.drawOnTheSlide")
       :onClick ~(handler pen-tool "pen"))
     (label#presenter-laser :style ~(if (= state.showPen.tool "laser") "presenterButtonOn" "presenterButton")
       :runs [{:icon mouse-pointer-2 :size 14} {:text ~(t "motion.laserPointer") :size 13}]
       :tooltip ~(t "motion.pointAtTheSlide")
       :onClick ~(handler pen-tool "laser"))
     (label#presenter-erase :style presenterButton
       :runs [{:icon eraser :size 14} {:text ~(t "motion.eraseAll") :size 13}]
       :disabled ~(empty? (or-else (get state.showInk computed.showSlide.id) []))
       :tooltip ~(t "motion.eraseThePenOnThis")
       :onClick erase-show-ink)))

(defview pen-layer "Over the slide shown while the pen or the laser is on: a drag draws, or points; a click stays."
  [id shown]
  `(node :id ~id
     :when ~(and shown (some? state.showPen.tool))
     :fill 0
     :color transparent
     :cursor ~(if (= state.showPen.tool "pen") "ibeam" "hand")
     :role region
     :label ~(t "motion.penLayer")
     :onPress ~(handler pen-press event.left event.top event.width event.height)
     :onDrag ~(handler pen-drag event.dx event.dy event.width event.height)
     :onRelease pen-release
     :onClick [{:note "a click with the pen or the laser isn't a click on the show"}]))

(defview fade-options "A fade's effect option, in place of the side: through black, or smoothly." []
  `(node#b-fade-option :use rbBig
     :when ~(= computed.slide.transition.kind "fade")
     :with {:icon ~(if (= computed.slide.transition.option "smoothly") "blend" "square")
            :title ~(if (= computed.slide.transition.option "smoothly") (t "motion.smoothly") (t "motion.throughBlack"))
            :width 56}
     :tooltip ~(t "motion.fadeThroughBlackOr")
     :onClick toggle-fade-option))

(defview motion-build "Build `a` in the animation pane, with its delay and repeats below it while it is selected." [a n]
  `(column :id ~(str "motion-build-" a.id) :fitContent height :alignItems stretch :color transparent
     ~(motion-row a n)
     ~(motion-timing a)))

(defview motion-timing "The delay and the repeats of build `a`, in the animation pane." [a]
  `(row :id ~(str "motion-" a.id "-timing")
     :when ~(contains state.board.selection a.target)
     :height 24
     :gap 2
     :alignItems center
     :color "@rbChecked"
     (label :id ~(str "motion-" a.id "-delay")
       :width 96
       :height 22
       :padding 4
       :textColor @rbInk
       :runs ~[{:icon "hourglass" :size 12} {:text (t "motion.delay" {:seconds (fixed (/ (or-else a.delay 0) 1000) 2)})
                                              :size 11}])
     (node :id ~(str "motion-" a.id "-delay-less")
       :use rbSmall
       :with {:icon minus :enabled ~(pos? (or-else a.delay 0))}
       :tooltip ~(t "motion.lessDelay")
       :onClick ~(handler build-delay-by a.id -250))
     (node :id ~(str "motion-" a.id "-delay-more")
       :use rbSmall
       :with {:icon plus}
       :tooltip ~(t "motion.moreDelay")
       :onClick ~(handler build-delay-by a.id 250))
     (label :id ~(str "motion-" a.id "-repeat")
       :width 50
       :height 22
       :padding 4
       :textColor @rbInk
       :runs ~[{:icon "repeat" :size 12} {:text (str " " (or-else a.repeat 1)) :size 11}])
     (node :id ~(str "motion-" a.id "-repeat-less")
       :use rbSmall
       :with {:icon minus :enabled ~(> (or-else a.repeat 1) 1)}
       :tooltip ~(t "motion.fewerRepeats")
       :onClick ~(handler build-repeat-by a.id -1))
     (node :id ~(str "motion-" a.id "-repeat-more")
       :use rbSmall
       :with {:icon plus :enabled ~(< (or-else a.repeat 1) 10)}
       :tooltip ~(t "motion.moreRepeats")
       :onClick ~(handler build-repeat-by a.id 1))))

(defview motion-timing-more "The Motion tab's delay and repeats of the selected objects' build." []
  `(column#motion-timing-more :width 150 :height 72 :gap 2 :alignItems stretch :color transparent
     (row#motion-delay-row :height 22 :gap 2 :alignItems center :color transparent
       (label#motion-delay :width 102
         :height 22
         :textColor @rbInk
         :runs ~[{:icon "hourglass" :size 14}
                 {:text (t "motion.delay" {:seconds (fixed (/ (or-else a.delay 0) 1000) 2)}) :size 12}])
       (node#motion-delay-less :use rbSmall
         :with {:icon minus :enabled ~(and (some? a) (pos? (or-else a.delay 0)))}
         :tooltip ~(t "motion.lessDelay")
         :onClick ~(handler animation-delay-by -250))
       (node#motion-delay-more :use rbSmall
         :with {:icon plus :enabled ~(some? a)}
         :tooltip ~(t "motion.moreDelay")
         :onClick ~(handler animation-delay-by 250)))
     (row#motion-repeat-row :height 22 :gap 2 :alignItems center :color transparent
       (label#motion-repeat :width 102
         :height 22
         :textColor @rbInk
         :runs ~[{:icon "repeat" :size 14} {:text (t "motion.repeat" {:n (or-else a.repeat 1)}) :size 12}])
       (node#motion-repeat-less :use rbSmall
         :with {:icon minus :enabled ~(and (some? a) (> (or-else a.repeat 1) 1))}
         :tooltip ~(t "motion.fewerRepeats")
         :onClick ~(handler animation-repeat-by -1))
       (node#motion-repeat-more :use rbSmall
         :with {:icon plus :enabled ~(and (some? a) (< (or-else a.repeat 1) 10))}
         :tooltip ~(t "motion.moreRepeats")
         :onClick ~(handler animation-repeat-by 1)))
     (label#motion-side :height 22
       :style ~(if (= a.effect "fly") "rbButton" "rbButtonOff")
       :padding 2
       :fitText true
       :runs ~[{:icon (second (or-else (find #(= (first %) (or-else a.direction "bottom")) computed.flySides)
                                        computed.flySides.0))
                :size 14}
               {:text (t "motion.flySide" {:side (or-else a.direction "bottom")}) :size 12}]
       :tooltip ~(t "motion.theSideItFlies")
       :disabled ~(not= a.effect "fly")
       :onClick next-fly-side)))

;;; What the buttons do

(defn emphasis? "Whether an effect is an emphasis." [effect]
  (contains? #{"pulse" "spin" "teeter"} effect))

(defn path? "Whether an effect is a motion path." [effect]
  (contains? #{"pathRight" "pathDown" "pathArc" "pathCircle"} effect))

(defn choose-effect "Gives the selected objects a build of an effect: an emphasis, or an entrance or exit as it was."
  [effect]
  (let [a computed.animation
        kind (cond (emphasis? effect) "emphasis"
                   (path? effect) "path"
                   (= (:kind a) "exit") "exit"
                   :else "entrance")
        start (or-else (:start a) "click")
        ms (or-else (:ms a) (if (#{"emphasis" "path"} kind) 1000 500))]
    (set-animation effect kind start ms)))

(defn animation-delay-by "Waits longer or shorter before the selected objects' build, by milliseconds." [by]
  (change-animation {:delay (clamp (+ (or-else computed.animation.delay 0) by) 0 59000)}))

(defn animation-repeat-by "Plays the selected objects' build more or fewer times, from 1 to 10." [by]
  (change-animation {:repeat (clamp (+ (or-else computed.animation.repeat 1) by) 1 10)}))

(defn next-fly-side "Turns the side the selected objects fly from, or to, to the next: bottom, left, top, right." []
  (let [sides (map first computed.flySides)
        at (indexOf sides (or-else computed.animation.direction "bottom"))]
    (change-animation {:direction (get sides (mod (inc at) 4))})))

(defn change-build "Merges properties into one build of the slide, by its id." [id props]
  (commit)
  (set "state.deck.slides[state.current].animations"
       (map #(if (= (:id %) id) (merge % props) %) computed.slide.animations)))

(defn build-delay-by "Waits longer or shorter before a build, by its id." [id by]
  (let [a (find #(= (:id %) id) computed.slide.animations)
        delay (+ by (or-else (:delay a) 0))]
    (change-build id {:delay (clamp delay 0 59000)})))

(defn build-repeat-by "Plays a build more or fewer times, by its id, from 1 to 10." [id by]
  (let [a (find #(= (:id %) id) computed.slide.animations)
        times (+ by (or-else (:repeat a) 1))]
    (change-build id {:repeat (clamp times 1 10)})))

(defn toggle-fade-option "Makes the slide's fade go through black, or smoothly." []
  (set-transition {:option (if (= computed.slide.transition.option "smoothly") "black" "smoothly")}))

;;; The presenter's pen and laser

(defn pen-tool "Picks the presenter's pen or laser pointer; the same again puts it down." [tool]
  (set "state.showPen" (merge state.showPen {:tool (if (= state.showPen.tool tool) nil tool) :stroke [] :laser nil})))

(defn on-slide "A point of the presenter's picture of the slide, drawn to fit, as a point of the 960 by 540 slide."
  [left top width height]
  (let [fit (max (min (/ width 960) (/ height 540)) 0.01)
        left-edge (/ (- width (* 960 fit)) 2)
        top-edge (/ (- height (* 540 fit)) 2)]
    [(clamp (/ (- left left-edge) fit) 0 960) (clamp (/ (- top top-edge) fit) 0 540)]))

(defn pen-press "Starts a pen stroke, or shows the laser, where the presenter presses." [left top width height]
  (let [point (on-slide left top width height)]
    (set "state.showPen.from" [left top])
    (if (= state.showPen.tool "pen")
      (set "state.showPen.stroke" [point])
      (set "state.showPen.laser" point))))

(defn pen-drag "Goes on with the stroke, or moves the laser, as the presenter drags." [dx dy width height]
  (let [[left top] state.showPen.from
        point (on-slide (+ left dx) (+ top dy) width height)]
    (if (= state.showPen.tool "pen")
      (when (< (count state.showPen.stroke) 2000) (push "state.showPen.stroke" point))
      (set "state.showPen.laser" point))))

(defn pen-release "Keeps the stroke on the slide shown, or hides the laser." []
  (when (> (count state.showPen.stroke) 1)
    (set "state.showInk"
         (assoc state.showInk computed.showSlide.id
                (conj (or-else (get state.showInk computed.showSlide.id) []) state.showPen.stroke))))
  (set "state.showPen" (merge state.showPen {:stroke [] :laser nil})))

(defn erase-show-ink "Erases the presenter's pen on the slide shown." []
  (set "state.showInk" (dissoc state.showInk computed.showSlide.id)))

;;; Keys

(defkey "mod+p" {:when ~state.show.on :do pen-tool :with {:tool "pen"}})

(defkey "l" {:when ~state.show.on :do pen-tool :with {:tool "laser"}})

(defkey "e" {:when ~state.show.on :do erase-show-ink})
