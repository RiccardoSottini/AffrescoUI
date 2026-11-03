;;; data.lisp --- Arcadia's state: today and the person
;;;
;;; Its records (the games, owned or only in the store, the friends, the library's news and the activity) are
;;; models.json's, seeded from seed/, all invented. A game's pictures are art/<id>/cover.svg (2:3), hero.svg (the wide
;;; banner) and shot1-3.svg. Times are milliseconds, measured from state.today.

;;; Definitions

(defstate today 1792713600000)

(defstate user {:name "Mira" :avatar fox :level 23 :wallet 42.17 :status online :since 2019 :code "4207 1186"})
