;;; cards.lisp --- a sample for the linter's configuration: the board's cards, in a file with an ns label

(ns board.cards)

(defstate cards [{:title "Write" :column 0}] "The cards, each in a column.")

(defn card-count "How many cards there are, in every column." []
  (count state.cards))

(defn cards-in "The cards of a column." [column]
  (filter #(= %.column column) state.cards))

(defn first-card "The first card's title." []
  (:title (first state.cards 0))) ; planted: first takes one argument
