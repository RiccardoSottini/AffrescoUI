;;; index.screen.lisp --- a sample for the linter's configuration: the entry of an app of three files, with an ns
;;;
;;; Not an example app: check.sh lints it with columns.lisp and cards.lisp and expects exactly the mistakes marked
;;; "planted" in them. The entry's ns requires the app's files by name and its strings by path, and the names the
;;; files define are one space: column-list is called here as its plain name.

(ns board.index
  (:require board.columns
            [board.cards :as cards]
            ["locales/en.lisp"]))

(defscreen main :window {:title ~(t "window.title") :width 480 :height 360}
  (column#page :gap 8 :padding 12
    (label#title :text ~(t "board.title"))
    ~(column-list)
    (label#count :text ~(t "board.count" {:n (cards/card-count)}))))
