;;; keys.lisp --- Tempera's keyboard shortcuts
;;;
;;; A letter chooses a tool, as in other image editors; the rest run a command of commands.lisp by name. The
;;; clipboard's keys leave a text field's own copy and paste alone.

;;; Tools

(defkey "v" {:do setTool :with {:args {:tool move}}})

(defkey "m" {:do setTool :with {:args {:tool marquee}}})

(defkey "shift+m" {:do setTool :with {:args {:tool ellipse}}})

(defkey "l" {:do setTool :with {:args {:tool lasso}}})

(defkey "shift+l" {:do setTool :with {:args {:tool polygon}}})

(defkey "w" {:do setTool :with {:args {:tool wand}}})

(defkey "c" {:do setTool :with {:args {:tool crop}}})

(defkey "b" {:do setTool :with {:args {:tool brush}}})

(defkey "shift+b" {:do setTool :with {:args {:tool pencil}}})

(defkey "e" {:do setTool :with {:args {:tool eraser}}})

(defkey "s" {:do setTool :with {:args {:tool clone}}})

(defkey "g" {:do setTool :with {:args {:tool gradient}}})

(defkey "shift+g" {:do setTool :with {:args {:tool fill}}})

(defkey "i" {:do setTool :with {:args {:tool eyedropper}}})

(defkey "h" {:do setTool :with {:args {:tool hand}}})

(defkey "z" {:do setTool :with {:args {:tool zoom}}})

;;; Colours, the brush and the quick mask

(defkey "d" resetColours)

(defkey "x" swapColours)

(defkey "q" toggleQuickMask)

(defkey "comma" brushSmaller)

(defkey "period" brushLarger)

(defkey "leftbracket" brushSmaller)

(defkey "rightbracket" brushLarger)

(defkey "shift+backspace" showFill)

(defkey "alt+backspace" fillForeground)

(defkey "mod+backspace" fillBackground)

;;; File

(defkey "mod+n" showNewDialog)

(defkey "mod+o" openFile)

(defkey "mod+w" closeDocument)

(defkey "mod+s" saveProject)

(defkey "mod+shift+s" saveProjectAs)

(defkey "mod+alt+shift+w" exportPng)

(defkey "f12" revert)

;;; Edit

(defkey "mod+z" undo)

(defkey "mod+shift+z" redo)

(defkey "mod+x" [{:when ~(not context.textInputFocus) :do cutPixels}])

(defkey "mod+c" [{:when ~(not context.textInputFocus) :do copyPixels}])

(defkey "mod+shift+c" [{:when ~(not context.textInputFocus) :do copyMerged}])

(defkey "mod+v" [{:when ~(not context.textInputFocus) :do pastePixels}])

(defkey "backspace" clearPixels)

(defkey "delete" clearPixels)

(defkey "mod+t" showTransform)

;;; Image

(defkey "mod+alt+i" showImageSize)

(defkey "mod+alt+c" showCanvasSize)

(defkey "mod+l" levelsLayer)

(defkey "mod+m" curvesLayer)

(defkey "mod+u" hueSaturationLayer)

(defkey "mod+b" colorBalanceLayer)

(defkey "mod+i" invertLayer)

(defkey "mod+alt+shift+b" blackWhiteLayer)

(defkey "mod+y" toggleProof)

;;; Layer

(defkey "mod+shift+n" newLayer)

(defkey "mod+j" duplicateActive)

(defkey "mod+shift+j" layerViaCut)

(defkey "mod+e" mergeDown)

(defkey "mod+shift+e" mergeVisible)

(defkey "mod+g" groupLayers)

(defkey "mod+shift+g" ungroupLayers)

(defkey "mod+alt+g" toggleClipping)

(defkey "mod+up" layerUp)

(defkey "mod+down" layerDown)

(defkey "mod+shift+up" layerToFront)

(defkey "mod+shift+down" layerToBack)

(defkey "mod+slash" toggleLockAll)

;;; Select

(defkey "mod+a" selectAll)

(defkey "mod+d" deselect)

(defkey "mod+shift+i" invertSelection)

(defkey "shift+f6" featherSelection)

;;; View and Help

(defkey "mod+equal" zoomIn)

(defkey "mod+minus" zoomOut)

(defkey "mod+0" fitOnScreen)

(defkey "mod+1" actualPixels)

(defkey "mod+r" toggleRulers)

(defkey "f" cycleScreenMode)

(defkey "mod+f" showSearch)
