{:$schema "../../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "Editing the deck: every change first keeps the deck as it was (commit, in sinopia.lisp, with Undo and Redo, the slides and the selection's order, alignment, spacing and case), so Undo and Redo step through up to 50 of them. Objects are changed on the current slide, the selected ones (the canvas's selection, which holds object ids); new slides take their layout's placeholders, empty, and new ids from deck.next."
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
                       :then [{:run commit}
                              {:set state.board.selection :to ["{{id}}"]}
                              {:set state.editing :to "{{id}}"}]}]
           :stopEditing [{:set state.editing :to nil} {:focus "#board"}]
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
           :placeObjects [{:run commit}
                          {:forEach moved
                           :as m
                           :do [{:run placeObject
                                 :with {:m "{{m}}" :k "{{indexOf(map(computed.objects, it.id), m.id)}}"}}]}]
           :placeObject [{:if "{{k >= 0 && !(computed.objects[k].locked ?? false)}}"
                          :then [{:set "state.deck.slides[state.current].objects[k]"
                                  :to "{{merge(computed.objects[k], { x: round(m.x * 10) / 10, y: round(m.y * 10) / 10, w: round(m.width * 10) / 10, h: round(m.height * 10) / 10, rotate: round((m.rotate ?? 0) * 10) / 10 })}}"}]}]
           :deleteObjects [{:if "{{length(computed.selected) > 0}}"
                            :then [{:run commit}
                                   {:set "state.deck.slides[state.current].objects"
                                    :to "{{filter(computed.objects, (it.locked ?? false) || !contains(state.board.selection, it.id))}}"}
                                   {:set "state.deck.slides[state.current].animations"
                                    :to "{{filter(computed.slide.animations ?? [], contains(map(state.deck.slides[state.current].objects, it.id), it.target))}}"}
                                   {:set state.board.selection
                                    :to "{{map(filter(computed.selected, it.locked ?? false), it.id)}}"}]}]
           :copyObjects [{:if "{{length(computed.selected) > 0}}" :then [{:set state.clip :to "{{computed.selected}}"}]}]
           :cutObjects [{:run copyObjects} {:run deleteObjects}]
           :pasteObjects [{:if "{{length(state.clip) > 0}}"
                           :then [{:run commit}
                                  {:set "state.deck.slides[state.current].objects"
                                   :to "{{concat(computed.objects, map(state.clip, merge(it, { id: 'o' + (state.deck.next + index), x: (it.x ?? 0) + 18, y: (it.y ?? 0) + 18, ph: null })))}}"}
                                  {:set state.board.selection
                                   :to "{{map(state.clip, 'o' + (state.deck.next + index))}}"}
                                  {:increment state.deck.next :by "{{length(state.clip)}}"}]}]
           :duplicateObjects [{:run copyObjects} {:run pasteObjects}]
           :selectAll [{:set state.board.selection :to "{{map(computed.objects, it.id)}}"}]
           :insertObject [{:run commit}
                          {:push "state.deck.slides[state.current].objects"
                           :value "{{merge(object, { id: 'o' + state.deck.next })}}"}
                          {:set state.board.selection :to "{{['o' + state.deck.next]}}"}
                          {:increment state.deck.next :by 1}]
           :insertShape [{:run insertObject
                          :with {:object "{{computed.shapes[shape].line ? { kind: 'line', shape: shape, x: 380, y: 270, w: 200, h: 0, line: 'accent1', lineWidth: 1 } : { kind: 'shape', shape: shape, x: 405, y: 195, w: 150, h: 150, fill: 'accent1', line: 'accent1.4', lineWidth: 1, text: '' }}}"}}]
           :insertTextBox [{:run insertObject
                            :with {:object {:kind text :x 330 :y 240 :w 300 :h 40 :text "Text" :size 18}}}]
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
