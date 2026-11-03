;;; index.screen.lisp --- Arcadia: a desktop game store and launcher, in a look of its own
;;;
;;; A menu row and a centred navigation (store, library, friends, profile) with back and forward; a library with a
;;; game list on the left, a home of shelves and a page per game; a store with a page per game, a cart and a wishlist;
;;; downloads, friends with a chat, a profile, Settings and couch mode. Every game, studio, friend and picture is
;;; invented; built from the public docs only (see README.md next to it). What each part shows is in parts/, what it
;;; does in arcadia.lisp. Alt+Left and Alt+Right go back and forward, Cmd+1 to Cmd+3 change page, Cmd+, opens Settings.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp"
         "parts/data.lisp"
         "parts/look.lisp"
         "parts/places.lisp"
         "parts/chrome.lisp"
         "parts/library.lisp"
         "parts/game.lisp"
         "parts/store.lisp"
         "parts/store-game.lisp"
         "parts/cart.lisp"
         "parts/downloads.lisp"
         "parts/friends.lisp"
         "parts/profile.lisp"
         "parts/manage.lisp"
         "parts/settings.lisp"
         "parts/client.lisp"
         "parts/hub.lisp"
         "../../shared/addons/assistant/assistant.lisp"
         "arcadia.lisp")

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Arcadia's one window: the menu row, the navigation, the page, the bottom bar, and what opens over them."
  :window {:title "Arcadia"
           :width 1280
           :height 800
           :minWidth 1024
           :minHeight 680
           :focusRing {:color "#f5a524" :width 2 :offset 1}}
  :strings {:default en :locale ~settings.appearance.language :keep ["Arcadia" "English" "Español" "Français" "XXXXX-XXXXX-XXXXX" "GIFT-XXXX-XXXX"]}
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
      ~(pages)
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
