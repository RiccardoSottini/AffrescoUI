;;; arcadia.lisp --- what Arcadia does: its commands and functions
;;;
;;; The screen and its parts (index.screen.lisp, parts/store.lisp, parts/library.lisp...) say what is shown; this file
;;; says what happens. Every action is a command, as an outsider wrote them: a title, a category, and what a person or an
;;; agent may run by name from the palette. A command reads the running app through paths (`state.cart`, `data.games`,
;;; `computed.here`) and changes it only through steps (`set`, `update-record`, `insert`...). A handler runs one by name,
;;; its parameters by name: ~(handler openGame "g1").

(ns arcadia.arcadia)

;;; Lists, games and places

(defn otherwise "A value, or another when it is nil (an expression's ??)." [value other]
  (if (some? value) value other))

(defn has "Whether a list holds a value." [items value]
  (some #(= % value) items))

(defn without "A list without a value." [items value]
  (remove #(= % value) items))

(defn toggled "A list without a value it holds, or with it at the end." [items value]
  (if (has items value) (without items value) (concat items [value])))

(defn swapped "A list with the items at two places swapped." [items i j]
  (map-indexed (fn [at item] (condp = at i (get items j) j (get items i) item)) items))

(defn game-of "A game of the library, by its id." [id]
  (find #(= (:id %) id) data.games))

(defn hide-toast "Hides the notice at the bottom right." [] (set "state.toast.open" false))

(defn scroll-chat "Scrolls the chat to its last message." [] (scrollTo "#chat-end"))

;;; Going about

(defcommand go "Goes to a tab, a page and an id, dropping the pages ahead of this one, as a browser does."
  :title ~(t "command.go") :description ~(t "command.go.about") :category ~(t "category.view")
  [tab page id]
  (let [here computed.here]
    (when (or (not= tab (:tab here)) (not= page (:page here)) (not= id (:id here)))
      (set "state.nav" (concat (take (inc state.at) state.nav) [{:tab tab :page page :id id}]))
      (set "state.at" (inc state.at)))))

(defcommand back "Goes back to the page before."
  :title ~(t "command.back") :description ~(t "command.back.about") :category ~(t "category.view") :icon "arrow-left"
  :enablement ~(> state.at 0)
  []
  (set "state.at" (max 0 (dec state.at))))

(defcommand forward "Goes forward to the page after."
  :title ~(t "command.forward") :description ~(t "command.forward.about") :category ~(t "category.view")
  :icon "arrow-right" :enablement ~(< state.at (dec (count state.nav)))
  []
  (set "state.at" (min (dec (count state.nav)) (inc state.at))))

(defcommand showStore "Opens the store's front page."
  :title ~(t "command.showStore") :description ~(t "command.showStore.about") :category ~(t "category.view")
  [] (go :store :home nil))

(defcommand showLibrary "Opens the library's home."
  :title ~(t "command.showLibrary") :description ~(t "command.showLibrary.about") :category ~(t "category.view")
  [] (go :library :home nil))

(defcommand showCommunity "Opens the friends list and the chat."
  :title ~(t "command.showCommunity") :description ~(t "command.showCommunity.about") :category ~(t "category.view")
  [] (go :community :home nil))

(defcommand showProfile "Opens your profile."
  :title ~(t "command.showProfile") :description ~(t "command.showProfile.about") :category ~(t "category.view")
  [] (go :profile :home nil))

(defcommand toggleTheme "Switches the whole client between dark and light colours."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about") :category ~(t "category.view")
  []
  (set "settings.appearance.theme" (if (= settings.appearance.theme :dark) :light :dark)))

(defcommand nextLanguage "Changes to the next language: English, Spanish, French."
  :title ~(t "command.nextLanguage") :description ~(t "command.nextLanguage.about") :category ~(t "category.view")
  []
  (set "settings.appearance.language"
       (case settings.appearance.language :en :es :es :fr :en)))

(defn open-start-page
  "Opens Arcadia on the page Settings choose, when it isn't the library; the assistant's launcher sits above the
  bottom bar." []
  (set "state.assistant.floor" 40)
  (when (not= settings.appearance.startPage :library)
    (set "state.nav" [{:tab settings.appearance.startPage :page :home :id nil}])))

;;; The client: settings, notices, codes, backups, couch mode

(defcommand setSetting "Changes a setting to a value."
  :title ~(t "command.setSetting") :description ~(t "command.setSetting.about") :category ~(t "category.settings")
  [key value]
  (set (str "settings." key) value))

(defcommand openSettings "Opens Settings at a page (interface, downloads, storage or account)."
  :title ~(t "command.openSettings") :description ~(t "command.openSettings.about") :category ~(t "category.arcadia")
  :icon "settings"
  [page]
  (dismiss)
  (set "state.settingsDialog" {:open true :page (otherwise page "interface")}))

(defcommand notify "Shows a short notice at the bottom right; it goes away by itself."
  :title ~(t "command.notify") :description ~(t "command.notify.about") :category ~(t "category.arcadia")
  [text]
  (set "state.toast" {:open true :text text})
  (debounce 4000 hide-toast))

(defn up-to-date "Says the client is up to date." [] (notify (t "notice.upToDate")))

(defcommand checkUpdates "Checks whether a newer Arcadia is out."
  :title ~(t "command.checkUpdates") :description ~(t "command.checkUpdates.about") :category ~(t "category.arcadia")
  []
  (notify (t "notice.checking"))
  (after 1200 up-to-date))

(defcommand clearCache "Clears the download cache; installed games stay."
  :title ~(t "command.clearCache") :description ~(t "command.clearCache.about") :category ~(t "category.settings")
  []
  (notify (t "settings.cacheCleared")))

(defcommand toggleOffline "Offline, the store and friends wait for the network; installed games still play."
  :title ~(t "command.toggleOffline") :description ~(t "command.toggleOffline.about")
  :category ~(t "category.arcadia")
  []
  (toggle "state.session.offline")
  (notify (t (if state.session.offline "notice.offline" "notice.online"))))

(defcommand signOut "Signs out of the account, back to the sign-in screen."
  :title ~(t "command.signOut") :description ~(t "command.signOut.about") :category ~(t "category.arcadia")
  []
  (dismiss)
  (set "state.session.signedOut" true)
  (set "state.couch.open" false))

(defcommand signIn "Signs back in to the account."
  :title ~(t "command.signIn") :description ~(t "command.signIn.about") :category ~(t "category.arcadia")
  [] (set "state.session.signedOut" false))

(defcommand openCode "Opens the code window: a product key adds a game (activate), a gift code adds to the wallet (redeem)."
  :title ~(t "command.openCode") :description ~(t "command.openCode.about") :category ~(t "category.games")
  [kind]
  (set "state.code" {:open true :kind kind :text "" :error ""}))

(defcommand closeCode "Closes only the code window, so Settings stays open under it."
  :title ~(t "command.closeCode") :description ~(t "command.closeCode.about") :category ~(t "category.games")
  []
  (set "state.code.open" false))

(defcommand submitCode "Checks the code typed; a good one adds its game or its amount, a wrong one says why."
  :title ~(t "command.submitCode") :description ~(t "command.submitCode.about") :category ~(t "category.games")
  []
  (set "state.code.text" (-> state.code.text str/trim str/upper-case))
  (if (= state.code.kind :activate)
    (activateKey (get state.productKeys state.code.text))
    (redeemGift (get state.giftCodes state.code.text))))

(defcommand activateKey "Checks a product key, and adds its game to the library."
  :title ~(t "command.activateKey") :description ~(t "command.activateKey.about") :category ~(t "category.games")
  [id]
  (cond (nil? id) (set "state.code.error" (t "code.badKey"))
        (:owned (game-of id)) (set "state.code.error" (t "code.owned"))
        :else (do (update-record "games" :id id :fields {:owned true})
                  (set "state.code.open" false)
                  (notify (t "code.added" {:game (:title (game-of id))}))
                  (openGame id))))

(defcommand redeemGift "Checks a gift code, and adds its amount to the wallet."
  :title ~(t "command.redeemGift") :description ~(t "command.redeemGift.about") :category ~(t "category.games")
  [amount]
  (cond (nil? amount) (set "state.code.error" (t "code.badGift"))
        (has state.redeemed state.code.text) (set "state.code.error" (t "code.used"))
        :else (do (push "state.redeemed" state.code.text)
                  (set "state.user.wallet" (-> state.user.wallet (+ amount) (* 100) round (/ 100)))
                  (set "state.code.open" false)
                  (notify (t "code.funded" {:amount (formatCurrency amount "EUR")})))))

(defcommand openAddGame "Opens the window that adds a game from this computer."
  :title ~(t "command.openAddGame") :description ~(t "command.openAddGame.about") :category ~(t "category.games")
  :icon "package-plus"
  []
  (dismiss)
  (set "state.addGame" {:open true :name ""}))

(defn open-added "The game just added, its page." [result] (openGame result.id))

(defcommand addLocalGame "Adds a game Arcadia doesn't sell to the library, ready to play from it."
  :title ~(t "command.addLocalGame") :description ~(t "command.addLocalGame.about") :category ~(t "category.games")
  []
  (let [title (str/trim state.addGame.name)]
    (when-not (str/blank? title)
      (insert "games"
              :value {:title title :genre "Other" :owned true :installed true :status "ready"
                      :sizeGb 0 :local true :art "local" :logo "#f5a524" :achievements [0 0]}
              :then open-added)
      (set "state.addGame.open" false))))

(defcommand openBackup "Opens the window to back up the installed games or restore a backup."
  :title ~(t "command.openBackup") :description ~(t "command.openBackup.about") :category ~(t "category.arcadia")
  []
  (dismiss)
  (set "state.backup" {:open true :restored nil}))

(defn backed-up "Says the backup is saved." [] (notify (t "backup.saved")))

(defcommand backUp "Saves which games are installed, with their launch options and update choices, as a JSON file."
  :title ~(t "command.backUp") :description ~(t "command.backUp.about") :category ~(t "category.arcadia")
  []
  (saveFile {:format "arcadia-backup"
             :version 1
             :games (->> data.games
                         (filter :installed)
                         (map (fn [{:keys [id launchOptions overlay cloud updates]}]
                                {:id id :launchOptions (otherwise launchOptions "") :overlay overlay :cloud cloud
                                 :updates updates})))}
            :name "arcadia-backup.json"
            :onDone backed-up))

(defn restore-games "Puts back the settings of the games a backup names that are in the library." []
  (let [owned (into #{} (map :id computed.owned))]
    (doseq [{:keys [id launchOptions updates]} (otherwise state.backup.restored.games [])]
      (when (owned id)
        (update-record "games" :id id
                       :fields {:launchOptions (text (otherwise launchOptions "")) :updates (otherwise updates "auto")}))))
  (notify (t "backup.restored")))

(defcommand restore "Reads a backup and puts back the settings of the games it names that are in the library."
  :title ~(t "command.restore") :description ~(t "command.restore.about") :category ~(t "category.arcadia")
  []
  (openFile "state.backup.restored" :extensions ["json"] :onDone restore-games))

(defcommand showInfo "Shows About Arcadia, support, privacy or the system's details."
  :title ~(t "command.showInfo") :description ~(t "command.showInfo.about") :category ~(t "category.help")
  [kind]
  (dismiss)
  (set "state.info" {:open true :kind kind}))

(defcommand openNotice "Opens what a notification is about, and marks it read."
  :title ~(t "command.openNotice") :description ~(t "command.openNotice.about") :category ~(t "category.arcadia")
  [i]
  (set (str "state.notifications[" i "].unread") false)
  (case (:icon (get state.notifications i))
    :download (showDownloads)
    :user-plus (showCommunity)
    (showStore)))

(defcommand enterCouch "Big tiles for a television across the room, the whole screen, arrows and Enter."
  :title ~(t "command.enterCouch") :description ~(t "command.enterCouch.about") :category ~(t "category.view")
  :icon "sofa"
  []
  (dismiss)
  (set "state.couch" {:open true :at 0 :section "library"})
  (fullscreen true)
  (focus "#couch-shelf"))

(defcommand leaveCouch "Leaves couch mode."
  :title ~(t "command.leaveCouch") :description ~(t "command.leaveCouch.about") :category ~(t "category.view")
  []
  (set "state.couch.open" false)
  (fullscreen false))

(defcommand couchMove "Moves along the couch shelf, left or right."
  :title ~(t "command.couchMove") :description ~(t "command.couchMove.about") :category ~(t "category.view")
  [by]
  (let [lastAt (dec (count computed.couchItems))]
    (set "state.couch.at" (clamp (+ state.couch.at by) 0 lastAt))))

(defn show-settings-page "Shows a page of Settings: interface, downloads, storage or account." [page]
  (set "state.settingsDialog.page" page))

(defn couch-choose "Chooses a tile on the couch shelf, by its id." [id]
  (set "state.couch.at" (indexOf (map :id computed.couchItems) id)))

(defn couch-section "Shows a row of couch mode: your games, the store, your friends or settings, from its first tile."
  [section]
  (set "state.couch.section" section)
  (set "state.couch.at" 0)
  (focus "#couch-shelf"))

(defn couch-step "Shows the row of couch mode before or after this one." [by]
  (let [at (clamp (+ (indexOf couch-sections state.couch.section) by) 0 (dec (count couch-sections)))]
    (couch-section (get couch-sections at))))

(defn couch-change "Steps a couch setting to its next value, round to the first after the last." [item]
  (let [{:keys [values at key]} item
        next (mod (inc (indexOf values at)) (count values))]
    (setSetting key (get values next))))

(defn couch-enter
  "Does what the chosen tile's button says: plays a game of yours, opens a store game's page, chats with a friend (the
  last two leave couch mode), or steps a setting to its next value."
  []
  (let [id (:id computed.couchItem)]
    (case state.couch.section
      "settings" (couch-change computed.couchItem)
      "store" (do (leaveCouch) (openStore id))
      "friends" (do (leaveCouch) (showCommunity) (openChat id))
      (primary computed.couchGame))))

;;; Friends

(defcommand openChat "Opens the chat with a friend."
  :title ~(t "command.openChat") :description ~(t "command.openChat.about") :category ~(t "category.friends")
  :icon "message-circle"
  [id]
  (set "state.chatWith" id)
  (set "state.chatDraft" "")
  (set "state.read" (assoc state.read id (now)))
  (go :community :home nil)
  (scrollTo "#chat-end")
  (focus "#chat-input"))

(defn start-typing "Shows the friend replied to as typing." []
  (set "state.typing" state.replyTo)
  (scrollTo "#chat-end"))

(defn reply "The friend's answer, as a new message of theirs." []
  (let [sent (count (filter :mine data.messages))]
    (insert "messages" :value {:friend state.replyTo :at (now) :text (t (str "chat.reply" (mod sent 3)))}))
  (set "state.typing" nil)
  (set "state.replyTo" nil)
  (scrollTo "#chat-end"))

(defcommand awaitReply "An online friend starts typing a moment later and answers; an offline one reads it later."
  :title ~(t "command.awaitReply") :description ~(t "command.awaitReply.about") :category ~(t "category.friends")
  []
  (when (and (not= computed.chatFriend.presence :offline) (nil? state.replyTo))
    (set "state.replyTo" state.chatWith)
    (after 900 start-typing)
    (after 3200 reply)))

(defcommand sendMessage "Sends the message written to the friend in the chat."
  :title ~(t "command.sendMessage") :description ~(t "command.sendMessage.about") :category ~(t "category.friends")
  :icon "send" :enablement ~(and (some? computed.chatFriend) (not= (trim state.chatDraft) ""))
  []
  (let [draft (str/trim state.chatDraft)]
    (when (and (some? computed.chatFriend) (not (str/blank? draft)))
      (insert "messages" :value {:friend state.chatWith :mine true :at (now) :text draft})
      (set "state.chatDraft" "")
      (after 50 scroll-chat)
      (awaitReply))))

(defcommand inviteToPlay "Invites the friend in the chat to the game running, or the one played last."
  :title ~(t "command.inviteToPlay") :description ~(t "command.inviteToPlay.about") :category ~(t "category.friends")
  :icon "gamepad-2" :enablement ~(and (some? computed.chatFriend) (some? computed.inviteGame))
  []
  (when (and (some? computed.chatFriend) (some? computed.inviteGame))
    (insert "messages" :value {:friend state.chatWith :mine true :at (now) :game computed.inviteGame.id})
    (after 50 scroll-chat)))

(defcommand setStatus "Sets how you show to friends: online, away, invisible or offline."
  :title ~(t "command.setStatus") :description ~(t "command.setStatus.about") :category ~(t "category.friends")
  [status] (set "state.user.status" status))

(defcommand addFriend "Sends a friend request by name; they show as offline until they answer."
  :title ~(t "command.addFriend") :description ~(t "command.addFriend.about") :category ~(t "category.friends")
  :icon "user-plus"
  []
  (let [name (str/trim state.addFriend.name)]
    (when-not (str/blank? name)
      (insert "friends" :value {:name name :avatar "ghost" :presence "offline" :since 0})
      (set "state.addFriend" {:open false :name ""}))))

(defcommand addFriendDialog "Opens the window that adds a friend by name."
  :title ~(t "command.addFriendDialog") :description ~(t "command.addFriendDialog.about")
  :category ~(t "category.friends") :icon "user-plus"
  []
  (go :community :home nil)
  (set "state.addFriend" {:open true :name ""}))

(defcommand showProfileOf "Opens your profile from the friends list."
  :title ~(t "command.showProfileOf") :description ~(t "command.showProfileOf.about")
  :category ~(t "category.friends")
  [] (go :profile :home nil))

;;; Downloads

(defn download-path "A field of a download of the state, as a path." [i key]
  (str "state.downloads[" i "]." key))

(defcommand showDownloads "Opens the downloads page."
  :title ~(t "command.showDownloads") :description ~(t "command.showDownloads.about")
  :category ~(t "category.downloads") :icon "download"
  [] (go :downloads :home nil))

(defcommand dlPause "Pauses the downloads, or resumes them."
  :title ~(t "command.dlPause") :description ~(t "command.dlPause.about") :category ~(t "category.downloads")
  :icon "pause"
  [] (toggle "state.dlPaused"))

(defcommand dlStart "Moves an item to the top and starts it; what was downloading waits at the head of the queue."
  :title ~(t "command.dlStart") :description ~(t "command.dlStart.about") :category ~(t "category.downloads")
  :icon "download"
  [id]
  (doseq [[i d] (map-indexed vector state.downloads)]
    (when (= (:status d) :downloading) (set (download-path i "status") :queued))
    (when (= (:id d) id) (set (download-path i "status") :downloading)))
  (set "state.dlPaused" false))

(defcommand dlMove "Swaps two downloads, by their places in the list."
  :title ~(t "command.dlMove") :description ~(t "command.dlMove.about") :category ~(t "category.downloads")
  [i j]
  (set "state.downloads" (swapped state.downloads i j)))

(defcommand dlUnschedule "Takes a download off the schedule."
  :title ~(t "command.dlUnschedule") :description ~(t "command.dlUnschedule.about")
  :category ~(t "category.downloads")
  [id]
  (doseq [[i d] (map-indexed vector state.downloads)]
    (when (= (:id d) id) (set (download-path i "status") :paused))))

(defcommand dlRemove "Removes a download from the list."
  :title ~(t "command.dlRemove") :description ~(t "command.dlRemove.about") :category ~(t "category.downloads")
  :icon "x"
  [id]
  (let [game (game-of id)]
    (when (= (:status game) :downloading)
      (update-record "games" :id id :fields {:status (if (:installed game) "update" "none")})))
  (set "state.downloads" (remove #(= (:id %) id) state.downloads)))

(defcommand dlClear "Clears the completed downloads from the list."
  :title ~(t "command.dlClear") :description ~(t "command.dlClear.about") :category ~(t "category.downloads")
  []
  (set "state.downloads" (remove #(= (:status %) :done) state.downloads)))

(defcommand dlFinish "Marks a download done, its game ready, and starts the next in the queue."
  :title ~(t "command.dlFinish") :description ~(t "command.dlFinish.about") :category ~(t "category.downloads")
  [id]
  (doseq [[i d] (map-indexed vector state.downloads)]
    (when (= (:id d) id)
      (set (download-path i "status") :done)
      (set (download-path i "doneGb") (otherwise (:sizeGb d) (:sizeGb (game-of id))))))
  (update-record "games" :id id :fields {:status "ready" :installed true})
  (when-some [queued (find #(= (:status %) :queued) state.downloads)]
    (dlStart (:id queued))))

(defn dl-up "Moves the queued download at place k of the queue up one, unless it is first." [k]
  (when (pos? k)
    (dlMove (get-in computed.dlQueue [k :at]) (get-in computed.dlQueue [(dec k) :at]))))

(defn dl-down "Moves the queued download at place k of the queue down one, unless it is last." [k]
  (when (< k (dec (count computed.dlQueue)))
    (dlMove (get-in computed.dlQueue [k :at]) (get-in computed.dlQueue [(inc k) :at]))))

(defn region-share "How much of the full rate the download region's servers give: the nearest give it all." []
  (case settings.downloads.region
    :americas 0.75
    :asia 0.6
    1))

(defn next-rate
  "The next sample of the rate, in MB/s: a wave that repeats, as the region's servers give it, under the speed limit if
  there is one."
  []
  (let [wave (+ 34 (mod (* state.dlTick 7) 19))
        rate (round (* (region-share) wave))]
    (if (pos? settings.downloads.limit) (min settings.downloads.limit rate) rate)))

(defn hour-now "The hour of the day now, from 0 to 23 (UTC)." []
  (-> (now) (/ 3600000) floor (mod 24)))

(defn download-tick
  ;; Not while a game runs, unless Settings allow it.
  "Every second while a download runs: a new sample of the rate, the download moved on by it, finished when it's there;
  outside the schedule's hours, nothing moves."
  []
  (set "state.dlHour" (hour-now))
  (when (and (some? computed.download)
             (not state.dlPaused)
             (not computed.dlWaiting)
             (or settings.downloads.whilePlaying (nil? state.running)))
    (set "state.dlTick" (inc state.dlTick))
    (set "state.dlRate" (concat (rest state.dlRate) [(next-rate)]))
    (let [step (/ (get state.dlRate 47) 1024)]
      (doseq [[i d] (map-indexed vector state.downloads)]
        (when (= (:status d) :downloading)
          (set (download-path i "doneGb") (min computed.dlSize (+ (:doneGb d) step))))))
    (when (>= computed.download.doneGb computed.dlSize)
      (dlFinish computed.download.id))))

;;; A game's hub and the library

(defcommand openHub "Opens a game's player hub, at everything, the discussions or the guides."
  :title ~(t "command.openHub") :description ~(t "command.openHub.about") :category ~(t "category.game")
  [id section]
  (set "state.hubTab" (otherwise section "hub"))
  (set "state.hubPost" nil)
  (set "state.hubShot" nil)
  (go :library :hub id))

(defcommand toggleCollections "Folds every collection of the game list, or unfolds them all."
  :title ~(t "command.toggleCollections") :description ~(t "command.toggleCollections.about")
  :category ~(t "category.library")
  []
  (let [sections computed.sections]
    (set "state.collapsed"
         (if (not-every? #(get state.collapsed (:id %)) sections)
           (zipmap (map :id sections) (map (constantly true) sections))
           {}))))

(defcommand toggleFilter "Turns a library filter on or off: ready to play, favourites, recent first, or the news."
  :title ~(t "command.toggleFilter") :description ~(t "command.toggleFilter.about") :category ~(t "category.library")
  [key]
  (toggle (str "state." key)))

(defcommand clearFilters "Clears the library's filters and search."
  :title ~(t "command.clearFilters") :description ~(t "command.clearFilters.about") :category ~(t "category.library")
  []
  (set "state.readyOnly" false)
  (set "state.recentFirst" false)
  (set "state.favOnly" false)
  (set "state.query" ""))

(defcommand editNote "Opens the note about a game, to write in it."
  :title ~(t "command.editNote") :description ~(t "command.editNote.about") :category ~(t "category.game")
  [id]
  (set "state.note" {:open true :id id :text (otherwise (:note (game-of id)) "")}))

(defcommand saveNote "Saves the note with its game."
  :title ~(t "command.saveNote") :description ~(t "command.saveNote.about") :category ~(t "category.game")
  []
  (update-record "games" :id state.note.id :fields {:note (str/trim state.note.text)}))

(defcommand openGame "Opens a game's page in the library."
  :title ~(t "command.openGame") :description ~(t "command.openGame.about") :category ~(t "category.game")
  [id]
  (set "state.selected" id)
  (go :library :game id))

(defcommand libraryHome "Opens the library's home: what's new, recent games and the shelves."
  :title ~(t "command.libraryHome") :description ~(t "command.libraryHome.about") :category ~(t "category.view")
  :icon "layout-dashboard"
  []
  (go :library :home nil))

(defcommand play "Starts a game, in safe mode if asked."
  :title ~(t "command.play") :description ~(t "command.play.about") :category ~(t "category.game") :icon "play"
  [id]
  (update-record "games" :id id :fields {:status "running" :lastPlayed state.today})
  (set "state.running" id))

(defcommand stop "Stops the game running."
  :title ~(t "command.stop") :description ~(t "command.stop.about") :category ~(t "category.game") :icon "square"
  [id]
  (update-record "games" :id id :fields {:status "ready" :hours (+ (:hours (game-of id)) 0.5)})
  (set "state.running" nil))

(defcommand install "Queues the game, or its update: it downloads at once if nothing else is."
  :title ~(t "command.install") :description ~(t "command.install.about") :category ~(t "category.game")
  :icon "download"
  [id]
  (let [game (game-of id)
        updating (= (:status game) :update)]
    (when (and (some? game) (not-any? #(and (= (:id %) id) (not= (:status %) :done)) state.downloads))
      (push "state.downloads" {:id id
                               :sizeGb (if updating 1.2 (:sizeGb game))
                               :doneGb 0
                               :status (if computed.download :queued :downloading)
                               :kind (if updating :update :install)})
      (update-record "games" :id id :fields {:status "downloading"}))))

(defcommand toggleFavourite "Adds a game to the favourites, or takes it out."
  :title ~(t "command.toggleFavourite") :description ~(t "command.toggleFavourite.about")
  :category ~(t "category.game") :icon "star"
  [id]
  (update-record "games" :id id :fields {:favourite (not (:favourite (game-of id)))}))

(defcommand setSort "Sorts All Games by name, hours played or last played."
  :title ~(t "command.setSort") :description ~(t "command.setSort.about") :category ~(t "category.library")
  [by]
  (set "settings.library.sortBy" by))

(defcommand primary "Does what a game's button says: play, stop, install or update."
  :title ~(t "command.primary") :description ~(t "command.primary.about") :category ~(t "category.game")
  [game]
  (let [{:keys [id status]} game]
    (case status
      :ready (play id)
      :running (stop id)
      (:none :update) (install id)
      nil)))

(defn fold-section "Folds a section of the game list if it is open, or unfolds it." [id open]
  (set (str "state.collapsed." id) open))

(defn open-section "Opens a section of a game: its store page, its extra content, support, or a part of its hub." [id section]
  (case section
    :store (openStore id)
    :dlc (openProperties id "dlc")
    :support (showInfo "support")
    (openHub id section)))

(defn show-hub-tab "Shows a section of a game's hub: everything, the discussions or the guides." [tab]
  (set "state.hubTab" tab))

(defcommand openHubPost "Opens a post of the player hub on its thread: what it says, its replies, and a box to reply in."
  :title ~(t "command.openHubPost") :description ~(t "command.openHubPost.about") :category ~(t "category.game")
  [post]
  (set "state.hubDraft" "")
  (set "state.hubPost" post))

(defcommand closeHubPost "Goes back from a post's thread to the hub's list of posts."
  :title ~(t "command.closeHubPost") :description ~(t "command.closeHubPost.about") :category ~(t "category.game")
  :enablement ~(some? state.hubPost)
  []
  (set "state.hubPost" nil))

(defcommand replyToPost "Posts the reply written under the open post of the player hub."
  :title ~(t "command.replyToPost") :description ~(t "command.replyToPost.about") :category ~(t "category.game")
  :icon "send" :enablement ~(and (some? state.hubPost) (not= (trim state.hubDraft) ""))
  []
  (let [draft (str/trim state.hubDraft)]
    (when (and (some? state.hubPost) (not (str/blank? draft)))
      (push "state.hubReplies" {:post state.hubPost :text draft})
      (set "state.hubDraft" ""))))

(defcommand openHubShot "Opens a screenshot of the player hub big, by its place, with its caption and its vote."
  :title ~(t "command.openHubShot") :description ~(t "command.openHubShot.about") :category ~(t "category.game")
  [n]
  (set "state.hubShot" n))

(defcommand closeHubShot "Goes back from an open screenshot to the hub's screenshots."
  :title ~(t "command.closeHubShot") :description ~(t "command.closeHubShot.about") :category ~(t "category.game")
  :enablement ~(some? state.hubShot)
  []
  (set "state.hubTab" "screenshots")
  (set "state.hubShot" nil))

(defn step-hub-shot "Shows the hub's screenshot before (-1) or after (1) the open one, round the ends." [by]
  (let [n (count state.hubShots)]
    (set "state.hubShot" (mod (+ state.hubShot by n) n))))

(defcommand voteHubPost "Marks a post of the player hub as helpful, or takes the vote back."
  :title ~(t "command.voteHubPost") :description ~(t "command.voteHubPost.about") :category ~(t "category.game")
  :icon "thumbs-up"
  [post]
  (set "state.hubVoted" (toggled state.hubVoted post)))

;;; Managing a game

(defcommand openGameMenu "Opens the menu of a game at the pointer (or the focused item)."
  :title ~(t "command.openGameMenu") :description ~(t "command.openGameMenu.about") :category ~(t "category.game")
  [id]
  (set "state.manage" id)
  (set "state.gameMenu" {:open true :x event.x :y event.y}))

(defcommand openProperties "Opens a game's properties, at a page."
  :title ~(t "command.openProperties") :description ~(t "command.openProperties.about")
  :category ~(t "category.game") :icon "settings"
  [id tab]
  (set "state.manage" id)
  (set "state.properties" {:open true :tab (otherwise tab "general") :verify ""}))

(defcommand askUninstall "Asks before a game's files are removed."
  :title ~(t "command.askUninstall") :description ~(t "command.askUninstall.about") :category ~(t "category.game")
  :icon "trash-2"
  [id]
  (set "state.manage" id)
  (set "state.uninstall.open" true))

(defcommand uninstall "Removes the game's files; it stays in the library, ready to install again."
  :title ~(t "command.uninstall") :description ~(t "command.uninstall.about") :category ~(t "category.game")
  [id]
  (when (not= (:status (game-of id)) :running)
    (update-record "games" :id id :fields {:installed false :status "none"})
    (set "state.uninstall.open" false)))

(defn show-properties-tab "Shows a page of a game's properties: general, updates, files or dlc." [tab]
  (set "state.properties.tab" tab))

(defn set-launch-options "Keeps the launch options typed for the game being managed." [text]
  (update-record "games" :id computed.manageGame.id :fields {:launchOptions text}))

(defn set-overlay "Turns the in-game overlay on or off for the game being managed." [on]
  (update-record "games" :id computed.manageGame.id :fields {:overlay on}))

(defn set-cloud "Turns cloud saves on or off for the game being managed." [on]
  (update-record "games" :id computed.manageGame.id :fields {:cloud on}))

(defn set-updates "How the game being managed updates: auto, launch or high." [way]
  (update-record "games" :id computed.manageGame.id :fields {:updates way}))

(defn files-verified "Says the game's files were checked." [] (set "state.properties.verify" "done"))

(defn toggle-files "Shows the folders of the game's installed files, or hides them." [] (toggle "state.filesShown"))

(defn verify-files "Checks the game's files: a moment, then they are fine." []
  (set "state.properties.verify" "running")
  (after 1500 files-verified))

;;; The store

(defn leave-dialogs "The store's dialog closes and its search clears, before going to another of its pages." []
  (set "state.cartDialog" false)
  (set "state.storeQuery" ""))

(defcommand storeHome "Opens the store's front page."
  :title ~(t "command.storeHome") :description ~(t "command.storeHome.about") :category ~(t "category.store")
  :icon "store"
  []
  (leave-dialogs)
  (go :store :home nil))

(defcommand openStore "Opens a game's page in the store."
  :title ~(t "command.openStore") :description ~(t "command.openStore.about") :category ~(t "category.store")
  [id]
  (leave-dialogs)
  (set "state.shotIndex" 0)
  (go :store :app id))

(defcommand showCart "Opens the cart."
  :title ~(t "command.showCart") :description ~(t "command.showCart.about") :category ~(t "category.store")
  :icon "shopping-cart"
  []
  (leave-dialogs)
  (set "state.receipt" [])
  (go :store :cart nil))

(defcommand showWishlist "Opens the wishlist."
  :title ~(t "command.showWishlist") :description ~(t "command.showWishlist.about") :category ~(t "category.store")
  :icon "heart"
  []
  (leave-dialogs)
  (go :store :wishlist nil))

(defcommand storeSearch "Searches the store for a word."
  :title ~(t "command.storeSearch") :description ~(t "command.storeSearch.about") :category ~(t "category.store")
  [query]
  (set "state.storeQuery" query)
  (set "state.storeCatsOpen" false))

(defcommand addToCart "Adds the game to the cart and confirms it in a dialog."
  :title ~(t "command.addToCart") :description ~(t "command.addToCart.about") :category ~(t "category.store")
  :icon "shopping-cart"
  [id]
  (when-not (has state.cart id) (push "state.cart" id))
  (set "state.cartAdded" id)
  (set "state.cartDialog" true))

(defcommand removeFromCart "Takes a game out of the cart."
  :title ~(t "command.removeFromCart") :description ~(t "command.removeFromCart.about")
  :category ~(t "category.store")
  [id]
  (set "state.cart" (without state.cart id)))

(defcommand toWishlist "Moves a game from the cart to the wishlist."
  :title ~(t "command.toWishlist") :description ~(t "command.toWishlist.about") :category ~(t "category.store")
  [id]
  (set "state.cart" (without state.cart id))
  (when-not (has state.wishlist id) (push "state.wishlist" id)))

(defcommand toggleWishlist "Adds a game to the wishlist, or takes it off."
  :title ~(t "command.toggleWishlist") :description ~(t "command.toggleWishlist.about")
  :category ~(t "category.store") :icon "heart"
  [id]
  (set "state.wishlist" (toggled state.wishlist id)))

(defcommand toggleFollow "Follows a game's news, or stops."
  :title ~(t "command.toggleFollow") :description ~(t "command.toggleFollow.about") :category ~(t "category.store")
  [id]
  (set "state.followed" (toggled state.followed id)))

(defcommand wishMove "Swaps two games of the wishlist, by their ranks."
  :title ~(t "command.wishMove") :description ~(t "command.wishMove.about") :category ~(t "category.store")
  [i j]
  (set "state.wishlist" (swapped state.wishlist i j)))

(defcommand setWishSort "Sorts the wishlist by rank, name, price or discount."
  :title ~(t "command.setWishSort") :description ~(t "command.setWishSort.about") :category ~(t "category.store")
  [by] (set "state.wishSort" by))

(defcommand purchase
  "Pays from the wallet: the games join the library, not installed, and leave the wishlist; each euro earns 100 points."
  :title ~(t "command.purchase") :description ~(t "command.purchase.about") :category ~(t "category.store")
  :icon "credit-card" :enablement ~(and (pos? (count state.cart)) (<= computed.cartTotal state.user.wallet))
  []
  (let [earned (round (* computed.cartTotal 100))]
    (set "state.points" (+ state.points earned)))
  (update-record "games" :id state.cart :fields {:owned true})
  (set "state.user.wallet" (-> state.user.wallet (- computed.cartTotal) (* 100) round (/ 100)))
  (set "state.wishlist" (remove #(has state.cart %) state.wishlist))
  (set "state.receipt" state.cart)
  (set "state.cart" []))

;;; The store's choices

(defn reveal-node "Scrolls a node into view, by its selector." [selector] (scrollTo selector))

(defn open-store-bar
  "Follows a store bar link: Categories opens its menu, News goes to the library's news, the rest to the store."
  [item]
  (case item
    :categories (toggle "state.storeCatsOpen")
    :news (do (libraryHome) (after 50 reveal-node "#news-header"))
    :points (showRewards)
    :labs (showExperiments)
    (do (storeHome)
        (when (= item :noteworthy)
          (set "state.storeList" :new)
          (after 50 reveal-node "#store-tabs")))))

(defn show-feature "Shows a game of the featured carousel, by its place." [n] (set "state.storeFeature" n))

(defn previous-feature "Shows the featured game before the one shown, the last after the first." []
  (set "state.storeFeature" (mod (+ state.storeFeature 4) 5)))

(defn next-feature "Shows the featured game after the one shown, the first after the last." []
  (set "state.storeFeature" (mod (inc state.storeFeature) 5)))

(defn show-feature-shot "Shows a screenshot in the carousel's big picture, while the pointer is over it." [n]
  (set "state.storeShot" n))

(defn show-store-list "Shows a tab of the store's lists (new, top or specials), its first game previewed." [tab]
  (set "state.storeList" tab)
  (set "state.storeHover" nil))

(defn preview-game "Previews a game of the store's list beside it." [id] (set "state.storeHover" id))

(defn show-shot "Shows a picture of a game's gallery, by its place in the strip." [s] (set "state.shotIndex" s))

(defn show-requirements "Shows the system requirements for a platform: windows, macos or linux." [platform]
  (set "state.reqTab" platform))

(defn vote-review "Votes a review yes, no or funny; the same vote again takes it back." [author choice]
  (let [key (slug author)]
    (set (str "state.votes." key) (if (= (get state.votes key) choice) nil choice))))

(defn cart-or-add "Opens the cart if the game is in it, else adds the game to it." [id]
  (if (has state.cart id) (showCart) (addToCart id)))

(defn wish-up "Moves the wishlist's game at rank i up one, unless it is first." [i]
  (when (pos? i) (wishMove i (dec i))))

(defn wish-down "Moves the wishlist's game at rank i down one, unless it is last." [i]
  (when (< i (dec (count state.wishlist))) (wishMove i (inc i))))

;;; Rewards and experiments

(defcommand showRewards "Opens the store's Rewards: the points shop."
  :title ~(t "command.showRewards") :description ~(t "command.showRewards.about") :category ~(t "category.store")
  :icon "star"
  []
  (leave-dialogs)
  (go :store :points nil))

(defcommand redeemReward "Spends points on a reward, which becomes the person's."
  :title ~(t "command.redeemReward") :description ~(t "command.redeemReward.about") :category ~(t "category.store")
  [id]
  (let [item (find #(= (:id %) id) state.rewardItems)]
    (when (and (some? item) (not (has state.rewardsOwned id)) (>= state.points (:cost item)))
      (set "state.points" (- state.points (:cost item)))
      (push "state.rewardsOwned" id)
      (notify (t "rewards.redeemed" {:name (t (str "rewards.item." id))})))))

(defn show-reward-kind "Shows the rewards of one kind, or all of them." [kind] (set "state.rewardKind" kind))

(defcommand showExperiments "Opens the store's Experiments: ideas to try and turn off."
  :title ~(t "command.showExperiments") :description ~(t "command.showExperiments.about")
  :category ~(t "category.store") :icon "flask-conical"
  []
  (leave-dialogs)
  (go :store :labs nil))

(defcommand toggleDiscoveryQueue "Starts the discovery queue from its first game, or stops it."
  :title ~(t "command.toggleDiscoveryQueue") :description ~(t "command.toggleDiscoveryQueue.about")
  :category ~(t "category.store")
  []
  (set "state.labsAt" 0)
  (toggle "state.labsQueue"))

(defcommand nextQueueGame "Shows the discovery queue's next game."
  :title ~(t "command.nextQueueGame") :description ~(t "command.nextQueueGame.about") :category ~(t "category.store")
  :enablement ~(some? computed.labsGame)
  []
  (set "state.labsAt" (inc state.labsAt)))

(defcommand skipQueueGame "Passes on a game of the discovery queue: it leaves the queue for good."
  :title ~(t "command.skipQueueGame") :description ~(t "command.skipQueueGame.about") :category ~(t "category.store")
  [id]
  (push "state.labsSkipped" id))

(defcommand toggleHideOwned "Hides the games the person owns from the store's lists, or shows them again."
  :title ~(t "command.toggleHideOwned") :description ~(t "command.toggleHideOwned.about")
  :category ~(t "category.store")
  []
  (toggle "state.labsHideOwned"))

;;; Your screenshots and the controller

(defcommand takeScreenshot "Takes a screenshot of the game running, kept with the game's screenshots."
  :title ~(t "command.takeScreenshot") :description ~(t "command.takeScreenshot.about") :category ~(t "category.game")
  :icon "camera" :enablement ~(some? state.running)
  []
  (when-some [game state.running]
    (push "state.shots" {:id (now) :game game :file (get ["shot1" "shot2" "shot3" "hero"] (mod (count state.shots) 4))})
    (notify (t "shots.saved"))))

(defcommand openShot "Opens one of a game's screenshots in the viewer, by its place, newest first."
  :title ~(t "command.openShot") :description ~(t "command.openShot.about") :category ~(t "category.game")
  [game n]
  (set "state.shotView" {:open true :game game :at n}))

(defn step-shot "Shows the viewer's screenshot before (-1) or after (1) the one shown, round the ends." [by]
  (let [n (count computed.viewShots)]
    (set "state.shotView.at" (mod (+ state.shotView.at by n) n))))

(defcommand deleteShot "Deletes the screenshot the viewer shows; the viewer closes with the last one."
  :title ~(t "command.deleteShot") :description ~(t "command.deleteShot.about") :category ~(t "category.game")
  :icon "trash-2" :enablement ~(some? computed.viewShot)
  []
  (let [gone computed.viewShot.id
        left (dec (count computed.viewShots))
        last (max 0 (dec left))]
    (set "state.shots" (remove #(= (:id %) gone) state.shots))
    (set "state.shotView.at" (min state.shotView.at last))
    (when (zero? left) (set "state.shotView.open" false))))

(defcommand setPadLayout "Chooses a game's controller layout: default, southpaw or classic."
  :title ~(t "command.setPadLayout") :description ~(t "command.setPadLayout.about") :category ~(t "category.game")
  [id layout]
  (set (str "state.padLayouts." id) layout))
