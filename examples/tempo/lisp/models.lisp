{:models {:songs {:version 1
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
                              :tracks [{:title "Vertex Buffers" :artist "The Shaders" :album "Core Profile" :time "3:42"}
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
                                       {:title "Flow State" :artist "Quiet Rooms" :album "Hours" :time "9:45"}]}]}}}
