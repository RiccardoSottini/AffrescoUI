;;; design.lisp --- theme colours of one's own
;;;
;;; The format pane, with nothing selected, lists the ten theme colours with their hex codes; typing a code and Enter
;;; gives the deck that colour, as its own colours (state.deck.colors is then "own", state.deck.ownColors the ten),
;;; saved with the deck. A theme, a variant or another theme's colours chosen after put the theme's back.

(ns sinopia.parts.design)

;;; What the rows keep

(defstate colorDraft {} "The hex codes being typed, by slot, until Enter.")

(defcomputed ownColorsOn "Whether the deck shows colours of its own." (= state.deck.colors "own"))

;;; Views

(defview format-own-colors "The theme's ten colours, each with its hex code to type over; Reset puts the theme's back."
  []
  `(column#format-own-colors :width 236 :fitContent height :alignItems stretch :gap 3 :color transparent
     (row#format-own-head :width 236 :height 18 :alignItems center :color transparent
       (label#format-own-title :grow 1 :height 18 :textColor @rbMuted :runs [{:text ~(t "design.themeColors") :size 12}])
       (label#format-own-reset :when ~computed.ownColorsOn
         :height 18
         :fitText true
         :padding 4
         :style rbButton
         :runs [{:icon rotate-ccw :size 11} {:text ~(str " " (t "design.reset")) :size 11}]
         :tooltip ~(t "design.resetAbout")
         :onClick ~(handler setColorSet nil)))
     ~(repeat [slot computed.slotNames :key slot]
       (own-color-row slot))))

(defview own-color-row "Slot `slot`: its colour, its name, and its hex code, which Enter takes." [slot]
  `(row :id ~(str "own-" slot) :width 236 :height 22 :gap 6 :alignItems center :color transparent
     (node :id ~(str "own-" slot "-swatch")
       :width 18
       :height 18
       :radius 3
       :color ~(get computed.palette slot)
       :borderWidth 1
       :borderColor @rbFieldLine)
     (label :id ~(str "own-" slot "-name") :grow 1 :height 20 :textColor @rbInk
       :runs [{:text ~(get computed.slotTitles slot) :size 12}])
     (textarea :id ~(str "own-" slot "-hex")
       :width 76
       :height 22
       :padding 4
       :radius 4
       :color @rbFieldBg
       :borderWidth 1
       :borderColor @rbFieldLine
       :textColor @rbInk
       :bind ~(str "state.colorDraft." slot)
       :placeholder ~(get computed.colors slot)
       :placeholderColor @rbMuted
       :label ~(t "design.hexFor" {:slot (get computed.slotTitles slot)})
       :onSubmit ~(handler set-own-color slot))))

;;; What they do

(defn hex? "Whether a text is a colour's hex code, #rrggbb." [text]
  (and (string? text) (= (count text) 7) (str/starts-with? text "#")))

(defn set-own-color "Gives the deck the colour typed for a slot, as one of its own colours; a bad code is dropped."
  [slot]
  (let [typed (str/trim (or-else (get state.colorDraft slot) ""))
        base (if computed.ownColorsOn (or-else state.deck.ownColors computed.colors) computed.colors)]
    (when (hex? typed)
      (commit)
      (set "state.deck.ownColors" (assoc base slot (str/lower-case typed)))
      (set "state.deck.colors" "own"))
    (set "state.colorDraft" (dissoc state.colorDraft slot))))
