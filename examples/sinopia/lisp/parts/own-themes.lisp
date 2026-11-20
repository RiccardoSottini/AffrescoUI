;;; own-themes.lisp --- Themes of one's own: Design > Save Theme keeps the deck's look (its theme's background and
;;; decorations, the colours it shows and its fonts) as a theme of the gallery, for this deck and the next
;;;
;;; The saved themes are kept with the app's settings (persist), not in a deck, so every deck can use them.

(ns sinopia.parts.own-themes)

;;; Definitions

(defstate ownThemes [] "The themes the person saved, each like one of the app's own, with a name and its fonts.")

(defcomputed galleryThemes "The themes the Design tab's gallery shows: the saved ones first, six in all."
  (let [own (or-else state.ownThemes [])]
    (take 6 (concat (reverse own) computed.builtInThemes))))

(defn theme-name "A theme's name: a saved theme's own, else the app's name for it." [theme]
  (let [key (str "theme." (:id theme))]
    (or-else (:name theme) (t key))))

(defn save-theme "Keeps the deck's look as a theme of its own, and gives the deck that theme." []
  (let [n (inc (count (or-else state.ownThemes [])))
        id (str "own" n)
        base computed.theme]
    (set "state.ownThemes"
         (conj (vec (or-else state.ownThemes []))
               {:id id
                :name (t "design.myTheme" {:n n})
                :background (:background base)
                :colors computed.colors
                :variants [{}]
                :decor (:decor base)
                :titleDecor (:titleDecor base)
                :fonts (or-else state.deck.fonts {:heading "default" :body "default"})}))
    (setTheme id)))
