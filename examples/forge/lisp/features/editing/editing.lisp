{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's editing: the find and replace bar over the active editor (mod+f, mod+alt+f), plain, whole word, case-sensitive or a regular expression, in the whole file or in the selection, with the match count; and the editor's settings (word wrap, bracket matching, indentation guides). The editor node (features/editor) reads state.findBar and the settings; the status bar shows Ln and Col from state.caret."
  :state {:findBar {:open false
      :replacing false
      :text ""
      :replace ""
      :regex false
      :wholeWord false
      :caseSensitive false
      :inSelection false
      :step first
      :request 0}}
  :computed {:findQuery "{{state.findBar.open && !empty(state.findBar.text) ? {text: state.findBar.text, regex: state.findBar.regex, wholeWord: state.findBar.wholeWord, caseSensitive: state.findBar.caseSensitive, inSelection: state.findBar.inSelection, replace: state.findBar.replace, step: state.findBar.step, request: state.findBar.request} : null}}"
    :findMatches "{{merge({matches: 0}, state.caret).matches}}"
    :findSummary "{{empty(state.findBar.text) ? '' : computed.findMatches == 0 ? 'No results' : plural(computed.findMatches, 'match', 'matches')}}"}
  :settings {:editor.tabSize {:type integer
      :minimum 1
      :maximum 16
      :default 4
      :title "Tab size"
      :description "Spaces per indentation level: what Tab inserts and an opened file's tabs become."}
    :editor.wordWrap {:type boolean
      :default false
      :title "Word wrap"
      :description "Wrap lines that don't fit the editor's width."}
    :editor.bracketMatching {:type boolean
      :default true
      :title "Bracket matching"
      :description "Highlight the bracket that pairs with the one at the caret."}
    :editor.guides.indentation {:type boolean
      :default true
      :title "Indentation guides"
      :description "Draw a line at each indentation level."}}
  :actions {:openFind {:title Find
      :category Edit
      :icon search
      :do [{:set state.findBar.open :to true} {:set state.findBar.replacing :to false} {:focus "#find-text"}]}
    :openReplace {:title Replace
      :category Edit
      :icon replace
      :do [{:set state.findBar.open :to true} {:set state.findBar.replacing :to true} {:focus "#find-text"}]}
    :closeFind {:title "Close Find"
      :category Edit
      :enablement "{{state.findBar.open}}"
      :do [{:set state.findBar.open :to false}
        {:set state.findBar.inSelection :to false}
        {:after 0 :do [{:focus "#editor"}]}]}
    :findStep [{:set state.findBar.step :to "{{step}}"} {:increment state.findBar.request :by 1}]
    :findNext {:title "Find Next"
      :category Edit
      :enablement "{{state.findBar.open}}"
      :do [{:run findStep :with {:step next}}]}
    :findPrevious {:title "Find Previous"
      :category Edit
      :enablement "{{state.findBar.open}}"
      :do [{:run findStep :with {:step previous}}]}
    :replaceOne {:title Replace
      :category Edit
      :enablement "{{state.findBar.open}}"
      :do [{:run findStep :with {:step replace}}]}
    :replaceAllInFile {:title "Replace All"
      :category Edit
      :enablement "{{state.findBar.open}}"
      :do [{:run findStep :with {:step replaceAll}}]}
    :toggleFindInSelection {:title "Toggle Find in Selection"
      :category Edit
      :do [{:set state.findBar.inSelection :to "{{!state.findBar.inSelection}}"}]}
    :toggleWordWrap {:title "Toggle Word Wrap"
      :category View
      :do [{:set settings.editor.wordWrap :to "{{!settings.editor.wordWrap}}"}]}}
  :keys {:"mod+f" openFind :"mod+alt+f" openReplace :"alt+z" toggleWordWrap}
  :menus {:view {:toggleWordWrap {:run toggleWordWrap :group "4_editor" :order 1}}}
  :components {:forgeFindBar {:node (column #find-bar
        :width "100%"
        :height "{{state.findBar.replacing ? 60 : 30}}"
        :justify center
        :gap 4
        :color @color.tabs
        :keys {:escape closeFind :"shift+enter" findPrevious :"alt+l" toggleFindInSelection}
        (row #find-row :width "100%" :height 26 :gap 6 :alignItems center
          (textarea #find-text
            :bind state.findBar.text
            :grow 1
            :height 26
            :padding 5
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder Find
            :autofocus true
            :onChange {:run findStep :with {:step first}}
            :onSubmit {:run findNext})
          {:use forgeFindOption :with {:option case :bind state.findBar.caseSensitive :text Aa}}
          {:use forgeFindOption :with {:option word :bind state.findBar.wholeWord :text Word}}
          {:use forgeFindOption :with {:option regex :bind state.findBar.regex :text ".*"}}
          {:use forgeFindOption :with {:option selection :bind state.findBar.inSelection :text Sel}}
          (label #find-count :text "{{computed.findSummary}}" :width 80 :textColor @color.muted)
          {:use forgeFindButton :with {:name previous :icon arrow-up :label "Previous Match" :run findPrevious}}
          {:use forgeFindButton :with {:name next :icon arrow-down :label "Next Match" :run findNext}}
          {:use forgeFindButton :with {:name close :icon x :label Close :run closeFind}})
        (node :when "{{state.findBar.replacing}}"
          :type row
          #replace-row
          :width "100%"
          :height 26
          :gap 6
          :alignItems center
          (textarea #replace-text
            :bind state.findBar.replace
            :grow 1
            :height 26
            :padding 5
            :color @color.input
            :textColor @color.text
            :placeholderColor @color.muted
            :placeholder Replace
            :onSubmit {:run replaceOne})
          {:use forgeFindButton :with {:name replace :icon replace :label Replace :run replaceOne}}
          {:use forgeFindButton :with {:name replace-all :icon replace-all :label "Replace All" :run replaceAllInFile}}))}
    :forgeFindOption {:params {:option "" :bind "" :text ""}
      :node (row #"find-{{option}}" :height 26 :gap 4 :alignItems center
        (checkbox #"find-{{option}}-box"
          :bind "{{bind}}"
          :width 16
          :height 16
          :onChange {:run findStep :with {:step first}})
        {:type label :text "{{text}}" :textColor @color.text :fitText true})}
    :forgeFindButton {:params {:name "" :icon "" :label "" :run ""}
      :node (label #"find-{{name}}"
        :role button
        :label "{{label}}"
        :tooltip "{{label}}"
        :icon "{{icon}}"
        :width 24
        :height 24
        :radius 4
        :cursor hand
        :focusable true
        :textColor @color.text
        :hover {:color @color.hover}
        :onClick {:run "{{run}}"})}}}
