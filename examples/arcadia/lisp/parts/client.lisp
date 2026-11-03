;;; client.lisp --- the client's own windows, reached from the menus
;;;
;;; Product keys and gift codes, a game from this computer, back up and restore, About and Help, the notice in the
;;; corner, offline, signing out and couch mode (big tiles for a television). Settings is in settings.lisp. Product keys
;;; and gift codes are invented, for trying it out.

;;; What the windows keep

(defstate code {:open false :kind activate :text "" :error ""})

(defstate addGame {:open false :name ""})

(defstate backup {:open false :restored nil})

(defstate info {:open false :kind about})

(defstate session {:signedOut false :offline false})

(defstate couch {:open false :at 0})

(defstate toast {:open false :text ""})

(defstate productKeys {:AR7Q2-KX4MZ-P9T3L glasswing :HX5RE-2PWNA-7LDKC volt})

(defstate giftCodes {:GIFT-2026-ARCD 20 :GIFT-5555-PLUM 5})

(defstate redeemed [])

(defcomputed couchGames "The owned games for couch mode, the one played last first."
  (sort-by (fn [it] (or-else it.lastPlayed 0)) > computed.owned))

(defcomputed couchGame "The game chosen on the couch shelf." (get computed.couchGames state.couch.at))

;;; Codes and games from elsewhere

(defcomponent codeDialog "A product key (adds its game) or a gift code (adds to the wallet); a wrong code says why."
  {}
  `(node :use dialog
     :with {:open state.code.open
            :height 200
            :title ~(t (if (= state.code.kind "activate") "code.activateTitle" "code.redeemTitle"))
            :message ~(t (if (= state.code.kind "activate") "code.activateHelp" "code.redeemHelp"))
            :buttons [{:title ~(t "code.use") :run submitCode :default true :keepOpen true}
                      {:title ~(t "dialog.cancel") :run closeCode :keepOpen true}]}
     ~(code-field)
     (label#code-error :when ~(not= state.code.error "")
       :font small
       :anchors {:left 20 :right 20 :top 118}
       :height 18
       :text ~state.code.error
       :textColor @color.warmEnd
       :live polite)))

(defview code-field "The code's field, red while the code typed was wrong; Enter uses it." []
  `(textarea#code-text :height 32
     :padding 8
     :anchors {:left 20 :right 20 :top 80}
     :borderWidth 1
     :borderColor ~(if (not= state.code.error "") "@color.warmEnd" "@color.border")
     :focused {:borderColor @color.accent}
     :bind state.code.text
     :maxLength 40
     :radius 3
     :color @color.field
     :textColor @color.text
     :placeholderColor @color.dim
     :placeholder ~(if (= state.code.kind "activate") "XXXXX-XXXXX-XXXXX" "GIFT-XXXX-XXXX")
     :label ~(t (if (= state.code.kind "activate") "code.activateTitle" "code.redeemTitle"))
     :autofocus true
     :onSubmit ~(handler submitCode)))

(defcomponent addGameDialog "A game Arcadia doesn't sell, added to the library by its name."
  {}
  `(node :use dialog
     :with {:open state.addGame.open
            :height 180
            :title ~(t "menu.addOther")
            :message ~(t "addGame.help")
            :buttons [{:title ~(t "addGame.add") :run addLocalGame :default true} {:title ~(t "dialog.cancel")}]}
     (textarea#add-game-name :height 32
       :padding 8
       :anchors {:left 20 :right 20 :top 80}
       :borderWidth 1
       :borderColor @color.border
       :focused {:borderColor @color.accent}
       :bind state.addGame.name
       :maxLength 100
       :radius 3
       :color @color.field
       :textColor @color.text
       :placeholderColor @color.dim
       :placeholder ~(t "addGame.placeholder")
       :label ~(t "addGame.placeholder")
       :autofocus true
       :onSubmit ~(handler addLocalGame))))

;;; Backups, information and notices

(defcomponent backupDialog
  "Back up which games are installed (and how they are set up) to a file, or put a backup back."
  {}
  `(node :use dialog
     :with {:open state.backup.open
            :width 460
            :height 170
            :title ~(t "menu.backup")
            :message ~(t "backup.help")
            :buttons [{:title ~(t "backup.save") :run backUp :default true}
                      {:title ~(t "backup.restore") :run restore}
                      {:title ~(t "dialog.cancel")}]}))

(defcomponent infoDialog "About Arcadia, support, privacy and system information: a title and a paragraph."
  {}
  `(node :use dialog
     :with {:open state.info.open
            :width 480
            :height 210
            :title ~(t (str "menu." state.info.kind))
            :message ~(t (str "info." state.info.kind)
                         {:width window.width
                          :height window.height
                          :games (count computed.owned)
                          :installed (count computed.installedGames)
                          :language settings.appearance.language
                          :theme settings.appearance.theme})
            :buttons [{:title ~(t "dialog.close") :default true}]}))

(defcomponent clientToast "A short notice at the bottom right, which goes away by itself."
  {}
  `(node#client-toast :use toast
     :with {:open state.toast.open
            :message ~state.toast.text
            :width 340
            :closeLabel ~(t "dialog.close")
            :color @color.raised
            :textColor @color.text
            :accent @color.accent}))

;;; Offline and signed out

(defcomponent offlinePage "In place of the store and friends while offline."
  {}
  `(column#offline-page :fill true :justify center :alignItems center :color @color.page
     (column :width 420 :height 140 :gap 12 :alignItems center
       (label :icon wifi-off :text "" :font nav :fitText true :textColor @color.muted)
       (label :font heading :fitText true :text ~(t "offline.title") :textColor @color.text)
       (node#offline-online :style pillButton :text ~(t "offline.goOnline") :onClick toggleOffline))))

(defcomponent signedOutScreen "After signing out: the account to sign back in to."
  {}
  `(column#signed-out :overlay {:placement window :open state.session.signedOut :modal true}
     :fill true
     :justify center
     :alignItems center
     :gradient {:kind radial :stops [@color.raised @color.top]}
     (column :width 320 :height 300 :gap 14 :alignItems center :role dialog :label ~(t "signIn.title")
       (label :font logo :fitText true :text "Arcadia" :textColor @color.accent)
       (image :file ~(str "../assets/art/avatars/" state.user.avatar ".svg")
         :width 72
         :height 72
         :radius 36
         :fit cover)
       (label :font heading :fitText true :text ~state.user.name :textColor @color.bright)
       (node#sign-in :style pillButton
         :color @color.accent
         :textColor @color.top
         :font strong
         :text ~(t "signIn.button" {:name state.user.name})
         :autofocus true
         :onClick signIn))))

;;; Couch mode

(defcomponent couchMode
  "Couch mode: big tiles of the games played last; left and right choose, Enter plays, Escape leaves."
  {}
  `(node#couch :overlay {:placement window :open state.couch.open :modal true}
     :fill true
     :gradient {:kind linear :angle 160 :stops [@color.select @color.top]}
     (label :font logo :anchors {:left 48 :top 32} :fitText true :text "Arcadia" :textColor @color.accent)
     (node#couch-leave :style pillButton
       :anchors {:right 48 :top 44}
       :icon x
       :text ~(t "couch.leave")
       :onClick leaveCouch)
     ~(couch-shelf)
     (label :font logo
       :anchors {:left 48 :right 48 :top 500}
       :height 60
       :text ~(or-else computed.couchGame.title "")
       :textColor @color.bright)
     ~(couch-play)
     (label :font small :anchors {:left 48 :bottom 32} :fitText true :text ~(t "couch.hint") :textColor @color.muted)))

(defview couch-shelf "The shelf of big covers, the chosen one larger and outlined; the arrows move along it." []
  `(row#couch-shelf :anchors {:left 48 :right 48 :top 140}
     :height 330
     :gap 24
     :alignItems center
     :focusable true
     :role listbox
     :label ~(t "couch.shelf")
     :autofocus true
     :keys {:left ~(handler couchMove -1)
            :right ~(handler couchMove 1)
            :enter ~(handler primary computed.couchGame)
            :escape leaveCouch}
     ~(repeat [g
               (slice computed.couchGames (max 0 (- state.couch.at 1)) (+ (max 0 (- state.couch.at 1)) 6))
               :index
               i]
        (couch-cover g))))

(defview couch-cover "A game's cover on the couch shelf; a click chooses it." [g]
  `(image :id ~(str "couch-" g.id)
     :let {:on ~(= g.id computed.couchGame.id)}
     :role option
     :label ~g.title
     :selected ~on
     :file ~(str "../assets/art/" (or-else g.art g.id) "/cover.svg")
     :fit cover
     :width ~(if on 210 150)
     :height ~(if on 315 225)
     :radius 12
     :borderWidth ~(if on 4 0)
     :borderColor @color.accent
     :cursor hand
     :onClick ~(handler couchChoose g.id)))

(defview couch-play "The chosen game's button: Play, Stop, Update or Install, as its state is." []
  `(node#couch-play :style pillButton
     :anchors {:left 48 :top 576}
     :height 44
     :radius 22
     :padding 22
     :font button
     :color ~(if (= computed.couchGame.status "ready") "@color.play" "@color.accent")
     :textColor @color.top
     :text ~(t (cond (= computed.couchGame.status "ready") "menu.game.play"
                     (= computed.couchGame.status "running") "menu.game.stop"
                     (= computed.couchGame.status "update") "menu.game.update"
                     :else "menu.game.install"))
     :onClick ~(handler primary computed.couchGame)))
