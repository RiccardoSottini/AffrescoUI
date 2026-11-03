;;; moreinfo.lisp --- Haven's more-info dialog: one light, the climate, the blinds, the alarm or one device
;;;
;;; A click on a tile opens it in the middle of the window, with the big control of that kind, its details and its
;;; recent activity. Escape or a click outside closes it. The round buttons, the tall bar and the dial it draws with
;;; are here too, as the home page uses some of them.

;;; What the dialog keeps

(defstate info {:open false :kind light :index 0} "Whether it is open, the kind it shows, and which light or device.")

(defstate tempHistory
  [20.1 19.8 19.6 19.4 19.3 19.2 19.4 20.2 21.0 21.4 21.6 21.5
   21.7 21.8 21.6 21.5 21.6 21.9 22.1 22.0 21.8 21.6 21.4 21.5]
  "The room's temperature over the last day, hour by hour.")

(defstate coverTilt 50 "The blinds' tilt, from 0 to 100.")

(defstate speakerVolume 40 "The speaker's volume, from 0 to 100.")

(defstate infoMenu false "Whether the dialog's menu is open.")

(defstate modeMenu false "Whether the climate's menu of modes is open.")

(defstate deviceFacts
  {:door {:related welcome :battery 82 :changed "08:12" :log ["08:12" "07:40" "22:05"]}
   :vacuum {:related morning :battery 64 :changed "10:30" :log ["10:30" "09:55" "18:20"]}
   :speaker {:related welcome :battery 100 :changed "09:02" :log ["09:02" "08:47" "21:30"]}}
  "Each device's battery, its last change, the automation it is part of, and its last changes.")

;;; What it shows

(defcomputed infoLight "The light shown." (get computed.room.lights state.info.index))

(defcomputed infoDevice "The device shown." (get state.devices state.info.index))

(defcomputed infoFacts "The facts of the device shown." (get state.deviceFacts computed.infoDevice.id))

(defcomputed infoName "The name of what is shown."
  (cond (= state.info.kind "light") computed.infoLight.name
        (= state.info.kind "device") computed.infoDevice.name
        :else (t (str "info." state.info.kind))))

(defcomputed infoState "The state of what is shown, in big letters."
  (cond (= state.info.kind "alarm") (t (str "alarm." state.security))
        (= state.info.kind "light") (if computed.infoLight.on (str computed.infoLight.level "%") (t "light.off"))
        (= state.info.kind "climate") (str computed.room.target "°")
        (= state.info.kind "cover") (t "blinds.open" {:n (round computed.room.blinds)})
        computed.infoDevice.busy (t "device.waiting")
        :else (t (str "device." computed.infoDevice.id (if computed.infoDevice.on ".on" ".off")))))

;;; The dialog

(defcomponent moreInfo "The details of one light, the climate, the blinds, the alarm or one device."
  {}
  `(node :dir auto
     :overlay {:placement center :open state.info.open :dismiss outside :modal true}
     :width 400
     :height ~(cond (= state.info.kind "device") 600 (= state.info.kind "climate") 536 :else 500)
     :radius 28
     :color @card
     :shadow {:y 8 :blur 24 :color "#00000040"}
     :role dialog
     :label ~computed.infoName
     (label#infoState :when ~(not= state.info.kind "climate")
       :font title
       :textColor @text
       :text ~computed.infoState
       :anchors {:left 0 :right 0 :top 76}
       :height 36
       :align center
       :live polite)
     ~(light-details)
     ~(climate-details)
     ~(blinds-details)
     (node#infoAlarmBody :use infoBody :with {:kind alarm}
       (node#alarmKeypad :use alarmKeypad))
     ~(device-details)
     ~@(dialog-title)
     ~(dialog-button "infoToHistory" "chart-line" "page.history" 106 "history")
     ~(dialog-button "infoToSettings" "settings" "page.settings" 58 "settings")
     ~(dialog-menu)))

(defview dialog-title "The dialog's close button, the room or the home, and the name of what it shows." []
  [`(node#infoClose :use iconBtn
      :with {:icon x :tip info.close}
      :font icons
      :color transparent
      :anchors {:left 10 :top 10}
      :onClick {:set state.info.open :to false})
   `{:id infoRoom
     :style caption
     :text ~(if (or (= state.info.kind "device") (= state.info.kind "alarm")) (t "info.home") computed.room.name)
     :anchors {:left 64 :top 12}}
   `(label#infoName :font heading
      :textColor @text
      :text ~computed.infoName
      :anchors {:left 64 :right 156 :top 30}
      :height 22)])

(defview dialog-button "A round button of the dialog's corner that closes it and goes to a page."
  [id icon tip right page]
  `(node :id ~id :use iconBtn
     :with {:icon ~icon :tip ~tip}
     :font icons
     :color transparent
     :anchors {:right ~right :top 10}
     :onClick ~(handler closeInfoFor page)))

(defview light-details "A light: its brightness in a tall bar, and buttons that switch it, colour it or warm it." []
  `(node#infoLightBody :use infoBody :with {:kind light}
     (node :use bigBar
       :with {:share ~(if computed.infoLight.on (/ computed.infoLight.level 100) 0)
              :fill ~(slice computed.infoLight.color 0 7)
              :label ~(str (t "light.brightness")
                           ": "
                           (if computed.infoLight.on (str computed.infoLight.level "%") (t "light.off")))
              :pick pickLevel
              :up levelUp
              :down levelDown})
     (node#infoLightButtons :use infoRow
       (node#infoPower :use iconBtn
         :with {:icon power :tip info.power}
         :selected ~computed.infoLight.on
         :color ~(if computed.infoLight.on "@warmSoft" "@raised")
         :textColor ~(if computed.infoLight.on "@warm" "@text")
         :onClick ~(handler switchLight state.info.index))
       (node#infoColour :use iconBtn
         :with {:icon palette :tip colour.title}
         :onClick ~(handler colourFromInfo state.info.index))
       (node#infoWarm :use iconBtn
         :with {:icon thermometer-sun :tip colour.warm}
         :onClick ~(handler warmLight state.info.index)))
     {:id infoLightHint
      :style caption
      :text ~(t "info.lightHint")
      :anchors {:left 0 :right 0 :top 460}
      :height 16
      :align center
      :fitText false}))

(defview mode-field "The climate's mode, a field that opens the menu of modes." []
  `(node#infoModeRow :anchors {:left 24 :right 24 :top 276}
     :height 52
     :radius {:topLeft 4 :topRight 4}
     :color @raised
     :hover {:color @raisedHover}
     :cursor hand
     :focusable true
     :role combobox
     :label ~(t "climate.mode")
     :isOpen ~state.modeMenu
     :onClick {:toggle state.modeMenu}
     (label :icon ~(cond (= state.mode "Heat") "flame" (= state.mode "Cool") "snowflake" :else "sun-snow")
       :text ""
       :textColor ~computed.modeColor
       :anchors {:left 8 :top 10}
       :width 32
       :height 32
       :align center)
     {:style caption :text ~(t "climate.mode") :anchors {:left 48 :top 8}}
     (label#infoModeValue :text ~(t (str "mode." state.mode)) :textColor @text :anchors {:left 48 :top 24}
       :fitText true)
     (label :icon chevron-down :text "" :textColor @muted :anchors {:right 8 :top 14} :width 24 :height 24
       :align center)
     {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted}
     ~(mode-menu)))

(defview mode-menu "The menu of the heating modes." []
  `(node#modeMenu :use menu
     :with {:open state.modeMenu
            :placement below
            :width 352
            :items [{:title ~(t "mode.Heat") :checked ~(= state.mode "Heat") :run pickMode :with {:mode "Heat"}}
                    {:title ~(t "mode.Cool") :checked ~(= state.mode "Cool") :run pickMode :with {:mode "Cool"}}
                    {:title ~(t "mode.Auto") :checked ~(= state.mode "Auto") :run pickMode :with {:mode "Auto"}}]}))

(defview climate-details "The climate: the dial and its steps, the mode, and the temperature over the last day." []
  `(node#infoClimateBody :use infoBody :with {:kind climate}
     (node#infoDial :use thermoDial :with {:p info-} :anchors {:left 0 :right 0 :top 64})
     (node#infoDown :use iconBtn
       :with {:icon minus :tip climate.down}
       :anchors {:left 146 :top 212}
       :onClick ~(handler setTarget -0.5))
     (node#infoUp :use iconBtn
       :with {:icon plus :tip climate.up}
       :anchors {:right 146 :top 212}
       :onClick ~(handler setTarget 0.5))
     ~(mode-field)
     (label#infoHistoryTitle :textColor @text
       :icon history
       :text ~(t "info.history")
       :anchors {:left 24 :top 348}
       :fitText true)
     (node#infoHistory :style card
       :anchors {:left 24 :right 24 :top 376}
       :height 136
       :color @field
       :borderWidth 0
       :role img
       :label ~(t "info.historyLabel" {:low (min state.tempHistory) :high (max state.tempHistory)})
       (node :use lineChart
         :with {:values ~(map (fn [it] (- it 19)) state.tempHistory) :top 4 :color ~computed.modeColor :lineWidth 2}
         :anchors {:left 48 :right 12 :top 16 :bottom 28})
       {:id infoHistoryHigh :style caption :text "23°" :anchors {:left 12 :top 12}}
       {:id infoHistoryLow :style caption :text "19°" :anchors {:left 12 :bottom 28}}
       {:id infoHistoryStart :style caption :text "00:00" :anchors {:left 48 :bottom 8}}
       {:id infoHistoryEnd :style caption :text ~(t "info.now") :anchors {:right 12 :bottom 8}})))

(defview blinds-details "The blinds: how open they are and their tilt in tall bars, and open, stop and close." []
  `(node#infoCoverBody :use infoBody :with {:kind cover}
     (node :use bigBar
       :with {:share ~(/ computed.room.blinds 100)
              :fill @cover
              :label ~(str (t "blinds.title") ": " (t "blinds.open" {:n (round computed.room.blinds)}))
              :pick pickBlinds
              :up blindsUp
              :down blindsDown
              :left 96})
     (node#infoTilt :use bigBar
       :with {:share ~(/ state.coverTilt 100)
              :fill @cover
              :label ~(t "info.tilt" {:n state.coverTilt})
              :pick pickTilt
              :up tiltUp
              :down tiltDown
              :left 232
              :width 72})
     (node#infoCoverButtons :use infoRow
       (node#infoOpen :use iconBtn
         :with {:icon chevron-up :tip blinds.openButton}
         :onClick ~(handler moveBlinds 100))
       (node#infoStop :use iconBtn
         :with {:icon square :tip info.stop}
         :onClick stopBlinds)
       (node#infoShut :use iconBtn
         :with {:icon chevron-down :tip blinds.close}
         :onClick ~(handler moveBlinds 0)))
     {:id infoCoverHint
      :style caption
      :text ~(t "info.coverHint")
      :anchors {:left 0 :right 0 :top 460}
      :height 16
      :align center
      :fitText false}))

(defview speaker-transport "The speaker's previous (no queue here), play or pause, and next." []
  `(row#infoTransport :when ~(= computed.infoDevice.id "speaker")
     :anchors {:left 0 :right 0 :top 198}
     :height 44
     :gap 16
     :justify center
     :role toolbar
     :arrows horizontal
     :label ~computed.infoName
     (node :use iconBtn
       :with {:icon skip-back :tip info.previous}
       :tooltip ~(t "info.queueEmpty")
       :color transparent
       :hover {:color transparent}
       :textColor @off
       :cursor arrow
       :disabled true)
     (node#infoPlay :use iconBtn
       :with {:icon ~(if computed.infoDevice.on "pause" "play")
              :tip ~(if computed.infoDevice.on "info.pause" "info.play")}
       :color @accentSoft
       :textColor @accent
       :onClick ~(handler deviceCall state.info.index))
     (node :use iconBtn
       :with {:icon skip-forward :tip info.next}
       :tooltip ~(t "info.queueEmpty")
       :color transparent
       :hover {:color transparent}
       :textColor @off
       :cursor arrow
       :disabled true)))

(defview speaker-volume "The speaker's volume: a slider with no look of its own, over the bar that draws it." []
  `(node#infoVolume :when ~(= computed.infoDevice.id "speaker")
     :anchors {:left 24 :right 24 :top 248}
     :height 40
     :color transparent
     (label :icon volume-2 :text "" :textColor @muted :anchors {:left 0 :top 8} :width 24 :height 24 :align center)
     (node :use levelBar
       :with {:level ~(/ state.speakerVolume 100) :fill @accent :back @accentSoft}
       :anchors {:left 40 :right 0 :top 0})
     (slider#infoVolumeSlider :anchors {:left 40 :right 0 :top 0}
       :height 40
       :min 0
       :max 100
       :step 5
       :cursor hand
       :bind state.speakerVolume
       :label ~(t "info.volume")
       :tooltip ~(t "info.volume")
       :trackColor transparent
       :activeColor transparent
       :knobColor transparent)))

(defview device-facts "The device's battery, its last change, and the automation it is part of." []
  `(stack#infoFacts :anchors {:left 24 :right 24 :top ~(if (= computed.infoDevice.id "speaker") 292 276)}
     :height 120
     :gap 0
     :role list
     (label#infoBattery :icon battery-medium
       :text ~(t "info.battery" {:n computed.infoFacts.battery})
       :textColor @text
       :role listitem
       :anchors {:left 0 :right 0}
       :height 40
       :align {:horizontal start :vertical center})
     (label#infoChanged :icon history
       :text ~(t "info.changed" {:time computed.infoFacts.changed})
       :textColor @text
       :role listitem
       :anchors {:left 0 :right 0}
       :height 40
       :align {:horizontal start :vertical center})
     (label#infoRelated :icon workflow
       :text ~(t "info.related" {:name (t (str "rule." computed.infoFacts.related))})
       :textColor @accent
       :anchors {:left 0 :right 0}
       :height 40
       :align {:horizontal start :vertical center}
       :cursor hand
       :focusable true
       :role link
       :label ~(t "info.related" {:name (t (str "rule." computed.infoFacts.related))})
       :onClick openRelated)))

(defview device-details "A device: its icon, its controls, its facts and its last changes." []
  `(node#infoDeviceBody :use infoBody :with {:kind device}
     (label#infoDeviceIcon :font title
       :icon ~(if computed.infoDevice.on computed.infoDevice.icon computed.infoDevice.iconOff)
       :text ""
       :anchors {:left 164 :top ~(if (= computed.infoDevice.id "speaker") 120 128)}
       :width 72
       :height 72
       :radius 36
       :align center
       :textColor ~(if computed.infoDevice.on "@accent" "@muted")
       :color ~(if computed.infoDevice.on "@accentSoft" "@raised"))
     ~(speaker-transport)
     ~(speaker-volume)
     {:id infoDeviceAction
      :when ~(not= computed.infoDevice.id "speaker")
      :style choiceOn
      :anchors {:left 120 :top 216}
      :width 160
      :height 36
      :radius 18
      :text ~(t (str "device." computed.infoDevice.id (if computed.infoDevice.on ".doOff" ".doOn")))
      :onClick ~(handler deviceCall state.info.index)}
     ~(device-facts)
     (label#infoLogTitle :textColor @text
       :icon activity
       :text ~(t "info.activity")
       :anchors {:left 24 :top ~(if (= computed.infoDevice.id "speaker") 416 412)}
       :fitText true)
     (stack#infoLog :anchors {:left 24 :right 24 :top ~(if (= computed.infoDevice.id "speaker") 446 444)}
       :height 132
       :gap 4
       :role list
       ~(repeat [time computed.infoFacts.log :index n]
         (device-change time n)))))

(defview device-change "A change of the device: when, and what it became." [time n]
  `(node#infoLogRow :anchors {:left 0 :right 0} :height 40 :radius 8 :color @field :role listitem :label ~time
     {:id infoLogTime :style caption :text ~time :anchors {:left 12 :top 12}}
     (label#infoLogText :textColor @text
       :anchors {:left 64 :right 12 :top 10}
       :height 20
       :text ~(t (str "device." computed.infoDevice.id (if (= (= (mod n 2) 0) computed.infoDevice.on) ".on" ".off"))))))

(defview dialog-menu "The dialog's menu: the automation the device is part of, and its attributes." []
  `(node#infoMore :style round
     :icon ellipsis-vertical
     :text ""
     :label ~(t "auto.more")
     :tooltip ~(t "auto.more")
     :font icons
     :color transparent
     :anchors {:right 10 :top 10}
     :isOpen ~state.infoMenu
     :onClick {:toggle state.infoMenu}
     (node :use popMenu
       :with {:open state.infoMenu
              :width 200
              :items [{:title ~(t "info.menuRelated") :icon workflow :run infoRelatedMenu :with nil :separator false}
                      {:title ~(t "info.menuAttributes")
                       :icon list
                       :run showToast
                       :with {:message ~(t "info.attributesNote")}}]})))

;;; Its parts

(defcomponent iconBtn "A round button with an icon, named and explained by the string tip."
  {:icon "" :tip ""}
  `{:style round :icon ~icon :text "" :label ~(t tip) :tooltip ~(t tip)})

(defcomponent infoBody "The part of the dialog for one kind, shown while the dialog shows that kind."
  {:kind ""}
  `(node :when ~(= state.info.kind kind) :anchors {:left 0 :right 0 :top 0 :bottom 0} :color transparent
     {:slot true}))

(defcomponent infoRow "The dialog's row of round buttons under its big bar."
  {}
  `(row :anchors {:left 0 :right 0 :top 396} :height 44 :gap 16 :justify center :role toolbar :arrows horizontal
     {:slot true}))

;; A click on one of its ten bands runs pick with the band's level, 10 to 100.
(defcomponent bigBar "A tall rounded bar filled from the bottom to share (0 to 1)."
  {:share 0 :fill @accent :label "" :pick "" :up "" :down "" :left 140 :width 120}
  `(node :anchors {:left ~left :top 132}
     :width ~width
     :height 240
     :radius 28
     :color @raised
     :focusable true
     :role slider
     :label ~label
     :keys {:up ~(handler runNamed up) :down ~(handler runNamed down)}
     {:id barFill
      :anchors {:left 0 :right 0 :bottom 0}
      :height ~(* 240 share)
      :color ~fill
      :radius ~{:topLeft (if (> share 0.88) 28 0) :topRight (if (> share 0.88) 28 0) :bottomLeft 28 :bottomRight 28}}
     {:id barHandle
      :when ~(> share 0.06)
      :anchors {:left ~(/ (- width 40) 2) :bottom ~(- (* 240 share) (if (> share 0.92) 22 12))}
      :width 40
      :height 4
      :radius 2
      :color "#ffffffcc"}
     ~(repeat [k (range 10)]
       `{:id barBand
         :anchors {:left 0 :right 0 :top ~(* k 24)}
         :height 24
         :color transparent
         :cursor hand
         :onClick ~(handler pickBand pick (* (- 10 k) 10))})))

(defcomponent thermoDial "The target temperature on a dial coloured by the mode; p prefixes the inner ids."
  {:p "" :small false}
  `(node :width ~(if small 150 200) :height ~(if small 150 200) :color transparent
     ~@(dial-arcs p)
     (label :id ~(str p "label-mode-now")
       :font small
       :textColor @muted
       :text ~(t (str "mode." state.mode))
       :anchors {:left 0 :right 0 :top ~(if small 30 46)}
       :height 16
       :align center)
     (label :id ~(str p "target")
       :text ~(str computed.room.target "°")
       :font figure
       :textColor @text
       :anchors {:left 0 :right 0 :top ~(if small 46 66)}
       :height 60
       :align center
       :label ~(t "climate.targetIs" {:target computed.room.target})
       :live polite)
     {:id ~(str p "label-target")
      :when ~(not small)
      :style caption
      :icon thermometer
      :text ~(str " " computed.room.temp "°")
      :anchors {:left 0 :right 0 :top 128}
      :height 16
      :align center}))

(defview dial-arcs "The dial's track, its arc up to the target in the mode's colour, and its knob." [p]
  [`(path :id ~(str p "dialTrack")
      :fill 0
      :viewBox "0 0 200 200"
      :d "M 43.43 156.57 A 80 80 0 1 1 156.57 156.57"
      :stroke @off
      :strokeWidth 14
      :strokeCap round)
   `(path :id ~(str p "dialArc")
      :fill 0
      :viewBox "0 0 200 200"
      :d "M 43.43 156.57 A 80 80 0 1 1 156.57 156.57"
      :stroke ~computed.modeColor
      :strokeWidth 14
      :strokeCap round
      :strokeDash ~[(max (* computed.dialShare 377) 0.01) 400])
   `(path :id ~(str p "dialKnob")
      :fill 0
      :viewBox "0 0 200 200"
      :d "M 93 20 A 7 7 0 1 0 107 20 A 7 7 0 1 0 93 20"
      :fillColor @knob
      :stroke ~computed.modeColor
      :strokeWidth 3
      :rotate ~(+ (- 135) (* 270 computed.dialShare)))])

(defcomponent scrim "Dims the page behind a dialog that is open at the state path."
  {:open ""}
  `{:overlay {:placement window :open ~open} :x 0 :y 0 :width ~window.width :height ~window.height :color "#00000052"})
