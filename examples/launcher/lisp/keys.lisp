;;; keys.lisp --- the Launcher's keyboard shortcuts
;;;
;;; A key runs a command of launcher.lisp by name, or the action the host runs (launcher.refresh).

;;; Pages

(defkey "mod+1" showApps)

(defkey "mod+2" showMine)

(defkey "mod+3" showCreate)

(defkey "mod+n" showCreate)

(defkey "mod+comma" showSettings)

(defkey "mod+4" showSettings)

;;; Search and the folder

(defkey "mod+f" findApp)

(defkey "mod+r" launcher.refresh)
