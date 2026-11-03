;;; editing.lisp --- Forge's editing: the find and replace bar over the active editor (mod+f, mod+alt+f), plain, whole word, case-sensitive or a regular expression, in the whole file or in the selection, with the match count; and the editor's settings (word wrap, bracket matching, indentation guides)
;;;
;;; The editor node (features/editor) reads state.findBar and the settings; the status bar shows Ln and Col from
;;; state.caret.

;;; State and definitions

(defstate findBar
  {:open false
   :replacing false
   :text ""
   :replace ""
   :regex false
   :wholeWord false
   :caseSensitive false
   :inSelection false
   :step first
   :request 0})

(defcomputed findQuery
  (if (and state.findBar.open (not (empty? state.findBar.text)))
        {:text state.findBar.text
         :regex state.findBar.regex
         :wholeWord state.findBar.wholeWord
         :caseSensitive state.findBar.caseSensitive
         :inSelection state.findBar.inSelection
         :replace state.findBar.replace
         :step state.findBar.step
         :request state.findBar.request}
        nil))

(defcomputed findMatches (:matches (merge {:matches 0} state.caret)))

(defcomputed findSummary
  (cond (empty? state.findBar.text) ""
          (= computed.findMatches 0) "No results"
          :else (plural computed.findMatches "match" "matches")))

(defkey "mod+f" openFind)

(defkey "mod+alt+f" openReplace)

(defkey "alt+z" toggleWordWrap)

(defcustom editor.tabSize
  4 "Spaces per indentation level: what Tab inserts and an opened file's tabs become." :description ~(t "settings.editor.tabSize.about") :type integer :minimum 1 :maximum 16 :title ~(t "settings.editor.tabSize"))

(defcustom editor.wordWrap
  false "Wrap lines that don't fit the editor's width." :description ~(t "settings.editor.wordWrap.about") :type boolean :title ~(t "settings.editor.wordWrap"))

(defcustom editor.bracketMatching
  true "Highlight the bracket that pairs with the one at the caret." :description ~(t "settings.editor.bracketMatching.about") :type boolean :title ~(t "settings.editor.bracketMatching"))

(defcustom editor.guides.indentation
  true "Draw a line at each indentation level." :description ~(t "settings.editor.guides.indentation.about") :type boolean :title ~(t "settings.editor.guides.indentation"))

;;; Functions and commands

(defcommand openFind "Find"
  :title ~(t "command.openFind") :category ~(t "category.edit") :icon "search" []
  (set "state.findBar.open" true)
  (set "state.findBar.replacing" false)
  (focus "#find-text"))

(defcommand openReplace "Replace"
  :title ~(t "command.openReplace") :category ~(t "category.edit") :icon "replace" []
  (set "state.findBar.open" true)
  (set "state.findBar.replacing" true)
  (focus "#find-text"))

(defcommand closeFind "Close Find"
  :title ~(t "command.closeFind") :category ~(t "category.edit") :enablement ~state.findBar.open []
  (set "state.findBar.open" false)
  (set "state.findBar.inSelection" false)
  (after 0 closeFindLater))

(defn findStep "TODO" [step]
  (set "state.findBar.step" step)
  (increment "state.findBar.request" :by 1))

(defcommand findNext "Find Next"
  :title ~(t "command.findNext") :category ~(t "category.edit") :enablement ~state.findBar.open []
  (findStep "next"))

(defcommand findPrevious "Find Previous"
  :title ~(t "command.findPrevious") :category ~(t "category.edit") :enablement ~state.findBar.open []
  (findStep "previous"))

(defcommand replaceOne "Replace"
  :title ~(t "command.replaceOne") :category ~(t "category.edit") :enablement ~state.findBar.open []
  (findStep "replace"))

(defcommand replaceAllInFile "Replace All"
  :title ~(t "command.replaceAllInFile") :category ~(t "category.edit") :enablement ~state.findBar.open []
  (findStep "replaceAll"))

(defcommand toggleFindInSelection "Toggle Find in Selection"
  :title ~(t "command.toggleFindInSelection") :category ~(t "category.edit") []
  (set "state.findBar.inSelection" (not state.findBar.inSelection)))

(defcommand toggleWordWrap "Toggle Word Wrap"
  :title ~(t "command.toggleWordWrap") :category ~(t "category.view") []
  (set "settings.editor.wordWrap" (not settings.editor.wordWrap)))

(defn closeFindLater "What closeFind does when its time comes." []
  (focus "#editor"))
