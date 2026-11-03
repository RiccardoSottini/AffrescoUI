;;; arcadia.lisp --- what Arcadia does: its commands
;;;
;;; The screen and its parts (index.screen.lisp, parts/store.lisp, parts/library.lisp...) say what is shown; this file
;;; says what happens. Every action is a command, as an outsider wrote them: a title, a category, and what a person or an
;;; agent may run by name from the palette. A command reads the running app through paths (`state.cart`, `data.games`,
;;; `computed.here`) and changes it only through steps (`set`, `update-record`, `insert`...). A handler runs one by name,
;;; its parameters by name: ~(handler openGame "g1").

;;; Lists, games and places

(defn otherwise "A value, or another when it is nil (an expression's ??)." [value other]
  (if (nil? value) other value))

(defn has "Whether a list holds a value." [items value]
  (some (fn [item] (= item value)) items))

(defn without "A list without a value." [items value]
  (filter (fn [item] (not= item value)) items))

(defn toggled "A list without a value it holds, or with it at the end." [items value]
  (if (has items value) (without items value) (concat items [value])))

(defn swapped "A list with the items at two places swapped." [items i j]
  (map-indexed (fn [at item] (cond (= at i) (get items j) (= at j) (get items i) :else item)) items))

(defn gameOf "A game of the library, by its id." [id]
  (find (fn [game] (= (:id game) id)) data.games))

(defn hideToast "" [] (set "state.toast.open" false))

(defn scrollChat "" [] (scrollTo "#chat-end"))

;;; Going about

(defcommand go "Goes to { tab, page, id }, dropping the pages ahead of this one, as a browser does."
  :title "Go to a page" :category "View"
  [tab page id]
  (let [here computed.here]
    (when (or (not= tab (:tab here)) (not= page (:page here)) (not= id (:id here)))
      (set "state.nav" (concat (take (inc state.at) state.nav) [{:tab tab :page page :id id}]))
      (set "state.at" (inc state.at)))))

(defcommand back "" :title "Back" :category "View" :icon "arrow-left" :enablement ~(> state.at 0) []
  (set "state.at" (max 0 (dec state.at))))

(defcommand forward "" :title "Forward" :category "View" :icon "arrow-right"
  :enablement ~(< state.at (- (count state.nav) 1))
  []
  (set "state.at" (min (dec (count state.nav)) (inc state.at))))

(defcommand showStore "" :title "Discover" :category "View" [] (go "store" "home" nil))
(defcommand showLibrary "" :title "My games" :category "View" [] (go "library" "home" nil))
(defcommand showCommunity "" :title "Friends" :category "View" [] (go "community" "home" nil))
(defcommand showProfile "" :title "Profile" :category "View" [] (go "profile" "home" nil))

(defcommand toggleTheme "" :title "Switch between dark and light" :category "View" []
  (set "settings.appearance.theme" (if (= settings.appearance.theme "dark") "light" "dark")))

(defcommand nextLanguage "" :title "Next language" :category "View" []
  (set "settings.appearance.language"
       (cond (= settings.appearance.language "en") "es" (= settings.appearance.language "es") "fr" :else "en")))

;;; The client: settings, notices, codes, backups, couch mode

(defcommand setSetting "" :title "Change a setting" :category "Settings" [key value]
  (set (str "settings." key) value))

(defcommand openSettings "Opens Settings at a page (interface, downloads, storage or account)."
  :title "Settings" :category "Arcadia" :icon "settings"
  [page]
  (dismiss)
  (set "state.settingsDialog" {:open true :page (otherwise page "interface")}))

(defcommand notify "Shows a short notice at the bottom right; it goes away by itself."
  :title "Show a notice" :category "Arcadia"
  [text]
  (set "state.toast" {:open true :text text})
  (debounce 4000 hideToast))

(defn upToDate "" [] (notify (t "notice.upToDate")))

(defcommand checkUpdates "" :title "Check for client updates" :category "Arcadia" []
  (notify (t "notice.checking"))
  (after 1200 upToDate))

(defcommand clearCache "" :title "Clear the download cache" :category "Settings" []
  (notify (t "settings.cacheCleared")))

(defcommand toggleOffline "Offline, the store and friends wait for the network; installed games still play."
  :title "Go offline or online" :category "Arcadia"
  []
  (toggle "state.session.offline")
  (notify (t (if state.session.offline "notice.offline" "notice.online"))))

(defcommand signOut "" :title "Sign out" :category "Arcadia" []
  (dismiss)
  (set "state.session.signedOut" true)
  (set "state.couch.open" false))

(defcommand signIn "" :title "Sign in" :category "Arcadia" [] (set "state.session.signedOut" false))

(defcommand openCode "kind is activate (a product key adds a game) or redeem (a gift code adds to the wallet)."
  :title "Activate a product or redeem a gift code" :category "Games"
  [kind]
  (set "state.code" {:open true :kind kind :text "" :error ""}))

(defcommand closeCode "Closes only the code window, so Settings stays open under it."
  :title "Close the code window" :category "Games"
  []
  (set "state.code.open" false))

(defcommand submitCode "Checks the code typed; a good one adds its game or its amount, a wrong one says why."
  :title "Use the code" :category "Games"
  []
  (set "state.code.text" (upper (trim state.code.text)))
  (if (= state.code.kind "activate")
    (activateKey (get state.productKeys state.code.text))
    (redeemGift (get state.giftCodes state.code.text))))

(defcommand activateKey "" :title "Activate a product key" :category "Games" [id]
  (cond (nil? id) (set "state.code.error" (t "code.badKey"))
        (:owned (gameOf id)) (set "state.code.error" (t "code.owned"))
        :else (do (update-record "games" :id id :fields {:owned true})
                  (set "state.code.open" false)
                  (notify (t "code.added" {:game (:title (gameOf id))}))
                  (openGame id))))

(defcommand redeemGift "" :title "Redeem a gift code" :category "Games" [amount]
  (cond (nil? amount) (set "state.code.error" (t "code.badGift"))
        (has state.redeemed state.code.text) (set "state.code.error" (t "code.used"))
        :else (do (push "state.redeemed" state.code.text)
                  (set "state.user.wallet" (/ (round (* (+ state.user.wallet amount) 100)) 100))
                  (set "state.code.open" false)
                  (notify (t "code.funded" {:amount (formatCurrency amount "EUR")})))))

(defcommand openAddGame "" :title "Add a game from this computer" :category "Games" :icon "package-plus" []
  (dismiss)
  (set "state.addGame" {:open true :name ""}))

(defn openAdded "The game just added, its page." [result] (openGame result.id))

(defcommand addLocalGame "Adds a game Arcadia doesn't sell to the library, ready to play from it."
  :title "Add a game from this computer" :category "Games"
  []
  (when (not= (trim state.addGame.name) "")
    (insert "games"
            :value {:title (trim state.addGame.name) :genre "Other" :owned true :installed true :status "ready"
                    :sizeGb 0 :local true :art "local" :logo "#f5a524" :achievements [0 0]}
            :then openAdded)
    (set "state.addGame.open" false)))

(defcommand openBackup "" :title "Back up and restore games" :category "Arcadia" []
  (dismiss)
  (set "state.backup" {:open true :restored nil}))

(defn backedUp "" [] (notify (t "backup.saved")))

(defcommand backUp "Saves which games are installed, with their launch options and update choices, as a JSON file."
  :title "Back up the installed games" :category "Arcadia"
  []
  (saveFile {:format "arcadia-backup"
             :version 1
             :games (map (fn [game] {:id (:id game) :launchOptions (otherwise (:launchOptions game) "")
                                     :overlay (:overlay game) :cloud (:cloud game) :updates (:updates game)})
                         (filter :installed data.games))}
            :name "arcadia-backup.json"
            :onDone backedUp))

(defn restoreGames "Puts back the settings of the games a backup names that are in the library." []
  (let [owned computed.owned]
    (map (fn [b]
           (when (some? (find (fn [game] (= (:id game) (:id b))) owned))
             (update-record "games" :id (:id b)
                            :fields {:launchOptions (text (otherwise (:launchOptions b) "")) :updates (otherwise (:updates b) "auto")})))
         (otherwise state.backup.restored.games [])))
  (notify (t "backup.restored")))

(defcommand restore "Reads a backup and puts back the settings of the games it names that are in the library."
  :title "Restore a backup" :category "Arcadia"
  []
  (openFile "state.backup.restored" :extensions ["json"] :onDone restoreGames))

(defcommand showInfo "kind is about, support, privacy or system." :title "About and help" :category "Help" [kind]
  (dismiss)
  (set "state.info" {:open true :kind kind}))

(defcommand openNotice "" :title "Open a notification" :category "Arcadia" [i]
  (set (str "state.notifications[" i "].unread") false)
  (let [icon (:icon (get state.notifications i))]
    (cond (= icon "download") (showDownloads) (= icon "user-plus") (showCommunity) :else (showStore))))

(defcommand enterCouch "Big tiles for a television across the room, the whole screen, arrows and Enter."
  :title "Couch mode" :category "View" :icon "sofa"
  []
  (dismiss)
  (set "state.couch" {:open true :at 0})
  (fullscreen true)
  (focus "#couch-shelf"))

(defcommand leaveCouch "" :title "Leave couch mode" :category "View" []
  (set "state.couch.open" false)
  (fullscreen false))

(defcommand couchMove "" :title "Move along the couch shelf" :category "View" [by]
  (set "state.couch.at" (clamp (+ state.couch.at by) 0 (dec (count computed.couchGames)))))

;;; Friends

(defcommand openChat "" :title "Chat with a friend" :category "Friends" :icon "message-circle" [id]
  (set "state.chatWith" id)
  (set "state.chatDraft" "")
  (set "state.read" (assoc state.read id (now)))
  (go "community" "home" nil)
  (scrollTo "#chat-end")
  (focus "#chat-input"))

(defn startTyping "" []
  (set "state.typing" state.replyTo)
  (scrollTo "#chat-end"))

(defn reply "" []
  (insert "messages"
          :value {:friend state.replyTo :at (now) :text (t (str "chat.reply" (mod (count (filter :mine data.messages)) 3)))})
  (set "state.typing" nil)
  (set "state.replyTo" nil)
  (scrollTo "#chat-end"))

(defcommand awaitReply "An online friend starts typing a moment later and answers; an offline one reads it later."
  :title "The friend answers" :category "Friends"
  []
  (when (and (not= computed.chatFriend.presence "offline") (nil? state.replyTo))
    (set "state.replyTo" state.chatWith)
    (after 900 startTyping)
    (after 3200 reply)))

(defcommand sendMessage "" :title "Send the message" :category "Friends" :icon "send"
  :enablement ~(and (some? computed.chatFriend) (not= (trim state.chatDraft) ""))
  []
  (when (and (some? computed.chatFriend) (not= (trim state.chatDraft) ""))
    (insert "messages" :value {:friend state.chatWith :mine true :at (now) :text (trim state.chatDraft)})
    (set "state.chatDraft" "")
    (after 50 scrollChat)
    (awaitReply)))

(defcommand inviteToPlay "" :title "Invite to play" :category "Friends" :icon "gamepad-2"
  :enablement ~(and (some? computed.chatFriend) (some? computed.inviteGame))
  []
  (when (and (some? computed.chatFriend) (some? computed.inviteGame))
    (insert "messages" :value {:friend state.chatWith :mine true :at (now) :game computed.inviteGame.id})
    (after 50 scrollChat)))

(defcommand setStatus "" :title "Set your status" :category "Friends" [status] (set "state.user.status" status))

(defcommand addFriend "Sends a friend request by name; they show as offline until they answer."
  :title "Add a friend" :category "Friends" :icon "user-plus"
  []
  (when (not= (trim state.addFriend.name) "")
    (insert "friends" :value {:name (trim state.addFriend.name) :avatar "ghost" :presence "offline" :since 0})
    (set "state.addFriend" {:open false :name ""})))

(defcommand addFriendDialog "" :title "Add a friend" :category "Friends" :icon "user-plus" []
  (go "community" "home" nil)
  (set "state.addFriend" {:open true :name ""}))

(defcommand showProfileOf "" :title "Your profile" :category "Friends" [] (go "profile" "home" nil))

;;; Downloads

(defn downloadPath "A field of a download of the state, as a path." [i key]
  (str "state.downloads[" i "]." key))

(defcommand showDownloads "" :title "Downloads" :category "Downloads" :icon "download" [] (go "downloads" "home" nil))
(defcommand dlPause "" :title "Pause or resume downloads" :category "Downloads" :icon "pause" [] (toggle "state.dlPaused"))

(defcommand dlStart "Moves the item `id` to the top and starts it; what was downloading waits at the head of the queue."
  :title "Download now" :category "Downloads" :icon "download"
  [id]
  (map-indexed (fn [i d]
                 (when (= (:status d) "downloading") (set (downloadPath i "status") "queued"))
                 (when (= (:id d) id) (set (downloadPath i "status") "downloading")))
               state.downloads)
  (set "state.dlPaused" false))

(defcommand dlMove "Swaps the downloads at positions i and j." :title "Move in the queue" :category "Downloads" [i j]
  (set "state.downloads" (swapped state.downloads i j)))

(defcommand dlUnschedule "" :title "Unschedule" :category "Downloads" [id]
  (map-indexed (fn [i d] (when (= (:id d) id) (set (downloadPath i "status") "paused"))) state.downloads))

(defcommand dlRemove "" :title "Remove from downloads" :category "Downloads" :icon "x" [id]
  (let [game (gameOf id)]
    (when (= (:status game) "downloading")
      (update-record "games" :id id :fields {:status (if (:installed game) "update" "none")})))
  (set "state.downloads" (filter (fn [d] (not= (:id d) id)) state.downloads)))

(defcommand dlClear "" :title "Clear completed downloads" :category "Downloads" []
  (set "state.downloads" (filter (fn [d] (not= (:status d) "done")) state.downloads)))

(defcommand dlFinish "Marks the download `id` done, its game ready, and starts the next in the queue."
  :title "Finish the current download" :category "Downloads"
  [id]
  (map-indexed (fn [i d]
                 (when (= (:id d) id)
                   (set (downloadPath i "status") "done")
                   (set (downloadPath i "doneGb") (otherwise (:sizeGb d) (:sizeGb (gameOf id))))))
               state.downloads)
  (update-record "games" :id id :fields {:status "ready" :installed true})
  (let [queued (find (fn [d] (= (:status d) "queued")) state.downloads)]
    (when (some? queued) (dlStart (:id queued)))))

(defn dlUp "Moves the queued download at place k of the queue up one, unless it is first." [k]
  (when (> k 0)
    (dlMove (get-in computed.dlQueue [k :at]) (get-in computed.dlQueue [(dec k) :at]))))

(defn dlDown "Moves the queued download at place k of the queue down one, unless it is last." [k]
  (when (< k (dec (count computed.dlQueue)))
    (dlMove (get-in computed.dlQueue [k :at]) (get-in computed.dlQueue [(inc k) :at]))))

(defn nextRate "The next sample of the rate, in MB/s: a wave that repeats, under the speed limit if there is one." []
  (let [rate (- (+ 34 (* state.dlTick 7)) (* (floor (/ (* state.dlTick 7) 19)) 19))]
    (if (> settings.downloads.limit 0) (min settings.downloads.limit rate) rate)))

(defn downloadTick
  ;; Not while a game runs, unless Settings allow it.
  "Every second while a download runs: a new sample of the rate, the download moved on by it, finished when it's there."
  []
  (when (and (some? computed.download) (not state.dlPaused) (or settings.downloads.whilePlaying (nil? state.running)))
    (set "state.dlTick" (inc state.dlTick))
    (set "state.dlRate" (concat (rest state.dlRate) [(nextRate)]))
    (map-indexed (fn [i d]
                   (when (= (:status d) "downloading")
                     (set (downloadPath i "doneGb") (min computed.dlSize (+ (:doneGb d) (/ (get state.dlRate 47) 1024))))))
                 state.downloads)
    (when (>= computed.download.doneGb computed.dlSize)
      (dlFinish computed.download.id))))

;;; A game's hub and the library

(defcommand openHub "section is hub (everything), discussions or guides."
  :title "Open the game's player hub" :category "Game"
  [id section]
  (set "state.hubTab" (otherwise section "hub"))
  (go "library" "hub" id))

(defcommand toggleCollections "" :title "Collapse or expand every collection" :category "Library" []
  (let [sections computed.sections]
    (set "state.collapsed"
         (if (some (fn [s] (not (otherwise (get state.collapsed (:id s)) false))) sections)
           (zipmap (map :id sections) (map (fn [s] true) sections))
           {}))))

(defcommand toggleFilter "key is readyOnly, favOnly, recentFirst or showNews."
  :title "Turn a library filter on or off" :category "Library"
  [key]
  (toggle (str "state." key)))

(defcommand clearFilters "" :title "Clear the library filters" :category "Library" []
  (set "state.readyOnly" false)
  (set "state.recentFirst" false)
  (set "state.favOnly" false)
  (set "state.query" ""))

(defcommand editNote "" :title "Write a note about a game" :category "Game" [id]
  (set "state.note" {:open true :id id :text (otherwise (:note (gameOf id)) "")}))

(defcommand saveNote "" :title "Save the note" :category "Game" []
  (update-record "games" :id state.note.id :fields {:note (trim state.note.text)}))

(defcommand openGame "" :title "Open the game's page" :category "Game" [id]
  (set "state.selected" id)
  (go "library" "game" id))

(defcommand libraryHome "" :title "My games overview" :category "View" :icon "layout-dashboard" []
  (go "library" "home" nil))

(defcommand play "" :title "Play" :category "Game" :icon "play" [id]
  (update-record "games" :id id :fields {:status "running" :lastPlayed state.today})
  (set "state.running" id))

(defcommand stop "" :title "Stop" :category "Game" :icon "square" [id]
  (update-record "games" :id id :fields {:status "ready" :hours (+ (:hours (gameOf id)) 0.5)})
  (set "state.running" nil))

(defcommand install "Queues the game, or its update: it downloads at once if nothing else is."
  :title "Install" :category "Game" :icon "download"
  [id]
  (let [game (gameOf id)
        updating (= (:status game) "update")]
    (when (and (some? game) (nil? (find (fn [d] (and (= (:id d) id) (not= (:status d) "done"))) state.downloads)))
      (push "state.downloads" {:id id
                               :sizeGb (if updating 1.2 (:sizeGb game))
                               :doneGb 0
                               :status (if computed.download "queued" "downloading")
                               :kind (if updating "update" "install")})
      (update-record "games" :id id :fields {:status "downloading"}))))

(defcommand toggleFavourite "" :title "Add to or remove from favourites" :category "Game" :icon "star" [id]
  (update-record "games" :id id :fields {:favourite (not (:favourite (gameOf id)))}))

(defcommand setSort "Sorts All Games by name, hours played or last played." :title "Sort the library" :category "Library"
  [by]
  (set "settings.library.sortBy" by))

(defcommand primary "" :title "Play, install or update" :category "Game" [game]
  (let [status (:status game)]
    (cond (= status "ready") (play (:id game))
          (= status "running") (stop (:id game))
          (or (= status "none") (= status "update")) (install (:id game)))))

;;; Managing a game

(defcommand openGameMenu "Opens the menu of the game with this id at the pointer (or the focused item)."
  :title "A game's menu" :category "Game"
  [id]
  (set "state.manage" id)
  (set "state.gameMenu" {:open true :x event.x :y event.y}))

(defcommand openProperties "" :title "Properties" :category "Game" :icon "settings" [id tab]
  (set "state.manage" id)
  (set "state.properties" {:open true :tab (otherwise tab "general") :verify ""}))

(defcommand askUninstall "" :title "Uninstall" :category "Game" :icon "trash-2" [id]
  (set "state.manage" id)
  (set "state.uninstall.open" true))

(defcommand uninstall "Removes the game's files; it stays in the library, ready to install again."
  :title "Uninstall" :category "Game"
  [id]
  (when (not= (:status (gameOf id)) "running")
    (update-record "games" :id id :fields {:installed false :status "none"})
    (set "state.uninstall.open" false)))

;;; The store

(defn leaveDialogs "The store's dialog closes and its search clears, before going to another of its pages." []
  (set "state.cartDialog" false)
  (set "state.storeQuery" ""))

(defcommand storeHome "" :title "Store home" :category "Store" :icon "store" []
  (leaveDialogs)
  (go "store" "home" nil))

(defcommand openStore "" :title "Open the game's store page" :category "Store" [id]
  (leaveDialogs)
  (set "state.shotIndex" 0)
  (go "store" "app" id))

(defcommand showCart "" :title "Cart" :category "Store" :icon "shopping-cart" []
  (leaveDialogs)
  (set "state.receipt" [])
  (go "store" "cart" nil))

(defcommand showWishlist "" :title "Wishlist" :category "Store" :icon "heart" []
  (leaveDialogs)
  (go "store" "wishlist" nil))

(defcommand storeSearch "" :title "Search the store" :category "Store" [query]
  (set "state.storeQuery" query)
  (set "state.storeCatsOpen" false))

(defcommand addToCart "Adds the game to the cart and confirms it in a dialog."
  :title "Add to cart" :category "Store" :icon "shopping-cart"
  [id]
  (when (not (has state.cart id)) (push "state.cart" id))
  (set "state.cartAdded" id)
  (set "state.cartDialog" true))

(defcommand removeFromCart "" :title "Remove from cart" :category "Store" [id]
  (set "state.cart" (without state.cart id)))

(defcommand toWishlist "" :title "Move to the wishlist" :category "Store" [id]
  (set "state.cart" (without state.cart id))
  (when (not (has state.wishlist id)) (push "state.wishlist" id)))

(defcommand toggleWishlist "" :title "Add to or remove from the wishlist" :category "Store" :icon "heart" [id]
  (set "state.wishlist" (toggled state.wishlist id)))

(defcommand toggleFollow "" :title "Follow or unfollow" :category "Store" [id]
  (set "state.followed" (toggled state.followed id)))

(defcommand wishMove "Swaps the games at ranks i and j." :title "Move a wishlist game up or down" :category "Store"
  [i j]
  (set "state.wishlist" (swapped state.wishlist i j)))

(defcommand setWishSort "" :title "Sort the wishlist" :category "Store" [by] (set "state.wishSort" by))

(defcommand purchase "Pays from the wallet: the games join the library, not installed, and leave the wishlist."
  :title "Purchase for myself" :category "Store" :icon "credit-card"
  :enablement ~(and (> (count state.cart) 0) (<= computed.cartTotal state.user.wallet))
  []
  (update-record "games" :id state.cart :fields {:owned true})
  (set "state.user.wallet" (/ (round (* (- state.user.wallet computed.cartTotal) 100)) 100))
  (set "state.wishlist" (filter (fn [w] (not (has state.cart w))) state.wishlist))
  (set "state.receipt" state.cart)
  (set "state.cart" []))

;;; What the views choose

(defn showSettingsPage "Shows a page of Settings: interface, downloads, storage or account." [page]
  (set "state.settingsDialog.page" page))

(defn couchChoose "Chooses a game on the couch shelf, by its id." [id]
  (set "state.couch.at" (indexOf (map (fn [it] (:id it)) computed.couchGames) id)))

(defn showPropertiesTab "Shows a page of a game's properties: general, updates, files or dlc." [tab]
  (set "state.properties.tab" tab))

(defn setLaunchOptions "Keeps the launch options typed for the game being managed." [text]
  (update-record "games" :id computed.manageGame.id :fields {:launchOptions text}))

(defn setOverlay "Turns the in-game overlay on or off for the game being managed." [on]
  (update-record "games" :id computed.manageGame.id :fields {:overlay on}))

(defn setCloud "Turns cloud saves on or off for the game being managed." [on]
  (update-record "games" :id computed.manageGame.id :fields {:cloud on}))

(defn setUpdates "How the game being managed updates: auto, launch or high." [way]
  (update-record "games" :id computed.manageGame.id :fields {:updates way}))

(defn filesVerified "Says the game's files were checked." [] (set "state.properties.verify" "done"))

(defn verifyFiles "Checks the game's files: a moment, then they are fine." []
  (set "state.properties.verify" "running")
  (after 1500 filesVerified))

(defn foldSection "Folds a section of the game list if it is open, or unfolds it." [id open]
  (set (str "state.collapsed." id) open))

(defn openSection "Opens a section of a game: its store page, its extra content, support, or a part of its hub." [id section]
  (cond (= section "store") (openStore id)
        (= section "dlc") (openProperties id "dlc")
        (= section "support") (showInfo "support")
        :else (openHub id section)))

;;; The store's choices

(defn revealNode "Scrolls a node into view, by its selector." [selector] (scrollTo selector))

(defn openStoreBar
  "Follows a store bar link: Categories opens its menu, News goes to the library's news, the rest to the store."
  [item]
  (cond (= item "categories") (toggle "state.storeCatsOpen")
        (= item "news") (do (libraryHome) (after 50 revealNode "#news-header"))
        :else (do (storeHome)
                  (when (= item "noteworthy")
                    (set "state.storeList" "new")
                    (after 50 revealNode "#store-tabs")))))

(defn showFeature "Shows a game of the featured carousel, by its place." [n] (set "state.storeFeature" n))

(defn previousFeature "Shows the featured game before the one shown, the last after the first." []
  (set "state.storeFeature" (mod (+ state.storeFeature 4) 5)))

(defn nextFeature "Shows the featured game after the one shown, the first after the last." []
  (set "state.storeFeature" (mod (+ state.storeFeature 1) 5)))

(defn showFeatureShot "Shows a screenshot in the carousel's big picture, while the pointer is over it." [n]
  (set "state.storeShot" n))

(defn showStoreList "Shows a tab of the store's lists (new, top or specials), its first game previewed." [tab]
  (set "state.storeList" tab)
  (set "state.storeHover" nil))

(defn previewGame "Previews a game of the store's list beside it." [id] (set "state.storeHover" id))

(defn showShot "Shows a picture of a game's gallery, by its place in the strip." [s] (set "state.shotIndex" s))

(defn showRequirements "Shows the system requirements for a platform: windows, macos or linux." [platform]
  (set "state.reqTab" platform))

(defn voteReview "Votes a review yes, no or funny; the same vote again takes it back." [author choice]
  (let [key (slug author)]
    (set (str "state.votes." key) (if (= (get state.votes key) choice) nil choice))))

(defn cartOrAdd "Opens the cart if the game is in it, else adds the game to it." [id]
  (if (has state.cart id) (showCart) (addToCart id)))

(defn wishUp "Moves the wishlist's game at rank i up one, unless it is first." [i]
  (when (> i 0) (wishMove i (- i 1))))

(defn wishDown "Moves the wishlist's game at rank i down one, unless it is last." [i]
  (when (< i (- (count state.wishlist) 1)) (wishMove i (+ i 1))))
