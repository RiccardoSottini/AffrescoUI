;;; library.lisp --- TODO

(defview main-view "#main." []
  `(split#main :when ~(not state.mini)
     :anchors {:left 248 :right 0 :top 0 :bottom 112}
     :bind state.sizes
     :defaultSizes [744]
     :minSize [480 240]
     :sashSize 4
     :sashColor @border
     :sashActiveColor @accent
     (node#content :color transparent :role main
       ~(playlist-view-view)
       (node#libraryView :when ~(= state.page "library") :dir auto :fill true :color transparent
         ~(library-header)
         ~(library-table))
       (node#browseView :when ~(or (= state.page "albums") (= state.page "artists")) :use browsePage :fill true)
       (node#collectionView :when ~(= state.page "collection") :use collectionPage :fill true)
       (node#searchView :when ~(= state.page "search") :use searchPage :fill true)
       (node#nowPlayingView :when ~(= state.page "nowPlaying") :use nowPlayingPage :fill true)
       (node#likedView :when ~(= state.page "liked") :use likedPage :fill true)
       (node#soundView :when ~(= state.page "sound") :use soundPage :fill true))
     ~(queue-pane)))

(defview header "#header." []
  `(node#header :anchors {:left 0 :right 0 :top 0}
     :height 264
     :color @surface
     :gradient {:kind linear :angle 90 :stops [@accentSoft @surface]}
     (image#cover :file ~computed.list.cover
       :fit cover
       :radius 24
       :anchors {:right 40 :top 48}
       :width 168
       :height 168
       :role img
       :label ~computed.list.name)
     {:id label-playlist :style caption :text ~(t "playlist.caption") :anchors {:left 40 :top 62}}
     (label#listName :when ~(not state.renaming)
       :text ~computed.list.name
       :font title
       :textColor @text
       :anchors {:left 36 :top 80}
       :fitText true
       :tooltip ~(t "playlist.rename")
       :onDoubleClick startRename)
     ~(name-field)
     (label#listDescription :text
                            ~computed.list.description
                            :textColor
                            @muted
                            :anchors
                            {:left 40 :top 140}
                            :fitText
                            true)
     {:id label-plural-list-tracks-2
      :style caption
      :text ~(str (t "songs" {:n (count computed.list.tracks)}) "  ·  " (t "playlist.minutes" {:n computed.listMinutes}))
      :textColor @text
      :anchors {:left 40 :top 166}}
     {:id playButton
      :style playButton
      :text ~(t "play")
      :label ~(t "play.label" {:name computed.list.name})
      :anchors {:left 40 :top 196}
      :disabled ~(= (count computed.list.tracks) 0)
      :onClick {:run play :with {:list ~state.current :track 0}}}
     {:id listMore
      :style control
      :icon ellipsis
      :width 36
      :height 36
      :radius 18
      :color @button
      :anchors {:left 164 :top 196}
      :label ~(t "playlist.more" {:name computed.list.name})
      :tooltip ~(t "playlist.more" {:name computed.list.name})
      :onClick openListMenu}))

(defview track-view "#track." [track t]
  `(node#track :let {:playing ~(and (= state.current state.playingList) (= t state.playingTrack))}
     :anchors {:left 0 :right 0}
     :height 40
     :radius 6
     :color ~(if playing "@playingRow" "transparent")
     :hover {:color ~(if playing "@playingRow" "@rowHover")}
     :focused {:color @rowHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(str track.title ", " track.artist ", " track.time)
     :onClick {:run play :with {:list ~state.current :track ~t}}
     :onContextMenu {:run openSongMenu :with {:list ~state.current :track ~t}}
     (label#label-t-1 :icon ~(if (and playing state.isPlaying) "audio-lines" "")
       :text ~(if (and playing state.isPlaying) "" (+ t 1))
       :textColor ~(if playing "@accent" "@muted")
       :anchors {:left 12 :top 0 :bottom 0}
       :width 30
       :align {:horizontal start :vertical center})
     (label#label-track-title :text ~track.title
       :textColor ~(if playing "@accent" "@text")
       :anchors {:left 52 :top 3}
       :fitText true)
     {:id label-track-artist :style caption :text ~track.artist :anchors {:left 52 :top 22}}
     (label#label-track-album :text ~track.album :textColor @muted :x "55%" :y 11 :fitText true)
     (label#label-track-time :text ~track.time :textColor @muted :anchors {:right 16 :top 11} :fitText true)))

(defview playlist-view-view "#playlistView." []
  `(node#playlistView :when ~(= state.page "playlist") :dir auto :fill true :color transparent
     ~(header)
     (label#playlistEmpty :when ~(= (count computed.list.tracks) 0)
       :text ~(t "playlist.empty")
       :textColor @muted
       :anchors {:left 52 :top 340}
       :fitText true)
     (node#columns :anchors {:left 40 :right 40 :top 284} :height 28 :color transparent
       {:id label-caption :style caption :text ~(t "column.number") :anchors {:left 12 :top 6}}
       {:id label-title :style caption :text ~(upper-case (t "column.title")) :anchors {:left 52 :top 6}}
       {:id label-album :style caption :text ~(upper-case (t "column.album")) :x "55%" :y 6}
       {:id label-time :style caption :text ~(upper-case (t "column.time")) :anchors {:right 16 :top 6}}
       {:id panel-columns :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @border})
     (stack#tracks :anchors {:left 40 :right 40 :top 320 :bottom 8}
       :gap 2
       :scroll vertical
       :role list
       :label ~(t "playlist.tracks" {:name computed.list.name})
       ~(repeat [track computed.list.tracks :index t]
         (track-view track t)))))

(defview library-header "#libraryHeader." []
  `(node#libraryHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
     (image#libraryCover :file
                         ~state.libraryInfo.cover
                         :fit
                         cover
                         :radius
                         6
                         :anchors
                         {:left 32 :top 24}
                         :width
                         84
                         :height
                         84)
     {:id label-library :style caption :text ~(t "library.caption") :anchors {:left 140 :top 28}}
     (label#libraryName :text
                        ~(t "library.all")
                        :font
                        large
                        :textColor
                        @text
                        :anchors
                        {:left 138 :top 44}
                        :fitText
                        true)
     {:id libraryCount
      :style caption
      :text ~(t "library.count" {:n (count computed.library)})
      :anchors {:left 140 :top 88}}
     {:id playLibraryButton
      :style playButton
      :text ~(t "play")
      :label ~(t "play.label" {:name (t "library.all")})
      :anchors {:right 32 :top 50}
      :onClick playLibrary}))

(defview library-table "#libraryTable." []
  `(table#libraryTable :anchors {:left 24 :right 24 :top 148 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.library
     :columns [{:id number :title ~(t "column.number") :width 60}
               {:id title :title ~(t "column.title") :width 230}
               {:id artist :title ~(t "column.artist") :width 150}
               {:id album :title ~(t "column.album") :width 170}
               {:id time :title ~(t "column.time") :width 64}]
     :sort ~state.librarySort
     :bind state.libraryPick
     :label ~(t "library.label")
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromLibrary
     :onContextMenu {:run openPickMenu :with {:pick ~state.libraryPick}}
     :onSort {:set state.librarySort :to {:column ~event.column :descending ~event.descending}}))

