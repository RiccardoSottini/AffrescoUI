;;; sinopia.lisp --- what Sinopia does to its deck: undo and redo, the slides, and the selected objects
;;;
;;; The parts (parts/*.lisp) say what is shown, and keep the actions that are one step, or reach outside (opening,
;;; saving, exporting, the show). This file holds the edits that take a value worked out first, which an action has to
;;; pass to a second one: here a `let` does it. A function reads the running app through paths (`state.deck`,
;;; `computed.objects`) and changes it only through steps (`set`, `push`...). A handler runs one by name, as it would an
;;; action: {:run moveSlide :with {:from 2 :to 0}}.

(ns sinopia.sinopia)

;;; A form the others use

(defmacro each "Runs the body once for each item of a list, the item bound to a name: (each [o objects] (f o))."
  [[item items] & body]
  `(doseq [~item ~items] ~@body))

;;; Undo and redo

(defn either "A value, or another when it is nil: an expression's ??." [v otherwise]
  (if-some [value v] value otherwise))

(defn snapshot "The deck as it is, and the slide shown: what Undo goes back to." []
  {:deck state.deck :current state.current})

(defn commit "Keeps the deck as it is before a change: Undo goes back to it, up to 50 times, and Redo starts afresh." []
  (push "state.undo" (snapshot))
  (when (> (count state.undo) 50) (remove-state "state.undo" :at 0))
  (set "state.redo" []))

(defn travel "Undo and Redo: the deck as it is goes on one stack, and the last of the other takes its place."
  [from onto stack]
  (push onto (snapshot))
  (let [{:keys [deck current]} (last stack)]
    (set "state.deck" deck)
    (set "state.current" current))
  (remove-state from :at (dec (count stack)))
  (set "state.board.selection" []))

(defn undo "Goes back one change." []
  (when computed.canUndo (travel "state.undo" "state.redo" state.undo)))

(defn redo "Goes forward one change undone." []
  (when computed.canRedo (travel "state.redo" "state.undo" state.redo)))

;;; Slides

(defn pick-slide "Shows a slide, by its index." [index]
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

; style: keep, a string of the code names it, and a rename never changes a string
(defn newSlide "Adds a slide after this one, with its layout's placeholders, empty." [layout]
  (commit)
  (let [next state.deck.next
        holders (either (:ph (find #(= (:id %) layout) computed.layouts)) [])]
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

; style: keep, a string of the code names it, and a rename never changes a string
(defn duplicateSlide "Adds a copy of this slide after it, its objects with new ids, without its animations." []
  (commit)
  (let [next state.deck.next]
    (set "state.deck.slides"
         (after-current (assoc computed.slide
                               :id (str "s" next)
                               :objects (map-indexed (fn [i o] (assoc o :id (str "o" (+ next 1 i)))) computed.objects)
                               :animations []))))
  (increment "state.deck.next" :by (+ (count computed.objects) 2))
  (show-next))

; style: keep, a string of the code names it, and a rename never changes a string
(defn deleteSlide "Takes this slide away, unless it is the only one." []
  (when (> (count state.deck.slides) 1)
    (commit)
    (remove-state "state.deck.slides" :at state.current)
    (set "state.current" (min state.current (dec (count state.deck.slides))))
    (set "state.board.selection" [])))

(defn move-slide "Moves the slide at one index to another, and shows it there." [from to]
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
  (let [letters (str/split text "")]
    (str (upper (join "" (slice letters 0 at))) (lower (join "" (slice letters at))))))

;; toggle turns each letter the other way; sentence puts each line's first letter up, words each word's, the rest down
(defn cased "An object's text in a case: upper, lower, toggle, sentence or words."
  [how object]
  (let [text (either (:text object) "")]
    (case how
      :upper (upper text)
      :lower (lower text)
      :toggle (->> (str/split text "") (map #(if (= (upper %) %) (lower %) (upper %))) (join ""))
      ; style: keep, the host's split keeps the empty lines at the end, which the join gives back
      :sentence (->> (split text "\n") (map #(capital % (inc (indexOf % (str/trim %))))) (join "\n"))
      ; style: keep, the host's split keeps the empty lines and spaces at the end; #(...) can't hold another
      :words (->> (split text "\n") (map (fn [line] (join " " (map #(capital % 1) (split line " "))))) (join "\n"))
      (:text object))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn changeCase "Changes the case of the selected objects' text." [how]
  (when (seq computed.selected)
    (commit)
    (set-objects (map #(if (selected? %) (assoc % :text (cased how %)) %) computed.objects))))

(defn order "The selected objects move together in the slide's drawing order: front, forward, backward or back." [how]
  (when (seq computed.selected)
    (let [objects computed.objects
          selected computed.selected
          at computed.selectedAt
          others (remove selected? objects)
          ids (map :id others)
          above (:id (get objects (inc (apply max at))))
          below (when (pos? (apply min at)) (:id (get objects (dec (apply min at)))))
          between #(concat (take % others) selected (drop % others))]
      (commit)
      (set-objects (case how
                     :front (concat others selected)
                     :back (concat selected others)
                     :forward (if-some [id above] (between (inc (indexOf ids id))) objects)
                     (if-some [id below] (between (indexOf ids id)) objects))))))

(defn total "The sum of numbers." [xs]
  (reduce + 0 xs))

(defn tenth "A number rounded to a tenth." [x]
  (/ (round (* x 10)) 10))

;; One selected object aligns to the slide, several to the box around them; locked objects stay
(defn align "Aligns the selected objects: left, center, right, top, middle or bottom."
  [how]
  (let [boxes computed.boxes]
    (when (seq boxes)
      (let [ids (map :id boxes)
            {:keys [left top right bottom]} (if (= (count boxes) 1) computed.slideBounds computed.selectionBounds)
            place (fn [o]
                    (let [{:keys [w h]} (get boxes (indexOf ids (:id o)))]
                      (case how
                        :left {:x left}
                        :center {:x (tenth (- (/ (+ left right) 2) (/ w 2)))}
                        :right {:x (- right w)}
                        :top {:y top}
                        :middle {:y (tenth (- (/ (+ top bottom) 2) (/ h 2)))}
                        {:y (- bottom h)})))]
        (commit)
        (set-objects (map #(if (or (not (contains ids (:id %))) (either (:locked %) false)) % (merge % (place %)))
                          computed.objects))))))

;; From the first to the last, or across the slide for two objects; locked objects stay
(defn distribute "Puts equal gaps between the selected objects along an axis, x or y."
  [axis]
  (let [boxes computed.boxes
        n (count boxes)]
    (when (> n 1)
      (let [along (sort-by #(get % axis) boxes)
            ids (map :id along)
            sizes (map (if (= axis :x) :w :h) along)
            {:keys [left top right bottom]} computed.selectionBounds
            start (cond (= n 2) 0 (= axis :x) left :else top)
            span (cond (= n 2) (if (= axis :x) 960 540)
                       (= axis :x) (- right left)
                       :else (- bottom top))
            free (- span (total sizes))
            place (fn [o]
                    (let [i (indexOf ids (:id o))]
                      (assoc o axis (tenth (+ start (total (take i sizes)) (/ (* i free) (dec n)))))))]
        (commit)
        (set-objects (map #(if (or (not (contains ids (:id %))) (either (:locked %) false)) % (place %))
                          computed.objects))))))
