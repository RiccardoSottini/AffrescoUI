;;; state.lisp --- TODO

(defstate theme dark :type (string :enum [dark light]))

(defstate page
  playlist :type (string :enum [playlist library albums artists liked collection search nowPlaying sound]))

(defstate current 0 :type (number))

(defstate playingList 0 :type (number))

(defstate playingTrack 2 :type (number))

(defstate isPlaying true :type (boolean))

(defstate beat
  {:status stopped :playing false} :type (object :properties {:status (string) :playing (boolean) :spectrum (array :items (number))}))

(defstate sounds {} :type (object))

(defstate soundOn "" :type (string))

(defstate elapsed 84 :type (number))

(defstate volume 70 :type (number :minimum 0 :maximum 100))

(defstate muted false :type (boolean))

(defstate shuffle false :type (boolean))

(defstate shuffleSeed 0 :type (number))

(defstate repeat off :type (string :enum [off all one]))

(defstate order ["0:2" "0:3" "0:4" "0:5" "0:6" "0:7" "0:8"] :type (array :maxItems 1000 :items (string :maxLength 16)))

(defstate where {:page playlist :current 0 :collection nil})

(defstate backStack [] :type (array :maxItems 50))

(defstate forwardStack [] :type (array :maxItems 50))

(defstate navigating false :type (boolean))

(defstate queueOpen true :type (boolean))

(defstate queue ["0:3" "0:4" "0:5" "0:6" "0:7" "0:8"] :type (array :maxItems 1000 :items (string :maxLength 16)))

(defstate queuePick nil :type (node :type [string null]))

(defstate queueNote "" :type (string))

(defstate source "" :type (string))

(defstate mini false :type (boolean))

(defstate sizes [744] :type (array :items (number)))

(defstate menu
  [{:id library :icon music}
   {:id albums :icon disc-3}
   {:id artists :icon users}
   {:id liked :icon heart}
   {:id search :icon search}
   {:id nowPlaying :icon radio}
   {:id sound :icon sliders-horizontal}] :type (array :items (object :properties {:id (string) :icon (string)})))

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
   :versions ["Live" "Remix" "Demo" "Edit" "Acoustic" "Reprise" "Mono" "Dub" "Take 2"]} :type (object))

(defstate libraryInfo
  {:name "All songs" :cover "../assets/images/cover-library.svg"} :type (object :properties {:name (string) :cover (string)}))

(defstate librarySort nil :type (node :type [object null]))

(defstate libraryPick nil :type (node :type [string null]))

(defstate liked [] :type (array :maxItems 5000 :items (string :maxLength 16)))

(defstate likedPick nil :type (node :type [string null]))

(defstate songMenu
  {:open false :x 0 :y 0 :list -1 :track 0} :type (object :properties {:open (boolean) :x (number) :y (number) :list (integer) :track (integer :minimum 0)}))

(defstate listMenu {:open false :x 0 :y 0} :type (object :properties {:open (boolean) :x (number) :y (number)}))

(defstate renaming false :type (boolean))

(defstate nameDraft "" :type (string :maxLength 200))

(defstate deleting false :type (boolean))

(defstate quickPick {:open false :query ""} :type (object :properties {:open (boolean) :query (string)}))

(defstate collection {:kind album :name ""})

(defstate collectionPick nil)

(defstate search "")

(defstate searchPick nil)

(defstate eq {:points [[0 0.5] [0.25 0.5] [0.5 0.5] [0.75 0.5] [1 0.5]]})

(defstate eqOn true)

(defstate tileColors ["#f2b544" "#7dd3fc" "#c4b5fd" "#fda4af" "#5eead4" "#fdba74" "#a5b4fc" "#f0abfc"])

(defstate bands
  [{:label "60 Hz" :at 0}
   {:label "250 Hz" :at 0.25}
   {:label "1 kHz" :at 0.5}
   {:label "4 kHz" :at 0.75}
   {:label "16 kHz" :at 1}])

(defstate presets
  {:flat [[0 0.5] [0.25 0.5] [0.5 0.5] [0.75 0.5] [1 0.5]]
   :bass [[0 0.8] [0.25 0.68] [0.5 0.5] [0.75 0.48] [1 0.46]]
   :vocal [[0 0.4] [0.25 0.48] [0.5 0.7] [0.75 0.62] [1 0.45]]
   :treble [[0 0.46] [0.25 0.48] [0.5 0.52] [0.75 0.68] [1 0.8]]})

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
   "Close your eyes and let it play"])

