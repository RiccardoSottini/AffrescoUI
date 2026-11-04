;;; splash.lisp --- the start screen: Calamaio's mark, places to begin as cards, first actions as buttons
;;;
;;; It is the buffer *Calamaio* (b0), of its own kind, which a window draws with this component instead of an
;;; editor. Every card, button and link runs a command; the checkbox is the variable inhibit-startup-screen.

;;; What it offers

(def splash-links "The cards: a place to begin, the command it runs and its key."
  [{:name "tutorial" :run "help-with-tutorial" :key "ctrl+h t"}
   {:name "tour" :run "show-tour"}
   {:name "commands" :run "execute-extended-command" :key "alt+x"}
   {:name "messages" :run "show-messages" :key "ctrl+h e"}])

(def splash-actions "The buttons under the cards: the first things to do, and their icons."
  [{:name "open" :icon "folder-open" :run "find-file"}
   {:name "home" :icon "folder" :run "dired-home"}
   {:name "startup" :icon "settings" :run "customize-startup"}])

;;; The screen

(defcomponent splash
  "The start screen: Calamaio's mark, places to begin as cards, first actions as buttons; every link runs a command."
  {:w w1}
  `(column :fill 0 :alignItems center
     (node :grow 2 :width 1 :color transparent)
     ~@(splash-mark)
     (node :height 24 :color transparent)
     (grid :columns 2 :gap 10 :width 560 :height ~(card-rows-height (count splash-links)) :color transparent
       ~@(map splash-link splash-links))
     (node :height 18 :color transparent)
     (row :gap 8 :fitContent true :color transparent
       ~@(map splash-action splash-actions))
     (node :height 18 :color transparent)
     ~(show-at-start)
     (node :grow 3 :width 1 :color transparent)
     ~(splash-footer)
     (node :height 14)))

(defn card-rows-height "How tall the cards are, two a row, 60 high and 10 apart." [n]
  (- (* 70 (/ (+ n (mod n 2)) 2)) 10))

(defview splash-mark "Calamaio's mark on its tile, its name and what it is, one under the other." []
  [`(node :width 84 :height 84 :radius 24 :color @tile
      (label :font logo :icon feather :align center :fill 0 :textColor @logo))
   `(node :height 14 :color transparent)
   `(label :fitText true :runs [{:text "Calamaio" :size 32 :bold true :color @fg}])
   `(node :height 4 :color transparent)
   `(label :fitText true :textColor @kit.dim :text ~(t "splash.about"))])

(defview splash-link "A card: where it goes, in the link's colour, what it is, and its key in the corner." [link]
  (let [title (str "splash." (:name link))]
    `(node :height 60 :radius 8 :color @kit.surface :borderWidth 1 :borderColor @barLine
       :focusable true :role button :label ~(t title) :cursor hand
       :hover {:borderColor @link} :focused {:borderColor @link}
       :onClick ~(:run link)
       (label :anchors {:left 14 :top 10} :fitText true :runs [{:text ~(t title) :bold true :color @link}])
       (label :anchors {:left 14 :top 32} :fitText true :textColor @kit.dim :text ~(t (str title ".about")))
       ~@(when (:key link)
           [`(label :font code :anchors {:right 12 :top 10} :fitText true :padding 4 :radius 4 :color @fringe
               :textColor @kit.dim :text ~(keyLabel (:key link) "letters"))]))))

(defview splash-action "A button: its icon and what it does." [action]
  (let [title (str "splash." (:name action))]
    `(label :fitText true :padding 9 :radius 16 :color @button :textColor @barFg
       :focusable true :role button :label ~(t title) :cursor hand
       :hover {:color @barHover} :focused {:color @barHover}
       :onClick ~(:run action)
       :runs [{:icon ~(:icon action) :font icons :size 15} " " ~(t title)])))

(defview show-at-start "The checkbox that keeps Calamaio from opening on the start screen, and its label." []
  `(row :gap 8 :alignItems center :fitContent true :color transparent
     (checkbox#splashAtStart :width 16 :height 16
       :checked ~(not (get settings "inhibit-startup-screen"))
       :onChange show-splash-at-start
       :label ~(t "splash.atStart")
       :activeColor @link)
     (label :fitText true :textColor @fg :text ~(t "splash.atStart"))))

(defview splash-footer "What Calamaio is, under which licence, and a link that closes the start screen." []
  `(label :fitText true :textColor @kit.dim
     :runs [~(t "splash.footer") {:text ~(t "splash.close") :link splash-close :color @link}]
     :onLink run-link))

;;; What it runs

(defn show-splash-at-start "The checkbox: whether Calamaio opens on the start screen." [event]
  (set "settings.inhibit-startup-screen" (not event.value)))

(defn run-link "Runs the command a link of the start screen names." [event]
  (run event.link))

(defcommand show-splash "Shows the start screen again."
  :title ~(t "command.show-splash") :category ~(t "category.help")
  :description ~(t "command.show-splash.about")
  []
  (when (nil? state.bufs.b0)
    (set "state.bufs.b0" {:name "*Calamaio*" :mode "fundamental" :kind "splash" :text "" :readOnly true}))
  (show "b0"))

(defcommand splash-close "Closes the start screen and shows the scratch buffer."
  :title ~(t "command.splash-close") :category ~(t "category.help")
  :description ~(t "command.splash-close.about")
  []
  (when (some? state.bufs.b0) (kill-buffer-id "b0")))

(defn toggle-splash "Whether Calamaio starts with the start screen or in the scratch buffer: the other way round." []
  (toggle "settings.inhibit-startup-screen")
  (message (if settings.inhibit-startup-screen (t "echo.startInScratch") (t "echo.startOnSplash"))))
