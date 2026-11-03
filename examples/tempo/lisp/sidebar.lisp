;;; sidebar.lisp --- the sidebar: the logo, back and forward, the theme and the language, the pages, and the playlists

;;; The sidebar

(defview sidebar "The sidebar, hidden in the mini player." []
  `(node#sidebar :when ~(not state.mini)
     :dir auto
     :anchors {:left 0 :top 0 :bottom 112}
     :width 248
     :color @sidebar
     :role navigation
     :label ~(t "menu.label")
     (label#logo :text "Tempo" :font brand :textColor @text :anchors {:left 28 :top 28} :fitText true)
     (node#label-an-affresco-ui :style caption :text ~(t "app.tagline") :anchors {:left 28 :top 62})
     ~@(history-buttons)
     ~@(look-buttons)
     (stack#menu :anchors {:left 12 :right 12 :top 96} :height 264 :gap 2 :role tablist :arrows vertical :label ~(t "menu.label")
       ~(repeat [item state.menu] (page-tab item)))
     (node#label-playlists :style caption :text ~(t "playlists.caption") :anchors {:left 28 :top 372})
     (node#newPlaylist :style control
       :icon plus
       :width 28
       :height 28
       :radius 10
       :color @sidebar
       :anchors {:right 16 :top 364}
       :label ~(t "playlist.create")
       :tooltip ~(t "playlist.create")
       :onClick ~(handler newPlaylist false))
     (stack#playlists :anchors {:left 12 :right 12 :top 394 :bottom 12} :gap 4 :scroll vertical :role list :label ~(t "playlists.label")
       ~(repeat [playlist data.playlists] (playlist-row playlist)))))

(defview history-buttons "Back and forward through the pages shown: two buttons." []
  [`(node#backButton :style control
      :icon chevron-left
      :mirror true
      :anchors {:left 96 :top 26}
      :width 24
      :radius 8
      :color @sidebar
      :textColor ~(if (> (count state.backStack) 0) "@text" "@faint")
      :disabled ~(= (count state.backStack) 0)
      :label ~(t "nav.back")
      :tooltip ~(t "nav.back")
      :onClick goBack)
   `(node#forwardButton :style control
      :icon chevron-right
      :mirror true
      :anchors {:left 122 :top 26}
      :width 24
      :radius 8
      :color @sidebar
      :textColor ~(if (> (count state.forwardStack) 0) "@text" "@faint")
      :disabled ~(= (count state.forwardStack) 0)
      :label ~(t "nav.forward")
      :tooltip ~(t "nav.forward")
      :onClick goForward)])

(defview look-buttons "The theme's switch, a sun or a moon, and the language's, its code: two buttons." []
  [`(node#themeButton :style control
      :icon ~(if (= state.theme "dark") "sun" "moon")
      :anchors {:right 56 :top 26}
      :width 32
      :radius 8
      :color @sidebar
      :label ~(t "theme.toggle")
      :tooltip ~(t "theme.toggle")
      :onClick toggleTheme)
   `(node#languageButton :style control
      :font small
      :text ~(upper-case settings.ui.language)
      :anchors {:right 16 :top 26}
      :width 36
      :radius 8
      :color @sidebar
      :label ~(t "language.toggle")
      :tooltip ~(t "language.toggle")
      :onClick switchLanguage)])

(defview page-tab "A page of the menu: on while it is shown, or while an album or artist of it is." [item]
  `(label#label-menu :let {:on ~(or (= item.id state.page)
                                    (and (= item.id "albums") (= state.page "collection") (= state.collection.kind "album"))
                                    (and (= item.id "artists") (= state.page "collection") (= state.collection.kind "artist")))}
     :icon ~item.icon
     :text ~(t (str "menu." item.id))
     :anchors {:left 0 :right 0}
     :height 36
     :padding 16
     :radius 18
     :align {:horizontal start :vertical center}
     :color ~(if on "@accentSoft" "@sidebar")
     :hover {:color ~(if on "@accentSoft" "@surface")}
     :focused {:color ~(if on "@accentSoft" "@surface")}
     :textColor ~(if on "@accent" "@muted")
     :cursor hand
     :focusable true
     :role tab
     :selected ~on
     :label ~(t (str "menu." item.id))
     :tooltip ~(t (str "tip." item.id))
     :onClick ~(handler show item.id)))

(defview playlist-row "A playlist: its cover, name and size, a dot while it plays; a click opens it, a double click plays it."
  [list]
  `(node#playlist :let {:open ~(and (= state.page "playlist") (= index state.current))}
     :anchors {:left 0 :right 0}
     :height 56
     :radius 14
     :color ~(if open "@surface" "@sidebar")
     :hover {:color @surface}
     :focused {:color @surface}
     :cursor hand
     :focusable true
     :role button
     :selected ~open
     :label ~(str list.name ", " (t "songs" {:n (count list.tracks)}))
     :onClick ~(handler openList index)
     :onDoubleClick ~(handler play index 0)
     :tooltip ~(t "playlist.play")
     (image#image-list-cover :file ~list.cover :fit cover :radius 10 :anchors {:left 10 :top 8} :width 40 :height 40)
     (label#label-list-name :text ~list.name :textColor @text :anchors {:left 62 :top 10} :fitText true)
     (node#label-plural-list-tracks :style caption :text ~(t "songs" {:n (count list.tracks)}) :anchors {:left 62 :top 31})
     (node#panel-playlist :when ~(and (= index state.playingList) state.isPlaying)
       :anchors {:right 14 :top 25}
       :width 6
       :height 6
       :radius 3
       :color @accent)))
