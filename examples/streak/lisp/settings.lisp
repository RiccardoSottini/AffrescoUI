;;; settings.lisp --- the settings page: the interface and the data

;;; Settings
(defview settings-page "The settings, a page over the app." []
  `(node#settingsPage :overlay {:placement window :open state.settingsOpen :dismiss outside :modal true}
     :fill 0
     :color @c.bg
     :role dialog
     :label ~(t "settings")
     (column#settingsList :anchors {:left 0 :right 0 :top 56 :bottom 0} :scroll vertical :alignItems center
       (column#settingsColumn :width ~(min 720 window.width)
         :children [(node#settingsTop :height 4 :color transparent)
                    ~(settings-interface)
                    (column#settingsNotifications :style card :anchors {:left 0 :right 0} :fitContent height
                      (label#label-t-set-notifications
                        :text ~(t "set.notifications")
                        :style sectionTitle
                        :anchors {:left 0 :right 0})
                      (node#remindersRow :use toggleRow
                        :with {:title ~(t "set.reminders") :summary ~(t "set.remindersHint") :bind state.reminders :on ~state.reminders}))
                    ~(settings-data)
                    (node#settingsEnd :height 4 :color transparent)]
         :fitContent height
         :gap 12))
     ~(settings-bar)))

(defview settings-bar "The settings' bar: back, and the title." []
  `(node#settingsBar
     :anchors {:left 0 :right 0 :top 0}
     :height 56
     :color @c.bar
     :shadow {:y 2 :blur 4 :color "#00000030"}
     (node#settingsBack
       :style iconButton
       :icon arrow-left
       :label ~(t "back")
       :tooltip ~(t "back")
       :textColor @c.text
       :onClick {:set state.settingsOpen :to false}
       :anchors {:left 4 :top 8}
       :mirror true)
     (label#label-t-settings :text ~(t "settings")
       :font title
       :textColor @c.text
       :anchors {:left 60 :top 16}
       :fitText true
       :role heading)))

(defview settings-interface "The interface: theme, pure black, first weekday, the days' order, skips, reminders and the language." []
  `(column#settingsInterface :style card :anchors {:left 0 :right 0} :fitContent height
     (label#label-t-set-interface :text ~(t "set.interface") :style sectionTitle :anchors {:left 0 :right 0})
     (node#themeRow :use prefRow
       :with {:title ~(t "set.theme") :summary ~(t (str "theme." state.theme))}
       :isOpen ~state.menus.theme
       :onClick {:set state.menus.theme :to true}
       (node#menu-theme-row :use menu
         :with {:open state.menus.theme
                :width 220
                :placement below-end
                :items ~(map (fn [it]
                               {:title (t (str "theme." it)) :checked (= state.theme it) :run "setTheme" :with {:value it}})
                             ["light" "dark" "system"])}))
     (node#pureBlackRow :use toggleRow
       :with {:title ~(t "set.pureBlack") :summary ~(t "set.pureBlackHint") :bind state.pureBlack :on ~state.pureBlack})
     (node#reverseRow :use toggleRow
       :with {:title ~(t "set.reverse") :summary ~(t "set.reverseHint") :bind state.reverseDays :on ~state.reverseDays})
     (node#skipRow :use toggleRow
       :with {:title ~(t "set.skip") :summary ~(t "set.skipHint") :bind state.skipDays :on ~state.skipDays})
     (node#firstDayRow :use prefRow
       :with {:title ~(t "set.firstDay")
              :summary ~(formatDate (dateMs (addDays "2000-01-02" state.firstWeekday)) "EEEE" "utc")}
       :isOpen ~state.menus.firstDay
       :onClick {:set state.menus.firstDay :to true}
       (node#menu-first-day-row :use menu
         :with {:open state.menus.firstDay
                :width 220
                :placement below-end
                :items ~(map (fn [it]
                               {:title (formatDate (dateMs (addDays "2000-01-02" it)) "EEEE" "utc")
                                :checked (= state.firstWeekday it)
                                :run "setFirstDay"
                                :with {:value it}})
                             [6 0 1])}))
     (node#languageRow :use prefRow
       :with {:title ~(t "set.language") :summary ~(t (str "lang." state.locale))}
       :isOpen ~state.menus.language
       :onClick {:set state.menus.language :to true}
       (node#menu-language-row :use menu
         :with {:open state.menus.language
                :width 220
                :placement below-end
                :items ~(map (fn [it]
                               {:title (t (str "lang." it))
                                :checked (= state.locale it)
                                :run "setLocale"
                                :with {:value it}})
                             ["en" "es" "fr"])}))))

(defview settings-data "The data: export, back up and restore." []
  `(column#settingsData :style card :anchors {:left 0 :right 0} :fitContent height
     (label#label-t-set-data :text ~(t "set.data") :style sectionTitle :anchors {:left 0 :right 0})
     (node#exportRow :use prefRow :with {:title ~(t "set.export") :summary ~(t "set.exportHint")} :onClick exportAll)
     (node#backupRow :use prefRow :with {:title ~(t "set.backup") :summary ~(t "set.backupHint")} :onClick backupAll)
     (node#restoreRow :use prefRow
       :with {:title ~(t "set.restore") :summary ~(t "set.restoreHint")}
       :onClick restoreAll)
     (node#syncRow :use prefRow :with {:title ~(t "set.sync") :summary ~computed.syncText} :onClick syncNow
       (node#accountButton
         :style textButton
         :text ~(if computed.signedIn (t "signOut") (t "signIn"))
         :anchors {:right 8 :top 14}
         :onClick {:run ~(if computed.signedIn "signOutOfServer" "signInToServer")}))))

