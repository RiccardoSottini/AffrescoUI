;;; guides.lisp --- the tutorial (C-h t), a buffer of its own to practise in, and the tour of what Calamaio can do
;;;
;;; Both texts are Calamaio's own, in the locales (tutorial.text, tour.text).

;;; The guides

(defcommand help-with-tutorial "Opens the tutorial in a buffer of its own, to read and practise in."
  :title ~(t "command.help-with-tutorial") :category ~(t "category.help")
  :description ~(t "command.help-with-tutorial.about")
  []
  (if (contains (keys state.bufs) "bt")
    (show "bt")
    (listing-show "bt" {:name "TUTORIAL" :mode "text" :text (t "tutorial.text")})))

(defcommand show-tour "Shows what Calamaio can do, with the keys that do it."
  :title ~(t "command.show-tour") :category ~(t "category.help")
  :description ~(t "command.show-tour.about")
  []
  (listing-show "bg" {:name "*Tour*" :mode "help" :readOnly true :text (t "tour.text")}))
