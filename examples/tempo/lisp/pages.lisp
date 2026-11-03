;;; pages.lisp --- the pages of songs: albums and artists as tiles, one album's or artist's songs, the liked songs,
;;; and the search

;;; What the pages show

(defcomputed contentWidth "How wide the page is: the window but the sidebar, and the queue while it is open."
  (if state.queueOpen (clamp state.sizes.0 480 (- window.width 492)) (- window.width 248)))

(defcomputed browseItems "The albums or the artists, as tiles: a name, a kind, a count of songs and a colour."
  (if (= state.page "artists")
    (map-indexed (fn [index it] {:name it :kind "artist" :count 250 :hue index}) state.words.artists)
    (map-indexed (fn [index it] {:name it :kind "album" :count (* (+ 33 (if (< index 6) 1 0)) 5) :hue index}) state.words.albums)))

(defcomputed browseColumns "How many tiles fit across the page, from 2 to 8." (clamp (floor (/ (- computed.contentWidth 48) 176)) 2 8))

(defcomputed collectionRows "The songs of the album or the artist shown."
  (if (not= state.page "collection")
    []
    (filter (fn [it] (if (= state.collection.kind "album") (= it.cells.3 state.collection.name) (= it.cells.2 state.collection.name)))
            computed.library)))

(defcomputed collectionHue "The colour of the album or the artist shown, as its place among them."
  (if (= state.collection.kind "album")
    (indexOf state.words.albums state.collection.name)
    (indexOf state.words.artists state.collection.name)))

(defcomputed likedSongs "The songs liked that are still there, as {id, list, track}."
  (filter (fn [it] (if (< it.list 0) (< it.track (count computed.library)) (< it.track (or-else (get computed.listSizes it.list) 0))))
          (map (fn [it] {:id it :list (number (get (split it ":") 0)) :track (number (get (split it ":") 1))}) state.liked)))

(defcomputed likedRows "The liked songs as rows of the table: title, artist, album and time."
  (map (fn [it]
         (merge it
                {:cells (if (< it.list 0)
                          (slice (get-in computed.library [it.track :cells]) 1)
                          [(get-in data.playlists [it.list :tracks it.track :title])
                           (get-in data.playlists [it.list :tracks it.track :artist])
                           (get-in data.playlists [it.list :tracks it.track :album])
                           (get-in data.playlists [it.list :tracks it.track :time])])}))
       computed.likedSongs))

(defcomputed searchRows "The library's songs whose title and artist the search matches, loosely."
  (if (or (not= state.page "search") (= (trim state.search) ""))
    []
    (filter (fn [it] (some? (fuzzy (trim state.search) (str it.cells.1 " " it.cells.2)))) computed.library)))

;;; Tables of songs

(defview song-table-columns "The columns of a table of songs: number, title, artist, album and time." []
  [`{:id number :title ~(t "column.number") :width 60}
   `{:id title :title ~(t "column.title") :width 230}
   `{:id artist :title ~(t "column.artist") :width 150}
   `{:id album :title ~(t "column.album") :width 170}
   `{:id time :title ~(t "column.time") :width 64}])

;;; Albums and artists

(defcomponent browsePage "Albums or artists (state.page) as tiles in a grid; a tile opens its songs" {}
  `(node :dir auto :color transparent
     (node#browseTitle :style pageTitle :text ~(t (str "menu." state.page)))
     (node#browseSummary :style caption
       :anchors {:left 32 :top 70}
       :text ~(t (if (= state.page "artists") "browse.artists" "browse.albums") {:n (count computed.browseItems)}))
     (grid#browseGrid :anchors {:left 24 :right 24 :top 100 :bottom 0}
       :columns ~computed.browseColumns
       :gap 8
       :scroll vertical
       :role list
       :label ~(t (str "menu." state.page))
       ~(repeat [item computed.browseItems :key item.name] (tile item)))))

(defview tile "An album or an artist: its initial on its colour (round for an artist), its name and its count." [item]
  `(node#tile :height 204
     :radius 16
     :color transparent
     :hover {:color @rowHover}
     :focused {:color @rowHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "browse.label" {:name item.name :songs (t "songs" {:n item.count})})
     :onClick ~(handler openCollection item.kind item.name)
     (label#tileArt :text ~(slice item.name 0 1)
       :font large
       :align center
       :textColor "#141826"
       :anchors {:left 8 :right 8 :top 8}
       :height 140
       :radius ~(if (= item.kind "artist") 40 14)
       :color ~(get state.tileColors (mod item.hue 8)))
     (label#tileName :text ~item.name :textColor @text :align center :anchors {:left 8 :right 8 :top 156} :height 20)
     (node#tileCount :style caption
       :text ~(t "songs" {:n item.count})
       :align center
       :fitText false
       :anchors {:left 8 :right 8 :top 180}
       :height 16)))

;;; One album or artist

(defcomponent collectionPage "One album or artist's songs in a table; Enter or a double click plays one" {}
  `(node :dir auto :color transparent
     ~(collection-header)
     ~(collection-table)))

(defview collection-header "The album's or artist's way back, initial, kind, name and count, and Play." []
  `(node#collectionHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
     (node#collectionBack :style control
       :icon arrow-left
       :mirror true
       :anchors {:left 12 :top 12}
       :width 32
       :color @surface
       :focusable true
       :role button
       :label ~(t "collection.back")
       :tooltip ~(t "collection.back")
       :onClick browseBack)
     (label#collectionArt :text ~(slice state.collection.name 0 1)
       :font large
       :align center
       :textColor "#141826"
       :anchors {:left 56 :top 24}
       :width 84
       :height 84
       :radius ~(if (= state.collection.kind "artist") 24 12)
       :color ~(get state.tileColors (mod (max computed.collectionHue 0) 8)))
     (node#collectionKind :style caption
       :text ~(t (if (= state.collection.kind "artist") "collection.artist" "collection.album"))
       :anchors {:left 164 :top 28})
     (label#collectionName
       :text ~state.collection.name
       :font large
       :textColor @text
       :anchors {:left 162 :top 44}
       :fitText true)
     (node#collectionCount
       :style caption
       :text ~(t "songs" {:n (count computed.collectionRows)})
       :anchors {:left 164 :top 88})
     (node#playCollectionButton :style playButton
       :text ~(t "play")
       :anchors {:right 32 :top 50}
       :label ~(t "play.label" {:name state.collection.name})
       :onClick playCollection)))

(defview collection-table "The album's or artist's songs; Escape goes back to the tiles." []
  `(table#collectionTable :anchors {:left 24 :right 24 :top 148 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.collectionRows
     :columns ~(song-table-columns)
     :bind state.collectionPick
     :label ~state.collection.name
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromCollection
     :onContextMenu ~(handler openPickMenu state.collectionPick)
     :keys {:escape browseBack}))

;;; The liked songs

(defcomponent likedPage "The songs liked (the heart in the player, or a song's context menu), from the library and the mixes, in a table" {}
  `(node :dir auto :color transparent
     ~(liked-header)
     (node#likedEmpty :style caption
       :when ~(= (count computed.likedRows) 0)
       :text ~(t "liked.empty")
       :anchors {:left 32 :right 32 :top 164}
       :wrap true)
     ~(liked-table)))

(defview liked-header "The liked songs' heart, name and count, and Play." []
  `(node#likedHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
     (label#likedArt :icon heart
       :text ""
       :font iconsLarge
       :align center
       :textColor @onAccent
       :anchors {:left 32 :top 24}
       :width 84
       :height 84
       :radius 6
       :color @accent)
     (node#likedCaption :style caption :text ~(t "liked.caption") :anchors {:left 140 :top 28})
     (label#likedName :text ~(t "liked.title") :font large :textColor @text :anchors {:left 138 :top 44} :fitText true)
     (node#likedCount :style caption :text ~(t "songs" {:n (count computed.likedRows)}) :anchors {:left 140 :top 88})
     (node#playLikedButton :style playButton
       :text ~(t "play")
       :anchors {:right 32 :top 50}
       :when ~(> (count computed.likedRows) 0)
       :label ~(t "play.label" {:name (t "liked.title")})
       :onClick ~(handler playLiked computed.likedRows.0.id))))

(defview liked-table "The liked songs, title, artist, album and time; a row opened plays, a right click opens its menu." []
  `(table#likedTable :when ~(> (count computed.likedRows) 0)
     :anchors {:left 24 :right 24 :top 148 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.likedRows
     :columns [{:id title :title ~(t "column.title") :width 260}
               {:id artist :title ~(t "column.artist") :width 170}
               {:id album :title ~(t "column.album") :width 170}
               {:id time :title ~(t "column.time") :width 64}]
     :bind state.likedPick
     :label ~(t "liked.title")
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen ~(handler playLiked (or-else event.id event.value))
     :onContextMenu likedMenu))

;;; The search

(defcomponent searchPage "A search field over the library: fuzzy matches on the title and the artist, in a table" {}
  `(node :dir auto :color transparent
     (node#searchTitle :style pageTitle :text ~(t "menu.search"))
     (textarea#searchField :anchors {:left 32 :right 32 :top 72}
       :height 36
       :padding 10
       :radius 8
       :color @field
       :borderWidth 1
       :borderColor @border
       :focused {:borderColor @accent}
       :textColor @text
       :placeholder ~(t "search.placeholder")
       :placeholderColor @muted
       :label ~(t "search.label")
       :autofocus true
       :bind state.search
       :keys {:escape {:set state.search :to ""} :down {:focus "#searchTable"}})
     (node#searchCount :style caption
       :live polite
       :anchors {:left 32 :top 118}
       :text ~(if (= (trim state.search) "") (t "search.hint") (t "search.found" {:n (count computed.searchRows)})))
     ~(search-table)))

(defview search-table "The songs found; a row opened plays it, and the songs found after it come next." []
  `(table#searchTable :anchors {:left 24 :right 24 :top 144 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.searchRows
     :columns ~(song-table-columns)
     :bind state.searchPick
     :label ~(t "search.label")
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromSearch
     :onContextMenu ~(handler openPickMenu state.searchPick)))
