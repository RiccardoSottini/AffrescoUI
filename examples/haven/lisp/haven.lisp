;;; haven.lisp --- what Haven does: its functions and commands
;;;
;;; The screen and its views (index.screen.lisp, home.lisp, automations.lisp...) say what is shown; this file says
;;; what happens. A function reads the running app through paths (`state.rooms`, `computed.room`,
;;; `settings.scenes.blindsMs`) and changes it only through steps (`set`, `animate`, `after`...). A view runs one by
;;; name, its parameters taken by name from the variables around it (:onClick doneEditing), or with its arguments,
;;; (handler openInfo "light" i). The keys are in keys.lisp.

(ns haven.haven)

;;; Rooms and their lights

(def room-ids "The rooms' ids, as the rooms tree and the floor plan name them." ["r0" "r1" "r2" "r3"])

(defn room-path "A field of a room of the state, as a path." [r key]
  (str "state.rooms[" r "]." key))

(defn light-path "A field of a light of a room, as a path." [r i key]
  (str "state.rooms[" r "].lights[" i "]." key))

(defn set-lights "Sets a field of every light of a room." [r key value]
  (doseq [i (range (count (get-in state.rooms [r :lights])))]
    (set (light-path r i key) value)))

(defn every-room "Runs a change on each room, by its place." [change]
  (doseq [r (range (count state.rooms))]
    (change r)))

(defn show-room "Shows a room, by its place." [r] (set "state.room" r))

(defn pick-room "A row of the rooms tree: r0 to r3 are rooms, the floors only open and close." [id]
  (let [at (indexOf room-ids id)]
    (when (>= at 0) (show-room at))))

(defn open-room "Shows a room, by its id, on the home page." [id]
  (pick-room id)
  (set "state.page" :home))

(defn pick-room-named "Shows a room, by its name; the first if no room has it." [roomName]
  (-> (map :name state.rooms) (indexOf roomName) (max 0) show-room))

(defn step-room "The previous (-1) or next (1) room, wrapping." [by]
  (let [n (count state.rooms)]
    (show-room (mod (+ state.room by n) n))))

(defcommand nextRoom "Shows the next room."
  :title ~(t "command.nextRoom") :description ~(t "command.nextRoom.about") :category ~(t "category.rooms")
  :icon "arrow-down" []
  (step-room 1))

(defcommand previousRoom "Shows the room before."
  :title ~(t "command.previousRoom") :description ~(t "command.previousRoom.about") :category ~(t "category.rooms")
  :icon "arrow-up" []
  (step-room -1))

(defn switch-light "Switches a light of the room shown; the scene is no longer the one chosen." [i]
  (toggle (light-path state.room i "on"))
  (set "state.scene" ""))

(defn warm-light "Gives a light of the room shown its warm white." [i]
  (set (light-path state.room i "color") "#ffd58aff"))

(defn set-level "A step of the shown light's brightness; it switches the light on." [by]
  (set (light-path state.room state.info.index "level") (clamp (+ computed.infoLight.level by) 10 100))
  (set (light-path state.room state.info.index "on") true)
  (set "state.scene" ""))

(defn pick-level "The shown light's brightness, picked." [level] (set-level (- level computed.infoLight.level)))

(defn level-up "The shown light a step brighter." [] (set-level 10))

(defn level-down "The shown light a step dimmer." [] (set-level -10))

(defcommand allLightsOff "Switches every light of every room off."
  :title ~(t "command.allLightsOff") :description ~(t "command.allLightsOff.about") :category ~(t "category.rooms")
  :icon "lightbulb-off" []
  (every-room #(set-lights % "on" false))
  (set "state.scene" ""))

;;; The climate and the blinds

(defn set-target "The shown room's target, by a step in degrees." [by]
  (set (room-path state.room "target") (clamp (+ computed.room.target by) 16 28)))

(defn pick-mode "A heating mode." [mode] (set "state.mode" mode))

(defn move-blinds "Moves the shown room's blinds all the way open (100) or closed (0)." [to]
  (animate (room-path state.room "blinds") :to to :ms 500))

(defn stop-blinds "Stops the blinds where they are, to the nearest tenth." []
  (set (room-path state.room "blinds") (-> computed.room.blinds (/ 10) round (* 10))))

(defn pick-blinds "The blinds' opening, picked on the tall bar." [level]
  (animate (room-path state.room "blinds") :to level :ms 300))

(defn blinds-up "The blinds a tenth more open." []
  (set (room-path state.room "blinds") (clamp (+ computed.room.blinds 10) 0 100)))

; style: keep, a view or data holds it where it may be text
(defn blindsDown "The blinds a tenth more closed." []
  (set (room-path state.room "blinds") (clamp (- computed.room.blinds 10) 0 100)))

(defn pick-tilt "The blinds' tilt, picked on the tall bar." [level] (animate "state.coverTilt" :to level :ms 300))

(defn tilt-up "The blinds' tilt a tenth up." [] (set "state.coverTilt" (-> state.coverTilt (+ 10) (clamp 0 100))))

(defn tilt-down "The blinds' tilt a tenth down." [] (set "state.coverTilt" (-> state.coverTilt (- 10) (clamp 0 100))))

;;; Scenes

(defn run-scene "Runs a scene's command, by its name." [action] (run action))

(defcommand sceneMorning "Lights the room at 80%, opens its blinds."
  :title ~(t "command.sceneMorning") :description ~(t "command.sceneMorning.about") :category ~(t "category.scenes")
  :icon "sunrise" []
  (set-lights state.room "on" true)
  (set-lights state.room "level" 80)
  (animate (room-path state.room "blinds") :to 100 :ms settings.scenes.blindsMs)
  (set "state.scene" "Morning"))

(defcommand sceneMovie "Dims the room's lights to 20%, closes its blinds."
  :title ~(t "command.sceneMovie") :description ~(t "command.sceneMovie.about") :category ~(t "category.scenes")
  :icon "clapperboard" []
  (set-lights state.room "level" 20)
  (animate (room-path state.room "blinds") :to 0 :ms settings.scenes.blindsMs)
  (set "state.scene" "Movie night"))

(defcommand sceneAway "Switches every light off, and sets every room to the away temperature."
  :title ~(t "command.sceneAway") :description ~(t "command.sceneAway.about") :category ~(t "category.scenes")
  :icon "plane" []
  (every-room (fn [r]
               (set-lights r "on" false)
               (set (room-path r "target") settings.climate.awayTarget)))
  (set "state.scene" "Away"))

(defcommand sceneNight "Switches every light off, and closes every room's blinds."
  :title ~(t "command.sceneNight") :description ~(t "command.sceneNight.about") :category ~(t "category.scenes")
  :icon "moon" []
  (every-room (fn [r]
               (set-lights r "on" false)
               (set (room-path r "blinds") 0)))
  (set "state.scene" "Good night"))

;;; Notes and notifications

(defn notify "Keeps the newest twenty notifications, the newest first." [message icon]
  (increment "state.notifyCount" :by 1)
  (set "state.notifications"
       (take 20 (concat [{:id (str "n" state.notifyCount) :message message :icon icon}] state.notifications))))

(defn hide-toast "Hides the note in the corner." [] (set "state.toast.open" false))

(defn show-toast "A note in the corner for a few seconds, kept in the notifications too." [message]
  (set "state.toast" {:open true :message message})
  (notify message "bell")
  (debounce 4000 hide-toast))

(defn close-at "Closes what is open at a path of the state." [path] (set path false))

(defcommand toggleNotifications "Opens or closes the notifications beside the bell."
  :title ~(t "command.toggleNotifications") :description ~(t "command.toggleNotifications.about")
  :category ~(t "category.security") :icon "bell" []
  (toggle "state.notificationsOpen"))

(defcommand clearNotifications "Clears every notification."
  :title ~(t "command.clearNotifications") :description ~(t "command.clearNotifications.about")
  :category ~(t "category.security") :icon "bell-off" []
  (set "state.notifications" []))

;;; Devices

(defn device-path "A field of a device of the state, as a path." [i key]
  (str "state.devices[" i "]." key))

(defn device-done "A device call ends: the new state, and a note." [i]
  (set (device-path i "busy") false)
  (toggle (device-path i "on"))
  (when settings.devices.notify
    (let [device (get state.devices i)]
      (show-toast (t "toast.device"
                    {:name (:name device)
                     :state (-> (str "device." (:id device) (if (:on device) ".on" ".off")) t lower)})))))

;; A host would make it a call with onAsyncCall; here a timer stands in for the device.
(defn device-call "A device call takes a while: the tile shows its progress, then the new state." [i]
  (when (nil? computed.busyDevice)
    (set (device-path i "busy") true)
    (set (device-path i "progress") 0)
    (animate (device-path i "progress") :to 1 :ms 900)
    (after 900 device-done i)))

;;; Security and the simulated service

;; Disarming takes a code on the keypad, any four digits or more here, as the simulated panel has no stored code.
(defn arm-alarm "Sets the security mode: off, home or away." [mode]
  (if (and (= mode "off") (not= state.security "off") (< (count state.alarmCode) 4))
    (show-toast (t "alarm.needCode"))
    (do (set "state.security" mode)
        (set "state.alarmCode" "")
        (notify (t "security.changed" {:mode (t (str "security." mode))}) "shield"))))

(defn press-alarm-mode "A mode of the alarm's tile: off opens the keypad, as it takes the code; the others arm it." [mode]
  (if (= mode :off) (open-info "alarm" 0) (arm-alarm mode)))

(defn press-alarm-key "A key of the alarm's keypad: a digit (up to eight), clear, or back." [key]
  (set "state.alarmCode"
       (case key
         "clear" ""
         "back" (->> state.alarmCode count dec (max 0) (subs state.alarmCode 0))
         (let [code (str state.alarmCode key)]
           (->> code count (min 8) (subs code 0))))))

(defcommand cycleSecurity "Moves the security mode on: off, home, away."
  :title ~(t "command.cycleSecurity") :description ~(t "command.cycleSecurity.about")
  :category ~(t "category.security") :icon "shield" []
  (set "state.security" (get state.securityModes (-> (indexOf state.securityModes state.security) inc (mod 3))))
  (notify (t "security.changed" {:mode (t (str "security." state.security))}) "shield"))

;; Every fourth tick while away, the hall reports motion.
(defn simulate "The simulated service, every ten seconds: each room 0.5° nearer its target." []
  (when settings.simulation.enabled
    (increment "state.tick" :by 1)
    (every-room (fn [r]
                 (let [{:keys [temp target]} (get state.rooms r)]
                   (set (room-path r "temp") (+ temp (clamp (- target temp) -0.5 0.5))))))
    (when (and (= state.security :away) settings.security.motionAlerts (zero? (mod state.tick 4)))
      (notify (t "notify.motion") "siren"))))

;;; The details of a light, the climate, the blinds or a device

(defn open-info "Opens the more-info of a light, climate, cover or device; i is the light or device it shows." [kind i]
  (set "state.info" {:open true :kind kind :index i}))

; style: keep, a string of the code names it, and a rename never changes a string
(defn openEntity "Picks the room (nil keeps the one shown), then opens that entity's more-info." [room kind i]
  (when-some [r room] (show-room r))
  (open-info kind i))

(defn close-info-for "Closes the more-info, and goes to a page." [page]
  (set "state.info.open" false)
  (set "state.page" page))

(defn open-related "Closes the more-info, and opens the automation its device is part of." []
  (close-info-for :automations)
  (set "state.autoOpen" (indexOf (map :id state.automations) computed.infoFacts.related)))

(defn info-related-menu "The automation of the device shown, if it has one; else a note that it has none." []
  (if (and (= state.info.kind :device) (some? computed.infoFacts.related))
    (open-related)
    (show-toast (t "info.noRelated"))))

(defn run-named "Runs a function or a command, by its name: a key of the tall bar." [action] (run action))

(defn pick-band "A band of the tall bar: its function, given the level the band stands for." [pick level]
  (run pick :with {:level level}))

;;; The floor plan, the colour of a light, and the speaker

(defn open-colour "Opens the dialog that colours a light of the room." [i]
  (set "state.light" i)
  (set "state.colourOpen" true))

(defn colour-from-info "Closes the more-info, and opens the colour of its light, its picker focused." [i]
  (set "state.info.open" false)
  (open-colour i)
  (focus "#colourPicker"))

(defn move-rooms "The canvas's items after a drag or a nudge." [moved]
  (set "state.planRooms"
       (reduce (fn [rooms item]
                 (map #(if (= (:id %) (:id item)) (merge % (select-keys item [:x :y :width :height])) %) rooms))
               state.planRooms moved)))

(defn plan-pick "One room chosen on the canvas becomes the room the other pages show." [view]
  (let [selection (:selection view)]
    (when (= (count selection) 1)
      (let [at (indexOf room-ids (first selection))]
        (when (>= at 0) (show-room at))))))

(defn step-track "The track before (-1) or after (1), wrapping." [by]
  (let [n (count state.tracks)]
    (set "state.media.track" (mod (+ state.media.track by n) n))))

(defcommand playPause "Plays or pauses the speaker: a device call."
  :title ~(t "command.playPause") :description ~(t "command.playPause.about") :category ~(t "category.media")
  :icon "play" []
  (device-call (indexOf (map :id state.devices) "speaker")))

(defcommand nextTrack "Plays the next track."
  :title ~(t "command.nextTrack") :description ~(t "command.nextTrack.about") :category ~(t "category.media")
  :icon "skip-forward" []
  (step-track 1))

(defcommand previousTrack "Plays the track before."
  :title ~(t "command.previousTrack") :description ~(t "command.previousTrack.about") :category ~(t "category.media")
  :icon "skip-back" []
  (step-track -1))

;;; Automations

(defn show-automation-tab "Shows a tab of the automations: the list or the flow." [tab] (set "state.autoTab" tab))

(defn open-automation "The automation to edit (-1 for the list)." [i] (set "state.autoOpen" i))

; style: keep, rule-name is already a definition
(defn ruleName "An automation's name, in the language shown." [automation]
  (->> (or (:title automation) (:id automation)) (str "rule.") t))

(defcommand newAutomation "Makes a new automation, with no steps, and opens it."
  :title ~(t "command.newAutomation") :description ~(t "command.newAutomation.about")
  :category ~(t "category.automations") :icon "plus" []
  (let [n (count state.automations)]
    (set "state.automations"
         (concat state.automations [{:id (str "new" n) :title "untitled" :icon "sparkles" :on true :ran "never"
                                     :steps []}]))
    (set "state.autoTab" :list)
    (set "state.autoOpen" n)))

(defn open-steps "The open automation's steps, as a path." [] (str "state.automations[" state.autoOpen "].steps"))

(defn add-step "The preset of a part (when, if or do), at the end of the open automation." [part]
  (set (open-steps) (concat (get-in state.automations [state.autoOpen :steps]) [(get state.autoPresets part)])))


(defn step-action "Duplicates or deletes a step of the open automation." [s what]
  (let [steps (get-in state.automations [state.autoOpen :steps])]
    (set (open-steps) (if (= what :delete)
                       (concat (take s steps) (drop (inc s) steps))
                       (concat (take (inc s) steps) (drop s steps))))))

(defn toggle-step-menu "Opens or closes the menu of a step of the open automation." [s]
  (toggle (str "state.stepMenu.s" s)))

(defn toggle-row-menu "Opens or closes the menu of a row of the list, by the automation's id." [id]
  (toggle (str "state.autoRowMenu." id)))

(defn duplicate-automation "A copy of the open automation, at the end of the list, opened." []
  (let [n (count state.automations)
        open computed.openAuto]
    (set "state.automations"
         (concat state.automations [(merge open {:id (str (:id open) "-" n) :title (or (:title open) (:id open))
                                                 :ran "never"})]))
    (set "state.autoOpen" n)))

(defn confirm-delete "Asks before the open automation is deleted." []
  (set "state.autoTarget" state.autoOpen)
  (set "state.autoConfirm" true))

(defn row-action "A row menu's choice for an automation of the list: run, duplicate, enable or delete." [i what]
  (case what
    :run (do (set (str "state.automations[" i "].ran") "now")
             (notify (t "auto.ranNote" {:name (ruleName (get state.automations i))}) "play"))
    :duplicate (do (set "state.autoOpen" i)
                   (duplicate-automation))
    :enable (set (str "state.automations[" i "].on") (not (get-in state.automations [i :on])))
    :delete (do (set "state.autoTarget" i)
                (set "state.autoConfirm" true))
    nil))

(defn delete-automation "Deletes the automation the question was about, once confirmed, and goes back to the list." []
  (set "state.autoConfirm" false)
  (let [target state.autoTarget
        automations state.automations]
    (notify (t "auto.deletedNote" {:name (ruleName (get automations target))}) "trash-2")
    (set "state.automations" (concat (take target automations) (drop (inc target) automations)))
    (set "state.autoOpen" -1)))

(defn run-automation "Runs the open automation now: it reports in the notifications and its trace starts again." []
  (set (str "state.automations[" state.autoOpen "].ran") "now")
  (notify (t "auto.ranNote" {:name (ruleName computed.openAuto)}) "play"))

(defn add-auto-node "A new node to the right of the others, chosen." [node]
  (let [n (count state.autoNodes)
        id (str (:kind node) n)]
    (set "state.autoNodes" (concat state.autoNodes [(merge node {:id id :x 640 :y (+ 20 (* 80 (mod n 4)))})]))
    (set "state.autoSelected" [id])))

(defn chosen? "Whether an id is among the graph event's." [id] (>= (indexOf event.ids id) 0))

(defn link-nodes "A link drawn in the flow, from a node's output to another's input." []
  (set "state.autoLinks" (concat state.autoLinks [{:from event.from :output event.output :to event.to
                                                   :input event.input}])))

(defn unlink-nodes "A link of the flow taken away." []
  (let [links state.autoLinks]
    (set "state.autoLinks" (concat (take event.index links) (drop (inc event.index) links)))))

(defn move-nodes "The flow's nodes chosen, moved." []
  (set "state.autoNodes"
       (map (fn [{:keys [id x y] :as node}]
              (if (chosen? id) (merge node {:x (+ x event.dx) :y (+ y event.dy)}) node))
            state.autoNodes)))

(defn select-nodes "The flow's nodes chosen." [] (set "state.autoSelected" event.ids))

(defn delete-nodes "The flow's nodes chosen deleted, with their links." []
  (set "state.autoLinks" (remove #(or (chosen? (:from %)) (chosen? (:to %))) state.autoLinks))
  (set "state.autoNodes" (remove #(chosen? (:id %)) state.autoNodes))
  (set "state.autoSelected" []))

(defn change-schedule "A schedule clip moved or trimmed: whole hours, at least one; it stays on its own track." []
  (let [track (or event.fromTrack event.track)]
    (set "state.schedules"
         (map (fn [schedule]
                (if (not= (:id schedule) track)
                  schedule
                  (merge schedule
                         {:clips (map (fn [clip]
                                        (if (not= (:id clip) event.id)
                                          clip
                                          (merge clip {:start (clamp (round event.start) 0 23)
                                                       :duration (max 1 (round event.duration))})))
                                      (:clips schedule))})))
              state.schedules))))

;;; Arranging the cards

(defcommand editDashboard "Shows the home page with its cards ready to be arranged."
  :title ~(t "command.editDashboard") :description ~(t "command.editDashboard.about") :category ~(t "category.view")
  :icon "pencil" []
  (set "state.page" :home)
  (set "state.editing" true))

(defn done-editing "Ends arranging the cards, and closes a card's editor." []
  (set "state.editing" false)
  (set "state.cardEdit.open" false))

(defn hide-card "Takes a card off the dashboard, by its key in state.cards; Add card puts it back." [card]
  (set "state.hiddenCards" (conj state.hiddenCards card)))

(defn add-card "Puts a card taken off the dashboard back, by its key in state.cards." [card]
  (set "state.hiddenCards" (filter #(not= % card) state.hiddenCards)))

(defn edit-card "Opens the editor of a card, by its key in state.cards." [card]
  (set "state.cardEdit" {:open true :id card :code false :draft (get state.cards card) :text "" :error ""}))

(defn show-card-code "Shows the card's configuration as code." []
  (set "state.cardEdit.text" computed.cardCode)
  (set "state.cardEdit.code" true))

(defn show-card-form "The card's code back as its form, if it is an object." []
  (let [value (parseJson state.cardEdit.text)]
    (if (= (type value) "object")
      (do (set "state.cardEdit.draft" (merge value {:type (:type (get state.cards state.cardEdit.id))}))
          (set "state.cardEdit.error" "")
          (set "state.cardEdit.code" false))
      (set "state.cardEdit.error" (t "edit.badCode")))))

(defn switch-card-view "Shows the card's configuration as code, or back as its form." []
  (if state.cardEdit.code (show-card-form) (show-card-code)))

(defn save-card "Keeps the card's configuration, from its code if that is shown, unless the code is wrong." []
  (when state.cardEdit.code (show-card-form))
  (when (empty state.cardEdit.error)
    (set (str "state.cards." state.cardEdit.id) state.cardEdit.draft)
    (set "state.cardEdit.open" false)))

;;; Pages, settings and the window

(defn go-page "Shows a page of the sidebar." [page]
  (set "state.settingsView" :main)
  (set "state.page" page))

(defcommand goHome "Shows the home page."
  :title ~(t "command.goHome") :description ~(t "command.goHome.about") :category ~(t "category.go") :icon "home" []
  (set "state.page" :home))

(defcommand goEnergy "Shows the energy page."
  :title ~(t "command.goEnergy") :description ~(t "command.goEnergy.about") :category ~(t "category.go") :icon "zap" []
  (set "state.page" :energy))

(defcommand goPlan "Shows the floor plan."
  :title ~(t "command.goPlan") :description ~(t "command.goPlan.about") :category ~(t "category.go") :icon "map" []
  (set "state.page" :plan))

(defcommand goAutomations "Shows the automations."
  :title ~(t "command.goAutomations") :description ~(t "command.goAutomations.about") :category ~(t "category.go")
  :icon "workflow" []
  (set "state.page" :automations))

(defcommand goAreas "Shows the rooms page."
  :title ~(t "command.goAreas") :description ~(t "command.goAreas.about") :category ~(t "category.go")
  :icon "door-open" []
  (set "state.page" :areas))

(defcommand goLogbook "Shows the activity log."
  :title ~(t "command.goLogbook") :description ~(t "command.goLogbook.about") :category ~(t "category.go")
  :icon "scroll-text" []
  (set "state.page" :logbook))

(defcommand goHistory "Shows the history."
  :title ~(t "command.goHistory") :description ~(t "command.goHistory.about") :category ~(t "category.go")
  :icon "activity" []
  (set "state.page" :history))

(defcommand goSettings "Shows the settings' main page."
  :title ~(t "command.goSettings") :description ~(t "command.goSettings.about") :category ~(t "category.go")
  :icon "settings" []
  (go-page :settings))

(defcommand commandPalette "Opens the search on the commands."
  :title ~(t "command.commandPalette") :description ~(t "command.commandPalette.about") :category ~(t "category.go")
  :icon "command" []
  (set "state.quickPick.query" ">")
  (set "state.quickPick.open" true))

(defcommand searchEntities "Opens the search on the controls."
  :title ~(t "command.searchEntities") :description ~(t "command.searchEntities.about") :category ~(t "category.go")
  :icon "search" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand toggleSidebar "Folds the sidebar to its icons, or opens it."
  :title ~(t "command.toggleSidebar") :description ~(t "command.toggleSidebar.about") :category ~(t "category.view")
  :icon "panel-left" []
  (toggle "state.sidebarCollapsed"))

(defcommand toggleTheme "Switches between the light and the dark theme."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about") :category ~(t "category.view")
  :icon "moon" []
  (set "state.theme" (if (= computed.themeNow :dark) :light :dark)))

(defcommand switchLanguage "Moves the language on: English, Spanish, French."
  :title ~(t "command.switchLanguage") :description ~(t "command.switchLanguage.about") :category ~(t "category.view")
  :icon "languages" []
  (set "settings.ui.language" (case settings.ui.language "en" "es" "es" "fr" "en")))

(defn open-settings-row "A row of the settings' main page: a sub-page of the settings, or another page." [id]
  (if (>= (indexOf computed.subPages id) 0)
    (do (set "state.settingsView" id)
        (set "state.page" :settings))
    (do (set "state.settingsView" :main)
        (set "state.page" id))))

(defn show-device-tab "Shows a tab of the devices and controls." [tab] (set "state.devTab" tab))

(defn show-energy-day "Shows a day on the energy page: 0 is today, 6 the oldest kept." [day]
  (set "state.energyDay" (clamp day 0 6)))

(defn step-energy-day "Moves the energy page a day back (1) or forward (-1), within the days kept." [step]
  (show-energy-day (+ state.energyDay step)))

(defn pick-log-period "The logbook's period: today, yesterday, twoDays or week." [period] (set "state.logPeriod" period))

(defn pick-hist-day "The day history shows: 0 is today, 6 the oldest kept." [day] (set "state.histDay" (clamp day 0 6)))

(defn pick-device-filter "Narrows the devices' tables to a room or a connection, or empty for all." [filter]
  (set "state.devFilter" filter))

(defn show-energy-view "What the energy page's week is by: day, device or room." [view] (set "state.energyView" view))

; style: keep, a string of the code names it, and a rename never changes a string
(defn pickSetting "A settings key, and its value." [key value] (set (str "settings." key) value))

(defn pick-language "A language code." [lang] (set "settings.ui.language" lang))

(defn reset-setting "A setting back to its default." [key] (remove-state (str "settings." key)))

(defn type-setting "A number typed in a setting's row, kept when it is a number within the setting's range." [s text]
  (let [n (number text)]
    (when (and (some? n) (<= (:minimum s) n (:maximum s)))

      (set (str "settings." (:key s)) n))))

(defn pick-menu-item "An item of a menu: the menu closes, and the item's action runs." [open action args]
  (set open false)
  (run action :with args))
