;;; edit.lisp --- editing the deck: the selection, text, layouts, transitions and builds
;;;
;;; Every change first keeps the deck as it was (commit, in sinopia.lisp), so Undo and Redo step through up to 50 of
;;; them. Objects are changed on the slide shown, the selected ones (the canvas's selection, which holds object ids);
;;; new slides take their layout's placeholders, empty, and new ids from deck.next.

;;; What editing keeps

(defstate undo [] "The decks Undo goes back to, the last one latest.")

(defstate redo [] "The decks Undo left, which Redo brings back.")

(defstate clip [] "The objects Copy or Cut took, which Paste puts back.")

(defstate editing nil "The id of the object whose text is being edited on the slide, or nil.")

;;; The slide shown and its selection

(defcomputed slide "The slide shown." (or-else (get state.deck.slides state.current) state.deck.slides.0))

(defcomputed objects "The objects of the slide shown, back to front." (or-else computed.slide.objects []))

(defcomputed selected "The selected objects, back to front."
  (filter (fn [it] (contains state.board.selection it.id)) computed.objects))

(defcomputed selectedAt "The places of the selected objects in the slide's drawing order."
  (filter (fn [it] (contains state.board.selection (get-in computed.objects [it :id])))
          (range (count computed.objects))))

(defcomputed first "The first selected object, or nil." (or-else computed.selected.0 nil))

(defcomputed firstPlaceholder "The layout's placeholder the first selected object fills, or an empty map."
  (or-else (find (fn [it] (= it.ph computed.first.ph)) computed.layout.ph) {}))

(defcomputed locked "Whether every selected object is locked, and there is one."
  (and (> (count computed.selected) 0) (every? (fn [it] (or-else it.locked false)) computed.selected)))

(defcomputed animation "The build of the first selected object that has one, or nil."
  (find (fn [it] (contains state.board.selection it.target)) (or-else computed.slide.animations [])))

(defcomputed canUndo "Whether there is a change to undo." (> (count state.undo) 0))

(defcomputed canRedo "Whether there is an undone change to redo." (> (count state.redo) 0))

;;; Where the selection is

(defcomputed phNames "The placeholder names of the slide's layout." (map (fn [it] it.ph) (or-else computed.layout.ph [])))

(defcomputed boxes "Each selected object's box: its own place, else its placeholder's, else 100 by 100 at the corner."
  (map (fn [it]
         (merge {:x 0 :y 0 :w 100 :h 100}
                (if (contains computed.phNames it.ph) (get computed.layout.ph (indexOf computed.phNames it.ph)) {})
                it))
       computed.selected))

(defcomputed slideBounds "The page, in points." {:left 0 :top 0 :right 960 :bottom 540})

(defcomputed selectionBounds "The box around the selected objects, or the page when nothing is selected."
  (if (= (count computed.boxes) 0)
    computed.slideBounds
    {:left (min (map (fn [it] it.x) computed.boxes))
     :top (min (map (fn [it] it.y) computed.boxes))
     :right (max (map (fn [it] (+ it.x it.w)) computed.boxes))
     :bottom (max (map (fn [it] (+ it.y it.h)) computed.boxes))}))

;;; Text

(defcomputed textSize "The first selected object's text size: its own, else its placeholder's, else 18."
  (if (nil? computed.first) 18 (or-else computed.first.size computed.firstPlaceholder.size 18)))

(defcomputed sizes "The font sizes the size box and Grow and Shrink step through."
  [8 9 10 10.5 11 12 14 16 18 20 24 28 32 36 40 44 48 54 60 66 72 80 88 96])

(defcomputed allOn "Each text style, whether every selected object has it: the font group's toggles light up."
  {:bold (every? (fn [it] (or-else it.bold false)) computed.selected)
   :italic (every? (fn [it] (or-else it.italic false)) computed.selected)
   :underline (every? (fn [it] (or-else it.underline false)) computed.selected)
   :strike (every? (fn [it] (or-else it.strike false)) computed.selected)
   :shadow (every? (fn [it] (or-else it.shadow false)) computed.selected)
   :bullets (every? (fn [it] (or-else it.bullets false)) computed.selected)})

;;; The Arrange and Shape Effects menus

(defcomputed arrangeItems
  "The Arrange menu: the drawing order, the alignment, then the spacing; each needs a selection, spacing two objects."
  (concat (map (fn [it]
                 {:title (t (str "arrange." it.0)) :icon it.1 :run "order" :with {:how it.0}
                  :enabled (> (count computed.selected) 0)})
               [["front" "bring-to-front"] ["forward" "arrow-up"] ["backward" "arrow-down"] ["back" "send-to-back"]])
          [{:separator true}]
          (map (fn [it]
                 {:title (t (str "align." it.0)) :icon it.1 :run "align" :with {:how it.0}
                  :enabled (> (count computed.selected) 0)})
               [["left" "align-start-vertical"]
                ["center" "align-center-vertical"]
                ["right" "align-end-vertical"]
                ["top" "align-start-horizontal"]
                ["middle" "align-center-horizontal"]
                ["bottom" "align-end-horizontal"]])
          [{:separator true}]
          (map (fn [it]
                 {:title (t (str "distribute." it.0)) :icon it.1 :run "distribute" :with {:axis it.0}
                  :enabled (> (count computed.selected) 1)})
               [["x" "align-horizontal-distribute-center"] ["y" "align-vertical-distribute-center"]])))

(defcomputed effectItems "The Shape Effects menu: none, a shadow or a glow on the selected objects."
  (map (fn [it]
         {:title (t (str "effect." (or-else it "none"))) :run "setObjects" :with {:props {:effect it}}
          :enabled (> (count computed.selected) 0)})
       [nil "shadow" "glow"]))

;;; Editing text on the slide

(defn editText "Edits a text box's or a shape's text in an editor over it, until Escape or a click elsewhere."
  [id]
  (when (contains ["text" "shape"] (or-else (:kind (find (fn [it] (= it.id id)) computed.objects)) ""))
    (commit)
    (set "state.board.selection" [id])
    (set "state.editing" id)))

(defn stopEditing "Leaves the text editor, and gives the canvas the keys again." []
  (set "state.editing" nil)
  (focus "#board"))

(defn setNotes "Keeps the speaker notes the notes box holds, when they changed." [text]
  (when (not= text (or-else computed.slide.notes ""))
    (set "state.deck.slides[state.current].notes" text)))

;;; The slide shown

(defn hideSlide "Leaves the slide out of the show, or puts it back." []
  (commit)
  (set "state.deck.slides[state.current].hidden" (not (or-else computed.slide.hidden false))))

(defn layoutHolders "A layout's placeholders." [layout]
  (:ph (find (fn [it] (= it.id layout)) computed.layouts)))

(defn setLayout "Gives the slide another layout: the placeholders it lacks are dropped, its new ones added, empty."
  [layout]
  (commit)
  (let [holders (layoutHolders layout)
        wanted (map (fn [it] it.ph) holders)
        had (map (fn [it] it.ph) computed.objects)]
    (set "state.deck.slides[state.current]"
         (merge computed.slide
                {:layout layout
                 :objects (concat (filter (fn [it] (or (nil? it.ph) (contains wanted it.ph))) computed.objects)
                                  (map-indexed (fn [index it]
                                                 {:id (str "o" (+ state.deck.next index)) :kind "text" :ph it.ph :text ""})
                                               (filter (fn [it] (not (contains had it.ph))) holders)))})))
  (increment "state.deck.next" :by 10))

(defn resetSlide "Puts the slide's placeholders back where their layout has them, keeping their text." []
  (commit)
  (set-objects (map (fn [it] (if (nil? it.ph) it {:id it.id :kind it.kind :ph it.ph :text it.text})) computed.objects)))

(defn addSection "Starts an untitled section at the slide shown, in place of one that started there." []
  (commit)
  (set "state.deck.sections"
       (sort-by :first (concat (filter (fn [it] (not= it.first state.current)) (or-else state.deck.sections []))
                               [{:id (str "x" state.deck.next) :name (t "section.untitled") :first state.current}])))
  (increment "state.deck.next" :by 1))

;;; The selected objects

(defn setObjects "Merges properties into every selected object." [props]
  (when (> (count computed.selected) 0)
    (commit)
    (set-objects (map (fn [it] (if (selected? it) (merge it props) it)) computed.objects))))

(defn toggleText "Turns a text style (bold, italic, underline...) on for the selection, or off if all have it." [key]
  (setObjects (assoc {} key (not (get computed.allOn key)))))

(defn growText "Steps the selection's text size to the next size up (by 1) or down (by -1) in the list." [by]
  (let [size computed.textSize]
    (setObjects {:size (if (> by 0)
                         (or-else (find (fn [it] (> it size)) computed.sizes) (+ size 8))
                         (or-else (find (fn [it] (< it size)) (reverse computed.sizes)) (max (- size 1) 1)))})))

(defn clearFormatting "Takes every text style off the selection: its theme's look comes back." []
  (setObjects {:bold nil :italic nil :underline nil :strike nil :shadow nil :size nil :color nil :highlight nil :font nil}))

(defn toggleLock "Locks the selected objects, or unlocks them if all are." []
  (setObjects {:locked (not (every? (fn [it] (or-else it.locked false)) computed.selected))}))

(defn placeObject "Puts an object, by its place k on the slide, where the canvas moved it to, unless it is locked."
  [k m]
  (when (and (>= k 0) (not (or-else (get-in computed.objects [k :locked]) false)))
    (set (str "state.deck.slides[state.current].objects[" k "]")
         (merge (get computed.objects k)
                {:x (tenth m.x) :y (tenth m.y) :w (tenth m.width) :h (tenth m.height) :rotate (tenth (or-else m.rotate 0))}))))

(defn placeObjects "Keeps where the canvas moved, sized or turned the objects, as one change." [moved]
  (commit)
  (let [ids (map (fn [it] it.id) computed.objects)]
    (each [m moved]
      (placeObject (indexOf ids m.id) m))))

(defn deleteObjects "Deletes the selected objects but the locked ones, and their builds." []
  (when (> (count computed.selected) 0)
    (commit)
    (set-objects (filter (fn [it] (or (or-else it.locked false) (not (selected? it)))) computed.objects))
    (let [left (map (fn [it] it.id) (get-in state.deck.slides [state.current :objects]))]
      (set "state.deck.slides[state.current].animations"
           (filter (fn [it] (contains left it.target)) (or-else computed.slide.animations []))))
    (set "state.board.selection" (map (fn [it] it.id) (filter (fn [it] (or-else it.locked false)) computed.selected)))))

(defn copyObjects "Copies the selected objects." []
  (when (> (count computed.selected) 0)
    (set "state.clip" computed.selected)))

(defn cutObjects "Copies the selected objects, then deletes them." []
  (copyObjects)
  (deleteObjects))

(defn pasteObjects "Pastes the copied objects onto the slide, a little down and right, with new ids, and selects them."
  []
  (when (> (count state.clip) 0)
    (commit)
    (let [next state.deck.next]
      (set-objects (concat computed.objects
                           (map-indexed (fn [index it]
                                          (merge it {:id (str "o" (+ next index))
                                                     :x (+ (or-else it.x 0) 18)
                                                     :y (+ (or-else it.y 0) 18)
                                                     :ph nil}))
                                        state.clip)))
      (set "state.board.selection" (map-indexed (fn [index it] (str "o" (+ next index))) state.clip)))
    (increment "state.deck.next" :by (count state.clip))))

(defn duplicateObjects "Pastes a copy of the selected objects." []
  (copyObjects)
  (pasteObjects))

(defn selectAll "Selects every object of the slide." []
  (set "state.board.selection" (map (fn [it] it.id) computed.objects)))

;;; New objects

(defn insertObject "Adds an object to the slide, with a new id, and selects it." [object]
  (commit)
  (let [id (str "o" state.deck.next)]
    (push "state.deck.slides[state.current].objects" (merge object {:id id}))
    (set "state.board.selection" [id]))
  (increment "state.deck.next" :by 1))

(defn insertShape "Adds a shape of the gallery in the middle of the slide: a line, or a filled shape." [shape]
  (insertObject (if (get-in computed.shapes [shape :line])
                  {:kind "line" :shape shape :x 380 :y 270 :w 200 :h 0 :line "accent1" :lineWidth 1}
                  {:kind "shape" :shape shape :x 405 :y 195 :w 150 :h 150 :fill "accent1" :line "accent1.4"
                   :lineWidth 1 :text ""})))

(defn insertTextBox "Adds a text box in the middle of the slide." []
  (insertObject {:kind "text" :x 330 :y 240 :w 300 :h 40 :text (t "object.newText") :size 18}))

;;; The deck's design

(defn setTheme "Gives the deck a theme, with its own colours and its first variant." [theme]
  (commit)
  (set "state.deck.theme" theme)
  (set "state.deck.variant" 0)
  (set "state.deck.colors" nil))

(defn setVariant "Gives the deck one of its theme's variants, with its own colours." [variant]
  (commit)
  (set "state.deck.variant" variant)
  (set "state.deck.colors" nil))

(defn setColorSet "Gives the deck another theme's colours (nil: its own), until a theme or a variant is chosen." [id]
  (commit)
  (set "state.deck.colors" id))

(defn setFonts "Sets the fonts of titles and of the other text (a text's own font wins)." [heading body]
  (commit)
  (set "state.deck.fonts" {:heading heading :body body}))

;;; Transitions and builds

(def no-transition "A slide's transition when it has none." {:kind "none" :ms 500})

(defn setTransition "Merges properties into the slide's transition." [props]
  (commit)
  (set "state.deck.slides[state.current].transition" (merge (or-else computed.slide.transition no-transition) props)))

(defn durationBy "Makes the slide's transition longer or shorter, by milliseconds, between 0 and 59 seconds." [by]
  (setTransition {:ms (clamp (+ (or-else computed.slide.transition.ms 500) by) 0 59000)}))

(defn transitionToAll "Gives every slide this slide's transition." []
  (commit)
  (let [transition (or-else computed.slide.transition no-transition)]
    (set "state.deck.slides" (map (fn [it] (merge it {:transition transition})) state.deck.slides))))

(defn setAnimation "Gives the selected objects a build (none: no build); the first starts as asked, the others with it."
  [effect kind start ms]
  (when (> (count computed.selected) 0)
    (commit)
    (let [next state.deck.next]
      (set "state.deck.slides[state.current].animations"
           (concat (filter (fn [it] (not (contains state.board.selection it.target)))
                           (or-else computed.slide.animations []))
                   (if (= effect "none")
                     []
                     (map-indexed (fn [index it]
                                    {:id (str "a" (+ next index)) :target it.id :effect effect :kind kind
                                     :start (if (= index 0) start "with") :ms ms :delay 0})
                                  computed.selected)))))
    (increment "state.deck.next" :by (count computed.selected))))

(defn changeAnimation "Merges properties into the builds of the selected objects." [props]
  (when (some? computed.animation)
    (commit)
    (set "state.deck.slides[state.current].animations"
         (map (fn [it] (if (contains state.board.selection it.target) (merge it props) it)) computed.slide.animations))))
