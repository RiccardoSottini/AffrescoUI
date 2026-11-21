;;; game-panels.lisp --- two side panels of a game's page: your screenshots, and the controller
;;;
;;; Your screenshots: the ones you took of the game, newest first, F12 taking one while it runs; each opens big in a
;;; viewer that steps through them and deletes one. The controller: how well the game supports a pad, the layout
;;; chosen for it (Arcadia's default, southpaw or classic) and what each control does in that layout.

(ns arcadia.parts.game-panels)

;;; What the panels keep

(defstate shots
  [{:id 1 :game starfall :file shot2}
   {:id 2 :game starfall :file hero}
   {:id 3 :game starfall :file shot1}
   {:id 4 :game ember :file shot3}
   {:id 5 :game neon :file shot1}])

(defstate shotView {:open false :game nil :at 0})

(defstate padLayouts {})

(defstate padBindings
  {:default [[pad.move pad.leftStick] [pad.look pad.rightStick] [pad.jump pad.bottom] [pad.interact pad.left]]
   :southpaw [[pad.move pad.rightStick] [pad.look pad.leftStick] [pad.jump pad.bottom] [pad.interact pad.left]]
   :classic [[pad.move pad.leftStick] [pad.look pad.rightStick] [pad.jump pad.right] [pad.interact pad.top]]})

(defcomputed myShots "The screenshots of the game on the page, newest first."
  (reverse (filter #(= (:game %) computed.game.id) state.shots)))

(defcomputed viewShots "The screenshots of the game the viewer shows, newest first."
  (reverse (filter #(= (:game %) state.shotView.game) state.shots)))

(defcomputed viewShot "The screenshot the viewer shows." (get computed.viewShots state.shotView.at))

(defcomputed padLayout "The controller layout chosen for the game on the page: Arcadia's default until one is chosen."
  (or-else (get state.padLayouts computed.game.id) "default"))

(defkey "f12" takeScreenshot)

;;; Your screenshots

(defview your-shots-card "Your screenshots of the game: how many, the three newest, each opening the viewer." []
  `(node#screenshots-card :use sideCard :with {:title ~(t "shots.title") :height 150}
     (label :font small
       :anchors {:left 14 :right 14 :top 38}
       :height 16
       :text ~(t "shots.count" {:n (count computed.myShots)})
       :textColor @color.muted)
     (label#shots-none :when ~(empty? computed.myShots)
       :font small
       :wrap true
       :anchors {:left 14 :right 14 :top 62}
       :height 64
       :text ~(t "shots.none")
       :textColor @color.dim)
     (row :when ~(pos? (count computed.myShots)) :gap 6 :anchors {:left 14 :right 14 :top 62} :height 74
       ~(repeat [s (slice computed.myShots 0 3) :index k]
          `(image :id ~(str "my-shot-" k)
             :file ~(str "../assets/art/" (or-else g.art g.id) "/" s.file ".svg")
             :width 90
             :height 74
             :fit cover
             :radius 3
             :focusable true
             :role button
             :cursor hand
             :label ~(t "shots.open")
             :hover {:borderWidth 2 :borderColor @color.accent}
             :onClick ~(handler openShot g.id k))))))

(defcomponent shotViewer
  "A screenshot of yours, big: which of how many and when it was taken, arrows to the others, Delete and Close."
  {}
  `(node :use dialog
     :with {:open state.shotView.open
            :width 860
            :height 600
            :title ~(t "shots.viewerTitle" {:game (or-else (:title (find #(= (:id %) state.shotView.game) data.games)) "")})
            :message ~(t "shots.of" {:n (inc state.shotView.at) :total (count computed.viewShots)})
            :buttons [{:title ~(t "shots.delete") :run deleteShot} {:title ~(t "dialog.close") :default true}]}
     (node :when ~(some? computed.viewShot) :anchors {:left 20 :right 20 :top 76} :height 448
       (image#shot-viewer-image :fill true
         :file ~(str "../assets/art/" state.shotView.game "/" computed.viewShot.file ".svg")
         :fit cover
         :radius 4
         :label ~(t "shots.open"))
       ~(shot-arrow "prev" "chevron-left" {:left 12 :top 204})
       ~(shot-arrow "next" "chevron-right" {:right 12 :top 204}))))

(defview shot-arrow "An arrow over the viewer's picture that shows the screenshot before or after, round the ends."
  [dir icon place]
  `(label :id ~(str "shot-viewer-" dir)
     :when ~(> (count computed.viewShots) 1)
     :icon ~icon
     :text ""
     :font heading
     :width 40
     :height 40
     :radius 20
     :align center
     :anchors ~place
     :color "#00000080"
     :textColor "#ffffff"
     :focusable true
     :role button
     :label ~(t (str "hub.shot." dir))
     :cursor hand
     :hover {:color "#000000c0"}
     :onClick ~(handler step-shot (if (= dir "prev") -1 1))))

;;; The controller

(defview controller-card "How well the game supports a pad; with one, the layout chosen and what each control does." []
  `(node#controller-card :use sideCard
     :with {:title ~(t "game.controller") :height ~(if (= g.controller "none") 74 204)}
     (label#controller-support :icon gamepad-2
       :text ~(t (str "controller." g.controller))
       :textColor @color.text
       :anchors {:left 14 :right 14 :top 42}
       :height 20)
     (column :when ~(not= g.controller "none") :anchors {:left 14 :right 14 :top 72} :height 120 :gap 8
       (row#pad-layouts :height 26 :anchors {:left 0 :right 0} :gap 4 :role tablist :arrows horizontal
         ~(repeat [l ["default" "southpaw" "classic"]]
            `(label :id ~(str "pad-layout-" l)
               :font small
               :fitText true
               :padding 9
               :height 26
               :radius 13
               :role tab
               :focusable true
               :cursor hand
               :selected ~(= computed.padLayout l)
               :text ~(t (str "pad.layout." l))
               :color ~(if (= computed.padLayout l) "@color.select" "@color.raised")
               :textColor ~(if (= computed.padLayout l) "@color.bright" "@color.text")
               :hover {:textColor @color.bright}
               :onClick ~(handler setPadLayout g.id l))))
       (column#pad-bindings :height 84 :anchors {:left 0 :right 0} :gap 2 :role list
         ~(repeat [b (get state.padBindings computed.padLayout)]
            `(row :height 19 :anchors {:left 0 :right 0} :role listitem
               (label :font small :grow 1 :height 19 :text ~(t (first b)) :textColor @color.muted)
               (label :font small :fitText true :height 19 :text ~(t (get b 1)) :textColor @color.text)))))))
