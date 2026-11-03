;;; security.lisp --- Haven's security: the alarm, the notifications, and the simulated service
;;;
;;; The alarm is a tile on the home page, a button per mode, and a keypad in its details: disarming takes a code.
;;; The notifications open beside the sidebar's bell. A timer stands in for the devices: each room drifts towards its
;;; target, and while the home is away, the hall reports motion.

;;; The settings

(defcustom security.motionAlerts
  true "While the security mode is Away, motion in the hall sends a notification."
  :type boolean :title ~(t "setting.motionAlerts") :description ~(t "setting.motionAlerts.about"))

(defcustom simulation.enabled
  true "Every ten seconds the simulated service moves each room towards its target and reports motion."
  :type boolean :title ~(t "setting.simulation") :description ~(t "setting.simulation.about"))

;;; What it keeps

(defstate security off "The security mode: off, home or away.")

(defstate securityModes [off home away] "The security modes, in the order the alarm's buttons and Cmd+Shift+A go.")

(defstate alarmCode "" "The code typed on the keypad.")

(defstate notifications [] "The notifications, the newest first: each one's id, message and icon.")

(defstate notificationsOpen false "Whether the notifications are open.")

(defstate notifyCount 0 "How many notifications were ever sent, for their ids.")

(defstate tick 0 "How many times the simulated service has run.")

(defcomputed securityLabel "The security mode's name." (t (str "security." state.security)))

(defcomputed alarmColor "The alarm's colour: green while it is off, red while it is on."
  (if (= state.security "off") "@ok" "@alert"))

;;; The alarm

(defcomponent alarmTile "The alarm as a tile: its shield, its state and a button per mode; it opens the keypad."
  {}
  `(node :style card
     :height 72
     :hover {:color @cardHover}
     :cursor hand
     :role group
     :label ~(t "alarm.title")
     :onClick ~(handler openInfo "alarm" 0)
     (label#alarmIcon :icon ~(if (= state.security "off") "shield-off" "shield-check")
       :text ""
       :textColor ~computed.alarmColor
       :color ~(if (= state.security "off") "@okSoft" "@alertSoft")
       :anchors {:left 12 :top 18}
       :width 36
       :height 36
       :radius 18
       :align center)
     (label#alarmName :text ~(t "alarm.title") :textColor @text :anchors {:left 60 :top 17} :height 18 :fitText true)
     {:id alarmState :style caption :text ~(t (str "alarm." state.security)) :anchors {:left 60 :top 38} :live polite}
     ~(alarm-modes)))

(defview alarm-modes "The tile's modes, the one on in its colour; Off opens the keypad, as it takes the code." []
  `(row#alarmModes :anchors {:right 12 :top 18} :width 280 :height 36 :gap 6 :justify end :role toolbar
     :arrows horizontal
     ~(repeat [mode state.securityModes]
       `{:id alarmMode
         :style ~(if (= mode state.security) "choiceOn" "choice")
         :font small
         :text ~(t (str "alarm.do." mode))
         :padding 12
         :fitText true
         :anchors {:top 0 :bottom 0}
         :radius 18
         :selected ~(= mode state.security)
         :color ~(if (= mode state.security) (if (= mode "off") "@okSoft" "@alertSoft") "@raised")
         :textColor ~(if (= mode state.security) (if (= mode "off") "@ok" "@alert") "@text")
         :onClick ~(handler pressAlarmMode mode)})))

(defcomponent alarmKeypad "The keypad of the alarm's details: the code typed, as dots, the keys, and the modes."
  {}
  `(node :anchors {:left 0 :right 0 :top 120 :bottom 0} :color transparent
     (label#alarmCode :text ~(if (empty? state.alarmCode)
                                 (t "alarm.code")
                                 (join " " (map (fn [it] "●") (split state.alarmCode ""))))
       :textColor ~(if (empty? state.alarmCode) "@muted" "@text")
       :anchors {:left 100 :right 100 :top 0}
       :height 40
       :align center
       :color @field
       :radius 8
       :label ~(t "alarm.code"))
     ~(alarm-keys)
     ~(alarm-actions)))

(defview alarm-keys "The keypad's keys: the digits, clear and back." []
  `(grid#alarmKeys :columns 3
     :gap 10
     :anchors {:left 92 :right 92 :top 56}
     :height 230
     :role group
     :label ~(t "alarm.keypad")
     ~(repeat [key ["1" "2" "3" "4" "5" "6" "7" "8" "9" "clear" "0" "back"]]
       `{:id alarmKey
         :style button
         :height 50
         :radius 25
         :font ~(if (= (count key) 1) "heading" "icons")
         :icon ~(cond (= key "clear") "x" (= key "back") "delete" :else "")
         :text ~(if (= (count key) 1) key "")
         :label ~(if (= (count key) 1) key (t (str "alarm." key)))
         :onClick ~(handler pressAlarmKey key)})))

(defview alarm-actions "The keypad's modes, which take the code typed." []
  `(row#alarmActions :anchors {:left 24 :right 24 :top 300}
     :height 40
     :gap 8
     :justify center
     :role toolbar
     :arrows horizontal
     ~(repeat [mode state.securityModes]
       `{:id alarmAction
         :style ~(if (= mode state.security) "choiceOn" "choice")
         :text ~(t (str "alarm.do." mode))
         :width 108
         :height 40
         :radius 20
         :color ~(if (= mode state.security) (if (= mode "off") "@okSoft" "@alertSoft") "@raised")
         :textColor ~(if (= mode state.security) (if (= mode "off") "@ok" "@alert") "@text")
         :selected ~(= mode state.security)
         :onClick ~(handler armAlarm mode)})))

;;; The notifications

(defcomponent notificationsPanel "The notifications, beside the bell; Escape or a click outside closes them."
  {}
  `(node :dir auto
     :overlay {:anchor "#bellButton" :placement right :flip true :open state.notificationsOpen :dismiss outside
               :modal true}
     :x 8
     :width 320
     :height 300
     :radius 12
     :color @card
     :shadow {:y 6 :blur 18 :color "#00000040"}
     :role dialog
     :label ~(t "notify.title")
     (label#notificationsTitle :font heading
       :icon bell
       :text ~(str " " (t "notify.title"))
       :textColor @text
       :anchors {:left 16 :top 14}
       :fitText true)
     {:id notificationsClear
      :style button
      :font small
      :text ~(t "notify.clear")
      :anchors {:right 12 :top 12}
      :width 96
      :height 26
      :onClick clearNotifications}
     {:id notificationsEmpty
      :when ~(= (count state.notifications) 0)
      :style caption
      :text ~(t "notify.empty")
      :anchors {:left 16 :top 60}}
     ~(notification-list)))

(defview notification-list "The notifications, a row each." []
  `(column#notificationList :anchors {:left 8 :right 8 :top 50 :bottom 8} :gap 4 :scroll vertical :role list
     :live polite
     ~(repeat [note state.notifications :key note.id]
       `(label#notification :font small
          :icon ~note.icon
          :text ~(str "  " note.message)
          :anchors {:left 0 :right 0}
          :height 34
          :padding 10
          :radius 6
          :align {:horizontal start :vertical center}
          :color @raised
          :textColor @text
          :role listitem))))

;;; The simulated service

(defcomponent simulator "No pixels: the timer of the simulated service."
  {}
  `{:width 0 :height 0 :color transparent :every {:ms 10000 :do simulate}})
