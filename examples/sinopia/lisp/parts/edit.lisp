{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Editing the deck: every change first keeps the deck as it was (commit), so Undo and Redo step through up to 50 of them. Objects are changed on the current slide, the selected ones (the canvas's selection, which holds object ids); new slides take their layout's placeholders, empty, and new ids from deck.next."
  :state {:undo [] :redo [] :clip [] :editing nil}
  :computed {:slide "{{state.deck.slides[state.current] ?? state.deck.slides[0]}}"
    :objects "{{computed.slide.objects ?? []}}"
    :selected "{{filter(computed.objects, contains(state.board.selection, it.id))}}"
    :first "{{computed.selected[0] ?? null}}"
    :firstPlaceholder "{{find(computed.layout.ph, it.ph == computed.first.ph) ?? {}}}"
    :textSize "{{computed.first == null ? 18 : computed.first.size ?? computed.firstPlaceholder.size ?? 18}}"
    :sizes [8 9 10 10.5 11 12 14 16 18 20 24 28 32 36 40 44 48 54 60 66 72 80 88 96]
    :allOn "{{ {bold: every(computed.selected, it.bold ?? false), italic: every(computed.selected, it.italic ?? false), underline: every(computed.selected, it.underline ?? false), strike: every(computed.selected, it.strike ?? false), shadow: every(computed.selected, it.shadow ?? false), bullets: every(computed.selected, it.bullets ?? false)} }}"
    :animation "{{find(computed.slide.animations ?? [], contains(state.board.selection, it.target))}}"
    :locked "{{length(computed.selected) > 0 && every(computed.selected, it.locked ?? false)}}"
    :phNames "{{map(computed.layout.ph ?? [], it.ph)}}"
    :boxes "{{map(computed.selected, merge({ x: 0, y: 0, w: 100, h: 100 }, contains(computed.phNames, it.ph) ? computed.layout.ph[indexOf(computed.phNames, it.ph)] : {}, it))}}"
    :slideBounds {:left 0 :top 0 :right 960 :bottom 540}
    :selectionBounds "{{length(computed.boxes) == 0 ? computed.slideBounds : { left: min(map(computed.boxes, it.x)), top: min(map(computed.boxes, it.y)), right: max(map(computed.boxes, it.x + it.w)), bottom: max(map(computed.boxes, it.y + it.h)) } }}"
    :selectedAt "{{filter(range(length(computed.objects)), contains(state.board.selection, computed.objects[it].id))}}"
    :arrangeItems "{{concat(map([['Bring to Front', 'front', 'bring-to-front'], ['Bring Forward', 'forward', 'arrow-up'], ['Send Backward', 'backward', 'arrow-down'], ['Send to Back', 'back', 'send-to-back']], { title: it[0], icon: it[2], run: 'order', with: { how: it[1] }, enabled: length(computed.selected) > 0 }), [{ separator: true }], map([['Align Left', 'left', 'align-start-vertical'], ['Align Center', 'center', 'align-center-vertical'], ['Align Right', 'right', 'align-end-vertical'], ['Align Top', 'top', 'align-start-horizontal'], ['Align Middle', 'middle', 'align-center-horizontal'], ['Align Bottom', 'bottom', 'align-end-horizontal']], { title: it[0], icon: it[2], run: 'align', with: { how: it[1] }, enabled: length(computed.selected) > 0 }), [{ separator: true }], map([['Distribute Horizontally', 'x', 'align-horizontal-distribute-center'], ['Distribute Vertically', 'y', 'align-vertical-distribute-center']], { title: it[0], icon: it[2], run: 'distribute', with: { axis: it[1] }, enabled: length(computed.selected) > 1 }))}}"
    :effectItems "{{map([['No Effect', null], ['Shadow', 'shadow'], ['Glow', 'glow']], { title: it[0], run: 'setObjects', with: { props: { effect: it[1] } }, enabled: length(computed.selected) > 0 })}}"
    :canUndo "{{length(state.undo) > 0}}"
    :canRedo "{{length(state.redo) > 0}}"}
  :actions {:editText [{:note "with: id, the text box or shape whose text is edited on the slide, in an editor over it, until Escape or a click elsewhere"}
      {:if "{{contains(['text', 'shape'], find(computed.objects, it.id == id).kind ?? '')}}"
        :then [{:run commit} {:set state.board.selection :to ["{{id}}"]} {:set state.editing :to "{{id}}"}]}]
    :stopEditing [{:set state.editing :to nil} {:focus "#board"}]
    :commit [{:push state.undo :value "{{ {deck: state.deck, current: state.current} }}"}
      {:if "{{length(state.undo) > 50}}" :then [{:remove state.undo :at 0}]}
      {:set state.redo :to []}]
    :undo [{:if "{{computed.canUndo}}"
        :then [{:push state.redo :value "{{ {deck: state.deck, current: state.current} }}"}
          {:set state.deck :to "{{state.undo[length(state.undo) - 1].deck}}"}
          {:set state.current :to "{{state.undo[length(state.undo) - 1].current}}"}
          {:remove state.undo :at "{{length(state.undo) - 1}}"}
          {:set state.board.selection :to []}]}]
    :redo [{:if "{{computed.canRedo}}"
        :then [{:push state.undo :value "{{ {deck: state.deck, current: state.current} }}"}
          {:set state.deck :to "{{state.redo[length(state.redo) - 1].deck}}"}
          {:set state.current :to "{{state.redo[length(state.redo) - 1].current}}"}
          {:remove state.redo :at "{{length(state.redo) - 1}}"}
          {:set state.board.selection :to []}]}]
    :pickSlide [{:set state.current :to "{{clamp(index, 0, length(state.deck.slides) - 1)}}"}
      {:set state.board.selection :to []}
      {:set state.editing :to nil}]
    :newSlide [{:run commit}
      {:set state.deck.slides
        :to "{{concat(slice(state.deck.slides, 0, state.current + 1), [{ id: 's' + state.deck.next, layout: layout, notes: '', transition: { kind: 'none', ms: 500 }, animations: [], objects: map(find(computed.layouts, it.id == layout).ph, { id: 'o' + (state.deck.next + 1 + index), kind: 'text', ph: it.ph, text: '' }) }], slice(state.deck.slides, state.current + 1))}}"}
      {:increment state.deck.next :by 20}
      {:set state.current :to "{{state.current + 1}}"}
      {:set state.board.selection :to []}]
    :duplicateSlide [{:run commit}
      {:set state.deck.slides
        :to "{{concat(slice(state.deck.slides, 0, state.current + 1), [merge(computed.slide, { id: 's' + state.deck.next, objects: map(computed.objects, merge(it, { id: 'o' + (state.deck.next + 1 + index) })), animations: [] })], slice(state.deck.slides, state.current + 1))}}"}
      {:increment state.deck.next :by "{{length(computed.objects) + 2}}"}
      {:set state.current :to "{{state.current + 1}}"}
      {:set state.board.selection :to []}]
    :deleteSlide [{:if "{{length(state.deck.slides) > 1}}"
        :then [{:run commit}
          {:remove state.deck.slides :at "{{state.current}}"}
          {:set state.current :to "{{min(state.current, length(state.deck.slides) - 1)}}"}
          {:set state.board.selection :to []}]}]
    :moveSlide [{:if "{{from >= 0 && to >= 0 && from != to && from < length(state.deck.slides) && to < length(state.deck.slides)}}"
        :then [{:run commit}
          {:set state.deck.slides
            :to "{{from < to ? concat(slice(state.deck.slides, 0, from), slice(state.deck.slides, from + 1, to + 1), [state.deck.slides[from]], slice(state.deck.slides, to + 1)) : concat(slice(state.deck.slides, 0, to), [state.deck.slides[from]], slice(state.deck.slides, to, from), slice(state.deck.slides, from + 1))}}"}
          {:set state.current :to "{{to}}"}]}]
    :hideSlide [{:run commit}
      {:set "state.deck.slides[state.current].hidden" :to "{{!(computed.slide.hidden ?? false)}}"}]
    :setLayout [{:run commit}
      {:set "state.deck.slides[state.current]"
        :to "{{merge(computed.slide, { layout: layout, objects: concat(filter(computed.objects, it.ph == null || contains(map(find(computed.layouts, it.id == layout).ph, it.ph), it.ph)), map(filter(find(computed.layouts, it.id == layout).ph, !contains(map(computed.objects, it.ph), it.ph)), { id: 'o' + (state.deck.next + index), kind: 'text', ph: it.ph, text: '' })) })}}"}
      {:increment state.deck.next :by 10}]
    :resetSlide [{:run commit}
      {:set "state.deck.slides[state.current].objects"
        :to "{{map(computed.objects, it.ph == null ? it : { id: it.id, kind: it.kind, ph: it.ph, text: it.text })}}"}]
    :addSection [{:run commit}
      {:set state.deck.sections
        :to "{{sort(concat(filter(state.deck.sections ?? [], it.first != state.current), [{ id: 'x' + state.deck.next, name: 'Untitled Section', first: state.current }]), 'first')}}"}
      {:increment state.deck.next :by 1}]
    :setObjects [{:if "{{length(computed.selected) > 0}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].objects"
            :to "{{map(computed.objects, contains(state.board.selection, it.id) ? merge(it, props) : it)}}"}]}]
    :toggleText [{:run setObjects
        :with {:props "{{parseJson('{' + json(key) + ':' + json(!computed.allOn[key]) + '}')}}"}}]
    :growText [{:run setObjects
        :with {:props "{{ {size: by > 0 ? find(computed.sizes, it > computed.textSize) ?? computed.textSize + 8 : find(reverse(computed.sizes), it < computed.textSize) ?? max(computed.textSize - 1, 1)} }}"}}]
    :clearFormatting [{:run setObjects
        :with {:props {:bold nil
            :italic nil
            :underline nil
            :strike nil
            :shadow nil
            :size nil
            :color nil
            :highlight nil
            :font nil}}}]
    :changeCase [{:if "{{length(computed.selected) > 0}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].objects"
            :to "{{map(computed.objects, contains(state.board.selection, it.id) ? merge(it, { text: how == 'upper' ? upper(it.text ?? '') : how == 'lower' ? lower(it.text ?? '') : how == 'toggle' ? join(map(split(it.text ?? '', ''), upper(it) == it ? lower(it) : upper(it)), '') : how == 'sentence' ? join(map(split(it.text ?? '', '\n'), upper(join(slice(split(it, ''), 0, indexOf(it, trim(it)) + 1), '')) + lower(join(slice(split(it, ''), indexOf(it, trim(it)) + 1), ''))), '\n') : how == 'words' ? join(map(split(it.text ?? '', '\n'), join(map(split(it, ' '), upper(join(slice(split(it, ''), 0, 1), '')) + lower(join(slice(split(it, ''), 1), ''))), ' ')), '\n') : it.text }) : it)}}"}]}]
    :placeObjects [{:run commit}
      {:forEach moved
        :as m
        :do [{:run placeObject :with {:m "{{m}}" :k "{{indexOf(map(computed.objects, it.id), m.id)}}"}}]}]
    :placeObject [{:if "{{k >= 0 && !(computed.objects[k].locked ?? false)}}"
        :then [{:set "state.deck.slides[state.current].objects[k]"
            :to "{{merge(computed.objects[k], { x: round(m.x * 10) / 10, y: round(m.y * 10) / 10, w: round(m.width * 10) / 10, h: round(m.height * 10) / 10, rotate: round((m.rotate ?? 0) * 10) / 10 })}}"}]}]
    :deleteObjects [{:if "{{length(computed.selected) > 0}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].objects"
            :to "{{filter(computed.objects, (it.locked ?? false) || !contains(state.board.selection, it.id))}}"}
          {:set "state.deck.slides[state.current].animations"
            :to "{{filter(computed.slide.animations ?? [], contains(map(state.deck.slides[state.current].objects, it.id), it.target))}}"}
          {:set state.board.selection :to "{{map(filter(computed.selected, it.locked ?? false), it.id)}}"}]}]
    :copyObjects [{:if "{{length(computed.selected) > 0}}" :then [{:set state.clip :to "{{computed.selected}}"}]}]
    :cutObjects [{:run copyObjects} {:run deleteObjects}]
    :pasteObjects [{:if "{{length(state.clip) > 0}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].objects"
            :to "{{concat(computed.objects, map(state.clip, merge(it, { id: 'o' + (state.deck.next + index), x: (it.x ?? 0) + 18, y: (it.y ?? 0) + 18, ph: null })))}}"}
          {:set state.board.selection :to "{{map(state.clip, 'o' + (state.deck.next + index))}}"}
          {:increment state.deck.next :by "{{length(state.clip)}}"}]}]
    :duplicateObjects [{:run copyObjects} {:run pasteObjects}]
    :selectAll [{:set state.board.selection :to "{{map(computed.objects, it.id)}}"}]
    :insertObject [{:run commit}
      {:push "state.deck.slides[state.current].objects" :value "{{merge(object, { id: 'o' + state.deck.next })}}"}
      {:set state.board.selection :to "{{['o' + state.deck.next]}}"}
      {:increment state.deck.next :by 1}]
    :insertShape [{:run insertObject
        :with {:object "{{computed.shapes[shape].line ? { kind: 'line', shape: shape, x: 380, y: 270, w: 200, h: 0, line: 'accent1', lineWidth: 1 } : { kind: 'shape', shape: shape, x: 405, y: 195, w: 150, h: 150, fill: 'accent1', line: 'accent1.4', lineWidth: 1, text: '' }}}"}}]
    :insertTextBox [{:run insertObject :with {:object {:kind text :x 330 :y 240 :w 300 :h 40 :text Text :size 18}}}]
    :order [{:note "with: how, front, forward, backward or back: the selected objects move in the slide's drawing order, together"}
      {:if "{{length(computed.selected) > 0}}"
        :then [{:run orderAround
            :with {:how "{{how}}"
              :rest "{{filter(computed.objects, !contains(state.board.selection, it.id))}}"
              :next "{{computed.objects[max(computed.selectedAt) + 1].id ?? null}}"
              :previous "{{min(computed.selectedAt) > 0 ? computed.objects[min(computed.selectedAt) - 1].id : null}}"}}]}]
    :orderAround [{:note "with: how, rest (the objects not selected), next and previous (the ids of the objects just above and below the selection)"}
      {:run commit}
      {:set "state.deck.slides[state.current].objects"
        :to "{{how == 'front' ? concat(rest, computed.selected) : how == 'back' ? concat(computed.selected, rest) : how == 'forward' ? (next == null ? computed.objects : concat(slice(rest, 0, indexOf(map(rest, it.id), next) + 1), computed.selected, slice(rest, indexOf(map(rest, it.id), next) + 1))) : (previous == null ? computed.objects : concat(slice(rest, 0, indexOf(map(rest, it.id), previous)), computed.selected, slice(rest, indexOf(map(rest, it.id), previous))))}}"}]
    :align [{:note "with: how, left, center, right, top, middle or bottom: one object aligns to the slide, several to the box around them"}
      {:if "{{length(computed.boxes) > 0}}"
        :then [{:run alignBoxes
            :with {:how "{{how}}"
              :boxes "{{computed.boxes}}"
              :ids "{{map(computed.boxes, it.id)}}"
              :b "{{length(computed.boxes) == 1 ? computed.slideBounds : computed.selectionBounds}}"}}]}]
    :alignBoxes [{:note "with: how, boxes (where the selected objects are), ids (theirs) and b (the bounds they align to); locked objects stay"}
      {:run commit}
      {:set "state.deck.slides[state.current].objects"
        :to "{{map(computed.objects, !contains(ids, it.id) || (it.locked ?? false) ? it : merge(it, how == 'left' ? { x: b.left } : how == 'center' ? { x: round(((b.left + b.right) / 2 - boxes[indexOf(ids, it.id)].w / 2) * 10) / 10 } : how == 'right' ? { x: b.right - boxes[indexOf(ids, it.id)].w } : how == 'top' ? { y: b.top } : how == 'middle' ? { y: round(((b.top + b.bottom) / 2 - boxes[indexOf(ids, it.id)].h / 2) * 10) / 10 } : { y: b.bottom - boxes[indexOf(ids, it.id)].h }))}}"}]
    :distribute [{:note "with: axis, x or y: equal gaps between the selected objects, from the first to the last (two objects spread across the slide)"}
      {:if "{{length(computed.boxes) > 1}}"
        :then [{:run distributeBoxes
            :with {:axis "{{axis}}"
              :ids "{{map(sort(computed.boxes, axis), it.id)}}"
              :sizes "{{map(sort(computed.boxes, axis), axis == 'x' ? it.w : it.h)}}"
              :start "{{axis == 'x' ? (length(computed.boxes) == 2 ? 0 : computed.selectionBounds.left) : (length(computed.boxes) == 2 ? 0 : computed.selectionBounds.top)}}"
              :span "{{length(computed.boxes) == 2 ? (axis == 'x' ? 960 : 540) : axis == 'x' ? computed.selectionBounds.right - computed.selectionBounds.left : computed.selectionBounds.bottom - computed.selectionBounds.top}}"}}]}]
    :distributeBoxes [{:note "with: axis, ids (the selected objects in order along the axis), sizes (theirs along it), start and span (the space they spread over)"}
      {:run commit}
      {:set "state.deck.slides[state.current].objects"
        :to "{{map(computed.objects, !contains(ids, it.id) || (it.locked ?? false) ? it : merge(it, parseJson('{' + json(axis) + ':' + json(round((start + sum(slice(sizes, 0, indexOf(ids, it.id))) + indexOf(ids, it.id) * (span - sum(sizes)) / (length(ids) - 1)) * 10) / 10) + '}')))}}"}]
    :setTheme [{:run commit}
      {:set state.deck.theme :to "{{theme}}"}
      {:set state.deck.variant :to 0}
      {:set state.deck.colors :to nil}]
    :setVariant [{:run commit} {:set state.deck.variant :to "{{variant}}"} {:set state.deck.colors :to nil}]
    :setColorSet [{:note "with: id, the theme whose colours the deck takes in place of its own (null: the theme's own); a theme or variant chosen after puts them back"}
      {:run commit}
      {:set state.deck.colors :to "{{id}}"}]
    :setFonts [{:note "with: heading and body, the fonts of titles and of the other text (a text's own font wins)"}
      {:run commit}
      {:set state.deck.fonts :to "{{ {heading: heading, body: body} }}"}]
    :setTransition [{:run commit}
      {:set "state.deck.slides[state.current].transition"
        :to "{{merge(computed.slide.transition ?? { kind: 'none', ms: 500 }, props)}}"}]
    :durationBy [{:run setTransition
        :with {:props "{{ {ms: clamp((computed.slide.transition.ms ?? 500) + by, 0, 59000)} }}"}}]
    :transitionToAll [{:run commit}
      {:set state.deck.slides
        :to "{{map(state.deck.slides, merge(it, { transition: computed.slide.transition ?? { kind: 'none', ms: 500 } }))}}"}]
    :toggleLock [{:run setObjects :with {:props "{{ {locked: !every(computed.selected, it.locked ?? false)} }}"}}]
    :setAnimation [{:if "{{length(computed.selected) > 0}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].animations"
            :to "{{concat(filter(computed.slide.animations ?? [], !contains(state.board.selection, it.target)), effect == 'none' ? [] : map(computed.selected, { id: 'a' + (state.deck.next + index), target: it.id, effect: effect, kind: kind, start: index == 0 ? start : 'with', ms: ms, delay: 0 }))}}"}
          {:increment state.deck.next :by "{{length(computed.selected)}}"}]}]
    :changeAnimation [{:if "{{computed.animation != null}}"
        :then [{:run commit}
          {:set "state.deck.slides[state.current].animations"
            :to "{{map(computed.slide.animations, contains(state.board.selection, it.target) ? merge(it, props) : it)}}"}]}]
    :setNotes [{:if "{{text != (computed.slide.notes ?? '')}}"
        :then [{:set "state.deck.slides[state.current].notes" :to "{{text}}"}]}]}}
