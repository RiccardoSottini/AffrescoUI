;;; editor.lisp --- the editor: its tabs, the code, and the find panel under it

(ns code-editor.editor)

;;; Views

(defview editor-column "The editor's column: the tabs and the file shown, over the bottom panel." []
  `(split#editorColumn :direction column
     :bind state.panelSizes
     :defaultSizes [560]
     :minSize [160 100]
     :sashSize 2
     :sashColor @border
     :sashActiveColor @accent
     (split#editorSplit :direction ~state.layout
       :bind state.splitSizes
       :minSize [240 200]
       :sashSize 2
       :sashColor @border
       :sashActiveColor @accent
       (node#editorGroup :color @background
         ~(editor-tabs)
         ~(repeat [open (if computed.file [state.active] []) :key open]
           (editor open))
         (label#welcome :when ~(not computed.file)
           :text ~(t "ui.noFileOpenPickOne")
           :textColor @faint
           :anchors {:left 0 :right 0 :top 32 :bottom 0}
           :align center))
       ~(side-group))
     ~(bottom-panel)))

(defview side-group "The second editor group, beside the first or under it: one file, its own caret." []
  `(node#sideGroup :when ~(and (>= state.split 0) (not state.zen)) :color @background
     (tabs#sideTabs
       :anchors {:left 0 :right 0 :top 0}
       :height 32
       :tabs ~[{:id (text state.split) :label (get-in state.files [state.split :name])
                :marker (get-in state.files [state.split :dirty])}]
       :value ~(text state.split)
       :color @tabBar
       :textColor @muted
       :activeTextColor @text
       :activeBackground @background
       :accentColor transparent
       :markerColor @accent
       :keepMarker true
       :tabRadius 7
       :tabGap 4
       :tabInset 5
       :onClose closeSplit)
     ~(repeat [open (if (>= state.split 0) [state.split] []) :key open]
       (side-editor open))))

(defview side-editor "The file `open` in the second group: the same text as in the first, edited in either." [open]
  `(editor#sideEditor
     :font code
     :lineNumbers true
     :language ~(get-in state.files [open :language])
     :grammar ~(or-else (get-in state.files [open :grammar]) (get-in state.files [open :name]))
     :readOnly ~(or-else (get-in state.files [open :readOnly]) false)
     :commands {"alt+up" moveLinesUp
                "alt+down" moveLinesDown
                "mod+shift+k" deleteLines
                "mod+[" outdentLines
                "mod+]" indentLines
                "mod+c" copyLine
                "mod+x" cutLine}
     :tabSize ~(or-else (get-in state.files [open :tabSize]) 4)
     :minimap false
     :indentGuides ~(not (get-in state.files [open :scratch]))
     :brackets true :bracketStyle underline :foldMarkers true
     :rulers ~(if state.ruler [80] [])
     :wrap ~state.wrap
     :bind "state.files[open].text"
     :anchors {:left 0 :right 0 :top 32 :bottom 0}
     :color @background :textColor @plain :lineNumberColor @faint :activeLineNumberColor @text
     :lineColor @line :selectionColor @selection :caretColor @accent :guideColor @guide
     :bracketColor @accent
     :onChange ~(handler mark-dirty open)))

(defview editor-tabs "The open files' tabs: a dot on an unsaved one; drag to reorder, right-click for the menu." []
  `(tabs#editorTabs :when ~(not state.zen)
     :anchors {:left 0 :right 0 :top 0}
     :height 32
     :tabs ~computed.tabItems
     :value ~(text state.active)
     :color @tabBar
     :textColor @muted
     :activeTextColor @text
     :activeBackground @background
     :accentColor transparent
     :markerColor @accent
     :keepMarker true
     :tabRadius 7
     :tabGap 4
     :tabInset 5
     :onContextMenu ~(handler open-tab-menu event.x event.y event.data.id)
     :onChange ~(handler show-file (number event.value))
     :onClose ~(handler close (number event.id))
     :onMove ~(handler move-tab event.from event.to (number event.id))))

(defview editor
  "The file `open` in the library's editor: its language gives the commands, its grammar the colours."
  [open]
  `(editor#editor
     :note "The library editor: its language gives the commands, pairs and indentation; its grammar, the colours"
     :font code
     :lineNumbers true
     :language ~(get-in state.files [open :language])
     :grammar ~(or-else (get-in state.files [open :grammar]) (get-in state.files [open :name]))
     :readOnly ~(or-else (get-in state.files [open :readOnly]) false)
     :commands {"alt+up" moveLinesUp
                "alt+down" moveLinesDown
                "mod+shift+k" deleteLines
                "mod+[" outdentLines
                "mod+]" indentLines
                "mod+c" copyLine
                "mod+x" cutLine}
     :minimap ~state.minimap :minimapStyle bars :minimapWidth 44 :minimapViewport always
     :matchStyle outline
     :indentGuides ~(not (get-in state.files [open :scratch]))
     :brackets true :bracketStyle underline :foldMarkers true
     :tabSize ~(or-else (get-in state.files [open :tabSize]) 4)
     :rulers ~(if state.ruler [80] [])
     :wrap ~state.wrap
     :find ~computed.findQuery
     :insert ~state.snippetInsert
     :bind "state.files[open].text"
     :caret state.caret
     :autofocus true
     :anchors {:left ~(if state.zen 160 0) :right ~(if state.zen 160 0) :top ~(if state.zen 24 32) :bottom 0}
     :color @background :textColor @plain :lineNumberColor @faint :activeLineNumberColor @text
     :lineColor @line :selectionColor @selection :caretColor @accent :guideColor @guide
     :matchColor @selection :bracketColor @accent
     :onShow editor-shown
     :onChange ~(handler mark-dirty open)
     :onContextMenu ~(handler open-editor-menu event.x event.y event.left event.top event.width event.height)
     :onDoubleClick ~(handler result-clicked open state.caret.line)))

(defview find-panel "The find panel under the editor: find, replace, where, its toggles and buttons." []
  `(node#findPanel :dir ~state.dir
     :when ~state.findBar.open
     :anchors {:left 0 :right 0 :bottom ~(if state.zen 0 24)}
     :height ~computed.findHeight
     :color @chrome
     :keys {:escape closeFind
            "shift+enter" findPrevious
            "alt+mod+r" toggleFindRegex
            "alt+mod+c" toggleFindCase
            "alt+mod+e" toggleFindWord
            "alt+mod+s" toggleFindInSelection}
     ~(find-regex)
     ~(find-case)
     ~(find-word)
     ~(find-selection)
     ~(find-text)
     (node#findCount :when ~(not state.findBar.inFiles)
       :style caption
       :text ~computed.findSummary
       :anchors {:right 220 :top 10})
     ~(find-next-button)
     ~(find-previous-button)
     (node#findFilesButton :when ~state.findBar.inFiles
       :style findButton
       :text ~(t "bar.find")
       :anchors {:right 12 :top 5}
       :onClick search-files)
     (node#label-find :when ~(> computed.findLeft 128) :style findLabel :text ~(t "ui.find2")
       :anchors {:left 128 :top 9})
     (node#label-where :when ~state.findBar.inFiles :style findLabel :text ~(t "ui.where") :anchors {:left 128 :top 39})
     ~(label-replace)
     ~(where-text)
     ~(replace-text)
     ~(replace-button)
     ~(replace-all-button)
     ~(replace-files-button)))

(defview find-text "The text to find: typing finds it in the file; Enter finds the next, or searches the files." []
  `(node#findText :style findField
     :anchors {:left ~computed.findLeft :right ~(if state.findBar.inFiles 112 300) :top 5}
     :placeholder ~(if (> computed.findLeft 128) "" (t "bar.find"))
     :bind state.findBar.text
     :autofocus true
     :onChange find-as-typed
     :onSubmit submit-find))

(defview replace-text "The replacement: Enter replaces one, or in the files." []
  `(node#replaceText :style findField
     :when ~state.findBar.replacing
     :anchors {:left ~computed.findLeft :right ~(if state.findBar.inFiles 112 300) :top ~computed.replaceTop}
     :bind state.findBar.replace
     :onSubmit submit-replace))

(defview find-selection "In selection: find only in the selected text." []
  `(node#findSelection :style findToggle
     :text "[ ]"
     :anchors {:left 92 :top 6}
     :color ~(if state.findBar.inSelection "@rowSelected" "transparent")
     :textColor ~(if state.findBar.inSelection "@accent" "@muted")
     :tooltip ~(t "ui.inSelection")
     :onClick toggleFindInSelection))

(defview find-case "Case sensitive." []
  `(node#findCase :style findToggle
     :text "Aa"
     :anchors {:left 36 :top 6}
     :color ~(if state.findBar.caseSensitive "@rowSelected" "transparent")
     :textColor ~(if state.findBar.caseSensitive "@accent" "@muted")
     :tooltip ~(t "ui.caseSensitive")
     :onClick toggleFindCase))

(defview where-text "Where Find in Files looks: open folders, open files, or parts of paths." []
  `(node#whereText :style findField
     :when ~state.findBar.inFiles
     :anchors {:left ~computed.findLeft :right 112 :top 35}
     :placeholder ~(t "ui.whereOpenFoldersOpenFiles")
     :bind state.findBar.where
     :onSubmit search-files))

(defview find-word "Whole word." []
  `(node#findWord :style findToggle
     :text "\"\""
     :anchors {:left 64 :top 6}
     :color ~(if state.findBar.wholeWord "@rowSelected" "transparent")
     :textColor ~(if state.findBar.wholeWord "@accent" "@muted")
     :tooltip ~(t "ui.wholeWord")
     :onClick toggleFindWord))

(defview find-regex "Regular expression." []
  `(node#findRegex :style findToggle
     :text ".*"
     :anchors {:left 8 :top 6}
     :color ~(if state.findBar.regex "@rowSelected" "transparent")
     :textColor ~(if state.findBar.regex "@accent" "@muted")
     :tooltip ~(t "ui.regularExpression")
     :onClick toggleFindRegex))

(defview replace-all-button "Replace All, in the file." []
  `(node#replaceAllButton :style findButton
     :text ~(t "ui.replaceAll")
     :when ~(and state.findBar.replacing (not state.findBar.inFiles))
     :anchors {:right 12 :top 35}
     :onClick replaceAllInFile))

(defview replace-files-button "Replace in the files Find in Files searched." []
  `(node#replaceFilesButton :style findButton
     :text ~(t "ui.replace")
     :when ~(and state.findBar.replacing state.findBar.inFiles)
     :anchors {:right 12 :top 65}
     :onClick replaceInFiles))

(defview replace-button "Replace, once." []
  `(node#replaceButton :style findButton
     :text ~(t "ui.replace")
     :when ~(and state.findBar.replacing (not state.findBar.inFiles))
     :anchors {:right 112 :top 35}
     :onClick replaceOne))

(defview find-previous-button "Find Prev." []
  `(node#findPreviousButton :when ~(not state.findBar.inFiles)
     :style findButton
     :text ~(t "ui.findPrev")
     :anchors {:right 12 :top 5}
     :onClick findPrevious))

(defview find-next-button "Find, the next match." []
  `(node#findNextButton :when ~(not state.findBar.inFiles)
     :style findButton
     :text ~(t "bar.find")
     :anchors {:right 112 :top 5}
     :onClick findNext))

(defview label-replace "Replace's caption." []
  `(node#label-replace :when ~state.findBar.replacing
     :style findLabel
     :text ~(t "ui.replace2")
     :anchors {:left 128 :top ~(+ computed.replaceTop 4)}))
