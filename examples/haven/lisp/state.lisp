;;; state.lisp --- what Haven keeps while it runs: the home's rooms, devices and scenes, and where each value starts
;;;
;;; Each defstate is one key of the state, with the value it starts from and, for some, its type. The pages keep
;;; their own state beside their views: the floor plan's in plan.lisp, the automations' and security's in
;;; automations.lisp, the more-info dialog's in moreinfo.lisp, the settings' in settings.lisp.

;;; The page shown

(defstate theme light "Light, dark, or auto to follow the system." :type {:enum [auto dark light]})

(defstate page home "The page shown." :type {:enum [home energy plan automations areas logbook history settings]})

(defstate pages
  [{:id home :icon home :key "Cmd+1"}
   {:id energy :icon zap :key "Cmd+2"}
   {:id plan :icon map :key "Cmd+3"}
   {:id logbook :icon scroll-text :key "Cmd+5"}
   {:id history :icon activity :key "Cmd+6"}
   {:id areas :icon door-open :key ""}
   {:id automations :icon workflow :key "Cmd+4"}
   {:id settings :icon settings :key "Cmd+,"}]
  "The sidebar's pages, in its order: each one's icon and the key that shows it."
  :type (array :items (object :properties {:id (string) :icon (string) :key (string)})))

(defstate toast {:open false :message ""} "The note in the corner." :type (object :properties {:open (boolean)
                                                                                               :message (string)}))

(defstate quickPick {:open false :query ""} "The search over every control and command: open, and its text.")

;;; The rooms

(defstate room 0 "The room the home page shows, by its place in rooms." :type (number))

(defstate rooms
  [{:name "Living room"
    :image "../assets/images/living-room.svg"
    :temp 21.5
    :target 22
    :humidity 44
    :blinds 60
    :kwh 21.4
    :lights [{:name "Ceiling" :on true :level 80 :color "#ffd58aff"}
             {:name "Floor lamp" :on true :level 40 :color "#ffd58aff"}
             {:name "TV backlight" :on false :level 30 :color "#8ab4ffff"}
             {:name "Shelf strip" :on false :level 60 :color "#ff9ad5ff"}]}
   {:name "Kitchen"
    :image "../assets/images/kitchen.svg"
    :temp 20.5
    :target 21
    :humidity 51
    :blinds 100
    :kwh 17.9
    :lights [{:name "Pendants" :on true :level 90 :color "#ffd58aff"}
             {:name "Under cabinet" :on false :level 70 :color "#ffe082ff"}]}
   {:name "Bedroom"
    :image "../assets/images/bedroom.svg"
    :temp 19
    :target 18.5
    :humidity 47
    :blinds 0
    :kwh 6.2
    :lights [{:name "Bedside left" :on false :level 30 :color "#ffc078ff"}
             {:name "Bedside right" :on false :level 30 :color "#ffc078ff"}
             {:name "Wardrobe" :on false :level 100 :color "#ffe082ff"}]}
   {:name "Office"
    :image "../assets/images/office.svg"
    :temp 22
    :target 22
    :humidity 40
    :blinds 80
    :kwh 11.6
    :lights [{:name "Desk lamp" :on true :level 100 :color "#ffe082ff"}
             {:name "Ceiling" :on false :level 70 :color "#ffe082ff"}]}]
  "Each room: its picture, its temperature and target, its blinds, its energy this week, and its lights."
  :type (array :items (object :properties {:name (string)
                                           :image (string)
                                           :temp (number)
                                           :target (number)
                                           :humidity (number)
                                           :blinds (number)
                                           :kwh (number)
                                           :lights (array :items (object :properties {:name (string)
                                                                                      :on (boolean)
                                                                                      :level (number)
                                                                                      :color (string)}))})))

(defstate mode "Heat" "The heating mode." :type (string))

(defstate modes ["Heat" "Cool" "Auto"] "The heating modes, in the climate card's order." :type (array :items (string)))

;;; Scenes and devices

(defstate scene "" "The scene last chosen, or empty once a light was changed by hand." :type (string))

(defstate scenes
  [{:name "Morning" :key morning :action sceneMorning :icon sunrise}
   {:name "Movie night" :key movie :action sceneMovie :icon clapperboard}
   {:name "Away" :key away :action sceneAway :icon plane}
   {:name "Good night" :key night :action sceneNight :icon moon}]
  "The scenes: the command each runs, and the key of its name in the locales."
  :type (array :items (object :properties {:name (string) :key (string) :action (string) :icon (string)})))

(defstate devices
  [{:id door :name "Front door" :icon lock :iconOff lock-open :on true :busy false :progress 0}
   {:id vacuum :name "Vacuum" :icon bot :iconOff bot :on false :busy false :progress 0}
   {:id speaker :name "Speaker" :icon speaker :iconOff speaker :on false :busy false :progress 0}]
  "The devices whose calls take a while: on or off, and the progress of a call."
  :type (array :items (object :properties {:id (string)
                                           :name (string)
                                           :icon (string)
                                           :iconOff (string)
                                           :on (boolean)
                                           :busy (boolean)
                                           :progress (number)})))

;;; Energy

(defstate energyView day "What the energy page's chart is by." :type {:enum [day device room]})

(defstate deviceEnergy
  [{:id heating :kwh 180} {:id kitchen :kwh 92} {:id lights :kwh 46} {:id media :kwh 31} {:id vacuum :kwh 9} {:id other
                                                                                                              :kwh 85}]
  "This month's use by device, in kWh."
  :type (array :items (object :properties {:id (string) :kwh (number)})))

(defstate hourly
  [0.4 0.3 0.3 0.3 0.3 0.4 0.9 1.6 1.4 0.9 0.7 0.8 1.1 0.9 0.7 0.8 1.2 2.1 2.6 2.3 1.8 1.3 0.8 0.5]
  "Today's use, hour by hour from midnight, in kWh."
  :type (array :items (number)))

(defpart energyRecords "The type of the energy records, which the data store holds rather than the state."
  :stateTypes {:energy (array :items (object :properties {:day (string) :kwh (number)}))})
