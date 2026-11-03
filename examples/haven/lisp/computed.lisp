;;; computed.lisp --- what Haven works out from its state for the home page and the window
;;;
;;; A defcomputed is a value the views read as computed.<name>, worked out again whenever what it reads changes. The
;;; pages' own are beside their views (energy.lisp, automations.lisp, settings.lisp...).

;;; The window

(defcomputed themeNow "The theme in use: the chosen one, or the system's when it is auto."
  (if (= state.theme "auto") window.colorScheme state.theme))

(defcomputed compact "A short window, where the home page drops the room's picture." (< window.height 760))

(defcomputed onProfile "Whether the profile is shown, so the user's entry in the sidebar is the chosen one."
  (and (= state.page "settings") (= state.settingsView "profile")))

(defcomputed palette "Every command the palette offers: its title, its key or category, its icon."
  (map (fn [it] {:title it.title :detail (or-else it.keyLabel it.category) :icon (or-else it.icon "terminal")
                 :run it.name})
       (filter (fn [it] it.enabled) commands)))

;;; The room shown

(defcomputed room "The room the home page shows." (get state.rooms state.room))

(defcomputed lightsOn "How many of its lights are on." (count (filter (fn [it] it.on) computed.room.lights)))

(defcomputed sceneKey "The key of the scene last chosen, in the locales, or empty."
  (or-else (:key (find (fn [it] (= it.name state.scene)) state.scenes)) ""))

(defcomputed modeColor "The heating mode's colour."
  (cond (= state.mode "Heat") "@heat" (= state.mode "Cool") "@cool" :else "@autoMode"))

(defcomputed modeSoft "The heating mode's soft colour, behind its button."
  (cond (= state.mode "Heat") "@heatSoft" (= state.mode "Cool") "@coolSoft" :else "@autoModeSoft"))

(defcomputed dialShare "Where the target sits on the dial, from 16° (0) to 28° (1)."
  (clamp (/ (- computed.room.target 16) 12) 0 1))

(defcomputed coverTint "The blinds' icon behind its colour while they are open."
  (if (> computed.room.blinds 0) "@coverSoft" "@raised"))

(defcomputed busyDevice "The device whose call is running, if any." (find (fn [it] it.busy) state.devices))

;;; Energy

(defcomputed energyTotal "This week's use, in kWh." (sum data.energy (fn [it] it.kwh)))

(defcomputed peakDay "The day of the week that used the most."
  (get-in (sort-by (fn [it] it.kwh) > data.energy) [0 :day]))

(defcomputed weekValues "Each day's use this week, for the chart." (map (fn [it] it.kwh) data.energy))

(defcomputed deviceValues "Each device's use this month, for the chart." (map (fn [it] it.kwh) state.deviceEnergy))

(defcomputed today "Today's use so far, in kWh, to one decimal." (fixed (sum state.hourly) 1))

(defcomputed peakHour "The hour that used the most today." (indexOf state.hourly (max state.hourly)))
