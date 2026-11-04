;;; sinopia.lisp --- what Sinopia does to its deck: undo and redo, the slides, and the selected objects
;;;
;;; The parts (parts/*.lisp) say what is shown, and keep the actions that are one step, or reach outside (opening,
;;; saving, exporting, the show). This file holds the edits that take a value worked out first, which an action has to
;;; pass to a second one: here a `let` does it. A function reads the running app through paths (`state.deck`,
;;; `computed.objects`) and changes it only through steps (`set`, `push`...). A handler runs one by name, as it would an
;;; action: {:run moveSlide :with {:from 2 :to 0}}.

;;; A form the others use

(defmacro each "Runs the body once for each item of a list, the item bound to a name: (each [o objects] (f o))."
  [[item items] & body]
  `(reduce (fn [_ ~item] ~@body nil) nil ~items))

;;; Undo and redo

(defn either "A value, or another when it is nil: an expression's ??." [v otherwise]
  (if (nil? v) otherwise v))

(defn snapshot "The deck as it is, and the slide shown: what Undo goes back to." []
  {:deck state.deck :current state.current})

(defn commit "Keeps the deck as it is before a change: Undo goes back to it, up to 50 times, and Redo starts afresh." []
  (push "state.undo" (snapshot))
  (when (> (count state.undo) 50) (remove-state "state.undo" :at 0))
  (set "state.redo" []))

(defn travel "Undo and Redo: the deck as it is goes on one stack, and the last of the other takes its place."
  [from onto stack]
  (push onto (snapshot))
  (let [kept (last stack)]
    (set "state.deck" (:deck kept))
    (set "state.current" (:current kept)))
  (remove-state from :at (dec (count stack)))
  (set "state.board.selection" []))

(defn undo "Goes back one change." []
  (when computed.canUndo (travel "state.undo" "state.redo" state.undo)))

(defn redo "Goes forward one change undone." []
  (when computed.canRedo (travel "state.redo" "state.undo" state.redo)))

;;; Slides

(defn pickSlide "Shows a slide, by its index." [index]
  (set "state.current" (clamp index 0 (dec (count state.deck.slides))))
  (set "state.board.selection" [])
  (set "state.editing" nil))

(defn after-current "The deck's slides with one more after the slide shown." [slide]
  (let [slides state.deck.slides
        at (inc state.current)]
    (concat (take at slides) [slide] (drop at slides))))

(defn show-next "Shows the slide after this one, with nothing selected." []
  (set "state.current" (inc state.current))
  (set "state.board.selection" []))

(defn newSlide "Adds a slide after this one, with its layout's placeholders, empty." [layout]
  (commit)
  (let [next state.deck.next
        holders (either (:ph (find (fn [l] (= (:id l) layout)) computed.layouts)) [])]
    (set "state.deck.slides"
         (after-current {:id (str "s" next)
                         :layout layout
                         :notes ""
                         :transition {:kind "none" :ms 500}
                         :animations []
                         :objects (map-indexed (fn [i p] {:id (str "o" (+ next 1 i)) :kind "text" :ph (:ph p) :text ""})
                                               holders)})))
  (increment "state.deck.next" :by 20)
  (show-next))

(defn duplicateSlide "Adds a copy of this slide after it, its objects with new ids, without its animations." []
  (commit)
  (let [next state.deck.next]
    (set "state.deck.slides"
         (after-current (merge computed.slide
                               {:id (str "s" next)
                                :objects (map-indexed (fn [i o] (merge o {:id (str "o" (+ next 1 i))}))
                                                      computed.objects)
                                :animations []}))))
  (increment "state.deck.next" :by (+ (count computed.objects) 2))
  (show-next))

(defn deleteSlide "Takes this slide away, unless it is the only one." []
  (when (> (count state.deck.slides) 1)
    (commit)
    (remove-state "state.deck.slides" :at state.current)
    (set "state.current" (min state.current (dec (count state.deck.slides))))
    (set "state.board.selection" [])))

(defn moveSlide "Moves the slide at one index to another, and shows it there." [from to]
  (let [slides state.deck.slides]
    (when (and (number? from) (number? to) (>= from 0) (>= to 0) (not= from to)
               (< from (count slides)) (< to (count slides)))
      (commit)
      (let [others (concat (take from slides) (drop (inc from) slides))]
        (set "state.deck.slides" (concat (take to others) [(get slides from)] (drop to others))))
      (set "state.current" to))))

;;; The selected objects

(defn selected? "Whether an object is one of the selected." [object]
  (contains state.board.selection (:id object)))

(defn set-objects "Puts a new list of objects on this slide." [objects]
  (set "state.deck.slides[state.current].objects" objects))

(defn capital "A text with its first letters, up to an index, in upper case, and the rest in lower case." [text at]
  (let [letters (split text "")]
    (str (upper (join "" (slice letters 0 at))) (lower (join "" (slice letters at))))))

;; toggle turns each letter the other way; sentence puts each line's first letter up, words each word's, the rest down
(defn cased "An object's text in a case: upper, lower, toggle, sentence or words."
  [how object]
  (let [text (either (:text object) "")]
    (cond (= how "upper") (upper text)
          (= how "lower") (lower text)
          (= how "toggle") (join "" (map (fn [c] (if (= (upper c) c) (lower c) (upper c))) (split text "")))
          (= how "sentence") (join "\n" (map (fn [line] (capital line (inc (indexOf line (trim line)))))
                                             (split text "\n")))
          (= how "words")
          (join "\n" (map (fn [line] (join " " (map (fn [word] (capital word 1)) (split line " ")))) (split text "\n")))
          :else (:text object))))

(defn changeCase "Changes the case of the selected objects' text." [how]
  (when (> (count computed.selected) 0)
    (commit)
    (set-objects (map (fn [o] (if (selected? o) (merge o {:text (cased how o)}) o)) computed.objects))))

(defn order "The selected objects move together in the slide's drawing order: front, forward, backward or back." [how]
  (when (> (count computed.selected) 0)
    (let [objects computed.objects
          selected computed.selected
          at computed.selectedAt
          others (filter (fn [o] (not (selected? o))) objects)
          ids (map (fn [o] (:id o)) others)
          above (:id (get objects (inc (apply max at))))
          below (when (> (apply min at) 0) (:id (get objects (dec (apply min at)))))
          between (fn [k] (concat (take k others) selected (drop k others)))]
      (commit)
      (set-objects (cond (= how "front") (concat others selected)
                         (= how "back") (concat selected others)
                         (= how "forward") (if (nil? above) objects (between (inc (indexOf ids above))))
                         :else (if (nil? below) objects (between (indexOf ids below))))))))

(defn total "The sum of numbers." [xs]
  (reduce + 0 xs))

(defn tenth "A number rounded to a tenth." [x]
  (/ (round (* x 10)) 10))

;; One selected object aligns to the slide, several to the box around them; locked objects stay
(defn align "Aligns the selected objects: left, center, right, top, middle or bottom."
  [how]
  (let [boxes computed.boxes]
    (when (> (count boxes) 0)
      (let [ids (map (fn [box] (:id box)) boxes)
            b (if (= (count boxes) 1) computed.slideBounds computed.selectionBounds)
            place (fn [o]
                    (let [box (get boxes (indexOf ids (:id o)))]
                      (cond (= how "left") {:x (:left b)}
                            (= how "center") {:x (tenth (- (/ (+ (:left b) (:right b)) 2) (/ (:w box) 2)))}
                            (= how "right") {:x (- (:right b) (:w box))}
                            (= how "top") {:y (:top b)}
                            (= how "middle") {:y (tenth (- (/ (+ (:top b) (:bottom b)) 2) (/ (:h box) 2)))}
                            :else {:y (- (:bottom b) (:h box))})))]
        (commit)
        (set-objects (map (fn [o] (if (or (not (contains ids (:id o))) (either (:locked o) false))
                                    o
                                    (merge o (place o))))
                          computed.objects))))))

;; From the first to the last, or across the slide for two objects; locked objects stay
(defn distribute "Puts equal gaps between the selected objects along an axis, x or y."
  [axis]
  (let [boxes computed.boxes
        n (count boxes)]
    (when (> n 1)
      (let [along (sort-by (fn [box] (get box axis)) boxes)
            ids (map (fn [box] (:id box)) along)
            sizes (map (fn [box] (if (= axis "x") (:w box) (:h box))) along)
            b computed.selectionBounds
            start (cond (= n 2) 0 (= axis "x") (:left b) :else (:top b))
            span (cond (= n 2) (if (= axis "x") 960 540)
                       (= axis "x") (- (:right b) (:left b))
                       :else (- (:bottom b) (:top b)))
            free (- span (total sizes))
            place (fn [o]
                    (let [i (indexOf ids (:id o))]
                      (assoc o axis (tenth (+ start (total (take i sizes)) (/ (* i free) (dec n)))))))]
        (commit)
        (set-objects (map (fn [o] (if (or (not (contains ids (:id o))) (either (:locked o) false)) o (place o)))
                          computed.objects))))))
