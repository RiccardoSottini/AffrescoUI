;;; index.screen.lisp --- a board of cells: the program's files, each with its namespace

(ns tally.index
  "The window: a title, and the board."
  (:require [clojure.string :as s]
            [tally.parts.board :as board :refer [board-title]]))

(defscreen main "A title, and the board under it."
  :window {:title "Tally" :width 480 :height 320}
  (column#root
    (label#title :text ~(s/upper-case (board-title)))
    ~(board/board-row)))
