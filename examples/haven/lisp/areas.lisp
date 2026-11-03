;;; areas.lisp --- Haven's rooms page: the rooms as picture cards, grouped by floor
;;;
;;; Each card shows the room's climate and how many of its lights are on; a click opens the room on the home page.

;;; What the page works out

(defcomputed areaFloors "The floors, and the rooms on each, by their place."
  [{:id "ground" :rooms [0 1]} {:id "upper" :rooms [2 3]}])

(defcomputed areaHeight "A card's height." (if computed.compact 176 212))

;;; The page

(defcomponent areasPage "The rooms page: its bar, the floors, and the button that adds a room (not there yet)."
  {}
  `(node :dir auto :color transparent
     (node#areasBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
       (label#areasTitle :text ~(t "page.areas")
         :font heading
         :textColor @onHeader
         :anchors {:left 24 :top 0 :bottom 0}
         :fitText true
         :align {:horizontal start :vertical center}))
     ~(floor-list)
     ~(add-room-button)))

(defview room-card "A room: its picture, name and climate, and a bulb while a light is on; it opens the room." [r]
  `(node#areaTile :let {:room ~(get state.rooms r)
                        :lit ~(count (filter (fn [it] it.on) (get-in state.rooms [r :lights])))}
     :style card
     :anchors {:top 0 :bottom 0}
     :grow 1
     :basis 0
     :hover {:color @cardHover}
     :cursor hand
     :focusable true
     :role button
     :label ~room.name
     :onClick ~(handler openRoom (str "r" r))
     (image :file ~room.image :fit cover :radius 12 :anchors {:left 0 :right 0 :top 0 :bottom 56})
     (label :text ~room.name
       :font heading
       :textColor @text
       :anchors {:left 16 :top ~(- computed.areaHeight 50)}
       :fitText true)
     (label :font small
       :textColor @muted
       :icon thermometer
       :text ~(str " " room.temp "°  ·  " room.humidity "%  ·  "
                   (t "summary.lights" {:on lit :total (count room.lights)}))
       :anchors {:left 16 :top ~(- computed.areaHeight 24)}
       :fitText true)
     (label :icon lightbulb
       :text ""
       :when ~(> lit 0)
       :textColor @warm
       :color @card
       :anchors {:right 12 :top 12}
       :width 32
       :height 32
       :radius 16
       :align center)))

(defview floor-list "The floors, each with its name and its rooms side by side." []
  `(column#areaList :anchors {:left 24 :right 24 :top 56 :bottom 0} :gap 0 :scroll vertical
     ~(repeat [f computed.areaFloors :key f.id]
       `(node#areaFloor :anchors {:left 0 :right 0}
          :height ~(+ computed.areaHeight 56)
          :color transparent
          :role group
          :label ~(t (str "floor." f.id))
          (label :icon layers-2
            :text ~(str "  " (t (str "floor." f.id)))
            :font heading
            :textColor @text
            :anchors {:left 0 :top 16}
            :fitText true)
          (row :anchors {:left 0 :right 0 :top 48} :height ~computed.areaHeight :gap 16 :role list
            ~(repeat [r f.rooms]
              (room-card r)))))))

(defview add-room-button "The floating button that adds a room, not there yet." []
  `{:id areasAdd
    :style button
    :icon plus
    :text ~(str " " (t "areas.add"))
    :color @raised
    :hover {:color @raised}
    :textColor @muted
    :anchors {:right 32 :bottom 32}
    :width 180
    :height 52
    :radius 16
    :shadow {:y 4 :blur 12 :color "#00000040"}
    :role button
    :label ~(t "areas.add")
    :tooltip ~(t "areas.addSoon")
    :cursor arrow
    :disabled true})
