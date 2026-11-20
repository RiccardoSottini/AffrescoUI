;;; keys.lisp --- Tempo's keyboard shortcuts, each running a command of tempo.lisp, and its settings

(ns tempo.keys)

;;; Keys

(defkey "space" playPause)

(defkey "right" nextCommand)

(defkey "left" previousCommand)

(defkey "up" volumeUp)

(defkey "down" volumeDown)

(defkey "mod+p" searchCommand)

(defkey "mod+shift+p" commandPalette)

(defkey "mod+l" showLibrary)

(defkey "mod+2" showAlbums)

(defkey "mod+3" showArtists)

(defkey "mod+4" showLiked)

(defkey "mod+f" showSearch)

(defkey "mod+5" showNowPlaying)

(defkey "mod+6" showSound)

(defkey "mod+u" toggleQueue)

(defkey "mod+m" toggleMini)

(defkey "mod+t" toggleTheme)

(defkey "mod+shift+l" switchLanguage)

(defkey "mod+s" toggleShuffle)

(defkey "mod+r" cycleRepeat)

(defkey "mod+alt+down" toggleMute)

(defkey "mod+leftbracket" goBack)

(defkey "mod+rightbracket" goForward)

;;; Settings

(defcustom playback.crossfade
  0 "How many seconds the end of a song overlaps the start of the next; 0 for none."
  :type integer :minimum 0 :maximum 12 :title ~(t "settings.crossfade") :description ~(t "settings.crossfade.about"))

(defcustom window.statusBar
  false "Show the revision on screen and the window's size in a bar at the bottom of the window: for developers."
  :type boolean :title ~(t "settings.developerStatusBar") :description ~(t "settings.developerStatusBar.about"))

(defcustom ui.language "en" "English, Spanish or French."
  :type string :enum [en es fr] :title ~(t "settings.language") :description ~(t "settings.language.about"))

