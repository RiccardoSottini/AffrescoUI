;;; library.lisp --- the main area: the page shown beside the queue; the playlist page (its header and its songs) and
;;; the library (its header and the table of its 3,000 songs)

;;; The main area

(defview main-area "The page shown (state.page), and the queue beside it; hidden in the mini player." []
  `(split#main :when ~(not state.mini)
     :anchors {:left 248 :right 0 :top 0 :bottom 112}
     :bind state.sizes
     :defaultSizes [744]
     :minSize [480 240]
     :sashSize 4
     :sashColor @border
     :sashActiveColor @accent
     (node#content :color transparent :role main
       ~(playlist-page)
       ~(library-page)
       (node#browseView :when ~(or (= state.page "albums") (= state.page "artists")) :use browsePage :fill true)
       (node#collectionView :when ~(= state.page "collection") :use collectionPage :fill true)
       (node#searchView :when ~(= state.page "search") :use searchPage :fill true)
       (node#nowPlayingView :when ~(= state.page "nowPlaying") :use nowPlayingPage :fill true)
       (node#likedView :when ~(= state.page "liked") :use likedPage :fill true)
       (node#soundView :when ~(= state.page "sound") :use soundPage :fill true))
     ~(queue-pane)))

;;; A playlist

(defview playlist-page "A playlist: its header, the columns' names, and its songs." []
  `(node#playlistView :when ~(= state.page "playlist") :dir auto :fill true :color transparent
     ~(playlist-header)
     (label#playlistEmpty :when ~(= (count computed.list.tracks) 0)
       :text ~(t "playlist.empty")
       :textColor @muted
       :anchors {:left 52 :top 340}
       :fitText true)
     ~(song-columns)
     (stack#tracks :anchors {:left 40 :right 40 :top 320 :bottom 8}
       :gap 2
       :scroll vertical
       :role list
       :label ~(t "playlist.tracks" {:name computed.list.name})
       ~(repeat [track computed.list.tracks :index place] (song-row track place)))))

(defview playlist-header "The playlist's cover, name (double-click renames it), description and size, Play and its menu." []
  `(node#header :anchors {:left 0 :right 0 :top 0}
     :height 264
     :color @surface
     :gradient {:kind linear :angle 90 :stops [@accentSoft @surface]}
     ~(playlist-cover)
     (node#label-playlist :style caption :text ~(t "playlist.caption") :anchors {:left 40 :top 62})
     ~(playlist-name)
     ~(name-field)
     (label#listDescription
       :text ~computed.list.description
       :textColor @muted
       :anchors {:left 40 :top 140}
       :fitText true)
     (node#label-plural-list-tracks-2 :style caption
       :text ~(str (t "songs" {:n (count computed.list.tracks)}) "  ·  " (t "playlist.minutes" {:n computed.listMinutes}))
       :textColor @text
       :anchors {:left 40 :top 166})
     (node#playButton :style playButton
       :text ~(t "play")
       :label ~(t "play.label" {:name computed.list.name})
       :anchors {:left 40 :top 196}
       :disabled ~(= (count computed.list.tracks) 0)
       :onClick ~(handler play state.current 0))
     ~(list-more-button)))

(defview playlist-cover "The playlist's cover, on the right of the header." []
  `(image#cover
     :file ~computed.list.cover
     :fit cover
     :radius 24
     :anchors {:right 40 :top 48}
     :width 168
     :height 168
     :role img
     :label ~computed.list.name))

(defview playlist-name "The playlist's name; a double-click renames it." []
  `(label#listName :when ~(not state.renaming)
     :text ~computed.list.name
     :font title
     :textColor @text
     :anchors {:left 36 :top 80}
     :fitText true
     :tooltip ~(t "playlist.rename")
     :onDoubleClick startRename))

(defview list-more-button "The playlist's menu: more that can be done with it." []
  `(node#listMore :style control
     :icon ellipsis
     :width 36
     :height 36
     :radius 18
     :color @button
     :anchors {:left 164 :top 196}
     :label ~(t "playlist.more" {:name computed.list.name})
     :tooltip ~(t "playlist.more" {:name computed.list.name})
     :onClick openListMenu))

(defview name-field "The field that renames the playlist, while it is being renamed: Enter keeps the name, Escape leaves it." []
  `(textarea#nameField :when ~state.renaming
     :bind state.nameDraft
     :placeholder ~computed.list.name
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
     :label ~(t "playlist.nameLabel")
     :onSubmit renameList
     :keys {:escape cancelRename}))

(defview song-columns "The names of the songs' columns, over a line." []
  `(node#columns :anchors {:left 40 :right 40 :top 284} :height 28 :color transparent
     (node#label-caption :style caption :text ~(t "column.number") :anchors {:left 12 :top 6})
     (node#label-title :style caption :text ~(upper-case (t "column.title")) :anchors {:left 52 :top 6})
     (node#label-album :style caption :text ~(upper-case (t "column.album")) :x "55%" :y 6)
     (node#label-time :style caption :text ~(upper-case (t "column.time")) :anchors {:right 16 :top 6})
     (node#panel-columns :anchors {:left 0 :right 0 :bottom 0} :height 1 :color @border)))

(defview song-row "A song of the playlist: its number (bars while it plays), title, artist, album and time; a click plays it."
  [track place]
  `(node#track :let {:playing ~(and (= state.current state.playingList) (= place state.playingTrack))}
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
     :onClick ~(handler play state.current place)
     :onContextMenu ~(handler openSongMenu state.current place)
     (label#label-t-1 :icon ~(if (and playing state.isPlaying) "audio-lines" "")
       :text ~(if (and playing state.isPlaying) "" (+ place 1))
       :textColor ~(if playing "@accent" "@muted")
       :anchors {:left 12 :top 0 :bottom 0}
       :width 30
       :align {:horizontal start :vertical center})
     (label#label-track-title
       :text ~track.title
       :textColor ~(if playing "@accent" "@text")
       :anchors {:left 52 :top 3}
       :fitText true)
     (node#label-track-artist :style caption :text ~track.artist :anchors {:left 52 :top 22})
     (label#label-track-album :text ~track.album :textColor @muted :x "55%" :y 11 :fitText true)
     (label#label-track-time :text ~track.time :textColor @muted :anchors {:right 16 :top 11} :fitText true)))

;;; The library

(defview library-page "The library: its header, and every song in a table." []
  `(node#libraryView :when ~(= state.page "library") :dir auto :fill true :color transparent
     ~(library-header)
     ~(library-table)))

(defview library-header "The library's cover, name and size, and Play." []
  `(node#libraryHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
     (image#libraryCover
       :file ~state.libraryInfo.cover
       :fit cover
       :radius 6
       :anchors {:left 32 :top 24}
       :width 84
       :height 84)
     (node#label-library :style caption :text ~(t "library.caption") :anchors {:left 140 :top 28})
     (label#libraryName
       :text ~(t "library.all")
       :font large
       :textColor @text
       :anchors {:left 138 :top 44}
       :fitText true)
     (node#libraryCount
       :style caption
       :text ~(t "library.count" {:n (count computed.library)})
       :anchors {:left 140 :top 88})
     (node#playLibraryButton :style playButton
       :text ~(t "play")
       :label ~(t "play.label" {:name (t "library.all")})
       :anchors {:right 32 :top 50}
       :onClick playLibrary)))

(defview library-table "The table of the library's songs, sorted by a column; a row opened plays, a right click opens its menu." []
  `(table#libraryTable :anchors {:left 24 :right 24 :top 148 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.library
     :columns ~(song-table-columns)
     :sort ~state.librarySort
     :bind state.libraryPick
     :label ~(t "library.label")
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromLibrary
     :onContextMenu ~(handler openPickMenu state.libraryPick)
     :onSort sortLibrary))
