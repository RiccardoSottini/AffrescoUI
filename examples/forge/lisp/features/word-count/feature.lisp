{:format affrescoui-feature
  :version 2
  :name wordCount
  :release "1.0.0"
  :title "Word count"
  :description "Shows the words (or characters) of the text being edited in the status bar; click to switch."
  :license Apache-2.0
  :note "RFC 0004's worked example, fitted to Forge: its toggle goes in the View menu (Forge has no status bar menu)."
  :requires {:format 2 :points [statusBar.right] :exports {:editor.text {:type string}}}
  :provides {:state {:characters false}
    :computed {:words "{{length(filter(split(join(split(app.editor.text, '\\n'), ' '), ' '), length(trim(it)) > 0))}}"
      :characters "{{length(app.editor.text)}}"}
    :actions {:toggle {:title "Toggle Word or Character Count"
        :category "Word Count"
        :do {:toggle state.wordCount.characters}}}
    :menus {:view {:items {:toggle {}}}}
    :views {:statusBar.right {:wordCount-label {:type label
          :padding 6
          :fitText true
          :textColor @color.statusText
          :cursor hand
          :text "{{state.wordCount.characters ? plural(computed.wordCount.characters, 'character') : plural(computed.wordCount.words, 'word')}}"
          :onClick wordCount.toggle}}}}}
