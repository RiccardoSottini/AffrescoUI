;;; index.screen.lisp --- Arcadia: a desktop game store and launcher, with every feature of the reference app in a look of its own
;;;
;;; A menu row and a centred navigation (store, library, community, profile) with back and forward; a library with a
;;; game list on the left, a home of shelves and a page per game; a bottom bar with the downloads and friends. Every
;;; game, studio, friend and picture is invented. The parts: parts/data.json (the data), parts/chrome.json (the bars
;;; around the pages), parts/library.json (the library), parts/store.json, parts/downloads.json, parts/manage.json,
;;; parts/client.json (Settings, codes, backup, couch mode, offline, sign-in) and parts/hub.json (a game's player
;;; hub). Built from the public docs only; see README.md next to it.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "parts/data.lisp"
         "parts/chrome.lisp"
         "parts/library.lisp"
         "parts/store.lisp"
         "parts/downloads.lisp"
         "parts/community.lisp"
         "parts/manage.lisp"
         "parts/client.lisp"
         "parts/hub.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "arcadia.lisp")

;;; Definitions

(defstate nav [{:tab library :page home :id nil}])

(defstate at 0)

(defcomputed rtl (= (slice locale 0 2) "ar"))

(defcomputed here (get state.nav state.at))

(defcomputed owned (filter (fn [it] it.owned) data.games))

(defcomputed game (find (fn [it] (= it.id (or-else computed.here.id state.selected))) data.games))

(deftokens tokens "Tokens."
  {:color.top {:dark "#16121b" :light "#e8e2ec"}
   :color.page {:dark "#211a28" :light "#f6f2f7"}
   :color.side {:dark "#1d1823" :light "#efe9f2"}
   :color.sideTop {:dark "#262030" :light "#f6f2f7"}
   :color.main {:dark "#231c2a" :light "#faf7fb"}
   :color.panel {:dark "#2a2232" :light "#ffffff"}
   :color.raised {:dark "#352c3f" :light "#ece5f0"}
   :color.hover {:dark "#463a52" :light "#e0d6e8"}
   :color.select {:dark "#54405e" :light "#f3dfc1"}
   :color.field {:dark "#30283a" :light "#ffffff"}
   :color.border {:dark "#3a3045" :light "#dcd2e3"}
   :color.text {:dark "#e2d8e6" :light "#2a2030"}
   :color.bright {:dark "#ffffff" :light "#150f1a"}
   :color.muted {:dark "#a597ad" :light "#6b5d75"}
   :color.dim {:dark "#6e6178" :light "#a796b0"}
   :color.onAccent {:dark "#1b1320" :light "#ffffff"}
   :color.accent {:dark "#f5a524" :light "#a65800"}
   :color.link {:dark "#ffc46b" :light "#94500a"}
   :color.play {:dark "#2ec4a0" :light "#1f9e80"}
   :color.playEnd {:dark "#17866c" :light "#137a62"}
   :color.warm {:dark "#ffc552" :light "#e09422"}
   :color.warmEnd {:dark "#c9560f" :light "#b8520c"}
   :color.ingame {:dark "#a5d66f" :light "#4f8a12"}
   :color.online {:dark "#c9a7ff" :light "#7a4fc0"}
   :color.offline {:dark "#898989" :light "#7a838c"}
   :color.badge {:dark "#1f6b58" :light "#1f6b58"}
   :color.onBadge {:dark "#7ff0cf" :light "#c8ffef"}
   :color.shade {:dark "#100c14cc" :light "#f6f2f7e6"}
   :assistant.accent @color.accent
   :assistant.accentHover @color.warm
   :assistant.surface @color.panel
   :assistant.raised @color.raised
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.dim
   :assistant.success @color.play
   :kit.background @color.page
   :kit.surface {:dark "#463a52" :light "#ffffff"}
   :kit.bar @color.top
   :kit.barHover @color.hover
   :kit.field @color.field
   :kit.text {:dark "#e8e0ec" :light "#2a2030"}
   :kit.dim @color.muted
   :kit.body @color.muted
   :kit.selection {:dark "#5e4a6c" :light "#f3dfc1"}
   :kit.border {:dark "#4d4058" :light "#dcd2e3"}
   :kit.primary @color.accent
   :kit.accent @color.link
   :kit.focus @color.accent
   :kit.radius 3
   :kit.cardRadius 4})

(defkey "alt+left" back)

(defkey "alt+right" forward)

(defkey "mod+1" showStore)

(defkey "mod+2" showLibrary)

(defkey "mod+3" showCommunity)

(defkey "mod+k mod+t" toggleTheme)

(defkey "mod+k mod+l" nextLanguage)

(defkey "mod+," {:do ~(handler openSettings "interface")})

(defcustom appearance.theme "dark"
  "Dark or light colours for the whole client."
  :type string
  :enum [dark light]
  :title "Theme")

(defcustom appearance.language "en" "English, Spanish or French." :type string :enum [en es fr] :title "Language")

(defcustom appearance.startPage "library"
  "The page Arcadia opens on."
  :type string
  :enum [library store community]
  :title "Start page")

(defcustom downloads.limit
  0
  "The most MB/s a download takes; 0 for no limit."
  :type number
  :enum [0 10 25]
  :title "Download speed limit")

(defcustom downloads.whilePlaying
  true
  "Off, downloads wait while a game is running."
  :type boolean
  :title "Download while playing")

(defcustom library.sortBy "name"
  "Name from A to Z, the most hours, or the most recently played."
  :type string
  :enum [name playtime recent]
  :title "Sort the library by")

(defpart index "What index adds."
  :onLoad [{:if ~(not= settings.appearance.startPage "library")
            :then [{:set state.nav :to ~[{:tab settings.appearance.startPage :page "home" :id nil}]}]}])

;;; The screen

(defscreen main
  :window {:title "Arcadia"
           :width 1280
           :height 800
           :minWidth 1024
           :minHeight 680
           :focusRing {:color "#f5a524" :width 2 :offset 1}}
  :strings {:default en :locale ~settings.appearance.language}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
          :caps {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :bold true :fallback [icons]}
          :strong {:file "../../shared/fonts/Roboto-Regular.ttf" :size 14 :bold true :fallback [icons]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :bold true :fallback [icons]}
          :nav {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :bold true :fallback [icons]}
          :button {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :bold true :fallback [icons]}
          :logo {:file "../../shared/fonts/Roboto-Regular.ttf" :size 48 :bold true}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio]}
  :theme ~settings.appearance.theme
  (node#app :color @color.page
    (column#app-body :fill true :dir auto
      (node#menu-row :use menuRow)
      (node#nav-row :use navRow)
      (node#body :grow 1 :anchors {:left 0 :right 0}
        (node#library :when ~(= computed.here.tab "library") :use libraryPage)
        (node :when ~(and (= computed.here.tab "store") (not state.session.offline)) :use storePage)
        (node :when ~(= computed.here.tab "downloads") :use downloadsPage)
        (node :when ~(and (= computed.here.tab "community") (not state.session.offline)) :use friendsPage)
        (node :when ~(and (contains ["store" "community"] computed.here.tab) state.session.offline) :use offlinePage)
        (node :when ~(= computed.here.tab "profile") :use profilePage)
        (node#elsewhere :when ~(not (contains ["library" "store" "downloads" "community" "profile"]
                                              computed.here.tab))
          :use comingSoon))
      (node#bottom-bar :use bottomBar))
    (node#settings-dialog :use settingsDialog)
    (node :use gameMenu)
    (node#properties-dialog :use propertiesDialog)
    (node#uninstall-dialog :use uninstallDialog)
    (node#code-dialog :use codeDialog)
    (node#add-game-dialog :use addGameDialog)
    (node#backup-dialog :use backupDialog)
    (node#note-dialog :use noteDialog)
    (node#info-dialog :use infoDialog)
    (node :use clientToast)
    (node :use couchMode)
    (node :use signedOutScreen)))
