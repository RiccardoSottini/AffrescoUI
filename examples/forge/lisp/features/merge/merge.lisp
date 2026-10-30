{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's merge conflicts: in a file with git's conflict markers (<<<<<<<, an optional |||||||, =======, >>>>>>>), each side is tinted and a bar over the editor accepts the current change, the incoming one or both for the conflict at the caret (or the first), and moves to the next; the file stays unsaved until Save."
  :computed {:conflictLines "{{computed.activeDoc != null && computed.activeDoc.kind == 'file' && contains(computed.activeDoc.text ?? '', '<<<<<<<') ? split(computed.activeDoc.text, '\\n') : []}}"
    :conflictMarkers "{{filter(map(computed.conflictLines, slice(it, 0, 7) == '<<<<<<<' ? {kind: 'start', line: index} : slice(it, 0, 7) == '|||||||' ? {kind: 'base', line: index} : it == '=======' ? {kind: 'mid', line: index} : slice(it, 0, 7) == '>>>>>>>' ? {kind: 'end', line: index} : null), it != null)}}"
    :conflictBlocks "{{filter(map(computed.conflictMarkers, it.kind != 'start' ? null : (computed.conflictMarkers[index + 1] ?? {}).kind == 'mid' && (computed.conflictMarkers[index + 2] ?? {}).kind == 'end' ? {start: it.line, base: -1, mid: computed.conflictMarkers[index + 1].line, end: computed.conflictMarkers[index + 2].line} : (computed.conflictMarkers[index + 1] ?? {}).kind == 'base' && (computed.conflictMarkers[index + 2] ?? {}).kind == 'mid' && (computed.conflictMarkers[index + 3] ?? {}).kind == 'end' ? {start: it.line, base: computed.conflictMarkers[index + 1].line, mid: computed.conflictMarkers[index + 2].line, end: computed.conflictMarkers[index + 3].line} : null), it != null)}}"
    :conflictAtCaret "{{find(computed.conflictBlocks, (state.caret.line ?? 1) - 1 >= it.start && (state.caret.line ?? 1) - 1 <= it.end) ?? computed.conflictBlocks[0]}}"
    :conflictNext "{{find(computed.conflictBlocks, it.start > (state.caret.line ?? 1) - 1) ?? computed.conflictBlocks[0]}}"
    :conflictSummary "{{some(computed.conflictBlocks, (state.caret.line ?? 1) - 1 >= it.start && (state.caret.line ?? 1) - 1 <= it.end) ? 'Merge conflict ' + count(computed.conflictBlocks, it.start <= (state.caret.line ?? 1) - 1) + ' of ' + length(computed.conflictBlocks) : plural(length(computed.conflictBlocks), 'merge conflict')}}"
    :conflictDecorations "{{filter(flat(map(computed.conflictBlocks, [{line: it.start, kind: 'background', color: '#7f7f7f40'}, {line: it.start + 1, endLine: (it.base >= 0 ? it.base : it.mid) - 1, kind: 'background', color: '#3fa66b40'}, {line: it.base, endLine: it.mid, kind: 'background', color: '#7f7f7f40'}, {line: it.mid, kind: 'background', color: '#7f7f7f40'}, {line: it.mid + 1, endLine: it.end - 1, kind: 'background', color: '#3f86d640'}, {line: it.end, kind: 'background', color: '#7f7f7f40'}])), it.line >= 0 && (it.endLine ?? it.line) >= it.line)}}"}
  :actions {:acceptCurrentChange {:title "Accept Current Change"
      :category "Merge Conflict"
      :icon arrow-up
      :description "Keep the lines above the conflict's =======, the file's own, and drop the markers."
      :enablement "{{computed.conflictAtCaret != null}}"
      :do [{:run acceptConflict :with {:keep current}}]}
    :acceptIncomingChange {:title "Accept Incoming Change"
      :category "Merge Conflict"
      :icon arrow-down
      :description "Keep the lines below the conflict's =======, the ones being merged in, and drop the markers."
      :enablement "{{computed.conflictAtCaret != null}}"
      :do [{:run acceptConflict :with {:keep incoming}}]}
    :acceptBothChanges {:title "Accept Both Changes"
      :category "Merge Conflict"
      :icon list-plus
      :description "Keep both sides, the current one first, and drop the markers."
      :enablement "{{computed.conflictAtCaret != null}}"
      :do [{:run acceptConflict :with {:keep both}}]}
    :nextConflict {:title "Next Conflict"
      :category "Merge Conflict"
      :icon chevron-down
      :enablement "{{computed.conflictNext != null}}"
      :do [{:set state.goTo.line :to "{{computed.conflictNext.start + 1}}"}
        {:set state.goTo.column :to 1}
        {:increment state.goTo.request :by 1}]}
    :acceptConflict [{:note "keep: current, incoming or both"}
      {:if "{{computed.conflictAtCaret != null}}"
        :then [{:run setConflictText
            :with (node #"{{computed.activeDoc.id}}"
              :text "{{join(concat(slice(computed.conflictLines, 0, computed.conflictAtCaret.start), keep != 'incoming' ? slice(computed.conflictLines, computed.conflictAtCaret.start + 1, computed.conflictAtCaret.base >= 0 ? computed.conflictAtCaret.base : computed.conflictAtCaret.mid) : [], keep != 'current' ? slice(computed.conflictLines, computed.conflictAtCaret.mid + 1, computed.conflictAtCaret.end) : [], slice(computed.conflictLines, computed.conflictAtCaret.end + 1)), '\\n')}}")}]}]
    :setConflictText [{:set state.docs :to "{{map(state.docs, it.id == id ? merge(it, {text: text}) : it)}}"}
      {:forEach "{{filter(state.docs, it.id == id)}}"
        :as doc
        :do [{:run markDirty :with (node #"{{id}}" :dirty "{{doc.text != doc.saved || doc.eol != doc.savedEol}}")}]}]}
  :components {:forgeConflictButton {:params {:run "" :text ""}
      :node {:type label
        :role button
        :text "{{text}}"
        :fitText true
        :padding 6
        :height 22
        :radius 3
        :cursor hand
        :textColor @color.accent
        :hover {:color @color.hover}
        :onClick {:run "{{run}}"}}}
    :forgeConflictBar {:node (row #conflict-bar :width "100%" :height 28 :gap 2 :alignItems center :color @color.tabs
        (label #conflict-summary
          :text "{{computed.conflictSummary}}"
          :textColor @color.muted
          :grow 1
          :height 22
          :padding 8)
        (node :use forgeConflictButton #conflict-accept-current :with {:run acceptCurrentChange :text "Accept Current"})
        (node :use forgeConflictButton
          #conflict-accept-incoming
          :with {:run acceptIncomingChange :text "Accept Incoming"})
        (node :use forgeConflictButton #conflict-accept-both :with {:run acceptBothChanges :text "Accept Both"})
        (node :use forgeConflictButton #conflict-next :with {:run nextConflict :text Next}))}}}
