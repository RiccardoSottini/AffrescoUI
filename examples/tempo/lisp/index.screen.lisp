{:$schema "../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :strings {:default en :locale "{{settings.ui.language}}"}
 :note "Tempo: a music player. Playlists, a library of 3,000 songs in a table, albums and artists in a grid, search, a queue to reorder by dragging or with Alt and the arrows, now playing with a visualiser of the spectrum of what plays and lyrics, an equaliser on a curves control, crossfade and a mini player; light and dark themes, English, Spanish and French. Cmd+P finds a song and Cmd+Shift+P runs any command. Space plays or pauses, the arrows skip and set the volume, Cmd+L, Cmd+2, Cmd+3, Cmd+4 (the liked songs), Cmd+F, Cmd+5 and Cmd+6 change the page, Cmd+U shows the queue, Cmd+M the mini player, Cmd+T switches light and dark and Cmd+Shift+L the language; Cmd+S shuffles, Cmd+R repeats all or one song, Cmd+Alt+Down mutes, Cmd+[ and Cmd+] go back and forward through the pages, and a click on a line of the lyrics goes there. The heart in the player likes the song playing; a right click (or Shift+F10) on a song opens its menu: play, play next, add to the queue, like. The songs are simulated; what plays is one beatbox loop (audio/beatbox.wav, made by audio/make_beat.py), on hosts that give the screen sound. A song may have a sound of its own: its \"sound\" (a track's in state.playlists[i].tracks[j], a library row's in state.library[i]) names a sound of the audio section, e.g. { \"file\": \"asset:song.mp3\", \"into\": \"state.sounds.own\" } for an asset the person attached, and playing that song plays it instead of the beat. The equaliser and the crossfade are visual only: they don't change the sound."
 :include ["locales/en.lisp"
           "locales/es.lisp"
           "locales/fr.lisp"
           pages.lisp
           "../../shared/addons/assistant/assistant.lisp"
           tempo.lisp]
 :window {:title "Tempo: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
 :persist [state.theme state.settings state.eq state.eqOn state.volume state.shuffle state.repeat state.liked]
 :audio {:beat {:file "../assets/audio/beatbox.wav"
                :loop true
                :volume 0.7
                :into state.beat
                :bands 32
                :updates 50
                :note "The sound of a song without a sound of its own: a beatbox loop made for Tempo (audio/make_beat.py)"}}
 :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
         :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [iconsSmall]}
         :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :fallback [icons]}
         :large {:file "../../shared/fonts/Roboto-Regular.ttf" :size 32}
         :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 44}
         :brand {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24}
         :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
         :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
         :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 36}}
 :theme "{{state.theme}}"
 :colors {:background {:dark "#10131c" :light "#f6f3ec"}
          :sidebar {:dark "#0b0d14" :light "#ece7dc"}
          :surface {:dark "#161a26" :light "#fffdf8"}
          :rowHover {:dark "#1e2332" :light "#f0eadd"}
          :playingRow {:dark "#2b2619" :light "#fbecc8"}
          :bar {:dark "#1a1f2d" :light "#fffdf8"}
          :text {:dark "#eef1f7" :light "#1d1a14"}
          :muted {:dark "#8d95a8" :light "#6d675a"}
          :faint {:dark "#323a4d" :light "#d8d0bf"}
          :border {:dark "#232a3a" :light "#e3dccd"}
          :accent {:dark "#f2b544" :light "#b45309"}
          :accentHover {:dark "#ffcb6b" :light "#cc6a14"}
          :accentSoft {:dark "#352d1a" :light "#fbe7c2"}
          :onAccent {:dark "#1c1405" :light "#ffffff"}
          :button {:dark "#232a3a" :light "#ece7dc"}
          :buttonHover {:dark "#2d3548" :light "#e2dbcb"}
          :menu {:dark "#1a1f2d" :light "#fffdf8"}
          :field {:dark "#10131c" :light "#f6f3ec"}}
 :tokens {:assistant.accent @accent
          :assistant.accentHover @accentHover
          :assistant.onAccent @onAccent
          :assistant.surface @menu
          :assistant.raised @button
          :assistant.border @border
          :assistant.text @text
          :assistant.dim @muted
          :assistant.dot @faint
          :assistant.warningBackground @accentSoft
          :kit.background @background
          :kit.surface @menu
          :kit.bar @bar
          :kit.barHover @buttonHover
          :kit.field @field
          :kit.text @text
          :kit.dim @muted
          :kit.body @muted
          :kit.selection @accentSoft
          :kit.border @border
          :kit.primary @accent
          :kit.onPrimary @onAccent
          :kit.secondary @button
          :kit.onSecondary @text
          :kit.accent @accent
          :kit.focus @accent
          :kit.radius 10
          :kit.cardRadius 16}
 :permissions {:allow [studio]
               :ask ["agent(/actions)" "agent(/program)"]
               :deny ["*(#statusBar)"]
               :reviewers [studio]
               :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
 :settings {:playback.crossfade (integer :minimum 0
                                  :maximum 12
                                  :default 0
                                  :title "Crossfade"
                                  :description "How many seconds the end of a song overlaps the start of the next; 0 for none.")
            :ui.language (string :enum [en es fr]
                           :default en
                           :title "Language"
                           :description "English, Spanish or French.")}
 :state {:theme dark
         :page playlist
         :current 0
         :playingList 0
         :playingTrack 2
         :isPlaying true
         :beat {:status stopped :playing false}
         :sounds {}
         :soundOn ""
         :elapsed 84
         :volume 70
         :muted false
         :shuffle false
         :shuffleSeed 0
         :repeat off
         :order ["0:2" "0:3" "0:4" "0:5" "0:6" "0:7" "0:8"]
         :where {:page playlist :current 0 :collection nil}
         :backStack []
         :forwardStack []
         :navigating false
         :queueOpen true
         :queue ["0:3" "0:4" "0:5" "0:6" "0:7" "0:8"]
         :queuePick nil
         :queueNote ""
         :source ""
         :mini false
         :sizes [744]
         :menu [{:id library :icon music}
                {:id albums :icon disc-3}
                {:id artists :icon users}
                {:id liked :icon heart}
                {:id search :icon search}
                {:id nowPlaying :icon radio}
                {:id sound :icon sliders-horizontal}]
         :words {:adjectives ["Silver"
                              "Quiet"
                              "Electric"
                              "Paper"
                              "Golden"
                              "Hollow"
                              "Velvet"
                              "Neon"
                              "Distant"
                              "Broken"
                              "Gentle"
                              "Midnight"
                              "Crystal"
                              "Wild"
                              "Slow"
                              "Bright"]
                 :nouns ["Rivers"
                         "Signals"
                         "Gardens"
                         "Engines"
                         "Harbours"
                         "Lanterns"
                         "Mirrors"
                         "Satellites"
                         "Islands"
                         "Letters"
                         "Horizons"
                         "Echoes"
                         "Windows"
                         "Machines"
                         "Shadows"
                         "Bridges"
                         "Comets"
                         "Forests"
                         "Parades"
                         "Tides"]
                 :artists ["The Shaders"
                           "Nora Vale"
                           "Kerning Club"
                           "Idle Loop"
                           "Halfway"
                           "Mira Sol"
                           "Ada Quartet"
                           "Leo Marsh Trio"
                           "Quiet Rooms"
                           "Pixel Choir"
                           "North Arcade"
                           "Low Orbit"]
                 :albums ["Core Profile"
                          "Redraw"
                          "Baseline"
                          "Zero Percent"
                          "Pace"
                          "Stride"
                          "Standards"
                          "Avenue"
                          "Hours"
                          "Night Shift"
                          "Vectors"
                          "Long Weekend"
                          "Blueprints"
                          "Static"
                          "Open Air"
                          "Afterglow"
                          "Field Notes"
                          "Overture"]
                 :versions ["Live" "Remix" "Demo" "Edit" "Acoustic" "Reprise" "Mono" "Dub" "Take 2"]}
         :libraryInfo {:name "All songs" :cover "../assets/images/cover-library.svg"}
         :librarySort nil
         :libraryPick nil
         :liked []
         :likedPick nil
         :songMenu {:open false :x 0 :y 0 :list -1 :track 0}
         :listMenu {:open false :x 0 :y 0}
         :renaming false
         :nameDraft ""
         :deleting false
         :quickPick {:open false :query ""}}
 :stateTypes {:theme (string :enum [dark light])
              :page (string :enum [playlist library albums artists liked collection search nowPlaying sound])
              :current (number)
              :playingList (number)
              :playingTrack (number)
              :isPlaying (boolean)
              :beat (object :properties {:status (string) :playing (boolean) :spectrum (array :items (number))})
              :sounds (object)
              :soundOn (string)
              :elapsed (number)
              :volume (number :minimum 0 :maximum 100)
              :muted (boolean)
              :shuffle (boolean)
              :shuffleSeed (number)
              :repeat (string :enum [off all one])
              :order (array :maxItems 1000 :items (string :maxLength 16))
              :backStack (array :maxItems 50)
              :forwardStack (array :maxItems 50)
              :navigating (boolean)
              :queueOpen (boolean)
              :queue (array :maxItems 1000 :items (string :maxLength 16))
              :queuePick (node :type [string null])
              :queueNote (string)
              :source (string)
              :mini (boolean)
              :sizes (array :items (number))
              :menu (array :items (object :properties {:id (string) :icon (string)}))
              :words (object)
              :libraryInfo (object :properties {:name (string) :cover (string)})
              :librarySort (node :type [object null])
              :libraryPick (node :type [string null])
              :liked (array :maxItems 5000 :items (string :maxLength 16))
              :likedPick (node :type [string null])
              :songMenu (object :properties {:open (boolean)
                                             :x (number)
                                             :y (number)
                                             :list (integer)
                                             :track (integer :minimum 0)})
              :listMenu (object :properties {:open (boolean) :x (number) :y (number)})
              :renaming (boolean)
              :nameDraft (string :maxLength 200)
              :deleting (boolean)
              :quickPick (object :properties {:open (boolean) :query (string)})}
 :models {:songs {:version 1
                  :title "Songs"
                  :note "The library, made once from the word lists when the app first starts (buildLibrary)"
                  :fields {:number (integer :minimum 1 :required true)
                           :title (string :required true)
                           :artist (string)
                           :album (string)
                           :time (string :maxLength 8)
                           :sound (string :maxLength 120
                                    :note "The name of a declared audio sound that plays for it (e.g. one whose file is an asset:); none: the beat")}}
          :playlists {:version 1
                      :title "Playlists"
                      :fields {:name (string :required true)
                               :cover (string)
                               :description (string)
                               :length (string)
                               :tracks (list :of (object :fields {:title (string)
                                                                  :artist (string)
                                                                  :album (string)
                                                                  :time (string)
                                                                  :sound (string)})
                                         :maxItems 1000)}
                      :seed [{:id p1
                              :name "Late Night Code"
                              :cover "../assets/images/cover-late-night.svg"
                              :description "Low-key beats for long sessions. Updated every Friday."
                              :length "38 min"
                              :tracks [{:title "Vertex Buffers"
                                        :artist "The Shaders"
                                        :album "Core Profile"
                                        :time "3:42"}
                                       {:title "Dirty Flags" :artist "Nora Vale" :album "Redraw" :time "4:05"}
                                       {:title "One Draw Call" :artist "The Shaders" :album "Core Profile" :time "3:18"}
                                       {:title "Glyph Atlas" :artist "Kerning Club" :album "Baseline" :time "5:01"}
                                       {:title "Sixteen Bytes" :artist "Nora Vale" :album "Redraw" :time "2:57"}
                                       {:title "Wait Events" :artist "Idle Loop" :album "Zero Percent" :time "4:33"}
                                       {:title "Reconcile" :artist "Kerning Club" :album "Baseline" :time "3:50"}
                                       {:title "Hot Path" :artist "Idle Loop" :album "Zero Percent" :time "3:24"}
                                       {:title "Retina" :artist "The Shaders" :album "Core Profile" :time "4:12"}]}
                             {:id p2
                              :name "Morning Run"
                              :cover "../assets/images/cover-morning-run.svg"
                              :description "Fast, bright and loud. 160 bpm and up."
                              :length "24 min"
                              :tracks [{:title "First Light" :artist "Halfway" :album "Pace" :time "3:05"}
                                       {:title "Uphill" :artist "Mira Sol" :album "Stride" :time "2:48"}
                                       {:title "Second Wind" :artist "Halfway" :album "Pace" :time "3:31"}
                                       {:title "Finish Line" :artist "Mira Sol" :album "Stride" :time "4:02"}]}
                             {:id p3
                              :name "Sunday Jazz"
                              :cover "../assets/images/cover-sunday-jazz.svg"
                              :description "Slow standards and new takes on old tunes."
                              :length "51 min"
                              :tracks [{:title "Blue Hour" :artist "Ada Quartet" :album "Standards" :time "6:12"}
                                       {:title "Rainy Avenue" :artist "Leo Marsh Trio" :album "Avenue" :time "5:40"}
                                       {:title "Slow Burn" :artist "Ada Quartet" :album "Standards" :time "7:03"}]}
                             {:id p4
                              :name "Focus"
                              :cover "../assets/images/cover-focus.svg"
                              :description "Ambient textures without words."
                              :length "1 h 12 min"
                              :tracks [{:title "Deep Work" :artist "Quiet Rooms" :album "Hours" :time "8:20"}
                                       {:title "Flow State" :artist "Quiet Rooms" :album "Hours" :time "9:45"}]}]}}
 :computed {:library "{{map(data.songs, { id: it.id, cells: [it.number, it.title, it.artist, it.album, it.time], sound: it.sound })}}"
            :list "{{data.playlists[state.current]}}"
            :playing "{{state.playingList < 0 ? state.libraryInfo : data.playlists[state.playingList]}}"
            :track "{{state.playingList < 0 ? { title: computed.library[state.playingTrack].cells[1], artist: computed.library[state.playingTrack].cells[2], album: computed.library[state.playingTrack].cells[3], time: computed.library[state.playingTrack].cells[4], sound: computed.library[state.playingTrack].sound } : computed.playing.tracks[state.playingTrack]}}"
            :sound "{{computed.track.sound ?? 'beat'}}"
            :loudness "{{state.muted ? 0 : state.volume / 100}}"
            :playingName "{{state.playingList >= 0 ? computed.playing.name : state.source != '' ? state.source : t('library.all')}}"
            :duration "{{number(split(computed.track.time, ':')[0]) * 60 + number(split(computed.track.time, ':')[1])}}"
            :elapsedText "{{floor(state.elapsed / 60)}}:{{pad(floor(state.elapsed % 60), 2)}}"
            :listSizes "{{map(data.playlists, length(it.tracks))}}"
            :queueSongs "{{map(filter(map(state.queue, { key: it, list: number(split(it, ':')[0]), track: number(split(it, ':')[1]) }), it.list < 0 ? it.track < length(computed.library) : it.track < (computed.listSizes[it.list] ?? 0)), merge(it, it.list < 0 ? { title: computed.library[it.track].cells[1], artist: computed.library[it.track].cells[2] } : { title: data.playlists[it.list].tracks[it.track].title, artist: data.playlists[it.list].tracks[it.track].artist }))}}"
            :upNext "{{map(computed.queueSongs, { id: it.key, label: it.title + '  ·  ' + it.artist })}}"
            :queueAt "{{state.queuePick == null ? -1 : indexOf(state.queue, state.queuePick)}}"
            :picks "{{state.quickPick.open ? map(computed.library, { title: it.cells[1], detail: it.cells[2] + ' · ' + it.cells[3], track: index }) : []}}"
            :playingKey "{{state.playingList}}:{{state.playingTrack}}"
            :songMenuKey "{{state.songMenu.list}}:{{state.songMenu.track}}"
            :songMenuSong "{{state.songMenu.list < 0 ? merge({ title: computed.library[state.songMenu.track].cells[1], artist: computed.library[state.songMenu.track].cells[2], album: computed.library[state.songMenu.track].cells[3], time: computed.library[state.songMenu.track].cells[4] }, computed.library[state.songMenu.track].sound == null ? {} : { sound: computed.library[state.songMenu.track].sound }) : data.playlists[state.songMenu.list].tracks[state.songMenu.track]}}"
            :listMinutes "{{round(sum(computed.list.tracks ?? [], number(split(it.time ?? '0:0', ':')[0]) * 60 + number(split(it.time ?? '0:0', ':')[1])) / 60)}}"
            :songMenuItems "{{concat([{ title: t('song.play'), run: 'play', with: { list: state.songMenu.list, track: state.songMenu.track } }, { title: t('song.playNext'), run: 'playNext', with: { key: computed.songMenuKey } }, { title: t('song.addToQueue'), run: 'addToQueue', with: { key: computed.songMenuKey } }, { separator: true }, { title: t('song.addToPlaylist'), submenu: concat(map(data.playlists, { title: it.name, enabled: index != state.songMenu.list, run: 'addToPlaylist', with: { list: index } }), [{ separator: true }, { title: t('playlist.new'), run: 'newPlaylist', with: { withSong: true } }]) }, { title: t(contains(state.liked, computed.songMenuKey) ? 'song.unlike' : 'song.like'), run: 'toggleLike', with: { key: computed.songMenuKey } }], state.songMenu.list < 0 ? [] : [{ title: t('song.remove'), run: 'removeFromPlaylist', with: { list: state.songMenu.list, track: state.songMenu.track } }])}}"
            :listMenuItems "{{[{ title: t('playlist.rename'), run: 'startRename' }, { title: t('playlist.delete'), run: 'askDelete' }]}}"
            :palette "{{map(filter(commands, it.enabled), { title: it.title, detail: it.key ? join(map(split(it.key, '+'), it == 'mod' ? 'Cmd' : upper(slice(it, 0, 1)) + slice(it, 1)), '+') : it.category, run: it.name })}}"
            :wave "{{map(range(64), 3 + (index * index * (length(computed.track.title) + state.playingTrack) + index * 7) % 21)}}"
            :played "{{clamp(floor(64 * state.elapsed / max(computed.duration, 1)), 0, 64)}}"
            :wavePlayed "{{join(map(slice(computed.wave, 0, computed.played), 'M ' + (index * 2 + 1) + ' ' + (28 - it) + ' V ' + (28 + it)), ' ') || 'M -40 -40'}}"
            :waveLeft "{{join(map(slice(computed.wave, computed.played), 'M ' + ((computed.played + index) * 2 + 1) + ' ' + (28 - it) + ' V ' + (28 + it)), ' ') || 'M -40 -40'}}"
            :meter "{{join(map(range(8), 'M ' + (2 + index * 4) + ' 20 V ' + (20 - (state.isPlaying ? 3 + (index * 7 + state.elapsed * (index + 3)) % 15 : 2))), ' ')}}"}
 :styles {:caption (label :font small :textColor @muted :fitText true)
          :pageTitle (label :font large :textColor @text :anchors {:left 32 :top 24} :fitText true)
          :control (label :text ""
                     :height 32
                     :radius 16
                     :align center
                     :color @bar
                     :hover {:color @buttonHover}
                     :focused {:color @buttonHover}
                     :textColor @text
                     :cursor hand
                     :focusable true
                     :role button)
          :chip (label :font small
                  :padding 9
                  :radius 15
                  :align center
                  :fitText true
                  :color @button
                  :hover {:color @buttonHover}
                  :focused {:borderColor @accent}
                  :borderWidth 1
                  :borderColor transparent
                  :textColor @text
                  :cursor hand
                  :focusable true
                  :role button)
          :playButton (label :icon play
                        :text "  Play"
                        :font heading
                        :width 112
                        :height 36
                        :radius 18
                        :align center
                        :color @accent
                        :hover {:color @accentHover}
                        :focused {:color @accentHover}
                        :textColor @onAccent
                        :cursor hand
                        :focusable true
                        :role button)}
 :keys {:space playPause
        :right nextCommand
        :left previousCommand
        :up volumeUp
        :down volumeDown
        "mod+p" searchCommand
        "mod+shift+p" commandPalette
        "mod+l" showLibrary
        "mod+2" showAlbums
        "mod+3" showArtists
        "mod+4" showLiked
        "mod+f" showSearch
        "mod+5" showNowPlaying
        "mod+6" showSound
        "mod+u" toggleQueue
        "mod+m" toggleMini
        "mod+t" toggleTheme
        "mod+shift+l" switchLanguage
        "mod+s" toggleShuffle
        "mod+r" cycleRepeat
        "mod+alt+down" toggleMute
        "mod+leftbracket" goBack
        "mod+rightbracket" goForward}
 :watch {:state.isPlaying [{:run followPlaying}]
         :state.playingTrack [{:run songChanged}]
         :state.playingList [{:run songChanged}]
         :state.volume [{:run followVolume}]
         :state.muted [{:run followVolume}]
         :state.page [{:run placeChanged}]
         :state.mini [{:run liftAssistant}]
         :state.current [{:run placeChanged}]
         :state.collection [{:run placeChanged}]}
 :onLoad [{:run liftAssistant} {:run buildLibrary}]
 :root (node#panel-root :color @background :every {:ms 1000 :do tick}
         (node#sidebar :when "{{!state.mini}}"
           :dir auto
           :anchors {:left 0 :top 0 :bottom 112}
           :width 248
           :color @sidebar
           :role navigation
           :label "{{t('menu.label')}}"
           (label#logo :text "Tempo" :font brand :textColor @text :anchors {:left 28 :top 28} :fitText true)
           (node#label-an-affresco-ui :style caption :text "{{t('app.tagline')}}" :anchors {:left 28 :top 62})
           (node#backButton :style control
             :icon chevron-left
             :mirror true
             :anchors {:left 96 :top 26}
             :width 24
             :radius 8
             :color @sidebar
             :textColor "{{length(state.backStack) > 0 ? '@text' : '@faint'}}"
             :disabled "{{length(state.backStack) == 0}}"
             :label "{{t('nav.back')}}"
             :tooltip "{{t('nav.back')}}"
             :onClick goBack)
           (node#forwardButton :style control
             :icon chevron-right
             :mirror true
             :anchors {:left 122 :top 26}
             :width 24
             :radius 8
             :color @sidebar
             :textColor "{{length(state.forwardStack) > 0 ? '@text' : '@faint'}}"
             :disabled "{{length(state.forwardStack) == 0}}"
             :label "{{t('nav.forward')}}"
             :tooltip "{{t('nav.forward')}}"
             :onClick goForward)
           (node#themeButton :style control
             :icon "{{state.theme == 'dark' ? 'sun' : 'moon'}}"
             :anchors {:right 56 :top 26}
             :width 32
             :radius 8
             :color @sidebar
             :label "{{t('theme.toggle')}}"
             :tooltip "{{t('theme.toggle')}}"
             :onClick toggleTheme)
           (node#languageButton :style control
             :font small
             :text "{{upper(settings.ui.language)}}"
             :anchors {:right 16 :top 26}
             :width 36
             :radius 8
             :color @sidebar
             :label "{{t('language.toggle')}}"
             :tooltip "{{t('language.toggle')}}"
             :onClick switchLanguage)
           (stack#menu :anchors {:left 12 :right 12 :top 96}
             :height 264
             :gap 2
             :role tablist
             :arrows vertical
             :label "{{t('menu.label')}}"
             (label#label-menu :repeat {:for item :in state.menu}
               :let {:on "{{item.id == state.page || (item.id == 'albums' && state.page == 'collection' && state.collection.kind == 'album') || (item.id == 'artists' && state.page == 'collection' && state.collection.kind == 'artist')}}"}
               :icon "{{item.icon}}"
               :text "    {{t('menu.' + item.id)}}"
               :anchors {:left 0 :right 0}
               :height 36
               :padding 16
               :radius 18
               :align {:horizontal start :vertical center}
               :color "{{on ? '@accentSoft' : '@sidebar'}}"
               :hover {:color "{{on ? '@accentSoft' : '@surface'}}"}
               :focused {:color "{{on ? '@accentSoft' : '@surface'}}"}
               :textColor "{{on ? '@accent' : '@muted'}}"
               :cursor hand
               :focusable true
               :role tab
               :selected "{{on}}"
               :label "{{t('menu.' + item.id)}}"
               :tooltip "{{t('tip.' + item.id)}}"
               :onClick {:set state.page :to "{{item.id}}"}))
           (node#label-playlists :style caption :text "{{t('playlists.caption')}}" :anchors {:left 28 :top 372})
           (node#newPlaylist :style control
             :icon plus
             :width 28
             :height 28
             :radius 10
             :color @sidebar
             :anchors {:right 16 :top 364}
             :label "{{t('playlist.create')}}"
             :tooltip "{{t('playlist.create')}}"
             :onClick {:run newPlaylist :with {:withSong false}})
           (stack#playlists :anchors {:left 12 :right 12 :top 394 :bottom 12}
             :gap 4
             :scroll vertical
             :role list
             :label "{{t('playlists.label')}}"
             (node#playlist :repeat {:for list :in data.playlists}
               :let {:open "{{state.page == 'playlist' && index == state.current}}"}
               :anchors {:left 0 :right 0}
               :height 56
               :radius 14
               :color "{{open ? '@surface' : '@sidebar'}}"
               :hover {:color @surface}
               :focused {:color @surface}
               :cursor hand
               :focusable true
               :role button
               :selected "{{open}}"
               :label "{{list.name}}, {{t('songs', { n: length(list.tracks) })}}"
               :onClick {:run openList :with {:list "{{index}}"}}
               :onDoubleClick {:run play :with {:list "{{index}}" :track 0}}
               :tooltip "{{t('playlist.play')}}"
               (image#image-list-cover :file "{{list.cover}}"
                 :fit cover
                 :radius 10
                 :anchors {:left 10 :top 8}
                 :width 40
                 :height 40)
               (label#label-list-name :text "{{list.name}}" :textColor @text :anchors {:left 62 :top 10} :fitText true)
               (node#label-plural-list-tracks :style caption
                 :text "{{t('songs', { n: length(list.tracks) })}}"
                 :anchors {:left 62 :top 31})
               (node#panel-playlist :when "{{index == state.playingList && state.isPlaying}}"
                 :anchors {:right 14 :top 25}
                 :width 6
                 :height 6
                 :radius 3
                 :color @accent))))
         (split#main :when "{{!state.mini}}"
           :anchors {:left 248 :right 0 :top 0 :bottom 112}
           :bind state.sizes
           :defaultSizes [744]
           :minSize [480 240]
           :sashSize 4
           :sashColor @border
           :sashActiveColor @accent
           (node#content :color transparent :role main
             (node#playlistView :when "{{state.page == 'playlist'}}" :dir auto :fill true :color transparent
               (node#header :anchors {:left 0 :right 0 :top 0}
                 :height 264
                 :color @surface
                 :gradient {:kind linear :angle 90 :stops [@accentSoft @surface]}
                 (image#cover :file "{{computed.list.cover}}"
                   :fit cover
                   :radius 24
                   :anchors {:right 40 :top 48}
                   :width 168
                   :height 168
                   :role img
                   :label "{{computed.list.name}}")
                 (node#label-playlist :style caption :text "{{t('playlist.caption')}}" :anchors {:left 40 :top 62})
                 (label#listName :when "{{!state.renaming}}"
                   :text "{{computed.list.name}}"
                   :font title
                   :textColor @text
                   :anchors {:left 36 :top 80}
                   :fitText true
                   :tooltip "{{t('playlist.rename')}}"
                   :onDoubleClick startRename)
                 (textarea#nameField :when "{{state.renaming}}"
                   :bind state.nameDraft
                   :placeholder "{{computed.list.name}}"
                   :placeholderColor @muted
                   :autofocus true
                   :font title
                   :textColor @text
                   :color @field
                   :radius 12
                   :borderWidth 1
                   :borderColor @accent
                   :anchors {:left 32 :right 232 :top 78}
                   :height 52
                   :label "{{t('playlist.nameLabel')}}"
                   :onSubmit renameList
                   :keys {:escape cancelRename})
                 (label#listDescription :text "{{computed.list.description}}"
                   :textColor @muted
                   :anchors {:left 40 :top 140}
                   :fitText true)
                 (node#label-plural-list-tracks-2 :style caption
                   :text "{{t('songs', { n: length(computed.list.tracks) })}}  ·  {{t('playlist.minutes', { n: computed.listMinutes })}}"
                   :textColor @text
                   :anchors {:left 40 :top 166})
                 (node#playButton :style playButton
                   :text "{{t('play')}}"
                   :label "{{t('play.label', { name: computed.list.name })}}"
                   :anchors {:left 40 :top 196}
                   :disabled "{{length(computed.list.tracks) == 0}}"
                   :onClick {:run play :with {:list "{{state.current}}" :track 0}})
                 (node#listMore :style control
                   :icon ellipsis
                   :width 36
                   :height 36
                   :radius 18
                   :color @button
                   :anchors {:left 164 :top 196}
                   :label "{{t('playlist.more', { name: computed.list.name })}}"
                   :tooltip "{{t('playlist.more', { name: computed.list.name })}}"
                   :onClick openListMenu))
               (label#playlistEmpty :when "{{length(computed.list.tracks) == 0}}"
                 :text "{{t('playlist.empty')}}"
                 :textColor @muted
                 :anchors {:left 52 :top 340}
                 :fitText true)
               (node#columns :anchors {:left 40 :right 40 :top 284} :height 28 :color transparent
                 (node#label-caption :style caption :text "{{t('column.number')}}" :anchors {:left 12 :top 6})
                 (node#label-title :style caption :text "{{upper(t('column.title'))}}" :anchors {:left 52 :top 6})
                 (node#label-album :style caption :text "{{upper(t('column.album'))}}" :x "55%" :y 6)
                 (node#label-time :style caption :text "{{upper(t('column.time'))}}" :anchors {:right 16 :top 6})
                 (node#panel-columns :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @border))
               (stack#tracks :anchors {:left 40 :right 40 :top 320 :bottom 8}
                 :gap 2
                 :scroll vertical
                 :role list
                 :label "{{t('playlist.tracks', { name: computed.list.name })}}"
                 (node#track :repeat {:for track :in computed.list.tracks :index t}
                   :let {:playing "{{state.current == state.playingList && t == state.playingTrack}}"}
                   :anchors {:left 0 :right 0}
                   :height 40
                   :radius 6
                   :color "{{playing ? '@playingRow' : 'transparent'}}"
                   :hover {:color "{{playing ? '@playingRow' : '@rowHover'}}"}
                   :focused {:color @rowHover}
                   :cursor hand
                   :focusable true
                   :role button
                   :label "{{track.title}}, {{track.artist}}, {{track.time}}"
                   :onClick {:run play :with {:list "{{state.current}}" :track "{{t}}"}}
                   :onContextMenu {:run openSongMenu :with {:list "{{state.current}}" :track "{{t}}"}}
                   (label#label-t-1 :icon "{{playing && state.isPlaying ? 'audio-lines' : ''}}"
                     :text "{{playing && state.isPlaying ? '' : t + 1}}"
                     :textColor "{{playing ? '@accent' : '@muted'}}"
                     :anchors {:left 12 :top 0 :bottom 0}
                     :width 30
                     :align {:horizontal start :vertical center})
                   (label#label-track-title :text "{{track.title}}"
                     :textColor "{{playing ? '@accent' : '@text'}}"
                     :anchors {:left 52 :top 3}
                     :fitText true)
                   (node#label-track-artist :style caption :text "{{track.artist}}" :anchors {:left 52 :top 22})
                   (label#label-track-album :text "{{track.album}}" :textColor @muted :x "55%" :y 11 :fitText true)
                   (label#label-track-time :text "{{track.time}}"
                     :textColor @muted
                     :anchors {:right 16 :top 11}
                     :fitText true))))
             (node#libraryView :when "{{state.page == 'library'}}" :dir auto :fill true :color transparent
               (node#libraryHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
                 (image#libraryCover :file "{{state.libraryInfo.cover}}"
                   :fit cover
                   :radius 6
                   :anchors {:left 32 :top 24}
                   :width 84
                   :height 84)
                 (node#label-library :style caption :text "{{t('library.caption')}}" :anchors {:left 140 :top 28})
                 (label#libraryName :text "{{t('library.all')}}"
                   :font large
                   :textColor @text
                   :anchors {:left 138 :top 44}
                   :fitText true)
                 (node#libraryCount :style caption
                   :text "{{t('library.count', { n: length(computed.library) })}}"
                   :anchors {:left 140 :top 88})
                 (node#playLibraryButton :style playButton
                   :text "{{t('play')}}"
                   :label "{{t('play.label', { name: t('library.all') })}}"
                   :anchors {:right 32 :top 50}
                   :onClick playLibrary))
               (table#libraryTable :anchors {:left 24 :right 24 :top 148 :bottom 8}
                 :color transparent
                 :rowHeight 30
                 :items "{{computed.library}}"
                 :columns [{:id number :title "{{t('column.number')}}" :width 60}
                           {:id title :title "{{t('column.title')}}" :width 230}
                           {:id artist :title "{{t('column.artist')}}" :width 150}
                           {:id album :title "{{t('column.album')}}" :width 170}
                           {:id time :title "{{t('column.time')}}" :width 64}]
                 :sort "{{state.librarySort}}"
                 :bind state.libraryPick
                 :label "{{t('library.label')}}"
                 :textColor @text
                 :selectedTextColor @text
                 :selectedBackground @accentSoft
                 :accentColor @accent
                 :onOpen playFromLibrary
                 :onContextMenu {:run openPickMenu :with {:pick "{{state.libraryPick}}"}}
                 :onSort {:set state.librarySort :to {:column "{{event.column}}" :descending "{{event.descending}}"}}))
             (node#browseView :when "{{state.page == 'albums' || state.page == 'artists'}}" :use browsePage :fill true)
             (node#collectionView :when "{{state.page == 'collection'}}" :use collectionPage :fill true)
             (node#searchView :when "{{state.page == 'search'}}" :use searchPage :fill true)
             (node#nowPlayingView :when "{{state.page == 'nowPlaying'}}" :use nowPlayingPage :fill true)
             (node#likedView :when "{{state.page == 'liked'}}" :use likedPage :fill true)
             (node#soundView :when "{{state.page == 'sound'}}" :use soundPage :fill true))
           (node#queuePane :when "{{state.queueOpen}}"
             :dir auto
             :color @surface
             :role complementary
             :label "{{t('queue.upNext')}}"
             (node#label-now-playing :style caption :text "{{t('queue.now')}}" :anchors {:left 20 :top 22})
             (image#queueCover :file "{{computed.playing.cover}}"
               :fit cover
               :radius 12
               :anchors {:left 20 :top 48}
               :width 96
               :height 96)
             (node#queueFrom :style caption
               :text "{{t('queue.from', { name: computed.playingName })}}"
               :anchors {:left 132 :top 54})
             (label#queueTitle :text "{{computed.track.title}}"
               :font heading
               :textColor @text
               :anchors {:left 20 :top 160}
               :fitText true)
             (label#queueArtist :text "{{computed.track.artist}}  ·  {{computed.track.album}}"
               :textColor @muted
               :anchors {:left 20 :top 184}
               :fitText true)
             (path#wavePlayed :d "{{computed.wavePlayed}}"
               :viewBox "0 0 128 56"
               :fit fill
               :anchors {:left 20 :right 20 :top 216}
               :height 48
               :stroke @accent
               :strokeWidth 2.5
               :strokeCap round)
             (path#waveLeft :d "{{computed.waveLeft}}"
               :viewBox "0 0 128 56"
               :fit fill
               :anchors {:left 20 :right 20 :top 216}
               :height 48
               :stroke @faint
               :strokeWidth 2.5
               :strokeCap round)
             (node#queueTime :style caption
               :text "{{computed.elapsedText}} / {{computed.track.time}}"
               :anchors {:left 20 :top 270})
             (node#label-up-next :style caption :text "{{t('queue.upNext')}}" :anchors {:left 20 :top 304})
             (node#queueNote :style caption :live polite :text "{{state.queueNote}}" :anchors {:right 20 :top 304})
             (list#queue :anchors {:left 8 :right 8 :top 326 :bottom 8}
               :color transparent
               :rowHeight 36
               :items "{{computed.upNext}}"
               :bind state.queuePick
               :label "{{t('queue.label')}}"
               :textColor @muted
               :selectedTextColor @text
               :selectedBackground @rowHover
               :accentColor @accent
               :drag {}
               :onDrop dropQueued
               :onOpen jumpTo
               :keys {"alt+up" {:run moveQueued :with {:by -1}} "alt+down" {:run moveQueued :with {:by 1}}})
             (node#queueEmpty :when "{{length(computed.upNext) == 0}}"
               :style caption
               :text "{{t('queue.empty')}}"
               :anchors {:left 20 :top 330})))
         (node#player :when "{{!state.mini}}"
           :anchors {:left 12 :right 12 :bottom 40}
           :height 64
           :radius 16
           :color @bar
           :borderWidth 1
           :borderColor @border
           :role region
           :label "{{t('queue.now')}}"
           (node#shuffleButton :style control
             :icon shuffle
             :anchors {:left 12 :top 16}
             :width 32
             :textColor "{{state.shuffle ? '@accent' : '@muted'}}"
             :checked "{{state.shuffle}}"
             :label "{{t('player.shuffle')}}"
             :tooltip "{{t('player.shuffle')}}"
             :onClick toggleShuffle)
           (node#label-previous :style control
             :icon skip-back
             :anchors {:left 48 :top 16}
             :width 32
             :label "{{t('player.previous')}}"
             :tooltip "{{t('player.previous')}}"
             :onClick previous)
           (node#playPause :style control
             :icon "{{state.isPlaying ? 'pause' : 'play'}}"
             :anchors {:left 86 :top 12}
             :width 40
             :height 40
             :radius 12
             :color @accent
             :hover {:color @accentHover}
             :focused {:color @accentHover}
             :textColor @onAccent
             :label "{{t('player.playPause')}}"
             :tooltip "{{t('player.playPause')}}"
             :onClick playPause)
           (node#label-next :style control
             :icon skip-forward
             :anchors {:left 132 :top 16}
             :width 32
             :label "{{t('player.next')}}"
             :tooltip "{{t('player.next')}}"
             :onClick next)
           (node#repeatButton :style control
             :icon "{{state.repeat == 'one' ? 'repeat-1' : 'repeat'}}"
             :anchors {:left 168 :top 16}
             :width 32
             :textColor "{{state.repeat == 'off' ? '@muted' : '@accent'}}"
             :checked "{{state.repeat != 'off'}}"
             :label "{{t('player.repeat.' + state.repeat)}}"
             :tooltip "{{t('player.repeat.' + state.repeat)}}"
             :onClick cycleRepeat)
           (node#panel-player :anchors {:left 212 :top 14 :bottom 14} :width 1 :color @border)
           (node#nowCard :dir auto :anchors {:left 224 :right 268 :top 0 :bottom 0} :color transparent
             (image#playingCover :file "{{computed.playing.cover}}"
               :fit cover
               :radius 10
               :anchors {:left 0 :top 10}
               :width 44
               :height 44
               :cursor hand
               :focusable true
               :role button
               :label "{{t('now.show', { name: computed.playingName })}}"
               :tooltip "{{t('now.show', { name: computed.playingName })}}"
               :onClick [{:if "{{state.playingList >= 0}}"
                          :then [{:run openList :with {:list "{{state.playingList}}"}}]
                          :else [{:set state.page :to library}]}])
             (label#nowPlaying :anchors {:left 56 :right 36 :top 9}
               :height 20
               :runs [{:text "{{computed.track.title}}" :color @text}
                      {:text "  ·  {{computed.track.artist}}" :color @muted :size 12}]
               :label "{{computed.track.title}}, {{computed.track.artist}}")
             (node#likeButton :style control
               :icon heart
               :anchors {:right 0 :top 2}
               :width 28
               :color @bar
               :textColor "{{contains(state.liked, computed.playingKey) ? '@accent' : '@muted'}}"
               :checked "{{contains(state.liked, computed.playingKey)}}"
               :label "{{t('player.like')}}"
               :tooltip "{{t('player.like')}}"
               :onClick likeSong)
             (node#elapsed :style caption :text "{{computed.elapsedText}}" :anchors {:left 56 :top 37})
             (slider#seek :anchors {:left 96 :right 44 :top 36}
               :height 16
               :max "{{computed.duration}}"
               :step 1
               :focusable true
               :label "{{t('player.seek')}}"
               :bind state.elapsed
               :trackColor @faint
               :activeColor @accent
               :knobColor @accent)
             (node#label-track-time-2 :style caption :text "{{computed.track.time}}" :anchors {:right 4 :top 37}))
           (path#levelMeter :d "{{computed.meter}}"
             :viewBox "0 0 32 20"
             :fit fill
             :anchors {:right 228 :top 22}
             :width 32
             :height 20
             :stroke @accent
             :strokeWidth 2
             :role img
             :label "{{t('player.level')}}"
             :tooltip "{{t('player.level')}}")
           (node#miniButton :style control
             :icon minimize-2
             :anchors {:right 188 :top 16}
             :width 32
             :radius 10
             :textColor @muted
             :label "{{t('player.mini')}}"
             :tooltip "{{t('player.mini')}}"
             :onClick toggleMini)
           (node#queueButton :style control
             :icon list-ordered
             :anchors {:right 152 :top 16}
             :width 32
             :radius 10
             :color "{{state.queueOpen ? '@accentSoft' : '@bar'}}"
             :textColor "{{state.queueOpen ? '@accent' : '@muted'}}"
             :checked "{{state.queueOpen}}"
             :label "{{t('player.queue')}}"
             :tooltip "{{t('player.queue')}}"
             :onClick toggleQueue)
           (node#muteButton :style control
             :icon "{{state.muted ? 'volume-x' : 'volume-2'}}"
             :anchors {:right 120 :top 16}
             :width 28
             :radius 10
             :textColor "{{state.muted ? '@accent' : '@muted'}}"
             :checked "{{state.muted}}"
             :label "{{t('player.mute')}}"
             :tooltip "{{t('player.mute')}}"
             :onClick toggleMute)
           (slider#volume :anchors {:right 16 :top 24}
             :width 100
             :height 16
             :step 5
             :focusable true
             :label "{{t('player.volume')}}"
             :bind state.volume
             :trackColor @faint
             :activeColor @muted
             :knobColor @text
             :tooltip "{{t('player.volume')}}"))
         (node#statusBar :when "{{!state.mini}}" :anchors {:left 0 :right 0 :bottom 0} :height 32 :color @sidebar
           (node#panel-status-bar :anchors {:left 0 :right 0 :top 0} :height 1 :color @border)
           (label#statusText :text "Revision {{revision.number}}  ·  {{revision.author}}: {{revision.message}}"
             :font small
             :textColor @muted
             :anchors {:left 24 :top 0 :bottom 0 :right 200}
             :align {:horizontal start :vertical center})
           (label#statusSize :text "{{window.width}} × {{window.height}}"
             :font small
             :textColor @muted
             :anchors {:right 24 :top 0 :bottom 0}
             :width 160
             :align {:horizontal end :vertical center}))
         (node#miniView :when "{{state.mini}}" :use miniPlayer :fill true)
         (node#songMenu :use contextMenu
           :with {:open state.songMenu.open
                  :items "{{computed.songMenuItems}}"
                  :x "{{state.songMenu.x}}"
                  :y "{{state.songMenu.y}}"
                  :width 240
                  :color @menu
                  :textColor @text
                  :dimColor @faint
                  :highlight @accentSoft})
         (node#listMenu :use contextMenu
           :with {:open state.listMenu.open
                  :items "{{computed.listMenuItems}}"
                  :x "{{state.listMenu.x}}"
                  :y "{{state.listMenu.y}}"
                  :width 180})
         (node#deleteDialog :use dialog
           :with {:open state.deleting
                  :title "{{t('playlist.deleteTitle')}}"
                  :message "{{t('playlist.deleteMessage', { name: computed.list.name ?? '' })}}"
                  :primaryLast true
                  :buttons [{:title "{{t('dialog.cancel')}}"}
                            {:title "{{t('playlist.delete')}}"
                             :default true
                             :run deleteList
                             :with {:list "{{state.current}}"}}]})
         (node#quickPick :when "{{state.quickPick.open}}"
           :use quickPick
           :with {:color @menu
                  :textColor @text
                  :dimColor @muted
                  :accent @accent
                  :highlight @accentSoft
                  :field @field
                  :providers [{:items "{{computed.picks}}" :run playFromPick :placeholder "{{t('pick.songs')}}"}
                              {:prefix ">" :items "{{computed.palette}}" :placeholder "{{t('pick.commands')}}"}]}))}
