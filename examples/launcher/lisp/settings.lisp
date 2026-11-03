;;; settings.lisp --- the Settings page: the look, the notation of the examples, the folder, the AI, the keys
;;;
;;; The AI provider's card is the shared intelligence add-on's (../../shared/addons/intelligence/settings.lisp): every
;;; app's assistant uses what is set there.

;;; The page

(defview settings-page "The Settings page: its title, and the cards." []
  `(node#settingsPage :when ~(= state.page "settings")
     :fill true
     :color transparent
     :role tabpanel
     :label ~(t "nav.settings")
     {:id settingsTitle :style pageTitle :text ~(t "settings.title") :role heading}
     {:id settingsSub :style pageSub :text ~(t "settings.sub")}
     ~(settings-cards)))

(defview settings-cards "The settings' cards, one under the other." []
  `(column#settingsList :anchors {:left 36 :top 112 :bottom 0}
     :width ~(min 760 (- (- window.width computed.sideWidth) 72))
     :gap 16
     :scroll vertical
     ~(appearance-card)
     ~(notation-card)
     (node#settingsFolder :use settingsCard :with {:title ~(t "settings.folder") :icon folder-heart :height 128}
       (label#folderText :wrap true
         :textColor @c.muted
         :text ~(t "settings.folderText" {:folder (or-else services.apps.folder "AffrescoUI")})
         :anchors {:left 54 :right 200 :top 56}
         :height 52)
       {:id folderReveal
        :style secondary
        :icon folder-open
        :text ~(t "mine.showFolder")
        :anchors {:right 20 :top 62}
        :width 160
        :height 38
        :onClick ~(handler revealMine "")})
     (node#settingsIntelligence :anchors {:left 0 :right 0} :height 960 :color transparent
       (node#intelligenceSettingsPage :use intelligenceSettings))
     ~(keys-card)
     (node#settingsAbout :use settingsCard :with {:title ~(t "settings.about") :icon info :height 112}
       (label#aboutText :wrap true
         :textColor @c.muted
         :text ~(t "settings.aboutText")
         :anchors {:left 54 :right 20 :top 54}
         :height 44))
     {:id settingsEnd :height 12 :color transparent :anchors {:left 0 :right 0}}))

(defcomponent settingsCard "A card of the settings: its icon and title, then what it holds."
  {:title "" :icon "" :height 120}
  `(node#settingsCardPanel :style card :height ~height :anchors {:left 0 :right 0}
     (label#settingsCardIcon :icon ~icon
       :font bigIcons
       :textColor @c.accent
       :anchors {:left 20 :top 18}
       :width 24
       :height 24)
     {:id settingsCardTitle :style sectionTitle :text ~title :anchors {:left 54 :right 20 :top 18}}
     {:slot true}))

;;; The cards

(defview appearance-card "The theme and the language." []
  `(node#settingsAppearance :use settingsCard :with {:title ~(t "settings.appearance") :icon sun :height 150}
     (label#themeLabel :text ~(t "settings.theme")
       :textColor @c.muted
       :anchors {:left 54 :top 62}
       :width 110
       :height 32
       :align "start center")
     (node#themeChoice :use segmentedControl
       :anchors {:left 170 :top 62}
       :with {:name theme
              :label ~(t "settings.theme")
              :height 32
              :color @c.raised
              :selectedColor @c.surface
              :highlight @c.hover
              :items [{:id system :title ~(t "settings.system") :icon monitor :width 116}
                      {:id light :title ~(t "settings.light") :icon sun :width 100}
                      {:id dark :title ~(t "settings.dark") :icon moon :width 100}]})
     (label#languageLabel :text ~(t "settings.language")
       :textColor @c.muted
       :anchors {:left 54 :top 102}
       :width 110
       :height 32
       :align "start center")
     (node#languageChoice :use segmentedControl
       :anchors {:left 170 :top 102}
       :with {:name locale
              :label ~(t "settings.language")
              :height 32
              :color @c.raised
              :selectedColor @c.surface
              :highlight @c.hover
              :items [{:id system :title ~(t "settings.system") :width 96}
                      {:id en :title "English" :width 80}
                      {:id es :title "Español" :width 80}
                      {:id fr :title "Français" :width 84}]})))

(defview notation-card "The notation the examples open in: JSON or Lisp." []
  `(node#settingsNotation :use settingsCard :with {:title ~(t "settings.notation") :icon braces :height 158}
     (label#notationLabel :text ~(t "settings.notationLabel")
       :textColor @c.muted
       :anchors {:left 54 :top 58}
       :width 110
       :height 32
       :align "start center")
     (node#notationChoice :use segmentedControl
       :anchors {:left 170 :top 58}
       :with {:name notation
              :label ~(t "settings.notationLabel")
              :height 32
              :color @c.raised
              :selectedColor @c.surface
              :highlight @c.hover
              :items [{:id json :title "JSON" :width 80} {:id lisp :title "Lisp" :width 80}]})
     (label#notationText :wrap true
       :textColor @c.muted
       :text ~(t "settings.notationText")
       :anchors {:left 54 :right 20 :top 102}
       :height 44)))

(defview keys-card "The keys, as the platform writes them, and what each does." []
  `(node#settingsKeys :use settingsCard :with {:title ~(t "settings.keys") :icon keyboard :height 208}
     (column#keysList :anchors {:left 54 :right 20 :top 56} :height 134 :gap 4
       ~(repeat [k [["⌘1 · ⌘2 · ⌘4" "settings.keyPages"]
                    ["⌘N" "settings.keyNew"]
                    ["⌘F" "settings.keySearch"]
                    ["⌘R" "settings.keyRefresh"]
                    ["Delete" "settings.keyDelete"]]]
         `(row#keyRow :height 22 :anchors {:left 0 :right 0} :gap 12
            (label#keyCombo :font small
              :text ~(if (= context.platform "mac") k.0 (join "Ctrl+" (split k.0 "⌘")))
              :textColor @c.text
              :color @c.raised
              :radius 6
              :width 150
              :height 22
              :align center)
            (label#keyWhat :font small :text ~(t k.1) :textColor @c.muted :grow 1 :height 22
              :align "start center"))))))
