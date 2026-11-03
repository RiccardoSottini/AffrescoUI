;;; keys.lisp --- TODO

(defkey "g" {:do grab})

(defkey "r" {:do rotate})

(defkey "s" {:do resize})

(defkey "x" [{:when ~(some? state.modal) :do lockAxis :with {:args {:axis x}}} {:do deleteMenu}])

(defkey "y" [{:when ~(some? state.modal) :do lockAxis :with {:args {:axis y}}}])

(defkey "z"
  [{:when ~(some? state.modal) :do lockAxis :with {:args {:axis z}}} {:do openPie :with {:args {:menu shading}}}])

(defkey "grave" {:when ~(nil? state.modal) :do openPie :with {:args {:menu view}}})

(defkey "shift+x" {:when ~(some? state.modal) :do lockAxis :with {:args {:axis yz}}})

(defkey "shift+y" {:when ~(some? state.modal) :do lockAxis :with {:args {:axis zx}}})

(defkey "shift+z" {:when ~(some? state.modal) :do lockAxis :with {:args {:axis xy}}})

(defkey "0" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "0"}}}])

(defkey "1" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "1"}}}])

(defkey "2" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "2"}}}])

(defkey "3" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "3"}}}])

(defkey "4" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "4"}}}])

(defkey "5" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "5"}}}])

(defkey "6" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "6"}}}])

(defkey "7" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "7"}}}])

(defkey "8" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "8"}}}])

(defkey "9" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "9"}}}])

(defkey "period" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "."}}}])

(defkey "minus" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key "-"}}}])

(defkey "backspace" [{:when ~(some? state.modal) :do typeDigit :with {:args {:key back}}}])

(defkey "enter" [{:when ~(some? state.modal) :do confirmTransform}])

(defkey "escape" [{:when ~(some? state.modal) :do cancelTransform}])

(defkey "shift+a" {:do addMenu})

(defkey "delete" [{:when ~(= (get state.areas state.hoverSlot) "shader") :do deleteShaderNodes} {:do deleteSelected}])

(defkey "shift+d" {:do duplicate})

(defkey "h" {:do hideSelected})

(defkey "m" {:do moveToCollectionMenu})

(defkey "f2" {:do startRename})

(defkey "shift+h" {:do hideUnselected})

(defkey "alt+h" {:do revealHidden})

(defkey "a" {:do selectAll})

(defkey "alt+a" {:do selectNone})

(defkey "mod+i" {:do selectInvert})

(defkey "numpad1" {:do viewFront})

(defkey "mod+numpad1" {:do viewBack})

(defkey "numpad3" {:do viewRight})

(defkey "mod+numpad3" {:do viewLeft})

(defkey "numpad7" {:do viewTop})

(defkey "mod+numpad7" {:do viewBottom})

(defkey "numpad5" {:do toggleProjection})

(defkey "numpad0" {:do viewCamera})

(defkey "numpadperiod" {:do frameSelected})

(defkey "home" {:do frameAll})

(defkey "numpad4" {:do orbitView :with {:args {:orbit [-15 0]}}})

(defkey "numpad6" {:do orbitView :with {:args {:orbit [15 0]}}})

(defkey "numpad8" {:do orbitView :with {:args {:orbit [0 -15]}}})

(defkey "numpad2" {:do orbitView :with {:args {:orbit [0 15]}}})

(defkey "numpadplus" {:do zoomView :with {:args {:steps 1}}})

(defkey "numpadminus" {:do zoomView :with {:args {:steps -1}}})

(defkey "mod+z" {:do undo})

(defkey "mod+c" {:when ~(not context.textInputFocus) :do copyObjects})

(defkey "mod+v" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do pasteObjects})

(defkey "f12" {:do renderImage})

(defkey "mod+shift+z" {:do redo})

(defkey "n" {:do toggleSidebar})

(defkey "f3" {:do search})

(defkey "f9" {:when ~(some? state.lastOp) :do adjustLastPanel})

(defkey "t" {:do toggleToolbar})

(defkey "alt+z" {:do toggleXray})

(defkey "shift+alt+z" {:do toggleOverlays})

(defkey "shift+tab" {:when ~(not context.textInputFocus) :do toggleSnap})

(defkey "alt+g" {:when ~(nil? state.modal) :do clearLocation})

(defkey "alt+r" {:when ~(nil? state.modal) :do clearRotation})

(defkey "alt+s" {:when ~(nil? state.modal) :do clearScale})

(defkey "shift+s" {:do snapMenu})

(defkey "ctrl+space" {:when ~(not context.textInputFocus) :do toggleMaximize})

(defkey "i" {:do insertKeyframe})

(defkey "alt+i" {:do deleteKeyframe})

(defkey "shift+alt+i" {:do clearKeyframes})

(defkey "up" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do nextKey})

(defkey "down" {:when ~(and (not context.textInputFocus) (nil? state.modal)) :do prevKey})

(defkey "shift+ctrl+space" {:when ~(not context.textInputFocus) :do playReverse})

(defkey "space" {:when ~(not context.textInputFocus) :do play})

(defkey "shift+left" {:do jumpStart})

(defkey "shift+right" {:do jumpEnd})

(defkey "mod+n" {:do newScene})

(defkey "mod+s" {:do saveScene})

(defkey "mod+o" {:do openScene})

(defkey "mod+pagedown" {:do nextWorkspace})

(defkey "mod+pageup" {:do previousWorkspace})

