;;; clipboard.lisp --- Cut, copy and paste with other apps
;;;
;;; Copy keeps the selected objects for Sinopia's own paste and puts their text on the system clipboard, so another
;;; app can paste it. Paste reads the system clipboard first: Sinopia's own text brings back the objects it copied,
;;; any other text becomes a text box on the slide.

(ns sinopia.parts.clipboard)

;;; Definitions

(defstate clipText "" "The text the last copy put on the system clipboard, to know it when it comes back.")

(defstate pasted "" "The system clipboard's text, as the last paste read it.")

(defn clip-text "The text of some objects, one paragraph each, as another app gets it." [objects]
  (->> objects
       (map #(or-else (:text %) ""))
       (remove str/blank?)
       (str/join "\n")))

;;; What its commands do

(defn copy-to-system "Puts the text of the objects just copied on the system clipboard." [objects]
  (let [text (clip-text objects)]
    (set "state.clipText" text)
    (when-not (str/blank? text)
      (copy text))))

(defn pasted-text
  "What the system clipboard held: Sinopia's own copy (or nothing) pastes the copied objects, other text a text box
  with it in the middle of the slide."
  [result]
  (let [text (str/trim (or-else result ""))]
    (if (or (str/blank? text) (= text (str/trim state.clipText)))
      (paste-clip)
      (insertObject {:kind "text" :x 180 :y 230 :w 600 :h 80 :text text :size 18}))))
