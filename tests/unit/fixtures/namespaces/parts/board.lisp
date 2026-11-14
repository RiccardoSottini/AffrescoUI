;;; board.lisp --- the board: a row of cells

(ns tally.parts.board
  (:require [tally.parts.cell :as cell]))

(defn board-title "What the board is called." [] "the board")

(defview board-row "The board's cells, in a row." []
  `(row#board ~(cell/cell "a") ~(cell/cell "b")))
