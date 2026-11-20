;;; key-hints.lisp --- Key hints: Alt pressed alone shows a letter on every tab and a digit on every quick command;
;;; that key then opens the tab or runs the command, and Alt, Escape or any other key hides them
;;;
;;; The hints are a layer that takes the keys while it is open (its "*"), just outside the window so that its focus
;;; ring doesn't show; each badge is placed under its command.

(ns sinopia.parts.key-hints)

;;; Definitions

(defstate keyHints false "Whether the key hints are shown, after Alt was pressed alone.")

(defcomputed hintKeys "The key of each tab and quick command, and the node its badge sits under."
  (concat (map #(tab-hint %) (concat computed.tabs computed.contextTabs))
          [{:key "1" :target "#qat-undo"}
           {:key "2" :target "#qat-redo"}
           {:key "3" :target "#qat-save"}
           {:key "4" :target "#qat-present"}]))

(defn tab-hint "A tab's hint: its letter, and the tab's title its badge sits under." [tab]
  {:key (get computed.tabKeys tab.id) :target (str "#tab-" tab.id "-label") :tab tab.id})

(defcomputed tabKeys "The letter that opens each tab."
  {"file" "f"
   "home" "h"
   "insert" "n"
   "draw" "d"
   "design" "g"
   "transitions" "k"
   "animations" "a"
   "slideShow" "s"
   "record" "c"
   "review" "r"
   "view" "w"
   "help" "y"
   "shapeFormat" "j"})

(defkey "alt" {:when ~(not state.show.on) :do toggle-key-hints})

(defcomponent keyHintsLayer "The key hints: a badge under every tab and quick command, and the keys they take." {}
  `(node#key-hints :overlay {:placement window}
     :x -20
     :y -20
     :width 1
     :height 1
     :color transparent
     :focusable true
     :autofocus true
     :label ~(t "bar.keyHints")
     :keys {"*" key-hint}
     ~(repeat [h computed.hintKeys :key h.key]
       `(label :id ~(str "hint-" h.key)
          :overlay {:anchor ~h.target :placement below}
          :width 18
          :height 18
          :radius 4
          :align center
          :color @rbInk
          :textColor @rb
          :runs [{:text ~(upper-case h.key) :size 11 :bold true}]))))

(defpart key-hints "The key hints over the window."
         :overlays {:zHints (node :use keyHintsLayer :when ~state.keyHints)})

;;; What its keys do

(defn toggle-key-hints "Shows the key hints, or hides them." []
  (set "state.keyHints" (not state.keyHints)))

(defn key-hint "A key while the hints show: a tab's letter opens it, a digit runs its quick command." [event]
  (set "state.keyHints" false)
  (let [hint (find #(= (:key %) event.key) computed.hintKeys)]
    (cond (some? (:tab hint)) (show-tab (:tab hint))
          (= event.key "1") (undo)
          (= event.key "2") (redo)
          (= event.key "3") (saveDeck)
          (= event.key "4") (present 0)
          :else nil)))
