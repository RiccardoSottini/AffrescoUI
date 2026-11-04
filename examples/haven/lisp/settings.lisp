;;; settings.lisp --- Haven's settings page, and the settings it edits
;;;
;;; The main page is grouped cards of rows: each opens another page of the app or a sub-page here. The sub-pages are
;;; the devices and controls (tables of the devices and the rooms), the profile (language and theme), General (every
;;; setting from the settings schema, each row drawn by its type) and About.

;;; The settings

(defcustom climate.awayTarget
  17 "The target the Away scene sets in every room, in degrees."
  :type number :minimum 12 :maximum 20
  :title ~(t "setting.awayTarget") :description ~(t "setting.awayTarget.about"))

(defcustom scenes.blindsMs
  600 "How long the blinds take to move in a scene, in milliseconds."
  :type integer :minimum 0 :maximum 3000
  :title ~(t "setting.blindsMs") :description ~(t "setting.blindsMs.about"))

(defcustom energy.dailyBudget
  12 "The energy a day should stay under, in kWh, shown on the Energy page."
  :type integer :minimum 5 :maximum 60
  :title ~(t "setting.dailyBudget") :description ~(t "setting.dailyBudget.about"))

(defcustom devices.notify
  true "Show a note in the corner when a device has answered."
  :type boolean :title ~(t "setting.notify") :description ~(t "setting.notify.about"))

(defcustom ui.language
  "en" "English, Spanish or French."
  :type string :enum [en es fr] :title ~(t "set.language") :description ~(t "setting.language.about"))

;;; What the page keeps

(defstate settingsView main "The sub-page shown, or main." :type {:enum [main devices profile general about]})

(defstate langMenu false "Whether the profile's language menu is open.")

(defstate genMenu false "Whether a General row's menu of values is open.")

(defstate devTab devices "The tab of the devices and controls.")

(defstate devQuery "" "The text the devices' and rooms' tables are filtered by.")

(defstate settingsGroups
  [{:id home
    :rows [{:id devices :icon cpu :color "#4a50b8"}
           {:id automations :icon bot :color "#2e8b63"}
           {:id areas :icon layout-grid :color "#d9573a"}
           {:id energy :icon zap :color "#c99312"}]}
   {:id you
    :rows [{:id profile :icon user :color "#9a4f9e"}
           {:id general :icon sliders-horizontal :color "#6b6f80"}
           {:id about :icon info :color "#3f8f8a"}]}]
  "The main page's cards, and their rows: a sub-page here, or another page of the app."
  :type (array :items (object)))

(defcomputed subPages "The rows that open a sub-page here, not another page." ["devices" "profile" "general" "about"])

;;; The tables of devices and rooms

(defcomputed devInBar "Whether the window is wide enough for the devices' tabs to sit in the bar."
  (>= (- window.width computed.sideWidth) 900))

(defcomputed devTop "Where the devices' search and tables start." (if computed.devInBar 56 104))

(defcomputed tableWidth "The width the tables' columns share." (- (- window.width computed.sideWidth) 48))

(defcomputed deviceMakers "Each device's maker, the room it is in, and how it is connected."
  {:door ["Keystead" 0 "Zigbee"] :vacuum ["Tidyworks" 1 "Wi-Fi"] :speaker ["Aurelle" 0 "Wi-Fi"]})

(defcomputed deviceRows "The devices table's rows, those whose name holds the search."
  (filter (fn [it] (contains (lower-case it.cells.0) (lower-case state.devQuery)))
          (map (fn [it]
                 {:id it.id
                  :cells [it.name
                          (get-in computed.deviceMakers [it.id 0])
                          (t (str "kind." it.id))
                          (get-in state.rooms [(get-in computed.deviceMakers [it.id 1]) :name])
                          (get-in computed.deviceMakers [it.id 2])]})
               state.devices)))

(defcomputed deviceColumns "The devices table's columns."
  [{:id "name" :title (t "table.device") :width (floor (* computed.tableWidth 0.28))}
   {:id "maker" :title (t "table.maker") :width (floor (* computed.tableWidth 0.18))}
   {:id "kind" :title (t "table.model") :width (floor (* computed.tableWidth 0.18))}
   {:id "area" :title (t "table.area") :width (floor (* computed.tableWidth 0.18))}
   {:id "via" :title (t "table.integration") :width (floor (* computed.tableWidth 0.18))}])

(defcomputed roomEntityRows "The rooms table's rows: each room's lights and climate, those whose name holds the search."
  (filter (fn [it] (contains (lower-case it.cells.0) (lower-case state.devQuery)))
          (map-indexed (fn [index it]
                         {:id (str "r" index)
                          :cells [it.name
                                  (t "summary.lights" {:on (count (filter (fn [it] it.on) it.lights))
                                                       :total (count it.lights)})
                                  (str it.temp "°  →  " it.target "°")]})
                       state.rooms)))

(defcomputed roomEntityColumns "The rooms table's columns."
  [{:id "room" :title (t "table.room") :width (floor (* computed.tableWidth 0.4))}
   {:id "lights" :title (t "lights.title") :width (floor (* computed.tableWidth 0.3))}
   {:id "climate" :title (t "climate.title") :width (floor (* computed.tableWidth 0.3))}])

;;; Components

(defcomponent settingsRow "A row of a settings card: an icon in a circle of colour, a title and a line, a chevron."
  {:row {}}
  `(node :anchors {:left 0 :right 0}
     :height 72
     :color transparent
     :hover {:color @cardHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t (str "set." row.id))
     :onClick ~(handler openSettingsRow row.id)
     (label :font icons :icon ~row.icon :text "" :textColor "#ffffff" :color ~row.color
       :anchors {:left 16 :top 16} :width 40 :height 40 :radius 20 :align center)
     (label :text ~(t (str "set." row.id))
       :textColor @text
       :anchors {:left 72 :right 48 :top 14}
       :height 22
       :align {:horizontal start :vertical center})
     (label :font small
       :text ~(t (str "set." row.id ".sub"))
       :textColor @muted
       :anchors {:left 72 :right 48 :top 38}
       :height 20
       :align {:horizontal start :vertical center})
     (label :font icons :icon chevron-right :mirror true :text "" :textColor @muted
       :anchors {:right 16 :top 24} :width 24 :height 24 :align center)))

(defcomponent settingsRadio "A radio button and its label, filled with the accent when it is the chosen one."
  {:text "" :chosen false}
  `(row :height 36
     :width 104
     :gap 10
     :alignItems center
     :color transparent
     :cursor hand
     :focusable true
     :role radio
     :label ~text
     :checked ~chosen
     (node :width 20 :height 20 :radius 10 :color transparent :borderWidth 2
       :borderColor ~(if chosen "@accent" "@muted")
       {:when ~chosen :anchors {:left 5 :top 5} :width 10 :height 10 :radius 5 :color @accent})
     (label :text ~text :textColor @text :fitText true)))

(defcomponent settingsSection "A row of the profile card: its title and line at the start, its choices at the end."
  {:title "" :sub ""}
  `(node :anchors {:left 0 :right 0} :height 76 :color transparent :role radiogroup :label ~title
     (label :text ~title :textColor @text :anchors {:left 20 :top 16} :height 22 :fitText true)
     (label :font small :text ~sub :textColor @muted :anchors {:left 20 :top 40} :height 20 :fitText true)
     (row :anchors {:right 20 :top 20} :width 360 :height 36 :gap 8 :justify end
       {:slot true})))

(defcomponent settingsPageMain "The settings page: its bar, and the main page or the sub-page chosen."
  {}
  `(node :dir auto :color transparent
     ~(settings-bar)
     ~(settings-main)
     ~(device-tabs)
     ~(device-tabs-rule)
     ~(device-filters)
     ~(device-search)
     ~(device-empty)
     ~(device-add)
     ~(device-tables)
     ~(profile-user)
     ~(profile-choices)
     ~(general-settings)
     ~(about-card)))

;;; The bar and the main page

(defview settings-bar "The bar: the back arrow on a sub-page, and its title." []
  `(node#settingsBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner
     (label#settingsBack :font icons
       :icon arrow-left
       :mirror true
       :text ""
       :textColor @onHeader
       :when ~(not= state.settingsView "main")
       :anchors {:left 8 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :hover {:color @headerHover}
       :cursor hand
       :focusable true
       :role button
       :label ~(t "set.back")
       :tooltip ~(t "set.back")
       :onClick {:set state.settingsView :to main})
     (label#settingsTitle :font heading
       :textColor @onHeader
       :fitText true
       :text ~(if (= state.settingsView "main") (t "page.settings") (t (str "set." state.settingsView)))
       :anchors {:left ~(if (= state.settingsView "main") 24 64) :top 0 :bottom 0}
       :align {:horizontal start :vertical center})))

(defview settings-main "The main page: a card of rows for each group." []
  `(column#settingsMain :when ~(= state.settingsView "main")
     :anchors {:left 24 :right 24 :top 80 :bottom 0}
     :gap 24
     :scroll vertical
     ~(repeat [g state.settingsGroups :key g.id]
       `(node#settingsGroup :style card
          :anchors {:left 0 :right 0}
          :height ~(+ (* (count g.rows) 72) 16)
          :role list
          :label ~(t (str "set.group." g.id))
          (column :anchors {:left 0 :right 0 :top 8 :bottom 8} :gap 0
            ~(repeat [r g.rows :key r.id]
              `(node#settingsRow :use settingsRow :with {:row ~r})))))))

;;; Devices and controls

(defview device-tabs "The devices' tabs: in the bar when the window is wide, else under it." []
  `(row#devTabs :when ~(= state.settingsView "devices")
     :anchors {:left ~(if computed.devInBar 260 0) :right ~(if computed.devInBar 24 0)
               :top ~(if computed.devInBar 0 56)}
     :height ~(if computed.devInBar 56 48)
     :color ~(if computed.devInBar "transparent" "@card")
     :justify center
     :role tablist
     :arrows horizontal
     :label ~(t "set.devices")
     ~(repeat [tab ["integrations" "devices" "entities" "helpers"]]
       (device-tab tab))))

(defview device-tab "One tab of the devices, underlined while it is the one shown." [tab]
  `(label#devTab :text ~(t (str "dev." tab))
     :textColor ~(cond computed.devInBar (if (= state.devTab tab) "@onHeader" "@onHeaderDim")
                       (= state.devTab tab) "@accent"
                       :else "@muted")
     :anchors {:top 0 :bottom 0}
     :width 128
     :align center
     :hover {:color ~(if computed.devInBar "@headerHover" "@cardHover")}
     :cursor hand
     :focusable true
     :role tab
     :selected ~(= state.devTab tab)
     :onClick ~(handler showDeviceTab tab)
     {:when ~(= state.devTab tab)
      :anchors {:left 0 :right 0 :bottom 0}
      :height 2
      :color ~(if computed.devInBar "@onHeader" "@accent")}))

(defview device-tabs-rule "The rule under the devices' tabs, while they are under the bar." []
  `(node#devTabsRule :when ~(and (= state.settingsView "devices") (not computed.devInBar))
     :anchors {:left 0 :right 0 :top 104}
     :height 1
     :color @divider))

(defview device-filters "The filters button, not there yet." []
  `(node#devFilters :when ~(= state.settingsView "devices")
     :style button
     :icon filter
     :text ~(t "dev.filters")
     :color transparent
     :hover {:color transparent}
     :textColor @muted
     :borderWidth 1
     :borderColor @border
     :anchors {:left 24 :top ~(+ computed.devTop 16)}
     :width 112
     :height 40
     :radius 8
     :tooltip ~(t "dev.filtersSoon")
     :cursor arrow
     :disabled true))

(defview device-search "The search over the devices or the rooms." []
  `(node#devSearchBox :when ~(= state.settingsView "devices")
     :anchors {:left 148 :right 24 :top ~(+ computed.devTop 16)}
     :height 40
     :radius 8
     :color @card
     :borderWidth 1
     :borderColor @border
     (label :icon search :text "" :textColor @muted :anchors {:left 10 :top 0 :bottom 0} :width 24 :align center)
     (textarea#devSearch :bind state.devQuery
       :placeholder ~(t (str "dev.searchN." state.devTab)
                        {:n (count (if (= state.devTab "entities") state.rooms state.devices))})
       :placeholderColor @muted
       :textColor @text
       :color transparent
       :anchors {:left 42 :right 12 :top 0 :bottom 0}
       :label ~(t "dev.search"))))

(defview device-empty "What the integrations and helpers tabs show: nothing yet." []
  `(node#devEmpty :when ~(and (= state.settingsView "devices")
                              (or (= state.devTab "integrations") (= state.devTab "helpers")))
     :style card
     :anchors {:left 24 :right 24 :top ~(+ computed.devTop 80)}
     :height 160
     (label :font iconsLarge
       :icon ~(if (= state.devTab "helpers") "wand-2" "puzzle")
       :text ""
       :textColor @muted
       :anchors {:left 0 :right 0 :top 32}
       :height 48
       :align center)
     (label :text ~(t (str "dev.empty." state.devTab))
       :textColor @muted
       :anchors {:left 24 :right 24 :top 96}
       :height 24
       :align center)))

(defview device-add "The floating button that adds to the tab shown (not there yet)." []
  `(node#devAdd :when ~(= state.settingsView "devices")
     :style button
     :icon plus
     :text ~(t (str "dev.add." state.devTab))
     :color @raised
     :hover {:color @raised}
     :textColor @muted
     :anchors {:right 32 :bottom 32}
     :width 200
     :height 52
     :radius 16
     :shadow {:y 4 :blur 12 :color "#00000040"}
     :role button
     :label ~(t (str "dev.add." state.devTab))
     :tooltip ~(t "dev.addSoon")
     :cursor arrow
     :disabled true))

(defview device-tables "The devices tab's table, or the controls tab's table of rooms." []
  `(column#settingsDevices :when ~(and (= state.settingsView "devices")
                                       (or (= state.devTab "devices") (= state.devTab "entities")))
     :anchors {:left 0 :right 0 :top ~(+ computed.devTop 72) :bottom 0}
     :gap 0
     :scroll vertical
     ~(device-table)
     ~(room-table)))

(defview device-table "The devices: name, maker, model, room and connection." []
  `(node#deviceTableCard :when ~(= state.devTab "devices")
     :anchors {:left 0 :right 0}
     :height ~(+ (* (count computed.deviceRows) 52) 29)
     :color @card
     (table#deviceTable :anchors {:left 12 :right 12 :top 0 :bottom 0}
       :rowHeight 52
       :color @card
       :items ~computed.deviceRows
       :columns ~computed.deviceColumns
       :label ~(t "devices.title")
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @selected
       :accentColor @border)
     {:anchors {:left 0 :right 0 :top 28} :height 1 :color @divider}
     ~(repeat [row computed.deviceRows]
       `(node#deviceTableLine :anchors {:left 0 :right 0 :top ~(+ 80 (* index 52))} :height 1 :color @divider))))

(defview room-table "The rooms: their lights and climate; opening one shows it on the home page." []
  `(node#roomTableCard :when ~(= state.devTab "entities")
     :anchors {:left 0 :right 0}
     :height ~(+ (* (count computed.roomEntityRows) 52) 29)
     :color @card
     (table#roomEntityTable :anchors {:left 12 :right 12 :top 0 :bottom 0}
       :rowHeight 52
       :color @card
       :items ~computed.roomEntityRows
       :columns ~computed.roomEntityColumns
       :label ~(t "set.entities")
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @selected
       :accentColor @border
       :onOpen ~(handler openRoom event.value))
     {:anchors {:left 0 :right 0 :top 28} :height 1 :color @divider}
     ~(repeat [row computed.roomEntityRows]
       `(node#roomEntityTableLine :anchors {:left 0 :right 0 :top ~(+ 80 (* index 52))} :height 1 :color @divider))))

;;; The profile

(defview profile-user "The person signed in, and Log out (not there yet)." []
  `(node#settingsUser :when ~(= state.settingsView "profile")
     :style card
     :anchors {:left 24 :right 24 :top 80}
     :height 88
     :role group
     :label "Alex"
     (label :font heading :text "A" :textColor @onAccent :color @accent
       :anchors {:left 20 :top 20} :width 48 :height 48 :radius 24 :align center)
     (label :font heading :text "Alex" :textColor @text :anchors {:left 84 :top 22} :height 24 :fitText true)
     (label :font small :text ~(t "profile.owner") :textColor @muted :anchors {:left 84 :top 48} :height 18
       :fitText true)
     (node#profileLogout :style button
       :color transparent
       :hover {:color transparent}
       :textColor @muted
       :text ~(t "profile.logout")
       :anchors {:right 16 :top 26}
       :width 104
       :height 36
       :radius 18
       :tooltip ~(t "profile.logoutSoon")
       :cursor arrow
       :disabled true)))

(defview profile-choices "The language and the theme." []
  `(node#settingsProfile :when ~(= state.settingsView "profile")
     :style card
     :anchors {:left 24 :right 24 :top 184}
     :height 168
     (column :anchors {:left 0 :right 0 :top 8 :bottom 8} :gap 0
       (node#profileLanguage :use settingsSection :with {:title ~(t "set.language") :sub ~(t "set.language.sub")}
         :role group
         ~(language-pick))
       (node#profileTheme :use settingsSection :with {:title ~(t "set.theme") :sub ~(t "set.theme.sub")}
         (node#pickAuto :use settingsRadio
           :with {:text ~(t "set.auto") :chosen ~(= state.theme "auto")}
           :onClick {:set state.theme :to auto})
         (node#pickLight :use settingsRadio
           :with {:text ~(t "set.light") :chosen ~(= state.theme "light")}
           :onClick {:set state.theme :to light})
         (node#pickDark :use settingsRadio
           :with {:text ~(t "set.dark") :chosen ~(= state.theme "dark")}
           :onClick {:set state.theme :to dark})))))

(defview language-pick "The language chosen, and the menu of the three, each named in its own language." []
  `(label#pickLanguage :text ~(t (str "set.value." settings.ui.language))
     :textColor @text
     :width 240
     :height 36
     :padding 12
     :color @raised
     :radius {:topLeft 4 :topRight 4}
     :align {:horizontal start :vertical center}
     :hover {:color @raisedHover}
     :cursor hand
     :focusable true
     :role combobox
     :label ~(t "set.language")
     :isOpen ~state.langMenu
     :onClick {:toggle state.langMenu}
     ~(menu-chevron 6)
     {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted}
     (node#langMenu :use menu
       :with {:open state.langMenu
              :placement below
              :width 240
              :items [{:title "English" :checked ~(= settings.ui.language "en") :run pickLanguage :with {:lang en}}
                      {:title "Español" :checked ~(= settings.ui.language "es") :run pickLanguage :with {:lang es}}
                      {:title "Français" :checked ~(= settings.ui.language "fr") :run pickLanguage
                       :with {:lang fr}}]})))

(defview menu-chevron "The chevron at the end of a field that opens a menu." [top]
  `(label :icon chevron-down :text "" :textColor @muted :anchors {:right 8 :top ~top} :width 24 :height 24
     :align center))

;;; General: every setting, each row drawn by its type

(defview general-settings "Every setting of the settings schema, a row each." []
  `(node#settingsGeneral :when ~(= state.settingsView "general")
     :anchors {:top 80}
     :x ~(max 24 (/ (- (- window.width computed.sideWidth) 1040) 2))
     :width ~(min 1040 (- (- window.width computed.sideWidth) 48))
     :height ~(+ (* (count settingsList) 72) 16)
     :style card
     (column :anchors {:left 0 :right 0 :top 8 :bottom 8} :gap 0
       ~(repeat [s (filter (fn [it] true) settingsList) :index i]
         (general-row s i)))))

(defview general-row "A setting: its title and description, Reset once it is changed, and its switch, field or menu."
  [s i]
  `(node#generalRow :anchors {:left 0 :right 0} :height 72 :color transparent :role group :label ~s.title
     {:when ~(> i 0) :anchors {:left 20 :right 20 :top 0} :height 1 :color @divider}
     (label :text ~s.title :textColor @text :anchors {:left 20 :top 14} :height 22 :fitText true)
     (label :font small :text ~s.description :textColor @muted :anchors {:left 20 :right 420 :top 38} :height 20)
     (label :when ~s.modified
       :font small
       :text ~(t "set.reset")
       :textColor @accent
       :anchors {:right 176 :top 26}
       :fitText true
       :cursor hand
       :focusable true
       :role button
       :label ~(t "set.resetOne" {:name s.title})
       :onClick ~(handler resetSetting s.key))
     (node#generalSwitch :when ~(= s.type "boolean")
       :use pill
       :with {:at ~(str "settings." s.key) :on ~s.value :name ~s.title}
       :anchors {:right 20 :top 24})
     ~(general-field s)
     {:when ~(or (= s.type "number") (= s.type "integer")) :anchors {:right 20 :top 57} :width 140 :height 1
      :color @muted}
     ~(general-pick s)))

(defview general-field "A number setting's field: what is typed is kept when it is in the setting's range." [s]
  `(textarea#generalField :when ~(or (= s.type "number") (= s.type "integer"))
     :text ~(text s.value)
     :anchors {:right 20 :top 14}
     :width 140
     :height 44
     :padding 12
     :radius {:topLeft 4 :topRight 4}
     :color @raised
     :hover {:color @raisedHover}
     :textColor @text
     :label ~s.title
     :onChange ~(handler typeSetting s event.value)))

(defview general-pick "A setting with a list of values: the one chosen, and the menu of them." [s]
  `(label#generalPick :when ~(and (= s.type "string") (some? s.enum))
     :text ~(t (str "set.value." s.value))
     :textColor @text
     :anchors {:right 20 :top 14}
     :width 240
     :height 44
     :padding 12
     :color @raised
     :radius {:topLeft 4 :topRight 4}
     :align {:horizontal start :vertical center}
     :hover {:color @raisedHover}
     :cursor hand
     :focusable true
     :role combobox
     :label ~s.title
     :isOpen ~state.genMenu
     :onClick {:toggle state.genMenu}
     ~(menu-chevron 10)
     {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted}
     (node :use menu
       :with {:open state.genMenu
              :placement below
              :width 240
              :items ~(map (fn [it] {:title (t (str "set.value." it)) :checked (= s.value it) :run "pickSetting"
                                     :with {:key s.key :value it}})
                           s.enum)})))

;;; About

(defview about-card "The app's mark, name and tagline, its facts, and links to its pages (offline here)." []
  `(node#settingsAbout :when ~(= state.settingsView "about") :style card :anchors {:left 24 :right 24 :top 80}
     :height 468
     (row :anchors {:left 0 :right 0 :top 32} :height 72 :justify center
       (node :color @accent :width 72 :height 72 :radius 36
         (label :font iconsLarge :icon home :text "" :textColor @onAccent :anchors {:left 0 :right 0 :top 0 :bottom 0}
           :align center)))
     (label :text "Haven" :font title :textColor @text :anchors {:left 0 :right 0 :top 120} :height 36 :align center)
     (label :text ~(t "app.tagline") :textColor @muted :anchors {:left 0 :right 0 :top 156} :height 22 :align center)
     (panel :color @border :anchors {:left 0 :right 0 :top 198} :height 1)
     (column :anchors {:left 20 :right 20 :top 206} :height 84 :gap 0
       (node :anchors {:left 0 :right 0} :height 28 :color transparent
         (label :text ~(t "about.version") :textColor @muted :anchors {:left 0 :top 0 :bottom 0} :fitText true)
         (label :text "2026.10" :textColor @text :anchors {:right 0 :top 0 :bottom 0} :fitText true))
       (node :anchors {:left 0 :right 0} :height 28 :color transparent
         (label :text ~(t "about.devices") :textColor @muted :anchors {:left 0 :top 0 :bottom 0} :fitText true)
         (label :text ~(+ (count state.devices) (count state.rooms))
           :textColor @text
           :anchors {:right 0 :top 0 :bottom 0}
           :fitText true))
       (node :anchors {:left 0 :right 0} :height 28 :color transparent
         (label :text ~(t "about.license") :textColor @muted :anchors {:left 0 :top 0 :bottom 0} :fitText true)
         (label :text "Apache-2.0" :textColor @text :anchors {:right 0 :top 0 :bottom 0} :fitText true)))
     (panel :color @border :anchors {:left 0 :right 0 :top 300} :height 1)
     (column :anchors {:left 0 :right 0 :top 304 :bottom 8} :gap 0 :role list
       ~(repeat [l [{:key "docs" :icon "book-open"} {:key "issue" :icon "bug"} {:key "source" :icon "code"}]]
         (about-link l)))))

(defview about-link "A link of the about card: its icon, its name, and the mark of a page outside the app." [l]
  `(node#aboutLink :anchors {:left 0 :right 0}
     :height 52
     :color transparent
     :role link
     :label ~(t (str "about." l.key))
     :tooltip ~(t "about.offline")
     :cursor arrow
     :disabled true
     (label :icon ~l.icon :text "" :textColor @muted :anchors {:left 20 :top 14} :width 24 :height 24 :align center)
     (label :text ~(t (str "about." l.key))
       :textColor @text
       :anchors {:left 64 :right 56 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label :icon external-link :text "" :textColor @muted :anchors {:right 20 :top 14} :width 24 :height 24
       :align center)))
