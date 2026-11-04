;;; keys.lisp --- Quaderno's menus and its keyboard

;;; The menus

(defmenu file "The File menu."
  {:new {:run newFile :group "1_new"}
   :open {:run quickOpen :group "1_new" :order 2}
   :reopen {:run reopenClosed :group "1_new" :order 3}
   :save {:run save :group "2_save"}
   :saveAll {:run saveAll :group "2_save" :order 2}
   :revert {:run revertFile :group "2_save" :order 3}
   :close {:run closeTab :group "3_close"}
   :closeAll {:run closeAll :group "3_close" :order 2}
   :settings {:run openSettings :group "4_settings"}
   :keyBindings {:run openKeyBindings :group "4_settings" :order 2}
   :theme {:run selectTheme :group "4_settings" :order 3}})

(defmenu edit "The Edit menu."
  {:undo {:run undo :group "0_undo"}
   :redo {:run redo :group "0_undo" :order 2}
   :cut {:run cut :group "0_clipboard"}
   :copy {:run copy :group "0_clipboard" :order 2}
   :paste {:run paste :group "0_clipboard" :order 3}
   :duplicate {:run duplicateLines :group "1_line"}
   :delete {:run deleteLines :group "1_line" :order 2}
   :up {:run moveLinesUp :group "1_line" :order 3}
   :down {:run moveLinesDown :group "1_line" :order 4}
   :join {:run joinLines :group "1_line" :order 5}
   :indent {:run indentLines :group "1_line" :order 6}
   :outdent {:run outdentLines :group "1_line" :order 7}
   :comment {:run toggleComment :group "2_comment"}
   :sort {:run sortLines :group "3_text"}
   :reverse {:run reverseLines :group "3_text" :order 2}
   :unique {:run uniqueLines :group "3_text" :order 3}
   :upper {:run upperCase :group "4_case"}
   :lower {:run lowerCase :group "4_case" :order 2}
   :title {:run titleCase :group "4_case" :order 3}
   :swap {:run swapCase :group "4_case" :order 4}})

(defmenu selection "The Selection menu."
  {:all {:run selectAll :group "0_all"}
   :lines {:run splitIntoLines :group "1_split"}
   :occurrences {:run selectAllOccurrences :group "1_split" :order 2}
   :line {:run expandToLine :group "2_expand"}
   :word {:run expandToWord :group "2_expand" :order 2}})

(defmenu view "The View menu."
  {:palette {:run commandPalette :group "1_palette"}
   :sidebar {:run toggleSidebar :group "2_layout"}
   :panel {:run togglePanel :group "2_layout" :order 2}
   :zen {:run toggleZen :group "2_layout" :order 3}
   :problems {:run showProblems :group "3_panel"}
   :terminal {:run showTerminal :group "3_panel" :order 2}
   :wrap {:run toggleWordWrap :group "4_editor"}
   :ruler {:run toggleRuler :group "4_editor" :order 2}
   :minimap {:run toggleMinimap :group "4_editor" :order 3}
   :unfold {:run unfoldAll :group "4_editor" :order 4}
   :syntax {:run selectSyntax :group "5_look"}
   :theme {:run selectTheme :group "5_look" :order 2}})

(defmenu find "The Find menu."
  {:find {:run openFind :group "1_find"}
   :next {:run findNext :group "1_find" :order 2}
   :previous {:run findPrevious :group "1_find" :order 3}
   :replace {:run openReplace :group "2_replace"}
   :replaceNext {:run replaceOne :group "2_replace" :order 2}
   :replaceAll {:run replaceAllInFile :group "2_replace" :order 3}
   :search {:run findInFile :group "3_search"}
   :files {:run findInFiles :group "4_files"}
   :nextResult {:run nextResult :group "4_files" :order 2}
   :previousResult {:run previousResult :group "4_files" :order 3}})

(defmenu go "The Go menu."
  {:file {:run quickOpen :group "1_file"}
   :symbol {:run goToSymbol :group "1_file" :order 2}
   :projectSymbol {:run goToSymbolInProject :group "1_file" :order 3}
   :line {:run goToLine :group "1_file" :order 4}
   :next {:run nextTab :group "2_tabs"}
   :previous {:run previousTab :group "2_tabs" :order 2}})

(defmenu terminal "The Tools menu."
  {:build {:run build :group "1_run"}
   :run {:run runApp :group "1_run" :order 2}
   :clear {:run clearConsole :group "2_clear"}
   :assistant {:run showAssistant :group "3_assistant"}})

(defmenu project "The Project menu."
  {:symbol {:run goToSymbolInProject :group "1_find"}
   :find {:run findInFiles :group "1_find" :order 2}
   :refresh {:run refreshFolders :group "2_folders"}})

(defmenu window "The Window menu."
  {:next {:run nextTab :group "1_tabs"}
   :previous {:run previousTab :group "1_tabs" :order 2}
   :closeAll {:run closeAll :group "2_close"}})

(defmenu help "The Help menu." {:palette {:run commandPalette :group "1_palette"} :about {:run about :group "2_about"}})

;;; Keys

(defkey "mod+p" quickOpen)

(defkey "mod+shift+p" commandPalette)

(defkey "mod+s" save)

(defkey "mod+alt+s" saveAll)

(defkey "mod+w" closeTab)

(defkey "mod+shift+t" reopenClosed)

(defkey "mod+k mod+b" toggleSidebar)

(defkey "mod+b" build)

(defkey "ctrl+backquote" showTerminal)

(defkey "mod+shift+d" duplicateLines)

(defkey "ctrl+shift+k" {:when ~(= context.platform "mac") :do deleteLines})

(defkey "ctrl+mod+up" moveLinesUp)

(defkey "ctrl+mod+down" moveLinesDown)

(defkey "mod+j" joinLines)

(defkey "mod+enter" insertLineAfter)

(defkey "mod+shift+enter" insertLineBefore)

(defkey "ctrl+t" transpose)

(defkey "f5" sortLines)

(defkey "mod+slash" toggleComment)

(defkey "mod+l" expandToLine)

(defkey "mod+shift+l" splitIntoLines)

(defkey "ctrl+cmd+g" {:when ~(= context.platform "mac") :do selectAllOccurrences})

(defkey "alt+f3" selectAllOccurrences)

(defkey "mod+r" goToSymbol)

(defkey "ctrl+g" goToLine)

(defkey "mod+shift+m" showProblems)

(defkey "mod+shift+b" build)

(defkey "mod+alt+right" nextTab)

(defkey "mod+alt+left" previousTab)

(defkey "mod+f" openFind)

(defkey "mod+alt+f" openReplace)

(defkey "cmd+g" {:when ~(= context.platform "mac") :do findNext})

(defkey "cmd+shift+g" {:when ~(= context.platform "mac") :do findPrevious})

(defkey "f3" findNext)

(defkey "shift+f3" findPrevious)

(defkey "mod+alt+w" toggleWordWrap)

(defkey "mod+shift+r" goToSymbolInProject)

(defkey "mod+shift+f" findInFiles)

(defkey "f4" nextResult)

(defkey "shift+f4" previousResult)

(defkey "escape" closeFind)

(defkey "mod+k mod+t" selectTheme)

(defkey "ctrl+cmd+shift+f" {:when ~(= context.platform "mac") :do toggleZen})

(defkey "shift+f11" toggleZen)
