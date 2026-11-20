;;; slide.lisp --- A slide drawn from the deck, on a 960 by 540 page: its background and the theme's decorations
;;; (slideBackground), then its objects (slideObject)
;;;
;;; The canvas holds them as its own children, so the objects are what it selects and moves; the thumbnails, the slide
;;; show and the presenter view draw the whole page (slidePage). An object in a placeholder takes the layout's frame
;;; and text style until it has its own. Text is drawn paragraph by paragraph, in the frame inset 7.2 by 3.6 points
;;; (and by the shape's own text inset), anchored at the top, middle or bottom; a body's levels drop 4 points each and
;;; indent 36. In edit mode an empty placeholder shows its prompt and a dashed outline.

(ns sinopia.parts.slide)

;;; Definitions

(defcomponent slideBackground "A slide's background: its colour, and its theme's decorations."
  {:slide nil :prefix ""}
  `(node :id ~(str prefix "bg")
     :x 0
     :y 0
     :width 960
     :height 540
     :color ~(or-else (get computed.palette (or-else slide.background computed.theme.background)) slide.background)
     ~(repeat [d (if (contains? #{:title :section} slide.layout)
                   computed.theme.titleDecor
                   computed.theme.decor)
               :key d.id]
       `(node :id ~(str prefix "decor-" d.id)
          :color transparent
          :x ~d.x
          :y ~d.y
          :width ~d.w
          :height ~d.h
          :overflow visible
          (node :use shapePath
            :with {:o ~(merge {:line "none" :flipH false :flipV false :gradient nil :lineWidth 0 :dash nil
                               :effect nil} d)
                   :name ~(str prefix "decor-" d.id)})))
     (label :id ~(str prefix "footer")
       :when ~(and (some? state.deck.footer) (not= slide.layout :title))
       :x 40
       :y 506
       :width 640
       :height 22
       :textColor ~(or-else computed.palette.dark2 computed.palette.dark1)
       :runs [{:text ~(or-else state.deck.footer "") :size 12}])
     (label :id ~(str prefix "number")
       :when ~(and (or-else state.deck.slideNumbers false) (not= slide.layout :title))
       :x 820
       :y 506
       :width 100
       :height 22
       :align {:horizontal end :vertical center}
       :textColor ~(or-else computed.palette.dark2 computed.palette.dark1)
       :runs [{:text ~(-> (map :id state.deck.slides) (indexOf slide.id) inc text) :size 12}])))

(defcomponent slideObject "An object of a slide: a shape, a line, an icon, ink, a table, a picture or text, at its place."
  {:o nil :prefix "" :layout nil :edit false}
  `(node :id ~(str prefix o.id)
     :let ~(object-measures)
     :color transparent
     :x ~box.x
     :y ~box.y
     :width ~box.w
     :height ~box.h
     :rotate ~(or-else o.rotate 0)
     :overflow visible
     :visible ~(not (or-else o.hidden false))
     ~(object-shape)
     ~(object-icon)
     ~(object-ink)
     (node :use slideTable :when ~(or (= o.kind :table) (and (= o.kind :chart) editing)) :with {:o ~o :prefix ~prefix :w ~box.w :h ~box.h :edit ~edit})
     (node :use slidePicture :when ~(= o.kind :picture) :with {:o ~o :prefix ~prefix :w ~box.w :h ~box.h})
     (node :use slideChart :when ~(and (= o.kind :chart) (not editing)) :with {:o ~o :prefix ~prefix :w ~box.w :h ~box.h})
     ~(placeholder-outline)
     ~(text-editor o)
     ~(object-text o prefix)))

;; The text's height is measured as the slide's text is laid out: half the size a letter, 1.2 the size a line, so a
;; text shrunk by a factor k takes about k squared of its height
(defn text-fit
  "The size a shrinking text box gives its text, and the height a resizing one takes: its text, size and spacing, its
  drawn box, its shape's text inset and its margins."
  [text pt spacing drawn inset mx my]
  (let [across (- 1 inset.0 inset.2)
        down (- 1 inset.1 inset.3)
        width (max (- (* drawn.w across) (* 2 mx)) 20)
        room (max (- (* drawn.h down) (* 2 my)) 1)
        line-height (* 1.2 pt (or-else spacing 1))
        lengths (map #(max (count (str/trim %)) 1) (str/split (or-else text "") "\n"))
        need (reduce + 0 (map #(* line-height
                             (ceil (/ (* % 0.5 pt) width)))
                         lengths))
        fraction (sqrt (/ room (max need 1)))]
    {:size (if (> need room)
             (max 6 (floor (* pt fraction)))
             pt)
     :h (/ (+ need (* 2 my)) (max down 0.1))}))

(defn fit-size "The size a shrinking text box gives its text (text-fit's arguments)." [text pt spacing drawn inset mx my]
  (:size (text-fit text pt spacing drawn inset mx my)))

(defn fit-height "The height a box resized to its text takes (text-fit's arguments)." [text pt spacing drawn inset mx my]
  (:h (text-fit text pt spacing drawn inset mx my)))

(defview object-measures "What object `o` is drawn from: its placeholder `p`, its box, its text and colour."
  []
  `{:p ~(or-else (find #(= (:ph %) o.ph) layout.ph) {})
    :drawn ~{:x (or-else o.x p.x 0) :y (or-else o.y p.y 0) :w (or-else o.w p.w 100) :h (or-else o.h p.h 100)}
    :mx ~(or-else o.marginX 7.2)
    :my ~(or-else o.marginY 3.6)
    :inset ~(if (= o.kind :shape)
                (or-else (get-in computed.shapes [(or-else o.shape "rect") :text]) [0 0 0 0])
                [0 0 0 0])
    :box ~(if (= o.autofit "resize")
              (assoc drawn :h (fit-height o.text (or-else o.size p.size 18) o.spacing drawn inset mx my))
              drawn)
    :shape ~(or (contains? #{:shape :line} o.kind)
                (not= (or-else o.fill :none) :none)
                (not= (or-else o.line :none) :none))
    :size ~(if (= o.autofit "shrink")
               (fit-size o.text (or-else o.size p.size 18) o.spacing drawn inset mx my)
               (or-else o.size p.size 18))
    :empty ~(= (str/trim (or-else o.text "")) "")
    :editing ~(and edit (= state.editing o.id))
    :prompt ~(and edit empty (some? o.ph) (not editing))
    :outline ~(str prefix o.id "-ph")
    :lines ~(if prompt [(or-else (get computed.prompts o.ph) "")] (str/split (or-else o.text "") "\n"))
    :bullets ~(or-else o.bullets p.bullets false)
    :anchor ~(or-else o.anchor p.anchor (if (= o.kind :shape) "middle" "top"))
    :align ~(or-else o.align p.align (if (= o.kind :shape) "center" "left"))
    :ink ~(or-else (get computed.palette (or-else o.color p.color (if (= o.kind :shape) "light1" "dark1")))
                   o.color)
    :frame ~{:x (+ (* box.w inset.0) mx)
             :y (+ (* box.h inset.1) my)
             :w (max (- (* box.w (- (- 1 inset.0) inset.2)) (* 2 mx)) 1)
             :h (max (- (* box.h (- (- 1 inset.1) inset.3)) (* 2 my)) 1)}})

(defview object-shape "Object `o`'s shape or line, or the fill and line a text box has." []
  `(node :use shapePath
     :when ~shape
     :with {:o ~(merge {:shape "rect"
                        :fill (if (= o.kind :shape) "accent1" "none")
                        :line (if (= o.kind :text) "none" nil)
                        :lineWidth 1
                        :flipH false
                        :flipV false
                        :gradient nil
                        :dash nil
                        :effect nil}
                       (merge o box))
            :name ~(str prefix o.id)}))

(defview object-icon "Object `o` if it is an icon." []
  `(label :id ~(str prefix o.id "-icon")
     :when ~(= o.kind :icon)
     :x 0
     :y 0
     :width ~box.w
     :height ~box.h
     :align center
     :textColor ~ink
     :runs [{:icon ~(or-else o.icon "star") :size ~(* (min box.w box.h) 0.9)}]))

(defview object-ink "Object `o` if it is ink: its stroke, a path in its own box." []
  `(path :id ~(str prefix o.id "-ink")
     :when ~(= o.kind :ink)
     :x 0
     :y 0
     :width ~box.w
     :height ~box.h
     :d ~(if edit (replayed-path o) (or-else o.d ""))
     :viewBox ~(str "0 0 " (or-else o.vw box.w) " " (or-else o.vh box.h))
     :fit fill
     :stroke ~(or-else o.color "#1f2933")
     :strokeWidth ~(or-else o.width 3)
     :strokeCap round
     :strokeJoin round))

(defview placeholder-outline "The dashed outline of an empty placeholder, while the slide is edited." []
  `(node :use shapePath
     :when ~prompt
     :with {:o ~(merge box
                       {:shape "rect"
                        :fill "none"
                        :line "#8c8c8c"
                        :lineWidth 0.75
                        :flipH false
                        :flipV false
                        :gradient nil
                        :dash "dash"
                        :effect nil})
            :name ~outline}))

(defcomponent slidePage "A whole slide: its background and its objects, as the show and the pictures draw it."
  {:slide nil :prefix ""}
  `(node :id ~(str prefix "page") :color transparent :x 0 :y 0 :width 960 :height 540
     (node :use slideBackground :with {:slide ~slide :prefix ~prefix})
     ~(repeat [o slide.objects :key o.id]
       `(node :use slideObject
          :with {:o ~o
                 :prefix ~prefix
                 :layout ~(or-else (find #(= (:id %) slide.layout) computed.layouts) computed.layouts.1)
                 :edit false}))))

;;; Views

(defview text-editor
  "The editor over an object whose text is edited, its first line where the text's is."
  [o]
  `(editor :id ~(str "edit-" o.id)
     :when ~(and editing (not (contains? #{:table :chart} o.kind)))
     :autofocus true
     :wrap true
     :caret "state.caret"
     :bind ~(str "state.deck.slides["
                 state.current
                 "].objects["
                 (indexOf (map :id computed.objects) o.id)
                 "].text")
     :note "Its gutter (12 px) and top padding (6 px), at its 48 px font scaled, are taken off; the anchor places it"
     :let {:k ~(/ size 48)
           :block ~(* (count (str/split (or-else o.text "") "\n")) size 1.2)
           :top ~(- (- (+ frame.y
                          (max (case anchor
                                 :middle (/ (- frame.h block) 2)
                                 :bottom (- frame.h block)
                                 0)
                               0))
                       (* 6 k))
                    (* size 0.14))}
     :x ~(- frame.x (* 12 k))
     :y ~top
     :width ~(/ (+ frame.w (* 24 k)) k)
     :height ~(/ (max (- (+ frame.y frame.h) top) (* size 1.7)) k)
     :scale ~(/ size 48)
     :font slideText
     :transformOrigin topLeft
     :color transparent
     :lineColor transparent
     :textColor ~ink
     :caretColor ~ink
     :selectionColor "#2a9d8f55"
     :label ~(t "slide.slideText")))

(defview paragraph-mark "Paragraph `n`'s bullet or number." [o prefix n]
  `(label :id ~(str prefix o.id "-p" n "-b")
     :when ~(and bullets (not prompt))
     :width ~mark
     :height ~(ceil (* pt 1.2))
     :runs ~[(cond (some? numbering)
                   {:text (+ (case numbering
                               "A." (let [at (mod (dec ordinal) 26)] (slice "ABCDEFGHIJKLMNOPQRSTUVWXYZ" at (inc at)))
                               "a)" (let [at (mod (dec ordinal) 26)] (slice "abcdefghijklmnopqrstuvwxyz" at (inc at)))
                               "I." (or-else (get ["I" "II" "III" "IV" "V" "VI" "VII" "VIII" "IX" "X" "XI" "XII" "XIII"
                                                   "XIV" "XV" "XVI" "XVII" "XVIII" "XIX" "XX"]
                                                  (dec ordinal))
                                             (text ordinal))
                               (text ordinal))
                             (if (contains? #{"1)" "a)"} numbering) ")" "."))
                    :size pt
                    :color ink
                    :font (or-else o.font "default")}
                   (some? o.bulletIcon) {:icon o.bulletIcon :size (* pt 0.75) :color ink}
                   :else {:text "•" :size pt :color ink})]))

(defview paragraph-text "Paragraph `n`'s text, wrapped to the box less its mark." [o prefix n]
  `(label :id ~(str prefix o.id "-p" n "-t")
     :wrap ~(or-else o.wrap true)
     :fitText {:maxWidth ~(if (= o.wrap false) 4000 (max (- (- (/ (- (if turned frame.h frame.w) (* size (dec cols))) cols) lead)
                                  (if (and bullets (not prompt)) mark 0))
                               1))}
     :align {:horizontal ~(case pa :center "center" :right "end" "start")}
     :runs [{:text ~(if (= (str/trim line) "") " " (str/trim line))
             :size ~pt
             :bold ~(or-else o.bold p.bold false)
             :font ~(or-else o.font
                             (if (contains? #{:title :ctrTitle} o.ph)
                                 computed.headingFont
                                 computed.bodyFont))
             :italic ~(or-else o.italic false)
             :color ~(if prompt (get computed.palette "dark1.2") ink)
             :underline ~(and (not prompt) (or (or-else o.underline false) (some? o.link)))
             :strike ~(and (not prompt) (or-else o.strike false))
             :background ~(if prompt nil (or-else o.highlight nil))
             :shadow ~(if (and (not prompt) (or-else o.shadow false)) "#00000059" nil)}]))

(defview paragraph "Paragraph `n` of object `o`: its mark and its text, at its list level." [o prefix n]
  `(row :id ~(str prefix o.id "-p" n)
     :fitContent true
     :overflow visible
     :alignSelf ~(case pa :center "center" :right "end" "start")
     :let {:pa ~(if prompt align (or-else (get o.aligns n) align))
           :line ~(get lines n)
           :level ~(get levels n)
           :pt ~(max (if bullets (- size (* 4 (min (get levels n) 2))) size) 6)
           :lead ~(if bullets (* 36 (get levels n)) 0)
           :mark ~(if (some? numbering) (max (* size 1.5) 18) 18)
           :ordinal ~(count (filter #(= % (get levels n))
                                    (slice levels
                                           (->> (range n)
                                                (filter #(< (get levels %) (get levels n)))
                                                (concat [-1])
                                                max
                                                inc)
                                           (inc n))))}
     (node :id ~(str prefix o.id "-p" n "-in") :color transparent :width ~lead :height 1 :when ~(pos? lead))
     ~(paragraph-mark o prefix n)
     ~(paragraph-text o prefix n)))

(defview object-text "Object `o`'s text, a paragraph a line, unless it is being edited." [o prefix]
  `(row :id ~(str prefix o.id "-text")
     :when ~(and (or (not empty) prompt) (not editing))
     :overflow visible
     :gap ~size
     :let {:turned ~(not= (or-else o.textRotate 0) 0)
           :cols ~(if prompt 1 (min (or-else o.columns 1) (max (count lines) 1)))
           :numbering ~(if prompt nil (or-else o.numbering nil))
           :levels ~(map #(min (floor (/ (indexOf % (str/trim %)) 4)) 8) lines)
           :colWidth ~(/ (- (if turned frame.h frame.w) (* size (dec cols))) cols)
           :heights ~(map-indexed (fn [index it]
                                    (+ (* (ceil (/ (* (max (count (str/trim it)) 1)
                                                      0.5
                                                      (max (if bullets (- size (* 4 (min (get levels index) 2))) size)
                                                           6))
                                                   (max (- colWidth (if bullets (+ (* 36 (get levels index)) 18) 0))
                                                        20)))
                                          1.2
                                          (max (if bullets (- size (* 4 (min (get levels index) 2))) size) 6))
                                       (if bullets (* size 0.35) 0)
                                       (* size (dec (if prompt 1 (or-else o.spacing 1))))))
                                  lines)
           :column ~(map-indexed (fn [index it]
                                   (if (= cols 1)
                                       0
                                       (min (floor (/ (+ (sum (slice heights 0 index)) (* it 0.999))
                                                      (if turned frame.w frame.h)))
                                            (dec cols))))
                                 heights)}
     :x ~(if turned (+ frame.x (/ (- frame.w frame.h) 2)) frame.x)
     :y ~(if turned (+ frame.y (/ (- frame.h frame.w) 2)) frame.y)
     :width ~(if turned frame.h frame.w)
     :height ~(if turned frame.w frame.h)
     :rotate ~(or-else o.textRotate 0)
     ~(repeat [k (range cols)] (text-column o prefix k))))

(defview text-column "Column `k` of object `o`'s text: the paragraphs that fall in it, by its anchor." [o prefix k]
  `(column :id ~(str prefix o.id "-text-c" k)
     :overflow visible
     :width ~colWidth
     :height ~(if turned frame.w frame.h)
     :justify ~(case anchor :bottom "end" :middle "center" "start")
     :alignItems ~(case align :center "center" :right "end" "start")
     :gap ~(+ (if bullets (* size 0.35) 0) (* size (dec (if prompt 1 (or-else o.spacing 1)))))
     ~(repeat [n (filter #(= (get column %) k) (range (count lines)))]
       (paragraph o prefix n))))
