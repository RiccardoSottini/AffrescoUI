;;; state.lisp --- what Tempo keeps while it runs, and where each value starts
;;;
;;; Each defstate is one key of the state, with the value it starts from and, for some, its type.

;;; Playing

(defstate isPlaying true "Whether the song plays, or is paused." :type (boolean))

(defstate playingList 0 "The list playing: a playlist's place, or -1 for the library." :type (number))

(defstate playingTrack 2 "The song playing, by its place in its list." :type (number))

(defstate elapsed 84 "How far the song is, in seconds." :type (number))

(defstate volume 70 "The volume, from 0 to 100." :type (number :minimum 0 :maximum 100))

(defstate muted false "Whether the sound is muted." :type (boolean))

(defstate shuffle false "Whether the queue plays shuffled." :type (boolean))

(defstate shuffleSeed 0 "The shuffle's order." :type (number))

(defstate repeat off "off, all, or one (the song playing)." :type (string :enum [off all one]))

(defstate order ["0:2" "0:3" "0:4" "0:5" "0:6" "0:7" "0:8"]
  "The songs as they were queued, for shuffle to put back." :type (array :maxItems 1000 :items (string :maxLength 16)))

(defstate source "" "The album or artist playing, or nothing." :type (string))

(defstate beat
  {:status stopped :playing false}
  "The beat's sound: its status and spectrum." :type (object :properties {:status (string) :playing (boolean) :spectrum (array :items (number))}))

(defstate sounds {} "The songs' own sounds, by name." :type (object))

(defstate soundOn "" "The sound playing: beat, a sound of its own, or none." :type (string))

;;; The queue

(defstate queueOpen true "Whether the queue is shown." :type (boolean))

(defstate queue ["0:3" "0:4" "0:5" "0:6" "0:7" "0:8"] "The songs up next, as list:track." :type (array :maxItems 1000 :items (string :maxLength 16)))

(defstate queuePick nil "The song picked in the queue." :type (node :type [string null]))

(defstate queueNote "" "What the queue says it did, for screen readers." :type (string))

;;; Pages and going back

(defstate theme dark "dark or light." :type (string :enum [dark light]))

(defstate page
  playlist
  "The page shown." :type (string :enum [playlist library albums artists liked collection search nowPlaying sound]))

(defstate current 0 "The playlist shown, by its place." :type (number))

(defstate menu
  [{:id library :icon music}
   {:id albums :icon disc-3}
   {:id artists :icon users}
   {:id liked :icon heart}
   {:id search :icon search}
   {:id nowPlaying :icon radio}
   {:id sound :icon sliders-horizontal}]
  "The sidebar's pages." :type (array :items (object :properties {:id (string) :icon (string)})))

(defstate where {:page playlist :current 0 :collection nil} "The place shown, for going back and forward.")

(defstate backStack [] "The places Back goes to, the last 50." :type (array :maxItems 50))

(defstate forwardStack [] "The places Forward goes to." :type (array :maxItems 50))

(defstate navigating false "Whether a place changes by going back or forward." :type (boolean))

(defstate mini false "Whether the mini player is shown." :type (boolean))

(defstate sizes [744] "The main split's sizes." :type (array :items (number)))

(defstate quickPick {:open false :query ""} "The quick pick: open, and what is typed." :type (object :properties {:open (boolean) :query (string)}))

;;; The library and the playlists

(defstate words
  {:adjectives ["Silver"
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
  "The word lists the library's songs are made from." :type (object))

(defstate libraryInfo
  {:name "All songs" :cover "../assets/images/cover-library.svg"}
  "The library's name and cover." :type (object :properties {:name (string) :cover (string)}))

(defstate librarySort nil "The library table's sort." :type (node :type [object null]))

(defstate libraryPick nil "The library table's selected row." :type (node :type [string null]))

(defstate liked [] "The songs liked, as list:track." :type (array :maxItems 5000 :items (string :maxLength 16)))

(defstate likedPick nil "The liked songs table's selected row." :type (node :type [string null]))

(defstate songMenu
  {:open false :x 0 :y 0 :list -1 :track 0}
  "The song menu: open, its song, and where." :type (object :properties {:open (boolean) :x (number) :y (number) :list (integer) :track (integer :minimum 0)}))

(defstate listMenu {:open false :x 0 :y 0} "The playlist's menu: open, and where." :type (object :properties {:open (boolean) :x (number) :y (number)}))

(defstate renaming false "Whether the playlist's name is being typed." :type (boolean))

(defstate nameDraft "" "The name typed." :type (string :maxLength 200))

(defstate deleting false "Whether the dialog asking to delete the playlist is shown." :type (boolean))

;;; Albums, artists and the search

(defstate collection {:kind album :name ""} "The album or artist shown: its kind and name.")

(defstate collectionPick nil "The album's or artist's selected row.")

(defstate search "" "What the search is typed.")

(defstate searchPick nil "The search's selected row.")

(defstate tileColors ["#f2b544" "#7dd3fc" "#c4b5fd" "#fda4af" "#5eead4" "#fdba74" "#a5b4fc" "#f0abfc"]
  "The tiles' colours.")

;;; The sound page and now playing

(defstate eq {:points [[0 0.5] [0.25 0.5] [0.5 0.5] [0.75 0.5] [1 0.5]]} "The equaliser's curve.")

(defstate eqOn true "Whether the equaliser is on.")

(defstate bands
  [{:label "60 Hz" :at 0}
   {:label "250 Hz" :at 0.25}
   {:label "1 kHz" :at 0.5}
   {:label "4 kHz" :at 0.75}
   {:label "16 kHz" :at 1}]
  "The equaliser's bands: where each is on the curve, and its name.")

(defstate presets
  {:flat [[0 0.5] [0.25 0.5] [0.5 0.5] [0.75 0.5] [1 0.5]]
   :bass [[0 0.8] [0.25 0.68] [0.5 0.5] [0.75 0.48] [1 0.46]]
   :vocal [[0 0.4] [0.25 0.48] [0.5 0.7] [0.75 0.62] [1 0.45]]
   :treble [[0 0.46] [0.25 0.48] [0.5 0.52] [0.75 0.68] [1 0.8]]}
  "The equaliser's presets, by name.")

(defstate lyrics
  ["Count the lights along the river"
   "Every window keeps a song"
   "We were drawing in the margins"
   "Of a page that ran too long"
   "Hold the line until the morning"
   "Let the echoes find their way"
   "All the signals in the static"
   "Turn to colour, turn to grey"
   "Keep the rhythm, keep it steady"
   "Close your eyes and let it play"]
  "The lyrics' ten lines.")
