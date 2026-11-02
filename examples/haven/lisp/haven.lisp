;;; haven.lisp --- what Haven does: its functions and commands
;;;
;;; The screen and its parts (index.screen.lisp, automations.lisp, plan.lisp...) say what is shown; this file says what
;;; happens. A function reads the running app through paths (`state.rooms`, `computed.room`, `settings.scenes.blindsMs`)
;;; and changes it only through steps (`set`, `animate`, `after`...). A handler runs one by name, its parameters by name:
;;; {:run setTarget :with {:by 0.5}}. The keys stay in the screen, as data.

;;; Rooms and their lights

(def roomIds "The rooms' ids, as the tree and the floor plan name them." ["r0" "r1" "r2" "r3"])

(defn roomPath "A field of a room of the state, as a path." [r key]
  (str "state.rooms[" r "]." key))

(defn lightPath "A field of a light of a room, as a path." [r i key]
  (str "state.rooms[" r "].lights[" i "]." key))

(defn setLights "Sets a field of every light of a room." [r key value]
  (map (fn [i] (set (lightPath r i key) value)) (range (count (:lights (get state.rooms r))))))

(defn everyRoom "Runs a change on each room, by its place." [change]
  (map change (range (count state.rooms))))

(defn setTarget "The shown room's target, by a step in degrees." [by]
  (set (roomPath state.room "target") (clamp (+ computed.room.target by) 16 28)))

(defn pickRoom "A row of the rooms tree: r0 to r3 are rooms, the floors only open and close." [id]
  (let [at (indexOf roomIds id)]
    (when (>= at 0) (set "state.room" at))))

(defn stepRoom "The previous (-1) or next (1) room, wrapping." [by]
  (set "state.room" (mod (+ state.room by (count state.rooms)) (count state.rooms))))

(defn setLevel "A step of the shown light's brightness; it switches the light on." [by]
  (set (lightPath state.room state.info.index "level") (clamp (+ computed.infoLight.level by) 10 100))
  (set (lightPath state.room state.info.index "on") true)
  (set "state.scene" ""))

(defn pickLevel "The shown light's brightness, picked." [level] (setLevel (- level computed.infoLight.level)))
(defn levelUp "" [] (setLevel 10))
(defn levelDown "" [] (setLevel -10))

(defn pickBlinds "" [level] (animate (roomPath state.room "blinds") :to level :ms 300))
(defn blindsUp "" [] (set (roomPath state.room "blinds") (clamp (+ computed.room.blinds 10) 0 100)))
(defn blindsDown "" [] (set (roomPath state.room "blinds") (clamp (- computed.room.blinds 10) 0 100)))

(defn pickTilt "" [level] (animate "state.coverTilt" :to level :ms 300))
(defn tiltUp "" [] (set "state.coverTilt" (clamp (+ state.coverTilt 10) 0 100)))
(defn tiltDown "" [] (set "state.coverTilt" (clamp (- state.coverTilt 10) 0 100)))
(defn pickMode "A heating mode." [mode] (set "state.mode" mode))

(defcommand sceneMorning "" :title "Scene: Morning" :category "Scenes" :icon "sunrise" []
  (setLights state.room "on" true)
  (setLights state.room "level" 80)
  (animate (roomPath state.room "blinds") :to 100 :ms settings.scenes.blindsMs)
  (set "state.scene" "Morning"))

(defcommand sceneMovie "" :title "Scene: Movie night" :category "Scenes" :icon "clapperboard" []
  (setLights state.room "level" 20)
  (animate (roomPath state.room "blinds") :to 0 :ms settings.scenes.blindsMs)
  (set "state.scene" "Movie night"))

(defcommand sceneAway "" :title "Scene: Away" :category "Scenes" :icon "plane" []
  (everyRoom (fn [r]
               (setLights r "on" false)
               (set (roomPath r "target") settings.climate.awayTarget)))
  (set "state.scene" "Away"))

(defcommand sceneNight "" :title "Scene: Good night" :category "Scenes" :icon "moon" []
  (everyRoom (fn [r]
               (setLights r "on" false)
               (set (roomPath r "blinds") 0)))
  (set "state.scene" "Good night"))

(defcommand allLightsOff "" :title "Switch every light off" :category "Rooms" :icon "lightbulb-off" []
  (everyRoom (fn [r] (setLights r "on" false)))
  (set "state.scene" ""))

(defcommand nextRoom "" :title "Next room" :category "Rooms" :icon "arrow-down" [] (stepRoom 1))
(defcommand previousRoom "" :title "Previous room" :category "Rooms" :icon "arrow-up" [] (stepRoom -1))

;;; Notifications and devices

(defn notify "Keeps the newest twenty notifications, the newest first." [message icon]
  (increment "state.notifyCount" :by 1)
  (set "state.notifications"
       (take 20 (concat [{:id (str "n" state.notifyCount) :message message :icon icon}] state.notifications))))

(defn hideToast "" [] (set "state.toast.open" false))

(defn showToast "A note in the corner for a few seconds, kept in the notifications too." [message]
  (set "state.toast" {:open true :message message})
  (notify message "bell")
  (debounce 4000 hideToast))

(defn devicePath "A field of a device of the state, as a path." [i key]
  (str "state.devices[" i "]." key))

(defn deviceDone "A device call ends: the new state, and a toast." [i]
  (set (devicePath i "busy") false)
  (toggle (devicePath i "on"))
  (when settings.devices.notify
    (let [device (get state.devices i)]
      (showToast (t "toast.device"
                    {:name (:name device)
                     :state (lower (t (str "device." (:id device) (if (:on device) ".on" ".off"))))})))))

(defn deviceCall "A device call takes a while: the tile shows its progress, then the new state. A host would make it a call with onAsyncCall; here a timer stands in for the device." [i]
  (when (nil? computed.busyDevice)
    (set (devicePath i "busy") true)
    (set (devicePath i "progress") 0)
    (animate (devicePath i "progress") :to 1 :ms 900)
    (after 900 deviceDone i)))

(defn armAlarm "off, home or away. Disarming takes a code on the keypad, any four digits or more here, as the simulated panel has no stored code." [mode]
  (if (and (= mode "off") (not= state.security "off") (< (count state.alarmCode) 4))
    (showToast (t "alarm.needCode"))
    (do (set "state.security" mode)
        (set "state.alarmCode" "")
        (notify (t "security.changed" {:mode (t (str "security." mode))}) "shield"))))

(defcommand cycleSecurity "" :title "Change the security mode" :category "Security" :icon "shield" []
  (set "state.security" (get state.securityModes (mod (inc (indexOf state.securityModes state.security)) 3)))
  (notify (t "security.changed" {:mode (t (str "security." state.security))}) "shield"))

(defcommand toggleNotifications "" :title "Show the notifications" :category "Security" :icon "bell" []
  (toggle "state.notificationsOpen"))

(defcommand clearNotifications "" :title "Clear the notifications" :category "Security" :icon "bell-off" []
  (set "state.notifications" []))

(defn simulate "The simulated service, every ten seconds: each room 0.5° nearer its target; every fourth tick while away, motion in the hall." []
  (when settings.simulation.enabled
    (increment "state.tick" :by 1)
    (everyRoom (fn [r]
                 (let [room (get state.rooms r)]
                   (set (roomPath r "temp") (+ (:temp room) (clamp (- (:target room) (:temp room)) -0.5 0.5))))))
    (when (and (= state.security "away") settings.security.motionAlerts (= (mod state.tick 4) 0))
      (notify (t "notify.motion") "siren"))))

;;; More info on a light, a cover or a device

(defn openInfo "Opens the more-info of a light, climate, cover or device; i is the light or device it shows." [kind i]
  (set "state.info" {:open true :kind kind :index i}))

(defn openEntity "Picks the room (nil keeps the one shown), then opens that entity's more-info." [room kind i]
  (when (some? room) (set "state.room" room))
  (openInfo kind i))

(defn openRelated "" []
  (set "state.info.open" false)
  (set "state.page" "automations")
  (set "state.autoOpen" (indexOf (map :id state.automations) computed.infoFacts.related)))

(defn infoRelatedMenu "" []
  (if (and (= state.info.kind "device") (some? computed.infoFacts.related))
    (openRelated)
    (showToast (t "info.noRelated"))))

;;; The floor plan and the speaker

(defn openColour "Opens the dialog that colours a light of the room." [i]
  (set "state.light" i)
  (set "state.colourOpen" true))

(defn moveRooms "The canvas's items after a drag or a nudge." [moved]
  (set "state.planRooms"
       (reduce (fn [rooms item]
                 (map (fn [room]
                        (if (= (:id room) (:id item))
                          (merge room {:x (:x item) :y (:y item) :width (:width item) :height (:height item)})
                          room))
                      rooms))
               state.planRooms moved)))

(defn planPick "One room chosen on the canvas becomes the room the other pages show." [view]
  (let [selection (:selection view)]
    (when (and (= (count selection) 1) (>= (indexOf roomIds (first selection)) 0))
      (set "state.room" (indexOf roomIds (first selection))))))

(defn stepTrack "" [by]
  (set "state.media.track" (mod (+ state.media.track by (count state.tracks)) (count state.tracks))))

(defcommand playPause "" :title "Play or pause the speaker" :category "Media" :icon "play" []
  (deviceCall (indexOf (map :id state.devices) "speaker")))

(defcommand nextTrack "" :title "Next track" :category "Media" :icon "skip-forward" [] (stepTrack 1))
(defcommand previousTrack "" :title "Previous track" :category "Media" :icon "skip-back" [] (stepTrack -1))

;;; Automations

(defn openAutomation "The automation to edit (-1 for the list)." [i] (set "state.autoOpen" i))

(defn addAutoNode "A new node to the right of the others, chosen." [node]
  (let [n (count state.autoNodes)
        id (str (:kind node) n)]
    (set "state.autoNodes" (concat state.autoNodes [(merge node {:id id :x 640 :y (+ 20 (* 80 (mod n 4)))})]))
    (set "state.autoSelected" [id])))

(defcommand newAutomation "" :title "New automation" :category "Automations" :icon "plus" []
  (let [n (count state.automations)]
    (set "state.automations"
         (concat state.automations [{:id (str "new" n) :title "untitled" :icon "sparkles" :on true :ran "never" :steps []}]))
    (set "state.autoTab" "list")
    (set "state.autoOpen" n)))

(defn openSteps "The open automation's steps, as a path." [] (str "state.automations[" state.autoOpen "].steps"))

(defn addStep "The preset of a part (when, if or do), at the end of the open automation." [part]
  (set (openSteps) (concat (:steps (get state.automations state.autoOpen)) [(get state.autoPresets part)])))

(defn duplicateAutomation "A copy of the open automation, at the end of the list, opened." []
  (let [n (count state.automations)
        open computed.openAuto]
    (set "state.automations"
         (concat state.automations [(merge open {:id (str (:id open) "-" n) :title (or (:title open) (:id open)) :ran "never"})]))
    (set "state.autoOpen" n)))

(defn stepAction "Duplicates or deletes a step of the open automation." [s what]
  (let [steps (:steps (get state.automations state.autoOpen))]
    (set (openSteps) (if (= what "delete")
                       (concat (take s steps) (drop (inc s) steps))
                       (concat (take (inc s) steps) (drop s steps))))))

(defn confirmDelete "" []
  (set "state.autoTarget" state.autoOpen)
  (set "state.autoConfirm" true))

(defn ruleName "An automation's name, in the language shown." [automation]
  (t (str "rule." (or (:title automation) (:id automation)))))

(defn rowAction "A row menu's choice for an automation of the list: run, duplicate, enable or delete." [i what]
  (cond (= what "run") (do (set (str "state.automations[" i "].ran") "now")
                           (notify (t "auto.ranNote" {:name (ruleName (get state.automations i))}) "play"))
        (= what "duplicate") (do (set "state.autoOpen" i)
                                 (duplicateAutomation))
        (= what "enable") (set (str "state.automations[" i "].on") (not (:on (get state.automations i))))
        (= what "delete") (do (set "state.autoTarget" i)
                              (set "state.autoConfirm" true))))

(defn deleteAutomation "Deletes the target automation, once confirmed, and goes back to the list." []
  (let [target state.autoTarget
        automations state.automations]
    (notify (t "auto.deletedNote" {:name (ruleName (get automations target))}) "trash-2")
    (set "state.automations" (concat (take target automations) (drop (inc target) automations)))
    (set "state.autoOpen" -1)))

(defn runAutomation "Runs the open automation now: it reports in the notifications and its trace starts again." []
  (set (str "state.automations[" state.autoOpen "].ran") "now")
  (notify (t "auto.ranNote" {:name (ruleName computed.openAuto)}) "play"))

(defn changeSchedule "A schedule clip moved or trimmed: whole hours, at least one; it stays on its own track." []
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

(defcommand editDashboard "" :title "Arrange cards" :category "View" :icon "pencil" []
  (set "state.page" "home")
  (set "state.editing" true))

(defn doneEditing "" []
  (set "state.editing" false)
  (set "state.cardEdit.open" false))

(defn editCard "Opens the editor of a card, by its key in state.cards." [card]
  (set "state.cardEdit" {:open true :id card :code false :draft (get state.cards card) :text "" :error ""}))

(defn showCardCode "" []
  (set "state.cardEdit.text" computed.cardCode)
  (set "state.cardEdit.code" true))

(defn showCardForm "The card's code back as its form, if it is an object." []
  (let [value (parseJson state.cardEdit.text)]
    (if (= (type value) "object")
      (do (set "state.cardEdit.draft" (merge value {:type (:type (get state.cards state.cardEdit.id))}))
          (set "state.cardEdit.error" "")
          (set "state.cardEdit.code" false))
      (set "state.cardEdit.error" (t "edit.badCode")))))

(defn saveCard "" []
  (when state.cardEdit.code (showCardForm))
  (when (empty state.cardEdit.error)
    (set (str "state.cards." state.cardEdit.id) state.cardEdit.draft)
    (set "state.cardEdit.open" false)))

;;; Pages, settings and the window

(defcommand goHome "" :title "Go to Home" :category "Go" :icon "home" [] (set "state.page" "home"))
(defcommand goEnergy "" :title "Go to Energy" :category "Go" :icon "zap" [] (set "state.page" "energy"))
(defcommand goPlan "" :title "Go to the floor plan" :category "Go" :icon "map" [] (set "state.page" "plan"))
(defcommand goAutomations "" :title "Go to Automations" :category "Go" :icon "workflow" []
  (set "state.page" "automations"))
(defcommand goAreas "" :title "Go to Rooms" :category "Go" :icon "door-open" [] (set "state.page" "areas"))
(defcommand goLogbook "" :title "Go to the activity log" :category "Go" :icon "scroll-text" [] (set "state.page" "logbook"))
(defcommand goHistory "" :title "Go to History" :category "Go" :icon "activity" [] (set "state.page" "history"))
(defcommand goSettings "" :title "Go to Settings" :category "Go" :icon "settings" []
  (set "state.settingsView" "main")
  (set "state.page" "settings"))

(defcommand commandPalette "" :title "Show all commands" :category "Go" :icon "command" []
  (set "state.quickPick.query" ">")
  (set "state.quickPick.open" true))

(defcommand searchEntities "" :title "Search controls" :category "Go" :icon "search" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand toggleSidebar "" :title "Fold the sidebar" :category "View" :icon "panel-left" [] (toggle "state.sidebarCollapsed"))

(defcommand toggleTheme "" :title "Switch light and dark" :category "View" :icon "moon" []
  (set "state.theme" (if (= computed.themeNow "dark") "light" "dark")))

(defcommand switchLanguage "" :title "Change the language" :category "View" :icon "languages" []
  (set "settings.ui.language" (cond (= settings.ui.language "en") "es" (= settings.ui.language "es") "fr" :else "en")))

(defn pickSetting "A settings key, and its value." [key value] (set (str "settings." key) value))
(defn pickLanguage "A language code." [lang] (set "settings.ui.language" lang))
