;;; home.lisp --- Haven's home page: the room shown, as cards
;;;
;;; The top bar holds the rooms as tabs; under it, a row of chips sums the room up. On the start side, the room's
;;; picture, its lights, the scenes and the alarm; on the end side, the climate, the blinds and the devices. While
;;; the cards are being arranged, the top bar turns into the editing bar and each card shows its frame.

;;; The page

(defview home-page "The home page: the top bar, the summary, and the room's cards." []
  `(node#main :dir auto
     :when ~(= state.page "home")
     :anchors {:left ~computed.sideWidth :right 0 :top 0 :bottom 32}
     :color transparent
     ~(top-bar)
     ~(summary-chips)
     ~(lights-column)
     ~(climate-card)
     ~(blinds-card)
     ~(devices-card)))

;;; The top bar

(defview top-bar "The rooms as tabs, the assistant, search and the menu; while editing, the editing bar." []
  `(node#toolbar :anchors {:left 0 :right 0 :top 0}
     :height 56
     :color ~(if state.editing "@editHeader" "@header")
     :role banner
     ~(close-editing-button)
     (label#editTitle :when ~state.editing
       :font heading
       :text ~(t "edit.dashboard")
       :textColor @onHeader
       :anchors {:left 60 :top 0 :bottom 0}
       :fitText true
       :align {:horizontal start :vertical center})
     ~(room-tabs)
     ~(assistant-button)
     ~(search-button)
     ~(add-card-button)
     (node#editDone :when ~state.editing
       :style button
       :text ~(t "edit.done")
       :color transparent
       :hover {:color @headerHover}
       :textColor @onHeader
       :anchors {:right 16 :top 10}
       :width 88
       :height 36
       :radius 18
       :onClick doneEditing)
     ~(page-menu)
     ~(page-menu-button)))

(defview close-editing-button "While editing, the cross that ends it." []
  `(label#editClose :when ~state.editing
     :font iconsMedium
     :icon x
     :text ""
     :textColor @onHeader
     :anchors {:left 8 :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :hover {:color @headerHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "edit.done")
     :tooltip ~(t "edit.done")
     :onClick doneEditing))

(defview assistant-button "The assistant, which opens and closes its panel." []
  `(label#assistButton :when ~(not state.editing)
     :font iconsMedium
     :icon sparkles
     :text ""
     :textColor @onHeader
     :anchors {:right 112 :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :hover {:color @headerHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "toolbar.assist")
     :tooltip ~(t "toolbar.assist")
     :onClick {:run assistant.toggle}))

(defview room-tabs "The rooms, a tab each; the one shown is filled." []
  `(row#viewTabs :anchors {:left ~(if state.editing 240 16) :top 0 :bottom 0 :right 168}
     :gap 6
     :role tablist
     :arrows horizontal
     :label ~(t "rooms.label")
     ~(repeat [r state.rooms :index i]
       `(node#viewTab :use viewTab
          :with {:title ~r.name :selected ~(= i state.room)}
          :label ~r.name
          :onClick ~(handler showRoom i)))))

(defview search-button "Search, which also names its key." []
  `(label#searchButton :when ~(not state.editing)
     :font iconsMedium
     :icon search
     :text ""
     :textColor @onHeader
     :anchors {:right 64 :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :hover {:color @headerHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "toolbar.search")
     :tooltip ~(t "toolbar.searchKey")
     :onClick searchEntities))

(defview add-card-button "While editing, Add card: not there yet." []
  `(node#editAddCard :when ~state.editing
     :style button
     :icon plus
     :text ~(t "edit.addCard")
     :color transparent
     :hover {:color transparent}
     :textColor @onHeaderDim
     :anchors {:right 112 :top 10}
     :padding 14
     :fitText true
     :height 36
     :radius 18
     :label ~(t "edit.addCard")
     :tooltip ~(t "edit.addCardSoon")
     :cursor arrow
     :disabled true))

(defview page-menu-button "The button of the page's menu." []
  `(label#dashMore :when ~(not state.editing)
     :font iconsMedium
     :icon ellipsis-vertical
     :text ""
     :textColor @onHeader
     :anchors {:right 16 :top 8}
     :width 40
     :height 40
     :radius 20
     :align center
     :hover {:color @headerHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "toolbar.more")
     :tooltip ~(t "toolbar.more")
     :isOpen ~state.dashMenu
     :onClick {:toggle state.dashMenu}))

(defview page-menu "The page's menu, under its button: search, and arranging the cards." []
  `(node#dashMoreAnchor :when ~(not state.editing) :anchors {:right 16 :top 8} :width 40 :height 40 :color transparent
     (node#dashMenu :use popMenu
       :with {:open state.dashMenu
              :width 220
              :items [{:title ~(t "toolbar.search") :icon search :run searchEntities :with nil}
                      {:separator true}
                      {:title ~(t "edit.dashboard") :icon pencil :run editDashboard}]})))

;;; The summary

(defview summary-chips "The room in three chips: its lights and scene, the weather outside, its climate." []
  `(row#chips :anchors {:left 24 :right 24 :top 68} :height 36 :gap 8 :role group :label ~(t "chips.label")
     (node#summary :style chip
       :icon lightbulb
       :text ~(str "  "
                 (t "summary.lights" {:on computed.lightsOn :total (count computed.room.lights)})
                 (if state.scene (t "summary.scene" {:scene (t (str "scene." computed.sceneKey))}) "")))
     (node#weather :style chip
       :icon cloud-sun
       :text ~(str "  14°  ·  " (t "weather.sky"))
       :label ~(str (t "weather.outside") ": 14°, " (t "weather.sky")))
     (node#climateChip :style chip
       :icon thermometer
       :text ~(str "  " computed.room.temp "°  ·  " computed.room.humidity "%")
       :label ~(t "climate.now" {:temp computed.room.temp :humidity computed.room.humidity}))))

;;; The start side: the room, its lights, the scenes and the alarm

(defview lights-column "The room's picture, then its lights, the scenes and the alarm, each with its editing frame." []
  `(node#lightsCard :anchors {:top 116 :bottom 16}
     :x 24
     :width ~(- (* (- window.width computed.sideWidth) 0.55) 64)
     :color transparent
     :scroll vertical
     ~(room-picture)
     (label#label-lights :font heading
       :textColor @text
       :text ~computed.cardTitle.lights
       :anchors {:left ~(if state.editing 14 4) :top ~(if computed.compact 0 136)}
       :fitText true)
     (node#label-click-a-light :when ~(not state.editing)
       :style caption
       :text ~(t "lights.hint")
       :anchors {:right 4 :top ~(if computed.compact 4 140)})
     ~(lights-grid)
     ~(card-heading "label-scenes" 'computed.cardTitle.scenes "#lights.bottom + 16")
     ~(scene-row)
     ~(card-heading "label-security" 'computed.cardTitle.security "#scenes.bottom + 16")
     (node#alarmTile :use alarmTile
       :anchors {:left ~(if state.editing 10 0) :right ~(if state.editing 10 0) :top "#label-security.bottom + 10"})
     ~(edit-frame "editLights" "lights" 'computed.cardTitle.lights {:top "#label-lights.top - 6"
                                                                    :bottom "#lights.bottom + 10"})
     ~(edit-frame "editScenes" "scenes" 'computed.cardTitle.scenes {:top "#label-scenes.top - 6"
                                                                    :bottom "#scenes.bottom + 10"})
     ~(edit-frame "editSecurity" "security" 'computed.cardTitle.security
                  {:top "#label-security.top - 6" :bottom "#alarmTile.bottom + 10"})))

(defview card-heading "The heading of a group of cards, moved in while the cards are being arranged." [id title top]
  `(label :id ~id
     :font heading
     :textColor @text
     :text ~title
     :anchors {:left ~(if state.editing 14 4) :top ~top}
     :fitText true))

(defview edit-frame "A group's frame while the cards are being arranged: its outline, and the pencil that edits it."
  [id card title edges]
  `(node :id ~id :use editFrame :with {:card ~card :name ~title} :anchors ~(merge {:left 0 :right 0} edges)))

(defview room-picture "The room's picture, its name and its climate on a shade at the bottom." []
  `(node#areaCard :when ~(not computed.compact)
     :anchors {:left 0 :right 0 :top 0}
     :height 120
     :radius 18
     :color @card
     :role group
     :label ~computed.room.name
     (image#roomImage :file ~computed.room.image
       :fit cover
       :radius 18
       :anchors {:left 0 :right 0 :top 0 :bottom 0}
       :label ~computed.room.name)
     (node#areaShade :anchors {:left 0 :right 0 :bottom 0} :height 44 :color "#00000088" :radius 18)
     (label#areaName :text ~computed.room.name :font heading :textColor "#ffffff" :anchors {:left 16 :bottom 10}
       :fitText true)
     (label#areaStats :font small
       :textColor "#ffffff"
       :icon thermometer
       :text ~(str " " computed.room.temp "°   " computed.room.humidity "%")
       :anchors {:right 16 :bottom 13}
       :fitText true)))

(defview lights-grid "The room's lights, two to a row." []
  `(grid#lights :columns 2
     :gap 8
     :anchors {:left ~(if state.editing 10 0) :right ~(if state.editing 10 0) :top ~(if computed.compact 30 166)}
     :height ~(- (* (ceil (/ (count computed.room.lights) 2)) 112) 8)
     :role list
     ~(repeat [light computed.room.lights :index i]
       (light-tile light i))))

(defview light-tile "A light: its icon switches it, its bar sets how bright it is, the rest opens its details."
  [light i]
  `(node#light :let {:tint ~(slice light.color 0 7)}
     :height 104
     :radius 18
     :color @card
     :borderWidth 0
     :borderColor @border
     :shadow {:y 1 :blur 6 :color "#0000001a"}
     :hover {:color @cardHover}
     :transition {:color 150}
     :cursor hand
     :role listitem
     :label ~light.name
     :description ~(t "info.open")
     :onClick ~(handler openInfo "light" i)
     (label#lightIcon :icon ~(if light.on "lightbulb" "lightbulb-off")
       :text ""
       :textColor ~(if light.on tint "@muted")
       :color ~(if light.on (str tint "33") "@raised")
       :anchors {:left 12 :top 12}
       :width 36
       :height 36
       :radius 18
       :align center)
     (label#label-light-name :text ~light.name :textColor @text :anchors {:left 56 :right 12 :top 12} :height 18)
     (node#label-light-on-on :style caption
       :text ~(if light.on (t "light.on" {:level light.level}) (t "light.off"))
       :anchors {:left 56 :top 32})
     ~(light-switch light)
     (node#level-bar-light :use levelBar
       :with {:level ~(if light.on (/ (- light.level 10) 90) 0)
              :fill ~(if light.on tint "@off")
              :back ~(if light.on (str tint "33") "@raised")}
       :anchors {:left 12 :right 12 :bottom 10})
     ~(light-level light)))

(defview light-switch "The light's switch: a toggle with no look of its own, over the icon that draws it." [light]
  `(toggle#toggle-rooms-room-lights :anchors {:left 12 :top 12}
     :width 36
     :height 36
     :radius 18
     :cursor hand
     :label ~light.name
     :tooltip ~(t (if light.on "light.switchOff" "light.switchOn"))
     :bind "state.rooms[state.room].lights[i].on"
     :activeColor transparent
     :trackColor transparent
     :knobColor transparent
     :onChange {:set state.scene :to ""}))

(defview light-level "The light's brightness: a slider with no look of its own, over the bar that draws it." [light]
  `(slider#slider-rooms-room-lights :anchors {:left 12 :right 12 :bottom 10}
     :height 40
     :min 10
     :max 100
     :step 10
     :cursor hand
     :label ~(str (t "light.brightness") ": " light.name)
     :bind "state.rooms[state.room].lights[i].level"
     :trackColor transparent
     :activeColor transparent
     :knobColor transparent
     :tooltip ~(t "light.brightness")))

(defview scene-row "The scenes, side by side." []
  `(row#scenes :anchors {:left ~(if state.editing 10 0) :right ~(if state.editing 10 0)
                         :top "#label-scenes.bottom + 10"}
     :height 76
     :gap 8
     :role toolbar
     :arrows horizontal
     :label ~(t "scenes.label")
     ~(repeat [scene state.scenes]
       (scene-tile scene))))

(defview scene-tile "A scene: its icon and name, outlined while it is the one last chosen." [scene]
  `(node#scene :let {:on ~(= scene.name state.scene)}
     :anchors {:top 0 :bottom 0}
     :grow 1
     :basis 0
     :radius 18
     :color ~(if on "@accentSoft" "@card")
     :borderWidth 1
     :borderColor ~(if on "@accent" "@border")
     :hover {:color ~(if on "@accentSoft" "@cardHover")}
     :cursor hand
     (label#sceneIcon :icon ~scene.icon
       :text ""
       :font title
       :textColor ~(if on "@accent" "@muted")
       :anchors {:left 0 :right 0 :top 12}
       :height 24
       :align center)
     (label#label-scene-name :text ~(t (str "scene." scene.key))
       :font small
       :anchors {:left 4 :right 4 :top 0 :bottom 0}
       :padding 10
       :align {:horizontal center :vertical end}
       :color transparent
       :textColor ~(if on "@accent" "@text")
       :focusable true
       :role button
       :selected ~on
       :onClick ~(handler runScene scene.action))))

;;; The end side: the climate, the blinds and the devices

(defview climate-card "The climate: the dial of the target, the steps that change it, and the heating modes." []
  `(node#climateCard :anchors {:right 24 :top 116}
     :x "52%"
     :width "45%"
     :height ~(if computed.compact 228 300)
     :style card
     :role group
     :label ~(t "climate.label")
     (node#label-climate :style cardTitle
       :icon thermometer
       :text ~computed.cardTitle.climate
       :cursor hand
       :focusable true
       :role button
       :tooltip ~(t "info.open")
       :onClick ~(handler openInfo "climate" 0))
     (node#label-now-room-temp :when ~(not state.editing)
       :style caption
       :text ~(t "climate.now" {:temp computed.room.temp :humidity computed.room.humidity})
       :anchors {:right 24 :top 24})
     (node#dial :use thermoDial :with {:p "" :small ~computed.compact} :anchors {:left 0 :right 0 :top 44})
     ~(target-steps)
     (row#modes :when ~state.cards.climate.modes
       :anchors {:left 0 :right 0 :bottom 16}
       :justify center
       :height 44
       :gap 12
       :role toolbar
       :arrows horizontal
       ~(repeat [mode state.modes]
         (mode-button mode)))
     ~(edit-frame "editClimate" "climate" 'computed.cardTitle.climate {:top 0 :bottom 0})))

(defview target-steps "A half degree cooler or warmer." []
  `(row#steps :anchors {:left 0 :right 0 :top ~(if computed.compact 136 192)}
     :height 40
     :gap 12
     :justify center
     :role toolbar
     :arrows horizontal
     :label ~(t "climate.label")
     (node#label-down-arrow :use iconBtn
       :with {:icon minus :tip climate.down}
       :width 40
       :height 40
       :radius 20
       :onClick ~(handler setTarget -0.5))
     (node#label-up-arrow :use iconBtn
       :with {:icon plus :tip climate.up}
       :width 40
       :height 40
       :radius 20
       :onClick ~(handler setTarget 0.5))))

(defview mode-button "A heating mode, in its colour while it is the one chosen." [mode]
  `(label#panel-mode :icon ~(cond (= mode "Heat") "flame" (= mode "Cool") "snowflake" :else "sun-snow")
     :text ""
     :width 44
     :height 44
     :radius 22
     :align center
     :color ~(if (= mode state.mode) computed.modeSoft "@raised")
     :hover {:color ~(if (= mode state.mode) computed.modeSoft "@raisedHover")}
     :textColor ~(if (= mode state.mode) computed.modeColor "@muted")
     :cursor hand
     :focusable true
     :role button
     :label ~(t (str "mode." mode))
     :tooltip ~(t (str "mode." mode))
     :selected ~(= mode state.mode)
     :onClick ~(handler pickMode mode)))

(defview blinds-card "The blinds: how open they are, open and close, and a bar that sets them." []
  `(node#blindsCard :anchors {:right 24 :top "#climateCard.bottom + 12"}
     :x "52%"
     :width "45%"
     :height 112
     :style card
     :role group
     :label ~(t "blinds.title")
     ~(blinds-icon)
     (label#label-blinds :text ~computed.cardTitle.cover
       :textColor @text
       :anchors {:left 60 :top 12}
       :fitText true
       :cursor hand
       :onClick ~(handler openInfo "cover" 0))
     (node#label-round-room-blinds :style caption
       :text ~(t "blinds.open" {:n (round computed.room.blinds)})
       :anchors {:left 60 :top 32})
     ~(blinds-button "label-open" "chevron-up" "blinds.openButton" 100 52 12)
     ~(blinds-button "label-close" "chevron-down" "blinds.close" 0 96 56)
     (node#level-bar-blinds-card :use levelBar
       :with {:level ~(/ computed.room.blinds 100) :fill @cover :back ~computed.coverTint}
       :anchors {:left 12 :right 12 :bottom 12})
     (slider#blinds :anchors {:left 12 :right 12 :bottom 12}
       :height 40
       :step 10
       :cursor hand
       :label ~(t "blinds.title")
       :bind "state.rooms[state.room].blinds"
       :trackColor transparent
       :activeColor transparent
       :knobColor transparent)
     ~(edit-frame "editCover" "cover" 'computed.cardTitle.cover {:top 0 :bottom 0})))

(defview blinds-icon "The blinds' icon, which opens their details." []
  `(label#blindsIcon :icon blinds
     :text ""
     :textColor @cover
     :color ~computed.coverTint
     :anchors {:left 12 :top 12}
     :width 36
     :height 36
     :radius 18
     :align center
     :cursor hand
     :focusable true
     :role button
     :label ~(t "info.cover")
     :tooltip ~(t "info.open")
     :onClick ~(handler openInfo "cover" 0)))

(defview blinds-button "A round button that moves the blinds all the way open (100) or closed (0)."
  [id icon key to editing-right right]
  `(label :id ~id
     :icon ~icon
     :text ""
     :anchors {:right ~(if state.editing editing-right right) :top 12}
     :width 36
     :height 36
     :radius 18
     :align center
     :color @raised
     :hover {:color @raisedHover}
     :textColor @text
     :cursor hand
     :focusable true
     :role button
     :label ~(t key)
     :tooltip ~(t key)
     :onClick ~(handler moveBlinds to)))

(defview devices-card "The devices, a tile each." []
  `(node#devicesCard :anchors {:right 24 :top "#blindsCard.bottom + 12" :bottom 16}
     :x "52%"
     :width "45%"
     :color transparent
     :role group
     :label ~(t "devices.title")
     (label#label-devices :when ~(not computed.compact)
       :font heading
       :textColor @text
       :text ~computed.cardTitle.devices
       :anchors {:left ~(if state.editing 14 4) :top 4}
       :fitText true)
     (row#devices :anchors {:left ~(if state.editing 10 0) :right ~(if state.editing 10 0)
                            :top ~(if computed.compact 0 34)}
       :height ~(if computed.compact 96 124)
       :gap 8
       ~(repeat [d state.devices :index i]
         (device-tile d i)))
     ~(edit-frame "editDevices" "devices" 'computed.cardTitle.devices {:top 0 :bottom "#devices.bottom + 10"})))

(defview device-tile "A device: its state, its action, and while a call runs, its progress; the rest opens its details."
  [d i]
  `(node#device :anchors {:top 0 :bottom 0}
     :grow 1
     :basis 0
     :radius 18
     :color @card
     :borderWidth 0
     :borderColor @border
     :shadow {:y 1 :blur 6 :color "#0000001a"}
     :hover {:color @cardHover}
     :cursor hand
     :role group
     :label ~d.name
     :onClick ~(handler openInfo "device" i)
     (label#deviceIcon :icon ~(if d.on d.icon d.iconOff)
       :text ""
       :textColor ~(if d.on "@accent" "@muted")
       :color ~(if d.on "@accentSoft" "@raised")
       :anchors {:left 10 :top 12}
       :width 32
       :height 32
       :radius 16
       :align center)
     (label#deviceName :text ~d.name :textColor @text :anchors {:left 50 :right 4 :top 12} :height 18)
     (node#deviceState :style caption
       :text ~(if d.busy (t "device.waiting") (t (str "device." d.id (if d.on ".on" ".off"))))
       :anchors {:left 50 :top 32}
       :live polite)
     ~(device-action d i)
     (node#deviceProgress :when ~d.busy :anchors {:left 0 :bottom 0} :width ~(str (* d.progress 100) "%") :height 3
       :color @accent)))

(defview device-action "The device's button: what a press does, or an ellipsis while a call runs." [d i]
  `(node#deviceAction :when ~state.cards.devices.actions
     :style button
     :text ~(if d.busy "…" (t (str "device." d.id (if d.on ".doOff" ".doOn"))))
     :font small
     :anchors {:left 12 :right 12 :bottom 12}
     :height 28
     :label ~(str d.name ": " (if d.busy (t "device.waiting") (t (str "device." d.id (if d.on ".doOff" ".doOn")))))
     :onClick ~(handler deviceCall i)))
