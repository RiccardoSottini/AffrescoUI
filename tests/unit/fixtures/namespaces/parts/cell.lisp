;;; cell.lisp --- one cell of the board

(ns tally.parts.cell
  (:require [clojure.string :refer [join]]))

(defview cell "A cell, labelled with its name." [name]
  `(label :text ~(join " " ["cell" name])))
