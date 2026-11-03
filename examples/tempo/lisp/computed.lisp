;;; computed.lisp --- what Tempo works out from its state: the library, the song playing, the queue, the menus, the
;;; waveform and the level meter

;;; Values

(defcomputed library "The library's songs as table rows: number, title, artist, album and time."
  (map (fn [it] {:id it.id :cells [it.number it.title it.artist it.album it.time] :sound it.sound}) data.songs))

(defcomputed list "The playlist shown."
  (get data.playlists state.current))

(defcomputed playing "The list playing: a playlist, or the library."
  (if (< state.playingList 0) state.libraryInfo (get data.playlists state.playingList)))

(defcomputed track "The song playing: title, artist, album, time and sound."
  (if (< state.playingList 0)
      {:title (get-in computed.library [state.playingTrack :cells 1])
       :artist (get-in computed.library [state.playingTrack :cells 2])
       :album (get-in computed.library [state.playingTrack :cells 3])
       :time (get-in computed.library [state.playingTrack :cells 4])
       :sound (get-in computed.library [state.playingTrack :sound])}
      (get computed.playing.tracks state.playingTrack)))

(defcomputed sound "The sound the song playing plays: its own, or the beat."
  (or-else computed.track.sound "beat"))

(defcomputed loudness "How loud the sound plays, from 0 to 1."
  (if state.muted 0 (/ state.volume 100)))

(defcomputed playingName "The name of what plays: the playlist, the album or artist, or the library."
  (cond (>= state.playingList 0) computed.playing.name (not= state.source "") state.source :else (t "library.all")))

(defcomputed duration "The song playing's length, in seconds."
  (+ (* (number (get (split computed.track.time ":") 0)) 60) (number (get (split computed.track.time ":") 1))))

(defcomputed elapsedText "How far the song is, as minutes:seconds."
  (str (floor (/ state.elapsed 60)) ":" (pad (floor (mod state.elapsed 60)) 2)))

(defcomputed listSizes "Each playlist's number of songs."
  (map (fn [it] (count it.tracks)) data.playlists))

(defcomputed queueSongs "The songs of the queue that are still there, with their titles and artists."
  (map (fn [it]
         (merge it
                (if (< it.list 0)
                    {:title (get-in computed.library [it.track :cells 1])
                     :artist (get-in computed.library [it.track :cells 2])}
                    {:title (get-in data.playlists [it.list :tracks it.track :title])
                     :artist (get-in data.playlists [it.list :tracks it.track :artist])})))
       (filter (fn [it]
                 (if (< it.list 0)
                     (< it.track (count computed.library))
                     (< it.track (or-else (get computed.listSizes it.list) 0))))
               (map (fn [it] {:key it :list (number (get (split it ":") 0)) :track (number (get (split it ":") 1))})
                    state.queue))))

(defcomputed upNext "The queue's rows: title and artist."
  (map (fn [it] {:id it.key :label (str it.title "  ·  " it.artist)}) computed.queueSongs))

(defcomputed queueAt "Where the song picked in the queue is, or -1."
  (if (nil? state.queuePick) -1 (indexOf state.queue state.queuePick)))

(defcomputed picks "The quick pick's songs, while it is open."
  (if state.quickPick.open
      (map-indexed (fn [index it] {:title it.cells.1 :detail (str it.cells.2 " · " it.cells.3) :track index})
                   computed.library)
      []))

(defcomputed playingKey "The song playing, as list:track."
  (str state.playingList ":" state.playingTrack))

(defcomputed songMenuKey "The song menu's song, as list:track."
  (str state.songMenu.list ":" state.songMenu.track))

(defcomputed songMenuSong "The song menu's song."
  (if (< state.songMenu.list 0)
      (merge {:title (get-in computed.library [state.songMenu.track :cells 1])
              :artist (get-in computed.library [state.songMenu.track :cells 2])
              :album (get-in computed.library [state.songMenu.track :cells 3])
              :time (get-in computed.library [state.songMenu.track :cells 4])}
             (if (nil? (get-in computed.library [state.songMenu.track :sound]))
                 {}
                 {:sound (get-in computed.library [state.songMenu.track :sound])}))
      (get-in data.playlists [state.songMenu.list :tracks state.songMenu.track])))

(defcomputed listMinutes "The playlist's length in minutes."
  (round (/ (sum (or-else computed.list.tracks [])
                 (fn [it]
                   (+ (* (number (get (split (or-else it.time "0:0") ":") 0)) 60)
                      (number (get (split (or-else it.time "0:0") ":") 1)))))
            60)))

(defcomputed songMenuItems "The song menu: play, play next, add to the queue or a playlist, like, remove."
  (concat [{:title (t "song.play") :run "play" :with {:list state.songMenu.list :track state.songMenu.track}}
           {:title (t "song.playNext") :run "playNext" :with {:key computed.songMenuKey}}
           {:title (t "song.addToQueue") :run "addToQueue" :with {:key computed.songMenuKey}}
           {:separator true}
           {:title (t "song.addToPlaylist")
            :submenu (concat (map-indexed (fn [index it]
                                            {:title it.name
                                             :enabled (not= index state.songMenu.list)
                                             :run "addToPlaylist"
                                             :with {:list index}})
                                          data.playlists)
                             [{:separator true}
                              {:title (t "playlist.new") :run "newPlaylist" :with {:withSong true}}])}
           {:title (t (if (contains state.liked computed.songMenuKey) "song.unlike" "song.like"))
            :run "toggleLike"
            :with {:key computed.songMenuKey}}]
          (if (< state.songMenu.list 0)
              []
              [{:title (t "song.remove")
                :run "removeFromPlaylist"
                :with {:list state.songMenu.list :track state.songMenu.track}}])))

(defcomputed listMenuItems "The playlist's menu: rename, delete."
  [{:title (t "playlist.rename") :run "startRename"} {:title (t "playlist.delete") :run "askDelete"}])

(defcomputed palette "The command palette's rows: every command that can run, with its keys."
  (map (fn [it]
         {:title it.title
          :detail (if it.key
                      (join "+"
                            (map (fn [it] (if (= it "mod") "Cmd" (str (upper-case (slice it 0 1)) (slice it 1))))
                                 (split it.key "+")))
                      it.category)
          :run it.name})
       (filter (fn [it] it.enabled) commands)))

(defcomputed wave "The song's waveform: 64 heights, from its title and place."
  (map-indexed (fn [index it]
                 (+ 3 (mod (+ (* index index (+ (count computed.track.title) state.playingTrack)) (* index 7)) 21)))
               (range 64)))

(defcomputed played "How many of the waveform's 64 lines are played."
  (clamp (floor (/ (* 64 state.elapsed) (max computed.duration 1))) 0 64))

(defcomputed wavePlayed "The waveform played, as a path."
  (or (join " "
            (map-indexed (fn [index it] (str "M " (+ (* index 2) 1) " " (- 28 it) " V " (+ 28 it)))
                         (slice computed.wave 0 computed.played)))
      "M -40 -40"))

(defcomputed waveLeft "The waveform left to play, as a path."
  (or (join " "
            (map-indexed (fn [index it]
                           (str "M " (+ (* (+ computed.played index) 2) 1) " " (- 28 it) " V " (+ 28 it)))
                         (slice computed.wave computed.played)))
      "M -40 -40"))

(defcomputed meter "The level meter: 8 bars, as a path."
  (join " "
        (map-indexed (fn [index it]
                       (str "M "
                            (+ 2 (* index 4))
                            " 20 V "
                            (- 20
                               (if state.isPlaying (+ 3 (mod (+ (* index 7) (* state.elapsed (+ index 3))) 15)) 2))))
                     (range 8))))

