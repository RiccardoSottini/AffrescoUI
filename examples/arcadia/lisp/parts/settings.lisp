;;; settings.lisp --- Settings: the client's settings, and the dialog that changes them
;;;
;;; A page per category on the left (interface, downloads, storage, account), like a game's properties. Each setting
;;; is a defcustom; the dialog shows each as a row of pills, the current value lit.

;;; The settings

(defcustom appearance.theme "dark" "Dark or light colours for the whole client."
  :type string
  :enum [dark light]
  :title ~(t "setting.theme")
  :description ~(t "setting.theme.about"))

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "setting.language")
  :description ~(t "setting.language.about"))

(defcustom appearance.startPage "library" "The page Arcadia opens on."
  :type string
  :enum [library store community]
  :title ~(t "setting.startPage")
  :description ~(t "setting.startPage.about"))

(defcustom downloads.limit 0 "The most MB/s a download takes; 0 for no limit."
  :type number
  :enum [0 10 25]
  :title ~(t "setting.limit")
  :description ~(t "setting.limit.about"))

(defcustom downloads.whilePlaying true "Off, downloads wait while a game is running."
  :type boolean
  :title ~(t "setting.whilePlaying")
  :description ~(t "setting.whilePlaying.about"))

(defcustom library.sortBy "name" "Name from A to Z, the most hours, or the most recently played."
  :type string
  :enum [name playtime recent]
  :title ~(t "setting.sortBy")
  :description ~(t "setting.sortBy.about"))

;;; What the dialog keeps

(defstate settingsDialog {:open false :page interface})

(defcomputed installedGames "The installed games, the largest first."
  (sort-by (fn [it] (or-else it.sizeGb 0)) > (filter (fn [it] it.installed) data.games)))

(defcomputed usedGb "How much the installed games take, in GB."
  (sum (filter (fn [it] it.installed) data.games) (fn [it] (or-else it.sizeGb 0))))

;;; Its looks: a caps title, a pill, a setting's choices

(defstyle sectionCaps "A section's title, in small capitals." (label :font caps :textColor @color.muted :anchors {:left 0 :right 0} :height 18))

(defstyle pillButton "A rounded button, raised from the page."
  (label :fitText true
    :padding 12
    :height 30
    :radius 15
    :focusable true
    :role button
    :cursor hand
    :color @color.raised
    :textColor @color.text
    :hover {:color @color.hover :textColor @color.bright}))

(defcomponent choice "One choice of a setting, as a pill: lit when it is the current value."
  {:title "" :key "" :value nil :current nil}
  `(node :style pillButton
     :role radio
     :text ~title
     :selected ~(= current value)
     :color ~(if (= current value) "@color.select" "@color.raised")
     :textColor ~(if (= current value) "@color.bright" "@color.text")
     :hover {:color ~(if (= current value) "@color.select" "@color.hover") :textColor @color.bright}
     :onClick ~(handler setSetting key value)))

(defcomponent choiceRow "A setting's caps title over its choices, each { value, title }."
  {:title "" :key "" :current nil :options [] :top 0}
  `(node :anchors {:left 0 :right 0 :top ~top} :height 56 :color transparent
     (node :style sectionCaps :text ~(upper-case title))
     (row :anchors {:left 0 :right 0 :top 22} :height 30 :gap 6 :role radiogroup :label ~title :arrows horizontal
       ~(repeat [o options]
          `(node :id ~(str "setting-" (slug key) "-" o.value)
             :use choice
             :with {:title ~o.title :key ~key :value ~o.value :current ~current})))))

;;; The dialog

(defcomponent settingsDialog "Settings: a page per category on the left, like a game's properties."
  {}
  `(node :use dialog
     :with {:open state.settingsDialog.open
            :width 700
            :height 470
            :title ~(t "menu.settings")
            :color @color.panel
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :buttons [{:title ~(t "dialog.close")}]}
     (row#settings-body :anchors {:left 20 :right 20 :top 52 :bottom 60} :gap 20
       ~(settings-pages)
       (panel :width 1 :alignSelf stretch :color @color.border)
       (node#settings-page :grow 1 :alignSelf stretch :color transparent
         (node :when ~(= state.settingsDialog.page "interface") :use settingsInterface)
         (node :when ~(= state.settingsDialog.page "downloads") :use settingsDownloads)
         (node :when ~(= state.settingsDialog.page "storage") :use settingsStorage)
         (node :when ~(= state.settingsDialog.page "account") :use settingsAccount)))))

(defview settings-pages "The categories down the dialog's left side, the page shown lit." []
  `(column#settings-pages :width 150 :alignSelf stretch :gap 2 :role tablist :arrows vertical
     ~(repeat [page ["interface" "downloads" "storage" "account"]]
        `(label :id ~(str "settings-page-" page)
           :anchors {:left 0 :right 0}
           :height 30
           :padding 10
           :radius 15
           :role tab
           :focusable true
           :cursor hand
           :selected ~(= state.settingsDialog.page page)
           :text ~(t (str "settings.page." page))
           :color ~(if (= state.settingsDialog.page page) "@color.select" "transparent")
           :textColor ~(if (= state.settingsDialog.page page) "@color.bright" "@color.muted")
           :hover {:textColor @color.bright}
           :onClick ~(handler showSettingsPage page)))))

;;; Interface

(defcomponent settingsInterface "The interface page: the colours, the language, the page to open on and the library's order."
  {}
  `(node :fill true :color transparent
     (node :use choiceRow
       :with {:top 0
              :title ~(t "settings.theme")
              :key appearance.theme
              :current ~settings.appearance.theme
              :options ~[{:value "dark" :title (t "settings.dark")} {:value "light" :title (t "settings.light")}]})
     (node :use choiceRow
       :with {:top 64
              :title ~(t "settings.language")
              :key appearance.language
              :current ~settings.appearance.language
              :options [{:value en :title "English"} {:value es :title "Español"} {:value fr :title "Français"}]})
     (node :use choiceRow
       :with {:top 128
              :title ~(t "settings.startPage")
              :key appearance.startPage
              :current ~settings.appearance.startPage
              :options ~(map (fn [it] {:value it :title (t (str "nav." it))}) ["library" "store" "community"])})
     (node :use choiceRow
       :with {:top 192
              :title ~(t "settings.sortBy")
              :key library.sortBy
              :current ~settings.library.sortBy
              :options ~(map (fn [it] {:value it :title (t (str "sort." it))}) ["name" "playtime" "recent"])})))

;;; Downloads

(defcomponent settingsDownloads "The downloads page: the speed limit, downloading while playing, and the download cache."
  {}
  `(node :fill true :color transparent
     (node :use choiceRow
       :with {:top 0
              :title ~(t "settings.limit")
              :key downloads.limit
              :current ~settings.downloads.limit
              :options ~(map (fn [it] {:value it :title (if (= it 0) (t "settings.noLimit") (t "settings.limitValue" {:n it}))})
                             [0 10 25])})
     (row#settings-while-playing :anchors {:left 0 :right 0 :top 72} :height 30 :alignItems center
       (label :grow 1 :height 22 :text ~(t "settings.whilePlaying") :textColor @color.text)
       (toggle#settings-while-playing-toggle :width 40
         :height 22
         :label ~(t "settings.whilePlaying")
         :checked ~settings.downloads.whilePlaying
         :activeColor @color.accent
         :onChange ~(handler setSetting "downloads.whilePlaying" event.value)))
     (node :style sectionCaps :anchors {:left 0 :right 0 :top 124} :text ~(upper-case (t "settings.cache")))
     (label :font small
       :wrap true
       :anchors {:left 0 :right 0 :top 146}
       :height 36
       :text ~(t "settings.cacheNote")
       :textColor @color.muted)
     (node#settings-clear-cache :style pillButton :anchors {:left 0 :top 186} :text ~(t "settings.clearCache") :onClick clearCache)))

;;; Storage

(defmacro used-text
  ;; A macro: the text is an expression, shown and read aloud.
  "How much of the drive is used: the games and 180 GB of everything else, of 1,000 GB."
  []
  `(t "settings.used" {:used (formatNumber (+ computed.usedGb 180) 0 1) :total (formatNumber 1000)}))

(defmacro game-colour
  ;; A macro: the colour is an expression of the repeat's index.
  "The colour of the installed game at place i, the same in the bar and the list."
  [i]
  `(get ["@color.accent" "@color.play" "@color.online" "@color.warm" "@color.link"] (mod ~i 5)))

(defcomponent settingsStorage "The storage page: the drive's use, a bar of the games on it, and each game with a way to uninstall it."
  {}
  `(node :fill true :color transparent
     (node :style sectionCaps :text ~(upper-case (t "settings.drive")))
     (label#storage-used :font small :anchors {:left 0 :right 0 :top 22} :height 18 :text ~(used-text) :textColor @color.text)
     (row#storage-bar :anchors {:left 0 :right 0 :top 46} :height 12 :radius 6 :color @color.raised :label ~(used-text)
       (node :width "18%" :alignSelf stretch :color @color.dim)
       ~(repeat [g computed.installedGames :index i]
          `(node :width ~(str (text (/ g.sizeGb 10)) "%") :alignSelf stretch :color ~(game-colour i))))
     (column#storage-games :anchors {:left 0 :right 0 :top 72 :bottom 0} :scroll vertical
       ~(repeat [g computed.installedGames :index i]
          (storage-game g i)))))

(defview storage-game "An installed game on the storage page: its colour, name and size, and Uninstall." [g i]
  `(row :id ~(str "storage-" g.id) :anchors {:left 0 :right 0} :height 32 :gap 10 :alignItems center
     (panel :width 10 :height 10 :radius 5 :color ~(game-colour i))
     (label :grow 1 :height 20 :text ~g.title :textColor @color.text)
     (label :font small
       :fitText true
       :text ~(t "properties.sizeValue" {:size (formatNumber g.sizeGb 0 1)})
       :textColor @color.muted)
     (label :id ~(str "storage-uninstall-" g.id)
       :font small
       :fitText true
       :padding 6
       :radius 3
       :text ~(t "menu.game.uninstall")
       :textColor ~(if (= g.status "running") "@color.dim" "@color.link")
       :disabled ~(= g.status "running")
       :focusable true
       :role button
       :cursor hand
       :hover {:color @color.hover}
       :onClick ~(handler askUninstall g.id))))

;;; Account

(defcomponent settingsAccount "The account page: the avatar, the name and friend code, the wallet, and signing out."
  {}
  `(node :fill true :color transparent
     (image :file ~(str "../assets/art/avatars/" state.user.avatar ".svg")
       :anchors {:left 0 :top 0}
       :width 64
       :height 64
       :radius 32
       :fit cover)
     (label :font heading :anchors {:left 80 :top 8} :fitText true :text ~state.user.name :textColor @color.bright)
     (label :font small
       :anchors {:left 80 :top 36}
       :fitText true
       :text ~(t "settings.friendCode" {:code state.user.code})
       :textColor @color.muted)
     (node :style sectionCaps :anchors {:left 0 :right 0 :top 88} :text ~(upper-case (t "settings.wallet")))
     (label#settings-wallet :font heading
       :anchors {:left 0 :top 110}
       :fitText true
       :text ~(formatCurrency state.user.wallet "EUR")
       :textColor @color.text)
     (node#settings-redeem :style pillButton :anchors {:left 0 :top 144} :text ~(t "menu.redeem") :onClick ~(handler openCode "redeem"))
     (node#settings-sign-out :style pillButton :anchors {:left 0 :top 196} :text ~(t "account.signOut") :onClick signOut)))
