;;; program.screen.lisp --- a small program: what it builds is drawn, and its definitions take revisions

;; The rooms the screen lists. Building reads them; nothing here reads the state.
(def rooms "The rooms shown." [{:id "kitchen" :name "Kitchen"} {:id "hall" :name "Hall"}])

;; One room, as a node.
(defn room-label "One room, as a label." [room]
  `(label :id ~(str "room-" (:id room)) :width 288 :height 24 :text ~(:name room) :textColor "#e2e8f0"))

(defcommand tidy "Tidies the list. Its body runs in a later stage." :title "Tidy" :category "Rooms" [] nil)

(defkey "mod+/" tidy) ; a chord whose address holds a slash

(defscreen main :window {:title "Rooms" :width 320 :height 200} :state {:shown 2}
  :actions {:undo-last [{:undo true}]}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size 16}}
  (panel#back :color "#0f172a"
    (column#rooms :x 16 :y 16 :width 288 :height 168 :gap 8
      (label#title :width 288 :height 24 :text "Rooms" :textColor "#f8fafc")
      ~@(map room-label rooms)
      (label#count :width 288 :height 24 :text "{{state.shown}} shown" :textColor "#94a3b8"))))
