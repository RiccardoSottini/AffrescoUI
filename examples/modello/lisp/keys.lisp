;;; keys.lisp --- Modello's keyboard shortcuts, as in other 3D modellers: each runs a command or a function
;;;
;;; A key that does one thing while a transform runs and another otherwise lists both, the first that applies winning.

;;; Moving, turning and scaling after the pointer: the keys work while a transform runs

(defkey "g" grab)

(defkey "r" rotate)

(defkey "s" resize)

(defkey "x" [{:when ~(some? state.modal) :do lockAxis :with {:axis x}} {:do deleteMenu}])

(defkey "y" [{:when ~(some? state.modal) :do lockAxis :with {:axis y}}])

(defkey "z"
  [{:when ~(some? state.modal) :do lockAxis :with {:axis z}} {:do openPie :with {:menu shading}}])

(defkey "shift+x" {:when ~(some? state.modal) :do lockAxis :with {:axis yz}})

(defkey "shift+y" {:when ~(some? state.modal) :do lockAxis :with {:axis zx}})

(defkey "shift+z" {:when ~(some? state.modal) :do lockAxis :with {:axis xy}})

(defkey "0" [{:when ~(some? state.modal) :do typeDigit :with {:key "0"}}])

(defkey "1" [{:when ~(some? state.modal) :do typeDigit :with {:key "1"}}])

(defkey "2" [{:when ~(some? state.modal) :do typeDigit :with {:key "2"}}])

(defkey "3" [{:when ~(some? state.modal) :do typeDigit :with {:key "3"}}])

(defkey "4" [{:when ~(some? state.modal) :do typeDigit :with {:key "4"}}])

(defkey "5" [{:when ~(some? state.modal) :do typeDigit :with {:key "5"}}])

(defkey "6" [{:when ~(some? state.modal) :do typeDigit :with {:key "6"}}])

(defkey "7" [{:when ~(some? state.modal) :do typeDigit :with {:key "7"}}])

(defkey "8" [{:when ~(some? state.modal) :do typeDigit :with {:key "8"}}])

(defkey "9" [{:when ~(some? state.modal) :do typeDigit :with {:key "9"}}])

(defkey "period" [{:when ~(some? state.modal) :do typeDigit :with {:key "."}}])

(defkey "minus" [{:when ~(some? state.modal) :do typeDigit :with {:key "-"}}])

(defkey "backspace" [{:when ~(some? state.modal) :do typeDigit :with {:key back}}])

(defkey "enter" [{:when ~(some? state.modal) :do confirmTransform}])

(defkey "escape" [{:when ~(some? state.modal) :do cancelTransform}])

;;; Objects

(defkey "shift+a" addMenu)

(defkey "delete" [{:when ~(= (get state.areas state.hoverSlot) "shader") :do deleteShaderNodes} {:do deleteSelected}])

(defkey "shift+d" duplicate)

(defkey "h" hideSelected)

(defkey "m" moveToCollectionMenu)

(defkey "f2" startRename)

(defkey "shift+h" hideUnselected)

(defkey "alt+h" revealHidden)

(defkey "a" selectAll)

(defkey "alt+a" selectNone)

(defkey "mod+i" selectInvert)

(defkey "mod+c" {:when ~(not context.textInputFocus) :do copyObjects})

(defkey "mod+v" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do pasteObjects})

(defkey "alt+g" {:when ~(nil? state.modal) :do clearLocation})

(defkey "alt+r" {:when ~(nil? state.modal) :do clearRotation})

(defkey "alt+s" {:when ~(nil? state.modal) :do clearScale})

(defkey "shift+s" snapMenu)

(defkey "shift+tab" {:when ~(not context.textInputFocus) :do toggleSnap})

;;; The view: the numpad turns it, as in other 3D modellers

(defkey "grave" {:when ~(nil? state.modal) :do openPie :with {:menu view}})

(defkey "numpad1" viewFront)

(defkey "mod+numpad1" viewBack)

(defkey "numpad3" viewRight)

(defkey "mod+numpad3" viewLeft)

(defkey "numpad7" viewTop)

(defkey "mod+numpad7" viewBottom)

(defkey "numpad5" toggleProjection)

(defkey "numpad0" viewCamera)

(defkey "numpadperiod" frameSelected)

(defkey "home" frameAll)

(defkey "numpad4" {:do orbitView :with {:orbit [-15 0]}})

(defkey "numpad6" {:do orbitView :with {:orbit [15 0]}})

(defkey "numpad8" {:do orbitView :with {:orbit [0 -15]}})

(defkey "numpad2" {:do orbitView :with {:orbit [0 15]}})

(defkey "numpadplus" {:do zoomView :with {:steps 1}})

(defkey "numpadminus" {:do zoomView :with {:steps -1}})

(defkey "alt+z" toggleXray)

(defkey "shift+alt+z" toggleOverlays)

(defkey "n" toggleSidebar)

(defkey "t" toggleToolbar)

(defkey "ctrl+space" {:when ~(not context.textInputFocus) :do toggleMaximize})

;;; Animation

(defkey "i" insertKeyframe)

(defkey "alt+i" deleteKeyframe)

(defkey "shift+alt+i" clearKeyframes)

(defkey "up" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do nextKey})

(defkey "down" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do prevKey})

(defkey "space" {:when ~(not context.textInputFocus) :do play})

(defkey "shift+ctrl+space" {:when ~(not context.textInputFocus) :do playReverse})

(defkey "shift+left" jumpStart)

(defkey "shift+right" jumpEnd)

;;; Editing, files and the window

(defkey "mod+z" undo)

(defkey "mod+shift+z" redo)

(defkey "f3" search)

(defkey "f9" {:when ~(some? state.lastOp) :do adjustLastPanel})

(defkey "f12" renderImage)

(defkey "mod+n" newScene)

(defkey "mod+s" saveScene)

(defkey "mod+o" openScene)

(defkey "mod+pagedown" nextWorkspace)

(defkey "mod+pageup" previousWorkspace)
