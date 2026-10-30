{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's Search view: an editor page (Find in Files, mod+shift+f) that searches the open folder with the search service (search.find, cancellable, one event per file, built into state.search.result), shows the matches as a tree by file, opens a match at its line, and replaces every match after a confirm, one file at a time (search.replace, then files.write). Build output and dependencies (state.search.skip) are never searched; files with unsaved edits are skipped, and the open ones that changed are read again."
  :state {:search {:query ""
      :replace ""
      :regex false
      :wholeWord false
      :caseSensitive false
      :include ""
      :exclude ""
      :result nil
      :ran nil
      :collapsed []
      :selected nil
      :menu false
      :x 0
      :y 0
      :reload []
      :replacing nil
      :skip "build, node_modules, out, target, dist, __pycache__"
      :busy false
      :confirm false
      :error ""}
    :searchEditor {:caret {:line 1 :column 1 :selected 0}}}
  :computed {:searchEditorRows "{{concat([{text: (empty(computed.searchSummary) ? 'No search yet' : computed.searchSummary) + (empty(state.search.query) ? '' : ' for ' + state.search.query), id: ''}, {text: '', id: ''}], reduce(state.search.result ? state.search.result.tree : [], concat(acc, [{text: slice(it.id, 5), id: it.id}], map(it.children ?? [], {text: '  ' + it.label, id: it.id}), [{text: '', id: ''}]), []))}}"
    :searchEditorText "{{join(map(computed.searchEditorRows, it.text), '\n')}}"
    :searchExpanded "{{state.search.result ? filter(map(state.search.result.tree, it.id), !contains(state.search.collapsed, it)) : []}}"
    :searchSummary "{{!empty(state.search.error) ? state.search.error : state.search.result ? (state.search.result.matches == 0 ? 'No results' : plural(state.search.result.matches, 'result') + ' in ' + plural(state.search.result.files, 'file')) + (state.search.result.truncated ? ', capped' : '') + (state.search.result.done ? '' : '...') : ''}}"}
  :actions {:findInFiles {:title "Find in Files"
      :category Search
      :icon search
      :do [{:set state.sideView :to search}
        {:set state.layout.sidebar :to true}
        {:after 0 :do [{:focus "#search-query"}]}]}
    :runSearch [{:cancel forge-search}
      {:set state.search.error :to ""}
      {:set state.search.collapsed :to []}
      {:if "{{empty(trim(state.search.query))}}"
        :then [{:set state.search.result :to nil} {:set state.search.ran :to nil}]
        :else [{:if "{{empty(state.folder.root)}}"
            :then [{:set state.search.result :to nil} {:set state.search.error :to "Open a folder to search it."}]
            :else [{:run startSearch}]}]}]
    :startSearch [{:set state.search.ran
        :to "{{ {pattern: state.search.query, regex: state.search.regex, wholeWord: state.search.wholeWord, caseSensitive: state.search.caseSensitive, include: state.search.include, exclude: state.search.skip + (empty(trim(state.search.exclude)) ? '' : ', ' + state.search.exclude)} }}"}
      {:set state.search.result :to {:tree [] :hits [] :files 0 :matches 0 :skipped 0 :truncated false :done false}}
      (node :invoke search.find
        :with "{{state.search.ran}}"
        #forge-search
        :timeout 600000
        :loading state.search.busy
        :onEvent [{:run searchFound :with {:found "{{event}}"}}]
        :onError [{:if "{{error.code != 'cancelled'}}"
            :then [{:set state.search.result :to nil} {:set state.search.error :to "{{error.message}}"}]}]
        :onDone [{:set state.search.result
            :to "{{merge(state.search.result, {tree: sort(state.search.result.tree, it.id), skipped: result.skipped, truncated: result.truncated, done: true})}}"}
          {:run log
            :with {:text "{{'Search: ' + plural(state.search.result.matches, 'result') + ' in ' + plural(state.search.result.files, 'file')}}"}}])]
    :searchFound [{:note "found: { file, matches: [{ line (from 1), column, length, text, at }] }, one file's matches: a tree item with a child per match, each naming its place in hits"}
      {:set state.search.result
        :to "{{merge(state.search.result, {tree: concat(state.search.result.tree, [{id: 'file:' + found.file, label: reverse(split(found.file, '/'))[0] + (contains(found.file, '/') ? '  ' + slice(found.file, 0, length(found.file) - length(reverse(split(found.file, '/'))[0]) - 1) : '') + '  (' + length(found.matches) + ')', icon: 'file', children: map(found.matches, {id: 'hit:' + (length(state.search.result.hits) + index), label: it.line + ': ' + trim(it.text)})}]), hits: concat(state.search.result.hits, map(found.matches, {file: found.file, label: reverse(split(found.file, '/'))[0], line: it.line, column: it.column})), files: state.search.result.files + 1, matches: state.search.result.matches + length(found.matches)})}}"}]
    :cancelSearch {:title "Cancel Search"
      :category Search
      :icon close
      :enablement "{{state.search.busy}}"
      :do [{:cancel forge-search}]}
    :openSearchHit [{:if "{{slice(event.id ?? '', 0, 4) == 'hit:'}}"
        :then [{:run openSearchMatch :with {:hit "{{state.search.result.hits[number(slice(event.id, 4))]}}"}}]
        :else [{:if "{{slice(event.id ?? '', 0, 5) == 'file:'}}"
            :then [{:run openPath :with {:path "{{slice(event.id, 5)}}" :label "{{reverse(split(event.id, '/'))[0]}}"}}]}]}]
    :openSearchEditor {:title "Open Results in Editor"
      :category Search
      :icon file-text
      :description "Show the search's results as text in an editor tab; Enter or a double click on a line opens it."
      :enablement "{{state.search.result != null}}"
      :do [{:run openPage :with (node #"forge:search-results" :label "Search Results" :kind searchResults)}]}
    :openSearchEditorLine [{:run openSearchRow
        :with {:row "{{computed.searchEditorRows[(state.searchEditor.caret.line ?? 1) - 1] ?? {id: ''} }}"}}]
    :openSearchRow [{:if "{{slice(row.id, 0, 4) == 'hit:'}}"
        :then [{:run openSearchMatch :with {:hit "{{state.search.result.hits[number(slice(row.id, 4))]}}"}}]
        :else [{:if "{{slice(row.id, 0, 5) == 'file:'}}"
            :then [{:run openPath :with {:path "{{slice(row.id, 5)}}" :label "{{reverse(split(row.id, '/'))[0]}}"}}]}]}]
    :searchDismiss {:title Dismiss
      :category Search
      :icon close
      :description "Take the selected file or match out of the results; the files stay as they are."
      :enablement "{{state.search.result != null && state.search.result.done && !empty(state.search.selected ?? '')}}"
      :do [{:if "{{slice(state.search.selected, 0, 5) == 'file:'}}"
          :then [{:run searchDismissFile
              :with {:file "{{find(state.search.result.tree, it.id == state.search.selected)}}"}}]
          :else [{:run searchDismissHit
              :with {:hit "{{state.search.selected}}"
                :file "{{find(state.search.result.tree, some(it.children, it.id == state.search.selected))}}"}}]}
        {:set state.search.selected :to nil}]}
    :searchDismissFile [{:if "{{file != null}}"
        :then [{:set state.search.result
            :to "{{merge(state.search.result, {tree: filter(state.search.result.tree, it.id != file.id), files: state.search.result.files - 1, matches: state.search.result.matches - length(file.children)})}}"}]}]
    :searchDismissHit [{:if "{{file != null && length(file.children) <= 1}}"
        :then [{:run searchDismissFile :with {:file "{{file}}"}}]
        :else [{:if "{{file != null}}"
            :then [{:set state.search.result
                :to "{{merge(state.search.result, {tree: map(state.search.result.tree, it.id != file.id ? it : merge(it, {children: filter(it.children, it.id != hit), label: slice(it.label, 0, length(it.label) - length('(' + length(it.children) + ')')) + '(' + (length(it.children) - 1) + ')'})), matches: state.search.result.matches - 1})}}"}]}]}]
    :openSearchMatch [{:if "{{hit != null}}"
        :then [{:run openPath :with {:path "{{hit.file}}" :label "{{hit.label}}"}}
          {:run goToLine :with {:line "{{hit.line}}"}}]}]
    :askReplace [{:if "{{state.search.result && state.search.result.done && state.search.result.files > 0}}"
        :then [{:set state.search.confirm :to true}]}]
    :replaceAll [{:note "One file at a time (the search service runs 4 calls at once): its text replaced by search.replace, written by files.write; open files with unsaved edits are skipped"}
      {:set state.search.replacing
        :to "{{ {queue: filter(map(state.search.result.tree, slice(it.id, 5)), !contains(computed.dirtyIds, it)), skipped: filter(map(state.search.result.tree, slice(it.id, 5)), contains(computed.dirtyIds, it)), replaced: 0, changed: [], failed: []} }}"}
      {:run replaceNext}]
    :replaceNext [{:if "{{empty(state.search.replacing.queue)}}"
        :then [{:run replaceFinished :with {:done "{{state.search.replacing}}"}}]
        :else [{:run replaceFile :with {:file "{{state.search.replacing.queue[0]}}"}}]}]
    :replaceFile [{:set state.search.replacing.queue :to "{{slice(state.search.replacing.queue, 1)}}"}
      {:invoke search.replace
        :with "{{ {path: file, pattern: state.search.ran.pattern, regex: state.search.ran.regex, wholeWord: state.search.ran.wholeWord, caseSensitive: state.search.ran.caseSensitive, replacement: state.search.replace} }}"
        :onError [{:run replaceFailed :with {:file "{{file}}" :message "{{error.message}}"}}]
        :onDone [{:if "{{result.replacements == 0}}"
            :then [{:run replaceNext}]
            :else [{:run replaceWrite :with {:file "{{file}}" :text "{{result.text}}" :count "{{result.replacements}}"}}]}]}]
    :replaceWrite [{:invoke files.write
        :with {:path "{{file}}" :text "{{text}}"}
        :onError [{:run replaceFailed :with {:file "{{file}}" :message "{{error.message}}"}}]
        :onDone [{:set state.search.replacing.replaced :to "{{state.search.replacing.replaced + count}}"}
          {:set state.search.replacing.changed :to "{{concat(state.search.replacing.changed, [file])}}"}
          {:run replaceNext}]}]
    :replaceFailed [{:set state.search.replacing.failed
        :to "{{concat(state.search.replacing.failed, [{file: file, error: message}])}}"}
      {:run replaceNext}]
    :replaceFinished [{:set state.search.replacing :to nil}
      {:set state.search.reload :to "{{filter(done.changed, contains(computed.docIds, it))}}"}
      {:forEach state.search.reload
        :as file
        :do [{:invoke files.read
            :with {:path "{{file}}"}
            :onDone [{:run reloaded :with (node #"{{file}}" :text "{{result.text}}")}]}]}
      {:run notify
        :with {:severity "{{length(done.failed) > 0 || length(done.skipped) > 0 ? 'warning' : 'info'}}"
          :message "{{'Replaced ' + plural(done.replaced, 'match', 'matches') + ' in ' + plural(length(done.changed), 'file') + (length(done.skipped) > 0 ? ', skipped ' + plural(length(done.skipped), 'unsaved file') : '') + (length(done.failed) > 0 ? ', ' + plural(length(done.failed), 'file') + ' failed' : '')}}"}}
      {:run runSearch}]}
  :keys {:"mod+shift+f" findInFiles}
  :menus {:go {:findInFiles {:run findInFiles :group "2_search" :order 1}}
    :searchResult {:dismiss {:run searchDismiss :group "1_dismiss" :order 1}}}
  :components {:forgeSearchEditor {:node (editor #search-editor
        :label "Search Results"
        :font code
        :width "100%"
        :grow 1
        :text "{{computed.searchEditorText}}"
        :readOnly true
        :lineNumbers false
        :caret state.searchEditor.caret
        :color @color.editor
        :textColor @color.editorText
        :selectionColor @color.selection
        :lineColor @color.hover
        :caretColor @color.text
        :scrollMarks true
        :find "{{empty(state.search.query) || state.search.result == null ? null : {text: state.search.query, regex: state.search.regex, wholeWord: state.search.wholeWord, caseSensitive: state.search.caseSensitive} }}"
        :keys {:enter openSearchEditorLine}
        :onDoubleClick {:run openSearchEditorLine})}
    :forgeSearchPage {:node (column #search-page :width "100%" :grow 1 :gap 4
        {:use forgeViewTitle :with {:text Search}}
        (node :use forgeInset #search-query-row :height 26
          (row #search-query-box :grow 1 :height 26 :color @color.input :alignItems center
            {:use forgeSearchField :with {:field query :hint Search} :onSubmit {:run runSearch}}
            {:use forgeSearchOption :with {:option case :bind caseSensitive :icon case-sensitive :tip "Match Case"}}
            {:use forgeSearchOption :with {:option word :bind wholeWord :icon whole-word :tip "Match Whole Word"}}
            {:use forgeSearchOption :with {:option regex :bind regex :icon regex :tip "Use Regular Expression"}}))
        (node :use forgeInset #search-replace-row :height 26 :alignItems center
          {:use forgeSearchField :with {:field replace :hint Replace}}
          (label #search-replace-all
            :role button
            :label "Replace All"
            :icon replace-all
            :font icons
            :width 26
            :height 26
            :align center
            :cursor hand
            :tooltip "Replace All"
            :textColor @color.text
            :hover {:color @color.hover}
            :onClick {:run askReplace}))
        {:use forgeSearchFilter :with {:title "files to include" :filter include :example "e.g. src/**, *.cpp"}}
        {:use forgeSearchFilter :with {:title "files to exclude" :filter exclude :example "e.g. build, *.md"}}
        {:use forgeInset
          :height 22
          :children [(label #search-summary
              :text "{{state.search.busy ? 'Searching... ' + computed.searchSummary : computed.searchSummary}}"
              :textColor @color.muted
              :grow 1
              :height 22)
            (node :when "{{state.search.result != null}}"
              :type label
              #search-open-editor
              :role button
              :label "Open Results in Editor"
              :icon file-text
              :font icons
              :width 22
              :height 22
              :align center
              :cursor hand
              :radius 3
              :tooltip "Open Results in Editor"
              :textColor @color.text
              :hover {:color @color.hover}
              :onClick {:run openSearchEditor})]}
        (tree #search-results
          :when "{{state.search.result != null}}"
          :width "100%"
          :grow 1
          :items "{{state.search.result.tree}}"
          :expanded "{{computed.searchExpanded}}"
          :bind state.search.selected
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onExpand [{:set state.search.collapsed
              :to "{{event.expanded ? filter(state.search.collapsed, it != event.id) : concat(state.search.collapsed, [event.id])}}"}]
          :onOpen {:run openSearchHit}
          :keys {:delete searchDismiss :backspace searchDismiss}
          :onContextMenu [{:set state.search.x :to "{{event.x}}"}
            {:set state.search.y :to "{{event.y}}"}
            {:set state.search.menu :to true}])
        (node :use contextMenu
          #search-menu
          :with {:open state.search.menu
            :items "{{menus.searchResult}}"
            :x "{{state.search.x}}"
            :y "{{state.search.y}}"})
        {:use dialog
          :with {:open state.search.confirm
            :title "Replace All"
            :message "{{state.search.result ? 'Replace ' + plural(state.search.result.matches, 'match', 'matches') + ' in ' + plural(state.search.result.files, 'file') + ' with \"' + state.search.replace + '\"? Files with unsaved edits are skipped.' : ''}}"
            :buttons [{:title Replace :run replaceAll} {:title Cancel}]}})}
    :forgeInset {:node {:type row :width "100%" :children [{:width 12} {:slot true} {:width 12}]}}
    :forgeSearchFilter {:params {:title "" :filter "" :example ""}
      :node {:use forgeInset
        :height 44
        :children [{:type column
            :grow 1
            :height 44
            :children [{:type label :text "{{title}}" :textColor @color.muted :width "100%" :height 18}
              {:use forgeSearchField
                :with {:field "{{filter}}" :hint "{{example}}"}
                :width "100%"
                :grow 0
                :onSubmit {:run runSearch}}]}]}}
    :forgeSearchField {:params {:field "" :hint ""}
      :node (textarea #"search-{{field}}"
        :bind "state.search.{{field}}"
        :grow 1
        :height 26
        :padding 5
        :color @color.input
        :textColor @color.text
        :placeholderColor @color.muted
        :placeholder "{{hint}}")}
    :forgeSearchOption {:params {:option "" :bind "" :icon "" :tip ""}
      :node (label #"search-{{option}}"
        :role checkbox
        :label "{{tip}}"
        :checked "{{state.search[bind]}}"
        :icon "{{icon}}"
        :font icons
        :width 22
        :height 22
        :align center
        :cursor hand
        :tooltip "{{tip}}"
        :radius 3
        :textColor @color.text
        :borderWidth 1
        :borderColor "{{state.search[bind] ? '@color.accent' : '@color.input'}}"
        :color "{{state.search[bind] ? '@color.selection' : '@color.input'}}"
        :hover {:color @color.hover}
        :onClick {:toggle "state.search.{{bind}}"})}}}
