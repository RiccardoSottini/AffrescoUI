;;; keys.lisp --- Haven's keyboard shortcuts
;;;
;;; A key runs a command of haven.lisp by name, or a function with its arguments.

;;; The room shown

(defkey "up" "A half degree warmer." ~(handler setTarget 0.5))

(defkey "down" "A half degree cooler." ~(handler setTarget -0.5))

(defkey "alt+down" nextRoom)

(defkey "alt+up" previousRoom)

;;; Pages

(defkey "mod+1" goHome)

(defkey "mod+2" goEnergy)

(defkey "mod+3" goPlan)

(defkey "mod+4" goAutomations)

(defkey "mod+5" goLogbook)

(defkey "mod+6" goHistory)

(defkey "mod+comma" goSettings)

;;; Search and commands

(defkey "mod+shift+p" commandPalette)

(defkey "mod+k" commandPalette)

(defkey "c" commandPalette)

(defkey "e" searchEntities)

;;; The window and security

(defkey "mod+t" toggleTheme)

(defkey "mod+l" switchLanguage)

(defkey "mod+shift+a" cycleSecurity)

(defkey "mod+shift+n" toggleNotifications)
