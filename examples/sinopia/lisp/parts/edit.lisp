;;; edit.lisp --- editing the deck: the selection, text, layouts, transitions and builds
;;;
;;; Every change first keeps the deck as it was (commit, in sinopia.lisp), so Undo and Redo step through up to 50 of
;;; them. Objects are changed on the slide shown, the selected ones (the canvas's selection, which holds object ids);
;;; new slides take their layout's placeholders, empty, and new ids from deck.next.

(ns sinopia.parts.edit)

;;; What editing keeps

(defstate undo [] "The decks Undo goes back to, the last one latest.")

(defstate redo [] "The decks Undo left, which Redo brings back.")

(defstate clip [] "The objects Copy or Cut took, which Paste puts back.")

(defstate editing nil "The id of the object whose text is being edited on the slide, or nil.")

;;; The slide shown and its selection

(defcomputed slide "The slide shown." (or-else (get state.deck.slides state.current) state.deck.slides.0))

(defcomputed objects "The objects of the slide shown, back to front." (or-else computed.slide.objects []))

(defcomputed selected "The selected objects, back to front."
  (filter #(contains state.board.selection (:id %)) computed.objects))

(defcomputed selectedAt "The places of the selected objects in the slide's drawing order."
  (filter #(contains state.board.selection (get-in computed.objects [% :id]))
          (range (count computed.objects))))

(defcomputed first "The first selected object, or nil." (or-else computed.selected.0 nil))

(defcomputed firstPlaceholder "The layout's placeholder the first selected object fills, or an empty map."
  (or-else (find #(= (:ph %) computed.first.ph) computed.layout.ph) {}))

(defcomputed locked "Whether every selected object is locked, and there is one."
  (and (pos? (count computed.selected)) (every? #(or-else (:locked %) false) computed.selected)))

(defcomputed animation "The build of the first selected object that has one, or nil."
  (find #(contains state.board.selection (:target %)) (or-else computed.slide.animations [])))

(defcomputed canUndo "Whether there is a change to undo." (pos? (count state.undo)))

(defcomputed canRedo "Whether there is an undone change to redo." (pos? (count state.redo)))

;;; Where the selection is

(defcomputed phNames "The placeholder names of the slide's layout."
  (map :ph (or-else computed.layout.ph [])))

(defcomputed boxes "Each selected object's box: its own place, else its placeholder's, else 100 by 100 at the corner."
  (map (fn [it]
         (merge {:x 0 :y 0 :w 100 :h 100}
                (if (contains computed.phNames it.ph) (get computed.layout.ph (indexOf computed.phNames it.ph)) {})
                it))
       computed.selected))

(defcomputed slideBounds "The page, in points." {:left 0 :top 0 :right 960 :bottom 540})

(defcomputed selectionBounds "The box around the selected objects, or the page when nothing is selected."
  (if (empty? computed.boxes)
      computed.slideBounds
      {:left (min (map :x computed.boxes))
       :top (min (map :y computed.boxes))
       :right (max (map #(+ (:x %) (:w %)) computed.boxes))
       :bottom (max (map #(+ (:y %) (:h %)) computed.boxes))}))

;;; Text

(defcomputed textSize "The first selected object's text size: its own, else its placeholder's, else 18."
  (if-some [object computed.first] (or-else object.size computed.firstPlaceholder.size 18) 18))

(defcomputed sizes "The font sizes the size box and Grow and Shrink step through."
  [8 9 10 10.5 11 12 14 16 18 20 24 28 32 36 40 44 48 54 60 66 72 80 88 96])

(defcomputed allOn "Each text style, whether every selected object has it: the font group's toggles light up."
  {:bold (every? #(or-else (:bold %) false) computed.selected)
   :italic (every? #(or-else (:italic %) false) computed.selected)
   :underline (every? #(or-else (:underline %) false) computed.selected)
   :strike (every? #(or-else (:strike %) false) computed.selected)
   :shadow (every? #(or-else (:shadow %) false) computed.selected)
   :bullets (every? #(or-else (:bullets %) false) computed.selected)})

;;; The Arrange and Shape Effects menus

(defcomputed arrangeItems
  "The Arrange menu: the drawing order, the alignment, then the spacing; each needs a selection, spacing two objects."
  (concat (map (fn [it]
                 {:title (t (str "arrange." it.0)) :icon it.1 :run "order" :with {:how it.0}
                  :enabled (pos? (count computed.selected))})
               [["front" "bring-to-front"] ["forward" "arrow-up"] ["backward" "arrow-down"] ["back" "send-to-back"]])
          [{:separator true}]
          (map (fn [it]
                 {:title (t (str "align." it.0)) :icon it.1 :run "align" :with {:how it.0}
                  :enabled (pos? (count computed.selected))})
               [["left" "align-start-vertical"]
                ["center" "align-center-vertical"]
                ["right" "align-end-vertical"]
                ["top" "align-start-horizontal"]
                ["middle" "align-center-horizontal"]
                ["bottom" "align-end-horizontal"]])
          [{:separator true}]
          [{:title (t "arrange.group") :icon "group" :keyLabel "Ctrl+G" :run "group-objects" :enabled computed.canGroup}
           {:title (t "arrange.ungroup") :icon "ungroup" :keyLabel "Ctrl+Shift+G" :run "ungroup-objects"
            :enabled computed.canUngroup}]
          [{:separator true}]
          (map (fn [it]
                 {:title (t (str "distribute." it.0)) :icon it.1 :run "distribute" :with {:axis it.0}
                  :enabled (> (count computed.selected) 1)})
               [["x" "align-horizontal-distribute-center"] ["y" "align-vertical-distribute-center"]])))

(defcomputed effectItems "The Shape Effects menu: none, a shadow or a glow on the selected objects."
  (map (fn [it]
         {:title (t (str "effect." (or-else it "none")))
          :run "setObjects"
          :with {:props {:effect it}}
          :enabled (pos? (count computed.selected))})
       [nil "shadow" "glow"]))

;;; Editing text on the slide

(defn edit-text "Edits a text box's or a shape's text in an editor over it, until Escape or a click elsewhere."
  [id]
  (when (#{:text :shape} (:kind (find #(= (:id %) id) computed.objects)))
    (commit)
    (set "state.board.selection" [id])
    (set "state.editing" id)))

(defn stop-editing "Leaves the text editor, and gives the canvas the keys again." []
  (set "state.editing" nil)
  (focus "#board"))

(defn set-notes "Keeps the speaker notes the notes box holds, when they changed." [text]
  (when (not= text (or-else computed.slide.notes ""))
    (set "state.deck.slides[state.current].notes" text)))

;;; The slide shown

; style: keep, a string of the code names it, and a rename never changes a string
(defn hideSlide "Leaves the slide out of the show, or puts it back." []
  (commit)
  (set "state.deck.slides[state.current].hidden" (not (or-else computed.slide.hidden false))))

(defn layout-holders "A layout's placeholders." [layout]
  (:ph (find #(= (:id %) layout) computed.layouts)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setLayout "Gives the slide another layout: the placeholders it lacks are dropped, its new ones added, empty."
  [layout]
  (commit)
  (let [holders (layout-holders layout)
        wanted (into #{} (map :ph holders))
        had (into #{} (map :ph computed.objects))]
    (set "state.deck.slides[state.current]"
         (assoc computed.slide
                :layout layout
                :objects (concat (filter #(or (nil? (:ph %)) (wanted (:ph %))) computed.objects)
                                 (map-indexed (fn [index {:keys [ph]}]
                                                {:id (str "o" (+ state.deck.next index)) :kind "text" :ph ph :text ""})
                                              (remove #(had (:ph %)) holders))))))
  (increment "state.deck.next" :by 10))

; style: keep, a string of the code names it, and a rename never changes a string
(defn resetSlide "Puts the slide's placeholders back where their layout has them, keeping their text." []
  (commit)
  (set-objects (map (fn [{:keys [id kind ph text] :as object}]
                      (if (some? ph) {:id id :kind kind :ph ph :text text} object))
                    computed.objects)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addSection "Starts an untitled section at the slide shown, in place of one that started there." []
  (commit)
  (set "state.deck.sections"
       (sort-by :first (concat (remove #(= (:first %) state.current) (or-else state.deck.sections []))
                               [{:id (str "x" state.deck.next) :name (t "section.untitled") :first state.current}])))
  (increment "state.deck.next" :by 1))

;;; The selected objects

; style: keep, set-objects is already a definition
(defn setObjects "Merges properties into every selected object." [props]
  (when (seq computed.selected)
    (commit)
    (set-objects (map #(if (selected? %) (merge % props) %) computed.objects))))

(defn toggle-text "Turns a text style (bold, italic, underline...) on for the selection, or off if all have it." [key]
  (setObjects (assoc {} key (not (get computed.allOn key)))))

(defn grow-text "Steps the selection's text size to the next size up (by 1) or down (by -1) in the list." [by]
  (let [size computed.textSize]
    (setObjects {:size (if (pos? by)
                         (or-else (find #(> % size) computed.sizes) (+ size 8))
                         (or-else (find #(< % size) (reverse computed.sizes)) (max (dec size) 1)))})))

(defn clear-formatting "Takes every text style off the selection: its theme's look comes back." []
  (setObjects {:bold nil :italic nil :underline nil :strike nil :shadow nil :size nil :color nil :highlight nil
               :font nil}))

(defn toggle-lock "Locks the selected objects, or unlocks them if all are." []
  (setObjects {:locked (not (every? #(or-else (:locked %) false) computed.selected))}))

(defn place-object "Puts an object, by its place k on the slide, where the canvas moved it to, unless it is locked."
  [k m]
  (when (and (>= k 0) (not (or-else (get-in computed.objects [k :locked]) false)))
    (let [{:keys [x y width height rotate]} m]
      (set (str "state.deck.slides[state.current].objects[" k "]")
           (assoc (get computed.objects k)
                  :x (tenth x) :y (tenth y) :w (tenth width) :h (tenth height) :rotate (tenth (or-else rotate 0)))))))

(defn place-objects "Keeps where the canvas moved, sized or turned the objects, as one change." [moved]
  (commit)
  (let [ids (map :id computed.objects)]
    (doseq [m moved]
      (place-object (indexOf ids (:id m)) m))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn deleteObjects "Deletes the selected objects but the locked ones, and their builds." []
  (when (seq computed.selected)
    (commit)
    (set-objects (filter #(or (or-else (:locked %) false) (not (selected? %))) computed.objects))
    (let [left (into #{} (map :id (get-in state.deck.slides [state.current :objects])))]
      (set "state.deck.slides[state.current].animations"
           (filter #(left (:target %)) (or-else computed.slide.animations []))))
    (set "state.board.selection" (->> computed.selected (filter :locked) (map :id)))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn copyObjects "Copies the selected objects." []
  (when (seq computed.selected)
    (set "state.clip" computed.selected)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn cutObjects "Copies the selected objects, then deletes them." []
  (copyObjects)
  (deleteObjects))

; style: keep, a string of the code names it, and a rename never changes a string
(defn pasteObjects "Pastes the copied objects onto the slide, a little down and right, with new ids, and selects them."
  []
  (when (seq state.clip)
    (commit)
    (let [next state.deck.next]
      (set-objects (concat computed.objects
                           (map-indexed (fn [index it]
                                          (assoc it
                                                 :id (str "o" (+ next index))
                                                 :x (+ (or-else (:x it) 0) 18)
                                                 :y (+ (or-else (:y it) 0) 18)
                                                 :ph nil
                                                 :group (some-> (:group it) (str "-" next))))
                                        state.clip)))
      (set "state.board.selection" (map #(str "o" (+ next %)) (range (count state.clip)))))
    (increment "state.deck.next" :by (count state.clip))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn duplicateObjects "Pastes a copy of the selected objects." []
  (copyObjects)
  (pasteObjects))

; style: keep, a string of the code names it, and a rename never changes a string
(defn selectAll "Selects every object of the slide." []
  (set "state.board.selection" (map :id computed.objects)))

;;; New objects

; style: keep, a string of the code names it, and a rename never changes a string
(defn insertObject "Adds an object to the slide, with a new id, and selects it." [object]
  (commit)
  (let [id (str "o" state.deck.next)]
    (push "state.deck.slides[state.current].objects" (assoc object :id id))
    (set "state.board.selection" [id]))
  (increment "state.deck.next" :by 1))

; style: keep, a string of the code names it, and a rename never changes a string
(defn insertShape "Adds a shape of the gallery in the middle of the slide: a line, or a filled shape." [shape]
  (insertObject (if (get-in computed.shapes [shape :line])
                  {:kind "line" :shape shape :x 380 :y 270 :w 200 :h 0 :line "accent1" :lineWidth 1}
                  {:kind "shape" :shape shape :x 405 :y 195 :w 150 :h 150 :fill "accent1" :line "accent1.4"
                   :lineWidth 1 :text ""})))

; style: keep, a string of the code names it, and a rename never changes a string
(defn insertTextBox "Adds a text box in the middle of the slide." []
  (insertObject {:kind "text" :x 330 :y 240 :w 300 :h 40 :text (t "object.newText") :size 18}))

;;; The deck's design

; style: keep, a string of the code names it, and a rename never changes a string
(defn setTheme "Gives the deck a theme, with its own colours and its first variant." [theme]
  (commit)
  (set "state.deck.theme" theme)
  (set "state.deck.variant" 0)
  (set "state.deck.colors" nil))

(defn set-variant "Gives the deck one of its theme's variants, with its own colours." [variant]
  (commit)
  (set "state.deck.variant" variant)
  (set "state.deck.colors" nil))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setColorSet "Gives the deck another theme's colours (nil: its own), until a theme or a variant is chosen." [id]
  (commit)
  (set "state.deck.colors" id))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setFonts "Sets the fonts of titles and of the other text (a text's own font wins)." [heading body]
  (commit)
  (set "state.deck.fonts" {:heading heading :body body}))

;;; Transitions and builds

(def no-transition "A slide's transition when it has none." {:kind "none" :ms 500})

(defn set-transition "Merges properties into the slide's transition." [props]
  (commit)
  (set "state.deck.slides[state.current].transition" (merge (or-else computed.slide.transition no-transition) props)))

(defn duration-by "Makes the slide's transition longer or shorter, by milliseconds, between 0 and 59 seconds." [by]
  (set-transition {:ms (clamp (+ (or-else computed.slide.transition.ms 500) by) 0 59000)}))

; style: keep, a string of the code names it, and a rename never changes a string
(defn transitionToAll "Gives every slide this slide's transition." []
  (commit)
  (let [transition (or-else computed.slide.transition no-transition)]
    (set "state.deck.slides" (map #(assoc % :transition transition) state.deck.slides))))

(defn set-animation "Gives the selected objects a build (none: no build); the first starts as asked, the others with it."
  [effect kind start ms]
  (when (seq computed.selected)
    (commit)
    (let [next state.deck.next]
      (set "state.deck.slides[state.current].animations"
           (concat (remove #(contains state.board.selection (:target %)) (or-else computed.slide.animations []))
                   (if (= effect :none)
                     []
                     (map-indexed (fn [index {:keys [id]}]
                                    {:id (str "a" (+ next index)) :target id :effect effect :kind kind
                                     :start (if (zero? index) start "with") :ms ms :delay 0})
                                  computed.selected)))))
    (increment "state.deck.next" :by (count computed.selected))))

(defn change-animation "Merges properties into the builds of the selected objects." [props]
  (when-some [_ computed.animation]
    (commit)
    (set "state.deck.slides[state.current].animations"
         (map #(if (contains state.board.selection (:target %)) (merge % props) %)
              computed.slide.animations))))
