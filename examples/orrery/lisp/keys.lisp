;;; keys.lisp --- Orrery's keyboard shortcuts: each runs a command or a function of orrery.lisp

;;; Keys

(defkey "mod+p" togglePlay)

(defkey "mod+n" nextEvent)

(defkey "mod+0" rewind)

(defkey "l" faster)

(defkey "k" slower)

(defkey "j" reverse)

(defkey "space" togglePlay)

(defkey "shift+1" dayZero)

(defkey "shift+k" realTime)

(defkey "g" goTo)

(defkey "c" centre)

(defkey "f" followSelected)

(defkey "o" toggleOrbits)

(defkey "p" toggleLabels)

(defkey "b" toggleBrowser)

(defkey "mod+d" addBookmark)

(defkey "period" zoomIn)

(defkey "comma" zoomOut)

(defkey "mod+comma" openRender)

(defkey "equal" fainter)

(defkey "minus" brighter)

(defkey "m" toggleMarker)
