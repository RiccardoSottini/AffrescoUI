{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's workbench extras: the secondary side bar with the Outline (the active file's symbols from the language server, else its headings or the lines that open a block), moving tabs between editor groups (by dragging, or Move Editor into Next or Previous Group), the breadcrumbs over the editor, the recently opened files (first in Go to File, kept by persist), the minimap setting, and the settings and keyboard shortcuts as JSON. The editor groups (features/editor) use forgeBreadcrumbs and the pages; the shell (features/shell) has state.layout.secondary."
  :state {:outline [] :outlineSymbols "" :settingsJson "" :settingsJsonError ""}
  :computed {:outlineItems "{{map(state.outline, {id: text(it.line) + ':' + index, label: slice('                ', 0, 4 * min(it.depth, 4)) + it.label, icon: it.kind == null ? (it.depth > 0 ? 'chevron-right' : 'box') : contains(['function', 'method', 'constructor'], it.kind) ? 'parentheses' : contains(['class', 'struct', 'interface'], it.kind) ? 'component' : contains(['namespace', 'module', 'package'], it.kind) ? 'braces' : contains(['variable', 'field', 'property', 'constant'], it.kind) ? 'variable' : contains(['enum', 'enumMember'], it.kind) ? 'list-tree' : 'hash'})}}"
    :symbolTrail "{{filter(reduce(filter(state.outline, it.line <= state.caret.line), concat(slice(concat(acc, [null, null, null, null, null, null, null, null]), 0, min(it.depth, 7)), [it]), []), it != null)}}"
    :keybindingsJson "{{json(map(keybindings, {key: it.key, command: it.command, when: it.when ?? ''}))}}"}
  :settings {:editor.minimap.enabled {:type boolean
      :default false
      :title Minimap
      :description "Show a narrow overview of the file at the editor's right edge; press or drag it to scroll."}
    :breadcrumbs.enabled {:type boolean
      :default true
      :title Breadcrumbs
      :description "Show the file's folders and the symbol at the caret above the editor."}
    :diffEditor.renderSideBySide {:type boolean
      :default true
      :title "Diff side by side"
      :description "Compare a changed file side by side; off, inline (the deleted lines above the inserted ones)."}}
  :actions {:toggleSecondarySidebar {:title "Toggle Outline Bar"
      :category View
      :icon panel-right
      :description "Show or hide the side bar at the right, with the Outline of the active file."
      :do [{:toggle state.layout.secondary} {:run refreshOutline}]}
    :refreshOutline {:title "Refresh Outline"
      :category Outline
      :icon refresh-cw
      :do [{:if "{{computed.activeDoc.kind == 'file'}}"
          :then [{:if "{{computed.lspLanguage != null}}"
              :then [{:run lspAsk :with {:kind outline :query "" :newName "" :line nil :column nil}}]}
            {:if "{{state.outlineSymbols != computed.activeDoc.id}}"
              :then [{:run outlineOf
                  :with {:rows "{{computed.activeDoc.language == 'Markdown' ? [] : map(slice(split(computed.activeDoc.text, '\\n'), 0, 20000), {line: index + 1, row: it, code: trim(it)})}}"
                    :headings "{{computed.activeDoc.language == 'Markdown' ? filter(map(slice(split(computed.activeDoc.text, '\\n'), 0, 20000), {line: index + 1, row: it}), indexOf(it.row, '#') == 0) : []}}"
                    :python "{{computed.activeDoc.language == 'Python'}}"}}]}]
          :else [{:set state.outline :to []} {:set state.outlineSymbols :to ""}]}]}
    :outlineOf [{:note "rows: a file's lines (line, row, code: the row trimmed); headings: a Markdown file's lines that start with #; python: whether rows are Python's. The outline: the headings, Python's classes and functions by their indentation, or the lines of code that open a block, at the first or second level"}
      {:set state.outline
        :to "{{slice(concat(map(filter(headings, contains(['# ', '## ', '### ', '#### '], slice(it.row, 0, indexOf(it.row, ' ') + 1))), {line: it.line, depth: indexOf(it.row, ' ') - 1, label: slice(trim(slice(it.row, indexOf(it.row, ' '))), 0, 120)}), map(map(filter(rows, python && (slice(it.code, 0, 4) == 'def ' || slice(it.code, 0, 6) == 'class ' || slice(it.code, 0, 10) == 'async def ')), {line: it.line, indent: indexOf(it.row, it.code), tab: slice(it.row, 0, 1) == '\t', kind: slice(it.code, 0, 6) == 'class ' ? 'class' : 'function', rest: trim(slice(it.code, slice(it.code, 0, 10) == 'async def ' ? 10 : slice(it.code, 0, 4) == 'def ' ? 4 : 6))}), {line: it.line, depth: min(it.tab ? it.indent : it.indent >= 12 ? 3 : it.indent >= 8 ? 2 : it.indent >= 1 ? 1 : 0, 3), kind: it.kind, label: slice(slice(it.rest, 0, indexOf(it.rest, '(') > 0 ? indexOf(it.rest, '(') : indexOf(it.rest, ':') > 0 ? indexOf(it.rest, ':') : length(it.rest)), 0, 120)}), map(filter(map(filter(rows, !python && slice(it.code, -1) == '{' && !contains(['}', '/', '#'], slice(it.code, 0, 1))), {line: it.line, indent: indexOf(it.row, it.code), tab: slice(it.row, 0, 1) == '\t', label: slice(trim(slice(it.code, 0, -1)), 0, 120)}), !empty(it.label) && (it.indent == 0 || (it.tab ? it.indent == 1 : it.indent <= 4))), {line: it.line, depth: it.indent == 0 ? 0 : 1, label: it.label, kind: contains(['class ', 'struct', 'interf'], slice(it.label, 0, 6)) ? 'class' : slice(it.label, 0, 10) == 'namespace ' ? 'namespace' : contains(it.label, '(') && !contains(['if ', 'if(', 'for', 'whi', 'swi'], slice(it.label, 0, 3)) ? 'function' : null})), 0, 2000)}}"}]
    :outlinePicked [{:if "{{number(split(event.value, ':')[0]) > 0}}"
        :then [{:run goToLine :with {:line "{{number(split(event.value, ':')[0])}}"}}]}]
    :toggleMinimap {:title "Toggle Minimap"
      :category View
      :icon map
      :do [{:set settings.editor.minimap.enabled :to "{{!settings.editor.minimap.enabled}}"}]}
    :toggleDiffInline {:title "Toggle Inline View"
      :category Compare
      :icon columns
      :do [{:set settings.diffEditor.renderSideBySide :to "{{!settings.diffEditor.renderSideBySide}}"}]}
    :toggleBreadcrumbs {:title "Toggle Breadcrumbs"
      :category View
      :icon chevrons-right
      :do [{:set settings.breadcrumbs.enabled :to "{{!settings.breadcrumbs.enabled}}"}]}
    :revealFolder [{:note "path: a folder's path. Opens the explorer with that folder and its parents open"}
      {:run explorerShow :with (node #"{{path}}")}
      {:if "{{!contains(state.explorer.expanded, path)}}"
        :then [{:set state.explorer.expanded :to "{{concat(state.explorer.expanded, [path])}}"}
          {:run explorerList :with {:folder "{{path}}"}}]}
      {:set state.sideView :to explorer}
      {:set state.layout.sidebar :to true}]
    :rememberRecent [{:note "id, label: a file just shown. Puts it first in the recently opened files (Go to File lists them first)"}
      {:set state.quickPick.recent
        :to "{{slice(concat([{id: id, title: label, detail: join(slice(split(id, '/'), 0, length(split(id, '/')) - 1), '/')}], filter(state.quickPick.recent ?? [], it.id != id)), 0, 10)}}"}
      {:run refreshOutline}]
    :openRecent {:title "Open Recent..."
      :category File
      :icon history
      :description "Go to File, with the recently opened files first."
      :do [{:run quickOpen}]}
    :clearRecent {:title "Clear Recently Opened"
      :category File
      :icon trash
      :enablement "{{length(state.quickPick.recent ?? []) > 0}}"
      :do [{:set state.quickPick.recent :to []}]}
    :moveTabToGroup [{:note "id, to, at: a tab dropped on group to's strip at place at. It leaves the group it was in (closed if empty)"}
      {:if "{{some(state.groups, index != to && contains(map(it.tabs, it.id), id))}}"
        :then [{:set "state.groups[to].tabs"
            :to "{{concat(slice(filter(state.groups[to].tabs, it.id != id), 0, at), [find(find(state.groups, index != to && contains(map(it.tabs, it.id), id)).tabs, it.id == id)], slice(filter(state.groups[to].tabs, it.id != id), at))}}"}
          {:set "state.groups[to].active" :to "{{id}}"}
          {:run closeTab
            :with (node :gi "{{indexOf(map(state.groups, index != to && contains(map(it.tabs, it.id), id)), true)}}"
              #"{{id}}")}
          {:set state.activeGroup :to "{{indexOf(map(state.groups, it.active == id), true)}}"}]}]
    :reorderTab [{:note "gi, from, to: a tab dragged along its strip"}
      {:set "state.groups[gi].tabs"
        :to "{{concat(slice(filter(state.groups[gi].tabs, index != from), 0, to), [state.groups[gi].tabs[from]], slice(filter(state.groups[gi].tabs, index != from), to))}}"}]
    :moveEditorToNextGroup {:title "Move Editor into Next Group"
      :category View
      :icon arrow-right
      :description "Move the active editor into the group at its right, splitting if there is none."
      :enablement "{{!empty(state.groups[state.activeGroup].active) && (state.activeGroup < length(state.groups) - 1 || length(state.groups[state.activeGroup].tabs) > 1)}}"
      :do [{:if "{{state.activeGroup == length(state.groups) - 1}}"
          :then [{:push state.groups :value {:tabs [] :active nil}}]}
        {:run moveTabToGroup
          :with (node #"{{state.groups[state.activeGroup].active}}" :to "{{state.activeGroup + 1}}" :at 1000)}]}
    :moveEditorToPreviousGroup {:title "Move Editor into Previous Group"
      :category View
      :icon arrow-left
      :enablement "{{!empty(state.groups[state.activeGroup].active) && state.activeGroup > 0}}"
      :do [{:run moveTabToGroup
          :with (node #"{{state.groups[state.activeGroup].active}}" :to "{{state.activeGroup - 1}}" :at 1000)}]}
    :focusFirstGroup {:title "Focus First Editor Group" :category View :do [{:set state.activeGroup :to 0}]}
    :focusSecondGroup {:title "Focus Second Editor Group"
      :category View
      :enablement "{{length(state.groups) > 1}}"
      :do [{:set state.activeGroup :to 1}]}
    :pinEditor {:title "Pin Editor"
      :category View
      :icon pin
      :description "Keep the active editor at the start of its group, without a close button; Close Other Editors leaves it open."
      :enablement "{{some(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active && !it.sticky)}}"
      :do [{:set "state.groups[state.activeGroup].tabs"
          :to "{{concat(map(filter(state.groups[state.activeGroup].tabs, it.sticky || it.id == state.groups[state.activeGroup].active), merge(it, {sticky: true, preview: false})), filter(state.groups[state.activeGroup].tabs, !it.sticky && it.id != state.groups[state.activeGroup].active))}}"}]}
    :unpinEditor {:title "Unpin Editor"
      :category View
      :icon pin-off
      :enablement "{{some(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active && it.sticky)}}"
      :do [{:set "state.groups[state.activeGroup].tabs"
          :to "{{concat(filter(state.groups[state.activeGroup].tabs, it.sticky && it.id != state.groups[state.activeGroup].active), map(filter(state.groups[state.activeGroup].tabs, it.id == state.groups[state.activeGroup].active), merge(it, {sticky: false})), filter(state.groups[state.activeGroup].tabs, !it.sticky && it.id != state.groups[state.activeGroup].active))}}"}]}
    :closeOtherEditors {:title "Close Other Editors"
      :category View
      :description "Close the active group's editors but the active one and the pinned ones."
      :enablement "{{some(state.groups[state.activeGroup].tabs, it.id != state.groups[state.activeGroup].active && !it.sticky)}}"
      :do [{:forEach "{{filter(state.groups[state.activeGroup].tabs, it.id != state.groups[state.activeGroup].active && !it.sticky)}}"
          :as tab
          :do [{:run closeTab :with (node :gi "{{state.activeGroup}}" #"{{tab.id}}")}]}]}
    :openSettingsJson {:title "Open Settings (JSON)"
      :category Preferences
      :icon braces
      :description "The settings you changed, as JSON: edit them and Apply."
      :do [{:set state.settingsJson :to "{{json(state.settings ?? {})}}"}
        {:set state.settingsJsonError :to ""}
        {:run openPage :with (node #"forge:settings.json" :label settings.json :kind settingsJson)}]}
    :applySettingsJson {:title "Apply Settings (JSON)"
      :category Preferences
      :icon check
      :do [{:if "{{type(parseJson(state.settingsJson)) == 'object'}}"
          :then [{:set state.settings :to "{{parseJson(state.settingsJson)}}"} {:set state.settingsJsonError :to ""}]
          :else [{:set state.settingsJsonError :to "Not applied: the text isn't a JSON object of settings."}]}]}
    :openKeybindingsJson {:title "Open Keyboard Shortcuts (JSON)"
      :category Preferences
      :icon braces
      :description "The keys every feature contributes, as JSON (read-only: a feature's keys section changes them)."
      :do [{:run openPage :with (node #"forge:keybindings.json" :label keybindings.json :kind keybindingsJson)}]}}
  :watch {:state.caret [{:debounce 300 :do [{:run refreshOutline}]}]}
  :keys {:"mod+alt+b" toggleSecondarySidebar
    :"mod+ctrl+right" moveEditorToNextGroup
    :"mod+ctrl+left" moveEditorToPreviousGroup
    :"ctrl+r" openRecent
    :"mod+1" focusFirstGroup
    :"mod+2" focusSecondGroup
    :"mod+k shift+enter" pinEditor
    :"mod+k mod+shift+enter" unpinEditor
    :"mod+k u" closeOtherEditors}
  :menus {:file {:openRecent {:run openRecent :group "1_open" :order 3}
      :clearRecent {:run clearRecent :group "1_open" :order 4}}
    :view {:secondary {:run toggleSecondarySidebar :group "2_layout" :order 2}
      :moveNext {:run moveEditorToNextGroup :group "3_groups" :order 1}
      :movePrevious {:run moveEditorToPreviousGroup :group "3_groups" :order 2}
      :pin {:run pinEditor :group "3_groups" :order 3}
      :unpin {:run unpinEditor :group "3_groups" :order 4}
      :closeOthers {:run closeOtherEditors :group "3_groups" :order 5}
      :minimap {:run toggleMinimap :group "4_editor" :order 2}
      :breadcrumbs {:run toggleBreadcrumbs :group "4_editor" :order 3}}
    :preferences {:settingsJson {:run openSettingsJson :group "1_settings" :order 3}
      :keybindingsJson {:run openKeybindingsJson :group "1_settings" :order 4}}}
  :components {:forgeSecondarySideBar {:node (column #secondary-side-bar
        :role complementary
        :label "Outline Bar"
        :width 240
        :height "100%"
        :color @color.sideBar
        :borderWidth 1
        :borderColor @color.border
        (row #outline-head :width "100%" :height 32 :alignItems center
          (label #outline-title :text Outline :textColor @color.muted :grow 1 :height 32 :padding 12)
          (label #outline-refresh
            :role button
            :label "Refresh Outline"
            :icon refresh-cw
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Refresh Outline"
            :hover {:color @color.hover}
            :onClick {:run refreshOutline})
          (label #outline-close
            :role button
            :label "Hide Outline Bar"
            :icon x
            :padding 6
            :fitText true
            :cursor hand
            :textColor @color.muted
            :tooltip "Hide (Cmd+Alt+B)"
            :hover {:color @color.hover}
            :onClick {:run toggleSecondarySidebar}))
        (label #outline-empty
          :when "{{empty(state.outline)}}"
          :width "100%"
          :padding 12
          :wrap true
          :textColor @color.muted
          :text "The active editor has no symbols to outline.")
        (list #outline
          :label Outline
          :when "{{!empty(state.outline)}}"
          :width "100%"
          :grow 1
          :items "{{computed.outlineItems}}"
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onChange {:run outlinePicked}))}
    :forgeBreadcrumbs {:params {:path ""}
      :node (row #breadcrumbs
        :role navigation
        :label Breadcrumbs
        :width "100%"
        :height 22
        :alignItems center
        :color @color.editor
        :gap 2
        (node #breadcrumbs-start :width 10)
        (label #breadcrumb
          :repeat {:for segment :in "{{split(path, '/')}}" :index si}
          :role button
          :label "{{segment}}"
          :text "{{segment}}"
          :icon "{{si > 0 ? 'chevron-right' : ''}}"
          :fitText true
          :padding 2
          :cursor hand
          :textColor @color.muted
          :hover {:textColor @color.text}
          :onClick [{:if "{{si < length(split(path, '/')) - 1}}"
              :then [{:run revealFolder :with {:path "{{join(slice(split(path, '/'), 0, si + 1), '/')}}"}}]
              :else [{:run goToSymbol}]}])
        (label #breadcrumb-symbol
          :repeat {:for symbol :in "{{computed.symbolTrail}}"}
          :role button
          :label "{{trim(symbol.label)}}"
          :text "{{trim(symbol.label)}}"
          :icon chevron-right
          :fitText true
          :padding 2
          :cursor hand
          :textColor @color.muted
          :hover {:textColor @color.text}
          :onClick {:run goToSymbol}))}
    :forgeSettingsJsonPage {:node (column #settings-json-page :width "100%" :grow 1 :color @color.editor
        (row #settings-json-head :width "100%" :height 30 :alignItems center :gap 8
          (label #settings-json-hint
            :text "The settings you changed; the others keep their defaults."
            :textColor @color.muted
            :padding 8
            :grow 1)
          (label #settings-json-error
            :when "{{!empty(state.settingsJsonError)}}"
            :text "{{state.settingsJsonError}}"
            :textColor @color.error
            :fitText true)
          (label #settings-json-apply
            :role button
            :label Apply
            :text Apply
            :icon check
            :padding 6
            :fitText true
            :radius 2
            :cursor hand
            :color @color.accent
            :textColor "#ffffff"
            :focusable true
            :onClick {:run applySettingsJson})
          (node #settings-json-end :width 4))
        (editor #settings-json
          :font code
          :width "100%"
          :grow 1
          :bind state.settingsJson
          :grammar json
          :lineNumbers true
          :color @color.editor
          :textColor @color.editorText
          :lineNumberColor @color.muted
          :selectionColor @color.selection
          :caretColor @color.text
          :keys {:"mod+enter" applySettingsJson}))}
    :forgeKeybindingsJsonPage {:node (editor #keybindings-json
        :font code
        :width "100%"
        :grow 1
        :readOnly true
        :text "{{computed.keybindingsJson}}"
        :grammar json
        :lineNumbers true
        :color @color.editor
        :textColor @color.editorText
        :lineNumberColor @color.muted
        :selectionColor @color.selection)}}}
