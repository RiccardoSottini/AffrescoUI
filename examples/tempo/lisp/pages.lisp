;;; pages.lisp --- TODO

(defcomputed contentWidth (if state.queueOpen (clamp state.sizes.0 480 (- window.width 492)) (- window.width 248)))

(defcomputed browseItems
  (if (= state.page "artists")
        (map-indexed (fn [index it] {:name it :kind "artist" :count 250 :hue index}) state.words.artists)
        (map-indexed (fn [index it] {:name it :kind "album" :count (* (+ 33 (if (< index 6) 1 0)) 5) :hue index})
                     state.words.albums)))

(defcomputed browseColumns (clamp (floor (/ (- computed.contentWidth 48) 176)) 2 8))

(defcomputed collectionRows
  (if (not= state.page "collection")
        []
        (filter (fn [it]
                  (if (= state.collection.kind "album")
                      (= it.cells.3 state.collection.name)
                      (= it.cells.2 state.collection.name)))
                computed.library)))

(defcomputed collectionHue
  (if (= state.collection.kind "album")
        (indexOf state.words.albums state.collection.name)
        (indexOf state.words.artists state.collection.name)))

(defcomputed likedSongs
  (filter (fn [it]
              (if (< it.list 0)
                  (< it.track (count computed.library))
                  (< it.track (or-else (get computed.listSizes it.list) 0))))
            (map (fn [it] {:id it :list (number (get (split it ":") 0)) :track (number (get (split it ":") 1))})
                 state.liked)))

(defcomputed likedRows
  (map (fn [it]
           (merge it
                  {:cells (if (< it.list 0)
                              (slice (get-in computed.library [it.track :cells]) 1)
                              [(get-in data.playlists [it.list :tracks it.track :title])
                               (get-in data.playlists [it.list :tracks it.track :artist])
                               (get-in data.playlists [it.list :tracks it.track :album])
                               (get-in data.playlists [it.list :tracks it.track :time])])}))
         computed.likedSongs))

(defcomputed searchRows
  (if (or (not= state.page "search") (= (trim state.search) ""))
        []
        (filter (fn [it] (some? (fuzzy (trim state.search) (str it.cells.1 " " it.cells.2)))) computed.library)))

(defcomponent browsePage "Albums or artists (state.page) as tiles in a grid; a tile opens its songs"
  {}
  `(node :dir auto :color transparent
     {:id browseTitle :style pageTitle :text ~(t (str "menu." state.page))}
     {:id browseSummary
      :style caption
      :anchors {:left 32 :top 70}
      :text ~(t (if (= state.page "artists") "browse.artists" "browse.albums") {:n (count computed.browseItems)})}
     (grid#browseGrid :anchors {:left 24 :right 24 :top 100 :bottom 0}
       :columns ~computed.browseColumns
       :gap 8
       :scroll vertical
       :role list
       :label ~(t (str "menu." state.page))
       ~(repeat [item computed.browseItems :key item.name]
         (tile item)))))

(defcomponent collectionPage "One album or artist's songs in a table; Enter or a double click plays one"
  {}
  `(node :dir auto :color transparent
     ~(collection-header)
     ~(collection-table)))

(defcomponent likedPage "The songs liked (the heart in the player, or a song's context menu), from the library and the mixes, in a table"
  {}
  `(node :dir auto :color transparent
     ~(liked-header)
     {:id likedEmpty
      :style caption
      :when ~(= (count computed.likedRows) 0)
      :text ~(t "liked.empty")
      :anchors {:left 32 :right 32 :top 164}
      :wrap true}
     ~(liked-table)))

(defcomponent searchPage "A search field over the library: fuzzy matches on the title and the artist, in a table"
  {}
  `(node :dir auto :color transparent
     {:id searchTitle :style pageTitle :text ~(t "menu.search")}
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
     {:id searchCount
      :style caption
      :live polite
      :anchors {:left 32 :top 118}
      :text ~(if (= (trim state.search) "") (t "search.hint") (t "search.found" {:n (count computed.searchRows)}))}
     ~(search-table)))

(defview tile "#tile." [item]
  `(node#tile :height 204
     :radius 16
     :color transparent
     :hover {:color @rowHover}
     :focused {:color @rowHover}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "browse.label" {:name item.name :songs (t "songs" {:n item.count})})
     :onClick {:run openCollection :with {:kind ~item.kind :name ~item.name}}
     (label#tileArt :text ~(slice item.name 0 1)
       :font large
       :align center
       :textColor "#141826"
       :anchors {:left 8 :right 8 :top 8}
       :height 140
       :radius ~(if (= item.kind "artist") 40 14)
       :color ~(get state.tileColors (mod item.hue 8)))
     (label#tileName :text ~item.name :textColor @text :align center :anchors {:left 8 :right 8 :top 156} :height 20)
     {:id tileCount
      :style caption
      :text ~(t "songs" {:n item.count})
      :align center
      :fitText false
      :anchors {:left 8 :right 8 :top 180}
      :height 16}))

(defview collection-header "#collectionHeader." []
  `(node#collectionHeader :anchors {:left 0 :right 0 :top 0} :height 132 :color @surface
     {:id collectionBack
      :style control
      :icon arrow-left
      :mirror true
      :anchors {:left 12 :top 12}
      :width 32
      :color @surface
      :focusable true
      :role button
      :label ~(t "collection.back")
      :tooltip ~(t "collection.back")
      :onClick {:set state.page :to ~(if (= state.collection.kind "artist") "artists" "albums")}}
     (label#collectionArt :text ~(slice state.collection.name 0 1)
       :font large
       :align center
       :textColor "#141826"
       :anchors {:left 56 :top 24}
       :width 84
       :height 84
       :radius ~(if (= state.collection.kind "artist") 24 12)
       :color ~(get state.tileColors (mod (max computed.collectionHue 0) 8)))
     {:id collectionKind
      :style caption
      :text ~(t (if (= state.collection.kind "artist") "collection.artist" "collection.album"))
      :anchors {:left 164 :top 28}}
     (label#collectionName :text ~state.collection.name
       :font large
       :textColor @text
       :anchors {:left 162 :top 44}
       :fitText true)
     {:id collectionCount
      :style caption
      :text ~(t "songs" {:n (count computed.collectionRows)})
      :anchors {:left 164 :top 88}}
     {:id playCollectionButton
      :style playButton
      :text ~(t "play")
      :anchors {:right 32 :top 50}
      :label ~(t "play.label" {:name state.collection.name})
      :onClick playCollection}))

(defview collection-table "#collectionTable." []
  `(table#collectionTable :anchors {:left 24 :right 24 :top 148 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.collectionRows
     :columns [{:id number :title ~(t "column.number") :width 60}
               {:id title :title ~(t "column.title") :width 230}
               {:id artist :title ~(t "column.artist") :width 150}
               {:id album :title ~(t "column.album") :width 170}
               {:id time :title ~(t "column.time") :width 64}]
     :bind state.collectionPick
     :label ~state.collection.name
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromCollection
     :onContextMenu {:run openPickMenu :with {:pick ~state.collectionPick}}
     :keys {:escape {:set state.page :to ~(if (= state.collection.kind "artist") "artists" "albums")}}))

(defview liked-header "#likedHeader." []
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
     {:id likedCaption :style caption :text ~(t "liked.caption") :anchors {:left 140 :top 28}}
     (label#likedName :text ~(t "liked.title") :font large :textColor @text :anchors {:left 138 :top 44} :fitText true)
     {:id likedCount :style caption :text ~(t "songs" {:n (count computed.likedRows)}) :anchors {:left 140 :top 88}}
     {:id playLikedButton
      :style playButton
      :text ~(t "play")
      :anchors {:right 32 :top 50}
      :when ~(> (count computed.likedRows) 0)
      :label ~(t "play.label" {:name (t "liked.title")})
      :onClick {:run playLiked :with {:key ~computed.likedRows.0.id}}}))

(defview liked-table "#likedTable." []
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
     :onOpen {:run playLiked :with {:key ~(or-else event.id event.value)}}
     :onContextMenu {:if ~(some? state.likedPick)
                     :then [{:run openSongMenu
                             :with {:list ~(number (get (split state.likedPick ":") 0))
                                    :track ~(number (get (split state.likedPick ":") 1))}}]}))

(defview search-table "#searchTable." []
  `(table#searchTable :anchors {:left 24 :right 24 :top 144 :bottom 8}
     :color transparent
     :rowHeight 30
     :items ~computed.searchRows
     :columns [{:id number :title ~(t "column.number") :width 60}
               {:id title :title ~(t "column.title") :width 230}
               {:id artist :title ~(t "column.artist") :width 150}
               {:id album :title ~(t "column.album") :width 170}
               {:id time :title ~(t "column.time") :width 64}]
     :bind state.searchPick
     :label ~(t "search.label")
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @accentSoft
     :accentColor @accent
     :onOpen playFromSearch
     :onContextMenu {:run openPickMenu :with {:pick ~state.searchPick}}))

