;;; file.lisp --- the File tab's new decks and recent decks
;;;
;;; New starts a deck from a theme: one title slide, its placeholders empty. The decks last opened or saved are kept
;;; (their names and their slides, across sessions), and a click on one opens it again, as a change Undo takes back.

(ns sinopia.parts.file)

;;; What the File tab keeps

(defstate recentDecks [] "The decks last opened or saved, newest first: {name, deck}, at most four.")

(defcomputed newThemes "The themes a new deck starts from, as the File tab shows them." (slice computed.themes 0 6))

;;; Views

(defview g-file-new "New: a deck from each theme, drawn as its background, Aa and its accents." []
  `(node#g-file-new :use rbGroup :with {:title ~(t "file.new")}
     (row#file-new-row :fitContent width :height 70 :gap 4 :alignItems center :color transparent
       ~(repeat [theme computed.newThemes :key theme.id]
         (new-choice theme)))))

(defview new-choice "A new deck in theme `theme`." [theme]
  `(panel :id ~(str "new-" theme.id)
     :width 64
     :height 52
     :radius 4
     :cursor hand
     :role button
     :label ~(t "file.newIn" {:theme (t (str "theme." theme.id))})
     :tooltip ~(t "file.newIn" {:theme (t (str "theme." theme.id))})
     :color ~(get theme.colors theme.background)
     :borderWidth 1
     :borderColor @rbFieldLine
     :hover {:borderColor @brand}
     :onClick ~(handler new-deck theme.id)
     (label :id ~(str "new-" theme.id "-aa")
       :x 6
       :y 6
       :width 40
       :height 22
       :textColor ~theme.colors.dark1
       :runs [{:text "Aa" :size 16}])
     (row :id ~(str "new-" theme.id "-bar") :x 6 :y 36 :width 52 :height 6 :color transparent
       ~(repeat [c ["accent1" "accent2" "accent3" "accent4"]]
         `(node :id ~(str "new-" theme.id "-" c) :width 13 :height 6 :color ~(get theme.colors c))))))

(defview g-file-recent "Recent: the decks last opened or saved; a click opens one again." []
  `(node#g-file-recent :use rbGroup :with {:title ~(t "file.recent")}
     (column#file-recent-list :width 200 :height 72 :gap 0 :alignItems stretch :color transparent
       (label#file-recent-none :when ~(empty? state.recentDecks)
         :height 72
         :wrap true
         :padding 4
         :textColor @rbMuted
         :runs [{:text ~(t "file.noRecent") :size 12}])
       ~(repeat [r (slice state.recentDecks 0 3) :index i]
         `(label :id ~(str "recent-" i)
            :height 22
            :padding 4
            :style rbButton
            :tooltip ~(t "file.openAgain" {:name r.name})
            :runs [{:icon file-clock :size 13} {:text ~(str " " r.name) :size 12}]
            :onClick ~(handler open-recent i))))))

;;; What they do

(defn new-deck "Starts a deck in a theme: one title slide with empty placeholders; Undo takes it back." [theme]
  (commit)
  (set "state.deck"
       {:title (t "view.untitledDeck")
        :size [960 540]
        :theme theme
        :variant 0
        :next 3
        :sections []
        :slides [{:id "s1"
                  :layout "title"
                  :notes ""
                  :objects [{:id "o1" :kind "text" :ph "ctrTitle" :text ""}
                            {:id "o2" :kind "text" :ph "subTitle" :text ""}]}]})
  (set "state.current" 0)
  (set "state.board.selection" [])
  (set "state.fileStatus" (t "file.started")))

(defn remember-deck "Keeps the deck shown among the recent ones, by its file's name, newest first." [name]
  (let [others (remove #(= (:name %) name) state.recentDecks)
        kept (take 4 (cons {:name name :deck state.deck} others))]
    (set "state.recentDecks" (vec kept))))

(defn open-recent "Opens a recent deck again, as a change Undo takes back." [index]
  (let [r (nth state.recentDecks index)]
    (commit)
    (set "state.deck" r.deck)
    (set "state.current" 0)
    (set "state.board.selection" [])
    (set "state.fileStatus" (t "file.opened" {:name r.name}))))
