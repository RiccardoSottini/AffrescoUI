;;; tempo.lisp --- what Tempo does: its functions and commands
;;;
;;; The screen and its pages (index.screen.lisp, pages.lisp) say what is shown; this file says what happens. A function
;;; reads the running app through paths (`state.queue`, `computed.playingKey`, `settings.playback.crossfade`) and changes
;;; it only through steps (`set`, `audio`, `insert`...). A handler runs one by name, its parameters by name:
;;; {:run play :with {:list -1 :track 0}}. The keys and what the state watches stay in the screen, as data.

;;; Songs, as keys

(defn keyOf "A song's key: list:track, the list -1 for the library." [list track]
  (str list ":" track))

(defn listOf "The list of a song's key." [key]
  (number (first (split key ":"))))

(defn trackOf "The track of a song's key." [key]
  (number (second (split key ":"))))

(defn without "The keys, without one." [keys key]
  (filter (fn [k] (not= k key)) keys))

(defn has "Whether the keys hold one." [keys key]
  (some (fn [k] (= k key)) keys))

(defn shuffled "Keys in shuffle's order: by their track and the seed." [keys]
  (sort-by (fn [k] (mod (+ (* (trackOf k) 7919) state.shuffleSeed) 1009)) keys))

(defn libraryKeys "Library rows as keys: what a table plays after the row opened, up to 50." [ids]
  (map (fn [id] (str "-1:" id)) ids))

(defn idsAfter "The ids of the rows after one, up to 50." [rows id]
  (let [ids (map :id rows)
        at (indexOf ids id)]
    (slice ids (inc at) (+ at 51))))

(defn opened "The row an event opened: its id, or its value." []
  (or event.id event.value))

;;; Playing

(defn playKey "Plays a song, as list:track, from the start." [key]
  (set "state.playingList" (listOf key))
  (set "state.playingTrack" (trackOf key))
  (set "state.queuePick" nil)
  (set "state.elapsed" 0)
  (set "state.isPlaying" true))

(defn startList "Plays a song, list -1 for the library; source is the album or artist it plays from, or \"\"; rest the queue after it, as keys." [list track source rest]
  (set "state.source" source)
  (set "state.order" (concat [(keyOf list track)] rest))
  (set "state.queue" (if state.shuffle (shuffled rest) rest))
  (playKey (keyOf list track)))

(defn songCount "How many songs a list has, -1 the library." [list]
  (if (< list 0) computed.library.length (count (:tracks (get data.playlists list)))))

(defn play "Plays a song; the queue is the songs after it, up to 50." [list track]
  (startList list track ""
             (map (fn [i] (keyOf list (+ track 1 i))) (range (max (min (- (songCount list) track 1) 50) 0)))))

(defn advance "The queue's first song plays, from `at` seconds; with an empty queue, the songs start again when repeating all, else playback stops. Songs no longer there (a playlist changed) leave the queue first." [at]
  (when (and (empty? state.queue) (= state.repeat "all") (not (empty? state.order)))
    (set "state.queue" (if state.shuffle (shuffled state.order) state.order)))
  (set "state.queue" (map :key computed.queueSongs))
  (if (empty? state.queue)
    (do (set "state.isPlaying" false)
        (set "state.elapsed" 0))
    (do (playKey (first state.queue))
        (set "state.queue" (drop 1 state.queue))
        (set "state.elapsed" at))))

(defn next "The next song." [] (advance 0))

(defn replay "The song playing, again from the start (repeating one)." []
  (set "state.elapsed" 0)
  (when (not= state.soundOn "") (sound "seek" state.soundOn :to 0)))

(defn previous "Past the first seconds, or on the first song, the song starts again; else the song before it in the order plays, and this one comes next." []
  (let [playing computed.playingKey
        at (indexOf state.order playing)]
    (if (or (> state.elapsed 3) (< at 1))
      (do (replay)
          (set "state.isPlaying" true))
      (do (set "state.queue" (concat [playing] (without state.queue playing)))
          (playKey (get state.order (dec at)))))))

(defn tick "Each second while playing; at the end of a song the next starts, past the crossfade's seconds that overlapped." []
  (when state.isPlaying
    (increment "state.elapsed")
    (let [fade settings.playback.crossfade]
      (when (and (> fade 0) (not= state.soundOn "") (not= state.repeat "one") (not (empty? state.queue))
                 (= state.elapsed (- computed.duration fade)))
        (sound "volume" state.soundOn :to 0 :ms (* fade 1000)))
      (when (>= state.elapsed computed.duration)
        (if (= state.repeat "one") (replay) (advance fade))))))

(defn seekLyric "The song goes to where a line of the lyrics starts: offset, the lines from the current one (-2 to 2)." [offset]
  (set "state.elapsed" (clamp (* (+ (floor (/ state.elapsed 12)) offset) 12) 0 (max (dec computed.duration) 0))))

;;; Sound: a host that gives none fails these quietly

(defn sound "A step of the audio section on a sound, quietly." [what of & options]
  (apply audio what :of of :onError quietly options))

(defn quietly "What a step's error does when it is no error here: nothing." [] nil)

(defn noSound "No sound plays: what a sound that fails leaves." [] (set "state.soundOn" ""))

(defn startSound "Plays the sound of the song playing (computed.sound: its own, else the beat), stopping the one before; state.soundOn names the one started, empty when none could." []
  (let [own computed.sound]
    (when (and (not= state.soundOn "") (not= state.soundOn own)) (sound "stop" state.soundOn))
    (set "state.soundOn" own)
    (sound "volume" own :to computed.loudness)
    (audio "play" :of own :onError noSound)))

(defn songChanged "A new song: its sound from the start while playing. After a crossfade (the song starts past 0) it fades in over the crossfade's seconds, as the song before faded out." []
  (when (and state.isPlaying (not= state.soundOn ""))
    (if (= state.soundOn computed.sound) (sound "seek" state.soundOn :to 0) (startSound))
    (if (and (> state.elapsed 0) (> settings.playback.crossfade 0))
      (do (sound "volume" state.soundOn :to 0)
          (sound "volume" state.soundOn :to computed.loudness :ms (* settings.playback.crossfade 1000)))
      (sound "volume" state.soundOn :to computed.loudness))))

(defn followPlaying "The song's sound follows the player: play and pause." []
  (if state.isPlaying
    (startSound)
    (when (not= state.soundOn "") (sound "pause" state.soundOn))))

(defn followVolume "The volume slider, or muting, sets the sound's." []
  (when (not= state.soundOn "") (sound "volume" state.soundOn :to computed.loudness :ms 80)))

;;; Menus and the queue

(defn openSongMenu "A song's menu at the pointer (or by the focused row, for Shift+F10)." [list track]
  (set "state.songMenu" {:open true
                         :x (min (or event.x 320) (- window.width 248))
                         :y (min (or event.y 200) (- window.height 240))
                         :list list
                         :track track}))

(defn openPickMenu "The menu of the library song a table has selected (a right click selects the row under the pointer), if any." [pick]
  (when (some? pick) (openSongMenu -1 (number pick))))

(defn playNext "Any song, as list:track, comes next." [key]
  (set "state.queue" (take 1000 (concat [key] (without state.queue key))))
  (set "state.queuePick" nil))

(defn addToQueue "Any song, as list:track, goes to the end of the queue." [key]
  (set "state.queue" (take 1000 (concat (without state.queue key) [key])))
  (set "state.queuePick" nil))

(defn toggleLike "A song, as list:track, is liked, or no longer." [key]
  (set "state.liked" (if (has state.liked key) (without state.liked key) (take 5000 (concat state.liked [key])))))

(defn jumpTo "A song of the queue opened: it plays, and the songs before it leave the queue." []
  (let [key (opened)]
    (set "state.queue" (drop (inc (indexOf state.queue key)) state.queue))
    (playKey key)))

(defn placeQueued "Moves a song of the queue (its key) to a place." [song at]
  (set "state.queueNote"
       (t "queue.moved" {:title (or (:title (first (filter (fn [s] (= (:key s) song)) computed.queueSongs))) "") :n (inc at)}))
  (let [others (without state.queue song)]
    (set "state.queue" (concat (slice others 0 at) [song] (slice others at)))))

(defn moveQueued "Moves the song picked in the queue up (-1) or down (1)." [by]
  (let [at computed.queueAt]
    (when (>= at 0)
      (placeQueued (get state.queue at) (clamp (+ at by) 0 (dec (count state.queue)))))))

(defn dropQueued "A song dragged within the queue: it lands before or after the row under the pointer, or last past the rows." []
  (let [song event.data.id]
    (placeQueued song (if (nil? event.id)
                        (dec (count state.queue))
                        (+ (indexOf (without state.queue song) event.id) (if (= event.place "after") 1 0))))))

;;; Playing from the pages

(defn playFromLibrary "A library row opened: its id is the song's place in the library." []
  (play -1 (number (opened))))

(defn playFromPick "A quick pick row." [item]
  (set "state.page" "library")
  (set "state.libraryPick" (text item.track))
  (play -1 item.track))

(defn playFromCollection "A row of the album or artist opened: the songs after it in the table come next." []
  (startList -1 (number (opened)) state.collection.name (libraryKeys (idsAfter computed.collectionRows (opened)))))

(defn playCollection "The album or artist from its first song." []
  (let [rows computed.collectionRows]
    (when (not (empty? rows))
      (startList -1 (number (:id (first rows))) state.collection.name (libraryKeys (slice (map :id rows) 1 51))))))

(defn playFromSearch "A search result opened: the results after it come next." []
  (startList -1 (number (opened)) "" (libraryKeys (idsAfter computed.searchRows (opened)))))

(defn playLikedAt "A liked song, by its place in computed.likedSongs (-1: none): the liked songs after it come next." [at]
  (when (>= at 0)
    (let [liked computed.likedSongs
          song (get liked at)]
      (startList (:list song) (:track song) (t "liked.title") (map :id (slice liked (inc at) (+ at 51)))))))

(defn playLiked "A liked song plays, by its key, and the liked songs after it come next." [key]
  (playLikedAt (indexOf (map :id computed.likedSongs) key)))

(defn openCollection "An album's or an artist's songs: kind is 'album' or 'artist'." [kind name]
  (set "state.collection" {:kind kind :name name})
  (set "state.collectionPick" nil)
  (set "state.page" "collection")
  (focus "#collectionTable"))

(defn setPreset "The equaliser takes a preset, a key of state.presets." [preset]
  (set "state.eq" {:points (get state.presets preset)})
  (set "state.eqOn" true))

;;; Going back and forward

(defn here "The place shown: the page, the playlist and the album or artist." []
  {:page state.page :current state.current :collection state.collection})

(defn pushedBack "The back stack with a place on top, the last 50." [stack place]
  (drop (max (- (count stack) 49) 0) (concat stack [place])))

(defn placeChanged "The page, the playlist or the album shown changed: the place before goes on the back stack, unless going back or forward moved there." []
  (when (not (and (= state.where.page state.page) (= state.where.current state.current)
                  (= state.where.collection state.collection)))
    (when (not state.navigating)
      (set "state.backStack" (pushedBack state.backStack state.where))
      (set "state.forwardStack" []))
    (set "state.navigating" false)
    (set "state.where" (here))))

(defn goTo "A place from the back or forward stack." [place]
  (set "state.navigating" true)
  (set "state.collection" place.collection)
  (set "state.current" place.current)
  (set "state.page" place.page))

;;; Playlists

(def covers "The covers new playlists take, in turn."
  ["../assets/images/cover-focus.svg" "../assets/images/cover-late-night.svg" "../assets/images/cover-morning-run.svg"
   "../assets/images/cover-sunday-jazz.svg"])

(defn openList "Shows a playlist." [list]
  (set "state.renaming" false)
  (set "state.current" list)
  (set "state.page" "playlist"))

(defn renameNew "The playlist just made opens, its name ready to type." []
  (openList (dec (count data.playlists)))
  (set "state.renaming" true))

(defn newPlaylist "A new playlist opens with its name ready to type (Enter on an empty field keeps the one it has); it holds the song of the song menu when withSong." [withSong]
  (let [n (count data.playlists)]
    (set "state.nameDraft" "")
    (insert "playlists"
            :value {:name (t "playlist.newName" {:n (inc n)})
                    :cover (get covers (mod n 4))
                    :description ""
                    :tracks (if (= withSong true) [computed.songMenuSong] [])}
            :then renameNew)))

(defn addToPlaylist "The song of the song menu goes to the end of a playlist." [list]
  (let [playlist (get data.playlists list)]
    (update-record "playlists" :id (:id playlist)
                   :fields {:tracks (take 1000 (concat (:tracks playlist) [computed.songMenuSong]))})))

(defn shiftTrack "A key after a song left a list: the songs after it in that list move up one." [key list track]
  (if (and (= (listOf key) list) (> (trackOf key) track)) (keyOf list (dec (trackOf key))) key))

(defn removeFromPlaylist "A song leaves a playlist; the liked songs, the queue and the song playing follow the places that moved." [list track]
  (let [gone (keyOf list track)
        playing computed.playingKey
        shift (fn [keys] (map (fn [k] (shiftTrack k list track)) keys))
        playlist (get data.playlists list)]
    (set "state.liked" (shift (without state.liked gone)))
    (set "state.queue" (shift (without state.queue gone)))
    (set "state.order" (shift (filter (fn [k] (or (not= k gone) (= k playing))) state.order)))
    (update-record "playlists" :id (:id playlist)
                   :fields {:tracks (concat (take track (:tracks playlist)) (drop (inc track) (:tracks playlist)))})
    (when (and (= state.playingList list) (> state.playingTrack track))
      (set "state.playingTrack" (dec state.playingTrack)))
    (when (and (= state.playingList list) (= state.playingTrack track))
      (set "state.order" (without state.order gone))
      (advance 0))))

(defn shiftList "A key after a list was deleted: the lists after it move up one." [key list]
  (if (> (listOf key) list) (str (dec (listOf key)) ":" (second (split key ":"))) key))

(defn deleteList "A playlist goes; the liked songs, the queue and the playing list follow the lists that moved, and going back starts again." [list]
  (let [kept (fn [keys] (map (fn [k] (shiftList k list)) (filter (fn [k] (not= (listOf k) list)) keys)))]
    (set "state.liked" (kept state.liked))
    (set "state.queue" (kept state.queue))
    (set "state.order" (kept state.order)))
  (cond (= state.playingList list) (do (set "state.isPlaying" false)
                                       (set "state.elapsed" 0)
                                       (set "state.playingList" -1)
                                       (set "state.playingTrack" 0)
                                       (set "state.source" ""))
        (> state.playingList list) (set "state.playingList" (dec state.playingList)))
  (set "state.renaming" false)
  (set "state.backStack" [])
  (set "state.forwardStack" [])
  (delete "playlists" :id (:id (get data.playlists list)))
  (set "state.current" (max (dec list) 0))
  (set "state.page" (if (> (count data.playlists) 0) "playlist" "library"))
  (set "state.where" (here)))

(defn openListMenu "The menu of the playlist shown: rename, delete." []
  (set "state.listMenu" {:open true :x (min (or event.x 200) (- window.width 200)) :y (min (or event.y 240) (- window.height 80))}))

(defn startRename "The playlist's name, ready to type in its header." []
  (set "state.nameDraft" computed.list.name)
  (set "state.renaming" true))

(defn renameList "The name typed in the header: the playlist takes it, unless it is empty." []
  (when (and (not= (trim state.nameDraft) "") (some? computed.list))
    (update-record "playlists" :id computed.list.id :fields {:name (slice (trim state.nameDraft) 0 60)}))
  (set "state.renaming" false))

(defn askDelete "Asks whether to delete the playlist shown." [] (set "state.deleting" true))

;;; The app

(defn liftAssistant "The assistant's button sits above the floating player (none in the mini player)." []
  (set "state.assistant.lift" (if state.mini 0 92)))

(defn whole "Whole division of a number from 0." [n by]
  (/ (- n (mod n by)) by))

(defn twoDigits "A number from 0 to 99 in two digits." [n]
  (if (< n 10) (str "0" n) (str n)))

(defn librarySong "The library's song at a place, made from the word lists." [words index]
  (let [seconds (+ 150 (mod (* index 37) 210))]
    {:id (str index)
     :number (inc index)
     :title (str (get words.adjectives (mod index 16)) " " (get words.nouns (mod (whole index 16) 20))
                 (if (< index 320) "" (str " (" (get words.versions (dec (whole index 320))) ")")))
     :artist (get words.artists (mod (* index 7) 12))
     :album (get words.albums (mod (whole index 5) 18))
     :time (str (whole seconds 60) ":" (twoDigits (mod seconds 60)))}))

(defn buildLibrary "Makes the library's 3,000 songs once, from the word lists: the first time the app starts." []
  (when (= data.songs.length 0)
    (let [words state.words]
      (insert "songs" :values (map (fn [i] (librarySong words i)) (range 3000))))))

(defn show "Shows a page." [page] (set "state.page" page))

(defn openQuickPick "The quick pick, with a query: \"\" finds a song, \">\" runs a command." [query]
  (set "state.quickPick.query" query)
  (set "state.quickPick.open" true))

;;; Commands

;; Tempo opens playing, silently: the first press starts the song's sound; without sound (a host that gives none) it
;; pauses
(defcommand playPause "Plays the song, or pauses it."
  :title ~(t "command.playPause") :description ~(t "command.playPause.about")
  :category ~(t "category.playback") :icon "play" []
  (if (and state.isPlaying (= state.soundOn ""))
    (do (startSound)
        (when (= state.soundOn "") (toggle "state.isPlaying")))
    (toggle "state.isPlaying")))

(defcommand nextCommand "Plays the next song."
  :title ~(t "command.nextCommand") :description ~(t "command.nextCommand.about")
  :category ~(t "category.playback") :icon "skip-forward" [] (next))
(defcommand previousCommand "Plays the song before, or this one again from the start."
  :title ~(t "command.previousCommand") :description ~(t "command.previousCommand.about")
  :category ~(t "category.playback") :icon "skip-back" [] (previous))

(defcommand volumeUp "Turns the volume up a tenth."
  :title ~(t "command.volumeUp") :description ~(t "command.volumeUp.about")
  :category ~(t "category.playback") :icon "volume-2" :enablement ~(< state.volume 100) []
  (set "state.volume" (min (+ state.volume 10) 100)))

(defcommand volumeDown "Turns the volume down a tenth."
  :title ~(t "command.volumeDown") :description ~(t "command.volumeDown.about")
  :category ~(t "category.playback") :icon "volume-1" :enablement ~(> state.volume 0) []
  (set "state.volume" (max (- state.volume 10) 0)))

(defcommand searchCommand "Finds a song by its title, artist or album."
  :title ~(t "command.searchCommand") :description ~(t "command.searchCommand.about")
  :category ~(t "category.go") :icon "search" [] (openQuickPick ""))
(defcommand commandPalette "Lists every command to run."
  :title ~(t "command.commandPalette") :description ~(t "command.commandPalette.about")
  :category ~(t "category.go") :icon "command" [] (openQuickPick ">"))
(defcommand showLibrary "Shows the library, every song in a table."
  :title ~(t "command.showLibrary") :description ~(t "command.showLibrary.about")
  :category ~(t "category.view") :icon "music" [] (show "library"))
(defcommand showAlbums "Shows the albums as tiles."
  :title ~(t "command.showAlbums") :description ~(t "command.showAlbums.about")
  :category ~(t "category.view") :icon "disc-3" [] (show "albums"))
(defcommand showArtists "Shows the artists as tiles."
  :title ~(t "command.showArtists") :description ~(t "command.showArtists.about")
  :category ~(t "category.view") :icon "users" [] (show "artists"))

(defcommand showSearch "Shows the search, its field ready to type."
  :title ~(t "command.showSearch") :description ~(t "command.showSearch.about")
  :category ~(t "category.view") :icon "search" []
  (show "search")
  (focus "#searchField"))

(defcommand showLiked "Shows the songs liked."
  :title ~(t "command.showLiked") :description ~(t "command.showLiked.about")
  :category ~(t "category.view") :icon "heart" [] (show "liked"))
(defcommand likeSong "Likes the song playing, or no longer."
  :title ~(t "command.likeSong") :description ~(t "command.likeSong.about")
  :category ~(t "category.playback") :icon "heart" [] (toggleLike computed.playingKey))
(defcommand showNowPlaying "Shows the song playing, its visualiser and its lyrics."
  :title ~(t "command.showNowPlaying") :description ~(t "command.showNowPlaying.about")
  :category ~(t "category.view") :icon "radio" [] (show "nowPlaying"))
(defcommand showSound "Shows the equaliser and the crossfade."
  :title ~(t "command.showSound") :description ~(t "command.showSound.about")
  :category ~(t "category.view") :icon "sliders-horizontal" [] (show "sound"))

(defcommand cancelRename "Leaves the playlist's name as it was."
  :title ~(t "command.cancelRename") :description ~(t "command.cancelRename.about")
  :category ~(t "category.view") :icon "x" :enablement ~state.renaming []
  (set "state.renaming" false))

(defcommand toggleQueue "Shows or hides the queue beside the page."
  :title ~(t "command.toggleQueue") :description ~(t "command.toggleQueue.about")
  :category ~(t "category.view") :icon "list-ordered" [] (toggle "state.queueOpen"))

;; The songs up next, shuffled, or back in their order (the songs added to the queue after them)
(defcommand toggleShuffle "Plays the queue in a shuffled order, or in its own."
  :title ~(t "command.toggleShuffle") :description ~(t "command.toggleShuffle.about")
  :category ~(t "category.playback") :icon "shuffle" []
  (toggle "state.shuffle")
  (set "state.shuffleSeed" (mod (now) 1009))
  (set "state.queue" (if state.shuffle
                       (shuffled state.queue)
                       (concat (filter (fn [k] (has state.queue k)) state.order)
                               (remove (fn [k] (has state.order k)) state.queue))))
  (set "state.queuePick" nil))

(defcommand cycleRepeat "Repeats nothing, the whole list, or the song playing."
  :title ~(t "command.cycleRepeat") :description ~(t "command.cycleRepeat.about")
  :category ~(t "category.playback") :icon "repeat" []
  (set "state.repeat" (cond (= state.repeat "off") "all" (= state.repeat "all") "one" :else "off")))

(defcommand toggleMute "Mutes the sound, or brings it back."
  :title ~(t "command.toggleMute") :description ~(t "command.toggleMute.about")
  :category ~(t "category.playback") :icon "volume-x" [] (toggle "state.muted"))

(defcommand goBack "Goes back to the page before."
  :title ~(t "command.goBack") :description ~(t "command.goBack.about")
  :category ~(t "category.view") :icon "chevron-left" :enablement ~(> (count state.backStack) 0) []
  (when (not (empty? state.backStack))
    (set "state.forwardStack" (concat [state.where] (take 49 state.forwardStack)))
    (goTo (last state.backStack))
    (set "state.backStack" (take (dec (count state.backStack)) state.backStack))))

(defcommand goForward "Goes forward to the page after."
  :title ~(t "command.goForward") :description ~(t "command.goForward.about")
  :category ~(t "category.view") :icon "chevron-right" :enablement ~(> (count state.forwardStack) 0) []
  (when (not (empty? state.forwardStack))
    (set "state.backStack" (pushedBack state.backStack state.where))
    (goTo (first state.forwardStack))
    (set "state.forwardStack" (drop 1 state.forwardStack))))

(defcommand toggleMini "Switches between the whole window and the mini player."
  :title ~(t "command.toggleMini") :description ~(t "command.toggleMini.about")
  :category ~(t "category.view") :icon "minimize-2" [] (toggle "state.mini"))

(defcommand toggleTheme "Switches between the light and the dark theme."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about")
  :category ~(t "category.view") :icon "moon" []
  (set "state.theme" (if (= state.theme "dark") "light" "dark")))

(defcommand switchLanguage "Switches the language: English, Spanish, French."
  :title ~(t "command.switchLanguage") :description ~(t "command.switchLanguage.about")
  :category ~(t "category.view") :icon "languages" []
  (set "settings.ui.language" (cond (= settings.ui.language "en") "es" (= settings.ui.language "es") "fr" :else "en")))

(defcommand flatEqualiser "Puts every band of the equaliser back at zero."
  :title ~(t "command.flatEqualiser") :description ~(t "command.flatEqualiser.about")
  :category ~(t "category.sound") :icon "sliders-horizontal" [] (setPreset "flat"))
(defcommand playLibrary "Plays the library from its first song."
  :title ~(t "command.playLibrary") :description ~(t "command.playLibrary.about")
  :category ~(t "category.playback") :icon "disc-3" [] (play -1 0))

;;; The sound, and what the app does by itself

(defpart sound "TODO"
  :audio {:beat {:file "../assets/audio/beatbox.wav"
                 :loop true
                 :volume 0.7
                 :into state.beat
                 :bands 32
                 :updates 50
                 :note "The sound of a song without a sound of its own: a beatbox loop made for Tempo (audio/make_beat.py)"}})

(defpart reactions "TODO"
  :watch {:state.isPlaying [{:run followPlaying}]
          :state.playingTrack [{:run songChanged}]
          :state.playingList [{:run songChanged}]
          :state.volume [{:run followVolume}]
          :state.muted [{:run followVolume}]
          :state.page [{:run placeChanged}]
          :state.mini [{:run liftAssistant}]
          :state.current [{:run placeChanged}]
          :state.collection [{:run placeChanged}]}
  :onLoad [{:run liftAssistant} {:run buildLibrary}])

(defn sortLibrary "The library's table sorted by a column, its header clicked." []
  (set "state.librarySort" {:column event.column :descending event.descending}))
