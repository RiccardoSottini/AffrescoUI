;;; index.screen.lisp --- Tempo, a music player
;;;
;;; Playlists, a library of 3,000 songs in a table, albums and artists in a grid, search, a queue to reorder by dragging
;;; or with Alt and the arrows, now playing with a visualiser of the spectrum of what plays and lyrics, an equaliser on a
;;; curves control, crossfade and a mini player; light and dark themes, English, Spanish and French. Cmd+P finds a song
;;; and Cmd+Shift+P runs any command; Space plays or pauses, the arrows skip and set the volume (keys.lisp has them all).
;;; A right click (or Shift+F10) on a song opens its menu. The songs are simulated: what plays is one beatbox loop
;;; (audio/beatbox.wav, made by audio/make_beat.py), on hosts that give the screen sound; a song may name a sound of its
;;; own (its "sound", a sound of the audio section), which then plays instead. The equaliser filters the sound, and a
;;; crossfade overlaps the song ending and the next (the beat plays on two decks).

(ns tempo.index
  (:require "locales/en.lisp"
            "locales/es.lisp"
            "locales/fr.lisp"
            "models.lisp"
            "sound.lisp"
            tempo.state
            tempo.computed
            tempo.look
            tempo.keys
            tempo.sidebar
            tempo.library
            tempo.queue
            tempo.player
            tempo.pages
            tempo.nowplaying
            tempo.dialogs
            "../../shared/addons/assistant/assistant.lisp"
            tempo.tempo))

;;; The window, and what is in it, in the order it is drawn

(defscreen main "Tempo's one window: the sidebar, the page and the queue, the player, and what opens over them."
  :window {:title "Tempo: an AffrescoUI template" :width 1280 :height 800 :minWidth 1040 :minHeight 640}
  :strings {:default en :locale ~settings.ui.language :keep ["Tempo" "Tempo: an AffrescoUI template"]}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
          :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [iconsSmall]}
          :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 17 :fallback [icons]}
          :large {:file "../../shared/fonts/Roboto-Regular.ttf" :size 32}
          :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 44}
          :brand {:file "../../shared/fonts/Roboto-Regular.ttf" :size 24}
          :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
          :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
          :iconsLarge {:file "../../shared/fonts/Lucide.ttf" :size 36}}
  :persist [state.theme state.settings state.eq state.eqOn state.volume state.shuffle state.repeat state.liked]
  :permissions {:allow [studio]
                :ask ["agent(/actions)" "agent(/program)"]
                :deny ["*(#statusBar)"]
                :reviewers [studio]
                :budgets [{:author agent :added 20 :removed 5 :changed 50}]
                :run {:allow ["agent(songPlay)" "agent(songPause)" "agent(songQueue)" "agent(songLike)"]}}
  :theme ~state.theme
  (node#panel-root :color @background :every {:ms 1000 :do tick}
    ~(sidebar)
    ~(main-area)
    ~(player)
    ~(status-bar)
    (node#miniView :when ~state.mini :use miniPlayer :fill true)
    ~(song-menu)
    ~(list-menu)
    ~(delete-dialog)
    ~(quick-pick)))
