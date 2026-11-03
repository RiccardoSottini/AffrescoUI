;;; plan.lisp --- Haven's floor plan and media: the rooms on a canvas, the picked room's lights, and the speaker
;;;
;;; Drag a room on the canvas to move it, click it to pick it: the other pages then show that room. Beside the
;;; canvas, the picked room's lights switch on and off, and the speaker's player plays, skips and sets the volume.
;;; The dialog that colours a light is here too.

;;; What the page keeps

(defstate planView {:camera {:x -40 :y -40 :zoom 1} :selection [] :guides {:vertical [] :horizontal []}}
  "The canvas's camera, the rooms chosen on it, and its guides.")

(defstate planRooms
  [{:id r0 :x 0 :y 0 :width 300 :height 200}
   {:id r1 :x 300 :y 0 :width 180 :height 200}
   {:id r2 :x 0 :y 260 :width 240 :height 180}
   {:id r3 :x 240 :y 260 :width 240 :height 180}]
  "Where each room is on the canvas, by the id the rooms tree names it with.")

(defstate light 0 "The light the colour dialog colours, by its place in the room.")

(defstate colourOpen false "Whether the colour dialog is open.")

(defstate media {:track 0 :volume 40} "The speaker's track, by its place, and its volume.")

(defstate tracks
  [{:title "Paper Lanterns" :artist "The Quiet Rooms"}
   {:title "Slow Tide" :artist "Marisol Grey"}
   {:title "Window Seat" :artist "Northbound Trio"}
   {:title "Copper Kettle" :artist "The Quiet Rooms"}]
  "The speaker's tracks.")

;;; What the page works out

(defmacro plan-room-place "The place in rooms of a room of the canvas." [room]
  `(indexOf ["r0" "r1" "r2" "r3"] (:id ~room)))

(defcomputed planItems "The rooms as the canvas draws them: named, and outlined in the accent while picked."
  (map (fn [it]
         (merge it
                {:color (if (= computed.themeNow "dark") "#1a2340" "#ffffff")
                 :borderColor (cond (= (plan-room-place it) state.room)
                                    (if (= computed.themeNow "dark") "#22d3ee" "#0891b2")
                                    (= computed.themeNow "dark") "#3a4363"
                                    :else "#94a3b8")
                 :borderWidth (if (= (plan-room-place it) state.room) 3 2)
                 :radius 4
                 :text (get-in state.rooms [(plan-room-place it) :name])
                 :textSize 16
                 :textColor (if (= computed.themeNow "dark") "#e5e9f5" "#0f172a")}))
       state.planRooms))

(defcomputed track "The speaker's track." (get state.tracks state.media.track))

(defcomputed speaker "The speaker, as a device." (find (fn [it] (= it.id "speaker")) state.devices))

;;; The page

(defcomponent planPage "The floor plan: the rooms on a canvas, and beside it the picked room and the speaker."
  {}
  `(node :color transparent
     {:id planTitle :style pageTitle :icon map :text ~(str " " (t "page.plan"))}
     {:id planSummary :style caption :text ~(t "plan.summary") :anchors {:left 32 :top 66}}
     (node#planCard :style card :anchors {:left 32 :right 336 :top 104 :bottom 24}
       (canvas#plan :fill 8
         :bind state.planView
         :focusable true
         :rulers false
         :grid 20
         :color @field
         :accentColor @accent
         :label ~(t "plan.label")
         :items ~computed.planItems
         :onTransform ~(handler moveRooms event.items)
         :onChange ~(handler planPick event.value)))
     ~(picked-room)
     (node#mediaCard :use mediaCard :anchors {:right 32 :bottom 24} :width 288 :height 196)))

(defview picked-room "The picked room: its name, its climate, its lights, and a button that shows it on the home page."
  []
  `(node#planRoomCard :style card
     :anchors {:right 32 :top 104 :bottom 236}
     :width 288
     :role group
     :label ~computed.room.name
     {:id planRoomName :style cardTitle :text ~computed.room.name}
     {:id planRoomSummary
      :style caption
      :anchors {:left 24 :top 46}
      :text ~(t "plan.room" {:temp computed.room.temp :lights computed.lightsOn})}
     (stack#planLights :anchors {:left 16 :right 16 :top 76 :bottom 60} :gap 4 :scroll vertical :role list
       ~(repeat [light computed.room.lights :index i]
         (light-switch-row light i)))
     {:id planShow
      :style button
      :text ~(t "plan.show")
      :anchors {:left 16 :right 16 :bottom 16}
      :height 32
      :onClick goHome}))

(defview light-switch-row "A light of the picked room, warm while it is on; a click switches it." [light i]
  `(label#planLight :icon ~(if light.on "lightbulb" "lightbulb-off")
     :text ~(str "  " light.name)
     :anchors {:left 0 :right 0}
     :height 32
     :padding 10
     :radius 6
     :align {:horizontal start :vertical center}
     :color ~(if light.on "@warmSoft" "@raised")
     :hover {:color ~(if light.on "@warmSoft" "@raisedHover")}
     :textColor ~(if light.on "@text" "@muted")
     :cursor hand
     :focusable true
     :role switch
     :checked ~light.on
     :label ~light.name
     :onClick ~(handler switchLight i)))

;;; The speaker

(defcomponent mediaCard "The speaker: the track, play and pause (a device call), previous and next, and the volume."
  {}
  `(node :style card :role group :label ~(t "media.title")
     {:id mediaTitle :style cardTitle :icon speaker :text ~(str " " (t "media.title"))}
     {:id mediaState
      :style caption
      :anchors {:right 24 :top 24}
      :text ~(if computed.speaker.busy
                 (t "device.waiting")
                 (t (str "device.speaker." (if computed.speaker.on "on" "off"))))
      :textColor ~(if computed.speaker.on "@accent" "@muted")}
     (label#mediaTrack :text ~computed.track.title
       :textColor @text
       :anchors {:left 24 :right 24 :top 54}
       :height 20
       :align {:horizontal start :vertical center}
       :live polite)
     {:id mediaArtist :style caption :text ~computed.track.artist :anchors {:left 24 :top 76}}
     ~(media-buttons)
     (label#mediaVolumeIcon :icon volume-2 :text "" :textColor @muted :anchors {:left 24 :top 152} :width 20 :height 20)
     (slider#mediaVolume :anchors {:left 56 :right 24 :top 152}
       :height 20
       :step 5
       :bind state.media.volume
       :label ~(t "media.volume")
       :tooltip ~(t "media.volume")
       :trackColor @off
       :activeColor @accent
       :knobColor @knob)))

(defview media-buttons "Previous, play or pause, and next." []
  `(row#mediaButtons :anchors {:left 24 :right 24 :top 100} :height 36 :gap 8 :role toolbar :arrows horizontal
     {:id mediaPrevious
      :style button
      :icon skip-back
      :text ""
      :grow 1
      :anchors {:top 0 :bottom 0}
      :label ~(t "media.previous")
      :tooltip ~(t "media.previous")
      :onClick previousTrack}
     {:id mediaPlay
      :style choiceOn
      :icon ~(if computed.speaker.on "pause" "play")
      :text ""
      :grow 1
      :anchors {:top 0 :bottom 0}
      :label ~(if computed.speaker.on (t "media.pause") (t "media.play"))
      :tooltip ~(if computed.speaker.on (t "media.pause") (t "media.play"))
      :onClick playPause}
     {:id mediaNext
      :style button
      :icon skip-forward
      :text ""
      :grow 1
      :anchors {:top 0 :bottom 0}
      :label ~(t "media.next")
      :tooltip ~(t "media.next")
      :onClick nextTrack}))

;;; The colour of a light

(defcomponent colourDialog "A light's colour, in the middle of the window; Escape, a click outside or Done closes it."
  {}
  `(node :dir auto
     :overlay {:placement center :open state.colourOpen :dismiss outside :modal true}
     :width 280
     :height 440
     :radius 12
     :color @card
     :shadow {:y 8 :blur 24 :color "#00000040"}
     :role dialog
     :label ~(t "colour.title")
     (label#colourTitle :font heading
       :textColor @text
       :anchors {:left 20 :top 16}
       :fitText true
       :text ~(or-else (get-in computed.room.lights [state.light :name]) (t "colour.title")))
     (colorPicker#colourPicker :anchors {:left 20 :right 20 :top 48}
       :height 320
       :focusable true
       :label ~(t "colour.title")
       :bind "state.rooms[state.room].lights[state.light].color"
       :textColor @text
       :accentColor @accent
       :swatches ["#ffd58aff" "#fff4e0ff" "#ffc078ff" "#8ab4ffff" "#ff9ad5ff" "#9af5c0ff"])
     {:id colourWarm
      :style button
      :text ~(t "colour.warm")
      :anchors {:left 20 :bottom 20}
      :width 116
      :height 32
      :onClick ~(handler warmLight state.light)}
     {:id colourDone
      :style choiceOn
      :text ~(t "colour.done")
      :anchors {:right 20 :bottom 20}
      :width 100
      :height 32
      :onClick {:set state.colourOpen :to false}}))
