;;; tempo.lisp --- what Tempo does: its functions and commands
;;;
;;; The views (sidebar.lisp, library.lisp, player.lisp, pages.lisp...) say what is shown; this file says what happens. A
;;; function reads the running app through paths (`state.queue`, `computed.playingKey`, `settings.playback.crossfade`)
;;; and changes it only through steps (`set`, `audio`, `insert`...). A view runs one by name (`:onClick playPause`) or
;;; with its arguments, ~(handler play index 0); keys.lisp binds the keys. At the end, the sound and what the app does by
;;; itself: what the state watches, and what runs on load.

(ns tempo.tempo)

;;; Songs, as keys

(defn key-of "A song's key: list:track, the list -1 for the library." [list track]
  (str list ":" track))

(defn list-of "The list of a song's key." [key]
  (number (first (str/split key ":"))))

(defn track-of "The track of a song's key." [key]
  (number (second (str/split key ":"))))

(defn without "The keys, without one." [keys key]
  (remove #(= % key) keys))

(defn has "Whether the keys hold one." [keys key]
  (some #(= % key) keys))

(defn shuffled "Keys in shuffle's order: by their track and the seed." [keys]
  (sort-by #(-> (track-of %) (* 7919) (+ state.shuffleSeed) (mod 1009)) keys))

(defn library-keys "Library rows as keys: what a table plays after the row opened, up to 50." [ids]
  (map #(str "-1:" %) ids))

(defn ids-after "The ids of the rows after one, up to 50." [rows id]
  (let [ids (map :id rows)
        at (indexOf ids id)]
    (slice ids (inc at) (+ at 51))))

(defn opened "The row an event opened: its id, or its value." []
  (or event.id event.value))

;;; Playing

(defn play-key "Plays a song, as list:track, from the start." [key]
  (set "state.playingList" (list-of key))
  (set "state.playingTrack" (track-of key))
  (set "state.queuePick" nil)
  (set "state.elapsed" 0)
  (set "state.isPlaying" true))

(defn start-list "Plays a song, list -1 for the library; source is the album or artist it plays from, or \"\"; rest the queue after it, as keys." [list track source rest]
  (set "state.source" source)
  (set "state.order" (concat [(key-of list track)] rest))
  (set "state.queue" (if (true? state.shuffle) (shuffled rest) rest))
  (play-key (key-of list track)))

(defn song-count "How many songs a list has, -1 the library." [list]
  (if (neg? list) computed.library.length (count (get-in data.playlists [list :tracks]))))

(defn play "Plays a song; the queue is the songs after it, up to 50." [list track]
  (start-list list track "" (map #(key-of list (+ track 1 %)) (range (-> (song-count list) (- track) dec (min 50) (max 0))))))

(defn advance "The queue's first song plays, from `at` seconds; with an empty queue, the songs start again when repeating all, else playback stops. Songs no longer there (a playlist changed) leave the queue first." [at]
  (when (and (empty? state.queue) (= state.repeat :all) (seq state.order))
    (set "state.queue" (if (true? state.shuffle) (shuffled state.order) state.order)))
  (set "state.queue" (map :key computed.queueSongs))
  (if (empty? state.queue)
    (do (set "state.isPlaying" false)
        (set "state.elapsed" 0))
    (do (play-key (first state.queue))
        (set "state.queue" (rest state.queue))
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
          (play-key (get state.order (dec at)))))))

(defn tick "Each second while playing; at the end of a song the next starts, past the crossfade's seconds that overlapped." []
  (when (true? state.isPlaying)
    (increment "state.elapsed")
    (let [fade settings.playback.crossfade]
      (when (and (pos? fade) (not= state.soundOn "") (not= state.repeat :one) (seq state.queue)
                 (= state.elapsed (- computed.duration fade)))
        (sound "volume" state.soundOn :to 0 :ms (* fade 1000)))
      (when (>= state.elapsed computed.duration)
        (if (= state.repeat :one) (replay) (advance fade))))))

(defn seek-lyric "The song goes to where a line of the lyrics starts: offset, the lines from the current one (-2 to 2)." [offset]
  (set "state.elapsed" (-> (/ state.elapsed 12) floor (+ offset) (* 12) (clamp 0 (max (dec computed.duration) 0)))))

;;; Sound: a host that gives none fails these quietly

(defn sound "A step of the audio section on a sound, quietly." [what of & options]
  (apply audio what :of of :onError quietly options))

(defn quietly "What a step's error does when it is no error here: nothing." [] nil)

(defn no-sound "No sound plays: what a sound that fails leaves." [] (set "state.soundOn" ""))

(defn start-sound "Plays the sound of the song playing (computed.sound: its own, else the beat), stopping the one before; state.soundOn names the one started, empty when none could." []
  (let [own computed.sound]
    (when (and (not= state.soundOn "") (not= state.soundOn own)) (sound "stop" state.soundOn))
    (set "state.soundOn" own)
    (sound "volume" own :to computed.loudness)
    (audio "play" :of own :onError no-sound)))

(defn song-changed "A new song: its sound from the start while playing. After a crossfade (the song starts past 0) it fades in over the crossfade's seconds, as the song before faded out." []
  (when (and (true? state.isPlaying) (not= state.soundOn ""))
    (if (= state.soundOn computed.sound) (sound "seek" state.soundOn :to 0) (start-sound))
    (if (and (pos? state.elapsed) (pos? settings.playback.crossfade))
      (do (sound "volume" state.soundOn :to 0)
          (sound "volume" state.soundOn :to computed.loudness :ms (* settings.playback.crossfade 1000)))
      (sound "volume" state.soundOn :to computed.loudness))))

(defn follow-playing "The song's sound follows the player: play and pause." []
  (if (true? state.isPlaying)
    (start-sound)
    (when (not= state.soundOn "") (sound "pause" state.soundOn))))

(defn follow-volume "The volume slider, or muting, sets the sound's." []
  (when (not= state.soundOn "") (sound "volume" state.soundOn :to computed.loudness :ms 80)))

;;; Menus and the queue

(defn open-song-menu "A song's menu at the pointer (or by the focused row, for Shift+F10)." [list track]
  (set "state.songMenu" {:open true
                         :x (min (or event.x 320) (- window.width 248))
                         :y (min (or event.y 200) (- window.height 240))
                         :list list
                         :track track}))

(defn open-pick-menu "The menu of the library song a table has selected (a right click selects the row under the pointer), if any." [pick]
  (when-some [row pick] (open-song-menu -1 (number row))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn playNext "Any song, as list:track, comes next." [key]
  (set "state.queue" (->> (without state.queue key) (concat [key]) (take 1000)))
  (set "state.queuePick" nil))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addToQueue "Any song, as list:track, goes to the end of the queue." [key]
  (set "state.queue" (take 1000 (conj (without state.queue key) key)))
  (set "state.queuePick" nil))

; style: keep, a string of the code names it, and a rename never changes a string
(defn toggleLike "A song, as list:track, is liked, or no longer." [key]
  (set "state.liked" (if (has state.liked key) (without state.liked key) (take 5000 (conj state.liked key)))))

(defn jump-to "A song of the queue opened: it plays, and the songs before it leave the queue." []
  (let [key (opened)]
    (set "state.queue" (drop (inc (indexOf state.queue key)) state.queue))
    (play-key key)))

(defn place-queued "Moves a song of the queue (its key) to a place." [song at]
  (let [moved (find #(= (:key %) song) computed.queueSongs)
        others (without state.queue song)]
    (set "state.queueNote" (t "queue.moved" {:title (or (:title moved) "") :n (inc at)}))
    (set "state.queue" (concat (take at others) [song] (drop at others)))))

(defn move-queued "Moves the song picked in the queue up (-1) or down (1)." [by]
  (let [at computed.queueAt]
    (when (>= at 0)
      (place-queued (get state.queue at) (clamp (+ at by) 0 (dec (count state.queue)))))))

(defn drop-queued "A song dragged within the queue: it lands before or after the row under the pointer, or last past the rows." []
  (let [song event.data.id]
    (place-queued song (if-some [over event.id]
                        (+ (indexOf (without state.queue song) over) (if (= event.place :after) 1 0))
                        (dec (count state.queue))))))

;;; Playing from the pages

(defn play-from-library "A library row opened: its id is the song's place in the library." []
  (play -1 (number (opened))))

(defn play-from-pick "A quick pick row." [item]
  (set "state.page" :library)
  (set "state.libraryPick" (text item.track))
  (play -1 item.track))

(defn play-from-collection "A row of the album or artist opened: the songs after it in the table come next." []
  (start-list -1 (number (opened)) state.collection.name (library-keys (ids-after computed.collectionRows (opened)))))

(defn play-collection "The album or artist from its first song." []
  (let [rows computed.collectionRows]
    (when-first [top rows]
      (start-list -1 (number (:id top)) state.collection.name (->> rows (map :id) (drop 1) (take 50) library-keys)))))

(defn play-from-search "A search result opened: the results after it come next." []
  (start-list -1 (number (opened)) "" (library-keys (ids-after computed.searchRows (opened)))))

(defn play-liked-at "A liked song, by its place in computed.likedSongs (-1: none): the liked songs after it come next." [at]
  (when (>= at 0)
    (let [liked computed.likedSongs
          {:keys [list track]} (get liked at)]
      (start-list list track (t "liked.title") (->> liked (drop (inc at)) (take 50) (map :id))))))

(defn play-liked "A liked song plays, by its key, and the liked songs after it come next." [key]
  (play-liked-at (indexOf (map :id computed.likedSongs) key)))

(defn open-collection "An album's or an artist's songs: kind is 'album' or 'artist'." [kind name]
  (set "state.collection" {:kind kind :name name})
  (set "state.collectionPick" nil)
  (set "state.page" :collection)
  (focus "#collectionTable"))

(defn set-preset "The equaliser takes a preset, a key of state.presets." [preset]
  (set "state.eq" {:points (get state.presets preset)})
  (set "state.eqOn" true))

;;; Going back and forward

(defn here "The place shown: the page, the playlist and the album or artist." []
  {:page state.page :current state.current :collection state.collection})

(defn pushed-back "The back stack with a place on top, the last 50." [stack place]
  (take-last 50 (conj stack place)))

(defn place-changed "The page, the playlist or the album shown changed: the place before goes on the back stack, unless going back or forward moved there." []
  (when-not (and (= state.where.page state.page) (= state.where.current state.current)
                 (= state.where.collection state.collection))
    (when-not (true? state.navigating)
      (set "state.backStack" (pushed-back state.backStack state.where))
      (set "state.forwardStack" []))
    (set "state.navigating" false)
    (set "state.where" (here))))

(defn go-to "A place from the back or forward stack." [place]
  (set "state.navigating" true)
  (set "state.collection" place.collection)
  (set "state.current" place.current)
  (set "state.page" place.page))

;;; Playlists

(def covers "The covers new playlists take, in turn."
  ["../assets/images/cover-focus.svg" "../assets/images/cover-late-night.svg" "../assets/images/cover-morning-run.svg"
   "../assets/images/cover-sunday-jazz.svg"])

(defn open-list "Shows a playlist." [list]
  (set "state.renaming" false)
  (set "state.current" list)
  (set "state.page" :playlist))

(defn rename-new "The playlist just made opens, its name ready to type." []
  (open-list (dec (count data.playlists)))
  (set "state.renaming" true))

; style: keep, a string of the code names it, and a rename never changes a string
(defn newPlaylist "A new playlist opens with its name ready to type (Enter on an empty field keeps the one it has); it holds the song of the song menu when withSong." [withSong]
  (let [n (count data.playlists)]
    (set "state.nameDraft" "")
    (insert "playlists"
            :value {:name (t "playlist.newName" {:n (inc n)})
                    :cover (get covers (mod n 4))
                    :description ""
                    :tracks (if (true? withSong) [computed.songMenuSong] [])}
            :then rename-new)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addToPlaylist "The song of the song menu goes to the end of a playlist." [list]
  (let [{:keys [id tracks]} (get data.playlists list)]
    (update-record "playlists" :id id :fields {:tracks (take 1000 (conj tracks computed.songMenuSong))})))

(defn shift-track "A key after a song left a list: the songs after it in that list move up one." [key list track]
  (if (and (= (list-of key) list) (> (track-of key) track)) (key-of list (dec (track-of key))) key))

; style: keep, a string of the code names it, and a rename never changes a string
(defn removeFromPlaylist "A song leaves a playlist; the liked songs, the queue and the song playing follow the places that moved." [list track]
  (let [gone (key-of list track)
        playing computed.playingKey
        shift #(shift-track % list track)
        {:keys [id tracks]} (get data.playlists list)]
    (set "state.liked" (map shift (without state.liked gone)))
    (set "state.queue" (map shift (without state.queue gone)))
    (set "state.order" (map shift (filter #(or (not= % gone) (= % playing)) state.order)))
    (update-record "playlists" :id id :fields {:tracks (concat (take track tracks) (drop (inc track) tracks))})
    (when (and (= state.playingList list) (> state.playingTrack track))
      (set "state.playingTrack" (dec state.playingTrack)))
    (when (and (= state.playingList list) (= state.playingTrack track))
      (set "state.order" (without state.order gone))
      (advance 0))))

(defn shift-list "A key after a list was deleted: the lists after it move up one." [key list]
  (if (> (list-of key) list) (key-of (dec (list-of key)) (track-of key)) key))

(defn delete-list "A playlist goes; the liked songs, the queue and the playing list follow the lists that moved, and going back starts again." [list]
  (let [kept (fn [keys]
               (->> keys (remove #(= (list-of %) list)) (map #(shift-list % list))))]
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
  (set "state.page" (if (seq data.playlists) :playlist :library))
  (set "state.where" (here)))

(defn open-list-menu "The menu of the playlist shown: rename, delete." []
  (set "state.listMenu" {:open true :x (min (or event.x 200) (- window.width 200)) :y (min (or event.y 240) (- window.height 80))}))

; style: keep, a string of the code names it, and a rename never changes a string
(defn startRename "The playlist's name, ready to type in its header." []
  (set "state.nameDraft" computed.list.name)
  (set "state.renaming" true))

(defn rename-list "The name typed in the header: the playlist takes it, unless it is empty." []
  (let [typed (str/trim state.nameDraft)]
    (when (and (not= typed "") (some? computed.list))
      (update-record "playlists" :id computed.list.id :fields {:name (slice typed 0 60)})))
  (set "state.renaming" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn askDelete "Asks whether to delete the playlist shown." [] (set "state.deleting" true))

;;; The app

; style: keep, a parameter or a local has the same name
(defn liftAssistant "The assistant's button sits above the floating player (none in the mini player)." []
  (set "state.assistant.lift" (if (true? state.mini) 0 92)))

(defn whole "Whole division of a number from 0." [n by]
  (quot n by))

(defn two-digits "A number from 0 to 99 in two digits." [n]
  (format "%02d" n))

(defn library-song "The library's song at a place, made from the word lists." [words index]
  (let [seconds (+ 150 (mod (* index 37) 210))]
    {:id (str index)
     :number (inc index)
     :title (str (get words.adjectives (mod index 16)) " " (get words.nouns (mod (whole index 16) 20))
                 (if (< index 320) "" (str " (" (get words.versions (dec (whole index 320))) ")")))
     :artist (get words.artists (mod (* index 7) 12))
     :album (get words.albums (mod (whole index 5) 18))
     :time (str (whole seconds 60) ":" (two-digits (mod seconds 60)))}))

; style: keep, a parameter or a local has the same name
(defn buildLibrary "Makes the library's 3,000 songs once, from the word lists: the first time the app starts." []
  (when (empty? data.songs)
    (let [words state.words]
      (insert "songs" :values (map #(library-song words %) (range 3000))))))

(defn show "Shows a page." [page] (set "state.page" page))

(defn open-quick-pick "The quick pick, with a query: \"\" finds a song, \">\" runs a command." [query]
  (set "state.quickPick.query" query)
  (set "state.quickPick.open" true))

;;; By names: what the commands an agent runs (songPlay, songPause, songQueue, songLike) do, on the library's songs. A
;;; song that isn't there changes nothing but the page: the search shows what its title finds.

(defn song-named "The library song with that title, and by that artist when given, in any case: its key, or nil." [title artist]
  (let [wanted (-> title str/trim str/lower-case)
        by (some-> artist str/trim str/lower-case)
        song (find #(and (= (str/lower-case (:title %)) wanted) (or (nil? by) (= (str/lower-case (:artist %)) by)))
                   data.songs)]
    (some->> song :id number (key-of -1))))

(defn search-for "Shows the search, with what was asked for." [title]
  (set "state.search" (str/trim title))
  (show :search))

(defn play-song "Plays the library song with that title (and artist); without a title, the song again where it paused." [title artist]
  (let [key (some-> title (song-named artist))]
    (cond (nil? title) (set "state.isPlaying" true)
          (nil? key) (search-for title)
          :else (play (list-of key) (track-of key)))))

(defn queue-song "Adds the library song with that title (and artist) to the end of the queue." [title artist]
  (if-some [key (song-named title artist)] (addToQueue key) (search-for title)))

(defn like-song-named "Likes the library song with that title (and artist); a liked song stays liked." [title artist]
  (if-some [key (song-named title artist)]
    (when-not (has state.liked key) (toggleLike key))
    (search-for title)))

(defcommand songPlay "Plays a song, by its title, or the song again."
  :title ~(t "command.songPlay") :description ~(t "command.songPlay.about")
  :category ~(t "category.playback") :icon "play"
  :arguments {:title {:type [string null] :description "The song's title, like Golden Rivers; the paused song when left out"}
              :artist {:type [string null] :description "Its artist, when two songs share a title"}}
  [title artist]
  (play-song title artist))

(defcommand songPause "Pauses the song."
  :title ~(t "command.songPause") :description ~(t "command.songPause.about")
  :category ~(t "category.playback") :icon "pause" []
  (set "state.isPlaying" false))

(defcommand songQueue "Adds a song, by its title, to the end of the queue."
  :title ~(t "command.songQueue") :description ~(t "command.songQueue.about")
  :category ~(t "category.playback") :icon "list-plus"
  :arguments {:title {:type string :description "The song's title, like Golden Rivers"}
              :artist {:type [string null] :description "Its artist, when two songs share a title"}}
  [title artist]
  (queue-song title artist))

(defcommand songLike "Likes a song, by its title."
  :title ~(t "command.songLike") :description ~(t "command.songLike.about")
  :category ~(t "category.playback") :icon "heart"
  :arguments {:title {:type string :description "The song's title, like Golden Rivers"}
              :artist {:type [string null] :description "Its artist, when two songs share a title"}}
  [title artist]
  (like-song-named title artist))

;;; Commands

;; Tempo opens playing, silently: the first press starts the song's sound; without sound (a host that gives none) it
;; pauses
(defcommand playPause "Plays the song, or pauses it."
  :title ~(t "command.playPause") :description ~(t "command.playPause.about")
  :category ~(t "category.playback") :icon "play" []
  (if (and (true? state.isPlaying) (= state.soundOn ""))
    (do (start-sound)
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
  :category ~(t "category.go") :icon "search" [] (open-quick-pick ""))
(defcommand commandPalette "Lists every command to run."
  :title ~(t "command.commandPalette") :description ~(t "command.commandPalette.about")
  :category ~(t "category.go") :icon "command" [] (open-quick-pick ">"))
(defcommand showLibrary "Shows the library, every song in a table."
  :title ~(t "command.showLibrary") :description ~(t "command.showLibrary.about")
  :category ~(t "category.view") :icon "music" [] (show :library))
(defcommand showAlbums "Shows the albums as tiles."
  :title ~(t "command.showAlbums") :description ~(t "command.showAlbums.about")
  :category ~(t "category.view") :icon "disc-3" [] (show :albums))
(defcommand showArtists "Shows the artists as tiles."
  :title ~(t "command.showArtists") :description ~(t "command.showArtists.about")
  :category ~(t "category.view") :icon "users" [] (show :artists))

(defcommand showSearch "Shows the search, its field ready to type."
  :title ~(t "command.showSearch") :description ~(t "command.showSearch.about")
  :category ~(t "category.view") :icon "search" []
  (show :search)
  (focus "#searchField"))

(defcommand showLiked "Shows the songs liked."
  :title ~(t "command.showLiked") :description ~(t "command.showLiked.about")
  :category ~(t "category.view") :icon "heart" [] (show :liked))
(defcommand likeSong "Likes the song playing, or no longer."
  :title ~(t "command.likeSong") :description ~(t "command.likeSong.about")
  :category ~(t "category.playback") :icon "heart" [] (toggleLike computed.playingKey))
(defcommand showNowPlaying "Shows the song playing, its visualiser and its lyrics."
  :title ~(t "command.showNowPlaying") :description ~(t "command.showNowPlaying.about")
  :category ~(t "category.view") :icon "radio" [] (show :nowPlaying))
(defcommand showSound "Shows the equaliser and the crossfade."
  :title ~(t "command.showSound") :description ~(t "command.showSound.about")
  :category ~(t "category.view") :icon "sliders-horizontal" [] (show :sound))

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
  (set "state.queue" (if (true? state.shuffle)
                       (shuffled state.queue)
                       (concat (filter #(has state.queue %) state.order)
                               (remove #(has state.order %) state.queue))))
  (set "state.queuePick" nil))

(defcommand cycleRepeat "Repeats nothing, the whole list, or the song playing."
  :title ~(t "command.cycleRepeat") :description ~(t "command.cycleRepeat.about")
  :category ~(t "category.playback") :icon "repeat" []
  (set "state.repeat" (case state.repeat :off :all :all :one :off)))

(defcommand toggleMute "Mutes the sound, or brings it back."
  :title ~(t "command.toggleMute") :description ~(t "command.toggleMute.about")
  :category ~(t "category.playback") :icon "volume-x" [] (toggle "state.muted"))

(defcommand goBack "Goes back to the page before."
  :title ~(t "command.goBack") :description ~(t "command.goBack.about")
  :category ~(t "category.view") :icon "chevron-left" :enablement ~(pos? (count state.backStack)) []
  (when (seq state.backStack)
    (set "state.forwardStack" (concat [state.where] (take 49 state.forwardStack)))
    (go-to (last state.backStack))
    (set "state.backStack" (drop-last state.backStack))))

(defcommand goForward "Goes forward to the page after."
  :title ~(t "command.goForward") :description ~(t "command.goForward.about")
  :category ~(t "category.view") :icon "chevron-right" :enablement ~(pos? (count state.forwardStack)) []
  (when (seq state.forwardStack)
    (set "state.backStack" (pushed-back state.backStack state.where))
    (go-to (first state.forwardStack))
    (set "state.forwardStack" (rest state.forwardStack))))

(defcommand toggleMini "Switches between the whole window and the mini player."
  :title ~(t "command.toggleMini") :description ~(t "command.toggleMini.about")
  :category ~(t "category.view") :icon "minimize-2" [] (toggle "state.mini"))

(defcommand toggleTheme "Switches between the light and the dark theme."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about")
  :category ~(t "category.view") :icon "moon" []
  (set "state.theme" (if (= state.theme :dark) :light :dark)))

(defcommand switchLanguage "Switches the language: English, Spanish, French."
  :title ~(t "command.switchLanguage") :description ~(t "command.switchLanguage.about")
  :category ~(t "category.view") :icon "languages" []
  (set "settings.ui.language" (case settings.ui.language :en :es :es :fr :en)))

(defcommand flatEqualiser "Puts every band of the equaliser back at zero."
  :title ~(t "command.flatEqualiser") :description ~(t "command.flatEqualiser.about")
  :category ~(t "category.sound") :icon "sliders-horizontal" [] (set-preset "flat"))
(defcommand playLibrary "Plays the library from its first song."
  :title ~(t "command.playLibrary") :description ~(t "command.playLibrary.about")
  :category ~(t "category.playback") :icon "disc-3" [] (play -1 0))

;;; What the app does by itself

(defpart reactions "What the state watches (the sound follows the player, going back follows the page), and what runs on load."
  :watch {:state.isPlaying [{:run follow-playing}]
          :state.playingTrack [{:run song-changed}]
          :state.playingList [{:run song-changed}]
          :state.volume [{:run follow-volume}]
          :state.muted [{:run follow-volume}]
          :state.page [{:run place-changed}]
          :state.mini [{:run liftAssistant}]
          :state.current [{:run place-changed}]
          :state.collection [{:run place-changed}]}
  :onLoad [{:run liftAssistant} {:run buildLibrary}])

(defn sort-library "The library's table sorted by a column, its header clicked." []
  (set "state.librarySort" {:column event.column :descending event.descending}))

(defn show-playing-list "The player's cover clicked: the list playing, or the library." []
  (if (>= state.playingList 0) (open-list state.playingList) (show :library)))

(defn browse-back "Back from an album or an artist to the tiles it was opened from." []
  (show (if (= state.collection.kind :artist) :artists :albums)))

(defn liked-menu "A right click on a liked song: its menu." []
  (when-some [key state.likedPick] (open-song-menu (list-of key) (track-of key))))


(defn switch-equaliser "The equaliser's switch: on or off." []
  (set "state.eqOn" event.value))

(defn set-theme "A theme chosen: dark or light." [theme]
  (set "state.theme" theme))

(defn set-language "A language chosen: en, es or fr." [language]
  (set "settings.ui.language" language))
