{:$schema "../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "Sinopia, a presentation editor, written only in the format: the deck lives in state.deck (parts/sample-deck.lisp), its themes and layouts in parts/themes.lisp, its shapes in parts/geometry.lisp, and every slide is drawn by parts/slide.lisp, in the thumbnails and on the canvas alike. The ribbon is parts/ribbon.lisp and parts/tabs.lisp; editing is parts/edit.lisp, and the program sinopia.lisp."
 :include ["parts/themes.lisp"
           "parts/geometry.lisp"
           "parts/slide.lisp"
           "parts/sample-deck.lisp"
           "parts/edit.lisp"
           "parts/ribbon.lisp"
           "parts/tabs.lisp"
           "parts/show.lisp"
           "parts/views.lisp"
           "parts/menus.lisp"
           "parts/review.lisp"
           "../../shared/addons/assistant/assistant.lisp"
           sinopia.lisp]
 :theme "{{state.theme}}"
 :tokens {:assistant.accent {:light "#0e7c6b" :dark "#3cc2aa"}
          :assistant.accentHover {:light "#0b6a5b" :dark "#62d6c1"}
          :assistant.onAccent {:light "#ffffff" :dark "#0b1f1b"}
          :assistant.surface {:light "#ffffff" :dark "#161b1a"}
          :assistant.raised {:light "#eef2f0" :dark "#1d2321"}
          :assistant.border {:light "#d9e1de" :dark "#343f3b"}
          :assistant.text {:light "#1d2926" :dark "#e3ebe8"}
          :assistant.dim {:light "#5a6a65" :dark "#9daea8"}
          :assistant.dot {:light "#a2afab" :dark "#6e7c77"}
          :assistant.warningBackground {:light "#e2e7e5" :dark "#131716"}
          :kit.border {:light "#c5d0cc" :dark "#42504b"}}
 :window {:title "Sinopia" :width 1280 :height 800 :minWidth 960 :minHeight 600}
 :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
         :icons {:file "../../shared/fonts/Lucide.ttf" :size 12}
         :mono {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 12 :fallback [icons]}
         :slideText {:file "../../shared/fonts/Roboto-Regular.ttf" :size 48 :fallback [icons]}}
 :permissions {:allow [studio] :ask [agent] :reviewers [studio]}
 :state {:current 0
         :theme light
         :tab home
         :ribbon true
         :view normal
         :notes true
         :foldedSections []
         :ruler false
         :gridlines false
         :guides false
         :fit true
         :board {:camera {:x 0 :y 0 :zoom 1} :selection [] :guides {:vertical [] :horizontal []}}}
 :persist [state.deck state.theme state.presenterView]
 :computed {:tabs [{:id file :title "File"}
                   {:id home :title "Home"}
                   {:id insert :title "Insert"}
                   {:id draw :title "Ink"}
                   {:id design :title "Design"}
                   {:id transitions :title "Transitions"}
                   {:id animations :title "Motion"}
                   {:id slideShow :title "Present"}
                   {:id record :title "Record"}
                   {:id review :title "Check"}
                   {:id view :title "View"}
                   {:id help :title "Help"}]
            :canvasWidth "{{max(window.width - (state.view == 'outline' ? 300 : 201) - (state.formatPane || state.sidePane != null ? 260 : 0), 100)}}"
            :canvasHeight "{{max(window.height - 45 - (state.ribbon ? 100 : 0) - 27 - (state.notes ? 77 : 0), 100)}}"
            :fitZoom "{{min((computed.canvasWidth - 56) / 960, (computed.canvasHeight - 40) / 540)}}"
            :fitCamera "{{ {x: 480 - computed.canvasWidth / (2 * computed.fitZoom), y: 270 - computed.canvasHeight / (2 * computed.fitZoom), zoom: computed.fitZoom} }}"
            :view "{{state.fit ? merge(state.board, {camera: computed.fitCamera}) : state.board}}"
            :zoom "{{computed.view.camera.zoom}}"
            :slideTitle "{{trim(split(find(computed.objects, it.ph == 'title' || it.ph == 'ctrTitle').text ?? '', '\n')[0])}}"
            :sectionFirsts "{{map(state.deck.sections ?? [], it.first)}}"
            :sectionAt "{{map(state.deck.slides, indexOf(computed.sectionFirsts, index) < 0 ? null : state.deck.sections[indexOf(computed.sectionFirsts, index)])}}"
            :sectionOf "{{scan(state.deck.slides, computed.sectionAt[index] != null ? computed.sectionAt[index].id : acc)}}"
            :folded "{{map(computed.sectionOf, it != null && contains(state.foldedSections, it))}}"}
 :actions {:fit [{:set state.fit :to true}]
           :zoomBy [{:set state.board
                     :to "{{merge(computed.view, {camera: {x: computed.view.camera.x + computed.canvasWidth / 2 / computed.zoom - computed.canvasWidth / 2 / clamp(computed.zoom * by, 0.1, 4), y: computed.view.camera.y + computed.canvasHeight / 2 / computed.zoom - computed.canvasHeight / 2 / clamp(computed.zoom * by, 0.1, 4), zoom: clamp(computed.zoom * by, 0.1, 4)}})}}"}
                    {:set state.fit :to false}]
           :viewChanged [{:set state.board :to "{{view}}"}
                         {:if "{{state.editing != null && !contains(view.selection ?? [], state.editing)}}"
                          :then [{:set state.editing :to nil}]}
                         {:if "{{abs(view.camera.zoom - computed.fitCamera.zoom) > 0.0001 || abs(view.camera.x - computed.fitCamera.x) > 0.01 || abs(view.camera.y - computed.fitCamera.y) > 0.01}}"
                          :then [{:set state.fit :to false}]}]}
 :keys {"alt+q" [{:when "{{!state.show.on}}" :do [{:focus "#search"}]}]
        "mod+f1" [{:when "{{!state.show.on}}" :do [{:toggle state.ribbon}]}]
        "mod+z" [{:when "{{!context.textInputFocus}}" :do undo}]
        "mod+y" [{:when "{{!context.textInputFocus}}" :do redo}]
        "mod+shift+z" [{:when "{{!context.textInputFocus}}" :do redo}]
        "mod+m" [{:when "{{!context.textInputFocus}}" :do newSlide :with {:layout content}}]
        "mod+d" [{:when "{{!context.textInputFocus && length(computed.selected) > 0}}" :do duplicateObjects}]
        "mod+c" [{:when "{{!context.textInputFocus}}" :do copyObjects}]
        "mod+x" [{:when "{{!context.textInputFocus}}" :do cutObjects}]
        "mod+v" [{:when "{{!context.textInputFocus}}" :do pasteObjects}]
        "mod+a" [{:when "{{!context.textInputFocus}}" :do selectAll}]
        "mod+b" [{:when "{{!context.textInputFocus}}" :do toggleText :with {:key bold}}]
        "mod+i" [{:when "{{!context.textInputFocus}}" :do toggleText :with {:key italic}}]
        "mod+u" [{:when "{{!context.textInputFocus}}" :do toggleText :with {:key underline}}]
        "mod+l" [{:when "{{!context.textInputFocus}}" :do setObjects :with {:props {:align left}}}]
        "mod+e" [{:when "{{!context.textInputFocus}}" :do setObjects :with {:props {:align center}}}]
        "mod+r" [{:when "{{!context.textInputFocus}}" :do setObjects :with {:props {:align right}}}]
        "mod+j" [{:when "{{!context.textInputFocus}}" :do setObjects :with {:props {:align justify}}}]
        "mod+shift+." [{:when "{{!context.textInputFocus}}" :do growText :with {:by 1}}]
        "mod+shift+," [{:when "{{!context.textInputFocus}}" :do growText :with {:by -1}}]
        :delete [{:when "{{!context.textInputFocus && length(computed.selected) > 0}}" :do deleteObjects}]
        :f2 [{:when "{{!context.textInputFocus && !state.show.on && length(computed.selected) == 1}}"
              :do editText
              :with {:id "{{state.board.selection[0]}}"}}]
        :backspace [{:when "{{state.show.on}}" :do showPrevious}
                    {:when "{{!context.textInputFocus && length(computed.selected) > 0}}" :do deleteObjects}]
        "mod+f" [{:when "{{!state.show.on}}" :do [{:run openFind :with {:replace false}}]}]
        "mod+h" [{:when "{{!state.show.on}}" :do [{:run openFind :with {:replace true}}]}]
        :escape [{:when "{{state.show.on}}" :do showEnd}
                 {:when "{{state.inkTool != null}}" :do inkTool :with {:tool nil}}
                 {:when "{{state.editing != null}}" :do stopEditing}
                 {:when "{{!context.textInputFocus && !state.drop.open && !state.colors.open && !state.find.open && length(state.board.selection) > 0}}"
                  :do [{:set state.board.selection :to []}]}]}
 :root (node#root :color @work
         (column#window :fill 0 :color @work :alignItems stretch
           (row#command-bar :height 44 :color @rb :alignItems center :gap 2
             (node#command-pad :width 8 :height 1 :color transparent)
             (label#app-mark :width 28
               :height 28
               :radius 8
               :align center
               :color @brand
               :textColor @onBrand
               :runs [{:icon presentation :size 16}]
               :tooltip "Sinopia")
             (label#doc-title :height 28
               :fitText {:maxWidth 220}
               :padding 8
               :radius 14
               :textColor @rbInk
               :cursor hand
               :hover {:color @rbHover}
               :runs [{:text "{{state.deck.title}}" :size 13 :bold true}]
               :tooltip "Open, save or export the deck"
               :onClick [{:set state.tab :to file} {:set state.ribbon :to true}])
             (node#command-divider :width 1 :height 20 :color @rbLine)
             (node#command-pad-2 :width 6 :height 1 :color transparent)
             (label :id "{{'tab-' + t.id + '-label'}}"
               :repeat {:for t :in "{{computed.tabs}}" :key "{{t.id}}"}
               :style "{{state.tab == t.id && state.ribbon ? 'tabOn' : 'tab'}}"
               :selected "{{state.tab == t.id}}"
               :runs [{:text "{{t.title}}" :size 12}]
               :onClick [{:set state.tab :to "{{t.id}}"} {:set state.ribbon :to true}])
             (node#command-gap :grow 1 :height 1 :color transparent)
             (textarea#search :width 200
               :height 28
               :padding 6
               :radius 14
               :color @rbFieldBg
               :borderWidth 1
               :borderColor @rbFieldLine
               :textColor @rbInk
               :bind state.search
               :placeholder "Search commands (Alt+Q)"
               :placeholderColor @rbMuted
               :label "Search commands"
               :tooltip "Type what you want to do, then Enter"
               :onSubmit {:run openDrop :with {:id search}})
             (node#command-gap-2 :width 8 :height 1 :color transparent)
             (label#qat-undo :style barIcon
               :runs [{:icon undo-2 :size 16}]
               :textColor "{{computed.canUndo ? '@rbInk' : '@inkFaded'}}"
               :tooltip "Undo (Ctrl+Z)"
               :onClick undo)
             (label#qat-redo :style barIcon
               :runs [{:icon redo-2 :size 16}]
               :textColor "{{computed.canRedo ? '@rbInk' : '@inkFaded'}}"
               :tooltip "Redo (Ctrl+Y)"
               :onClick redo)
             (label#qat-save :style barIcon
               :runs [{:icon hard-drive-download :size 16}]
               :tooltip "Save (Ctrl+S)"
               :onClick saveDeck)
             (label#qat-present :style barIcon
               :runs [{:icon skip-back :size 16}]
               :tooltip "Present from the start (F5)"
               :onClick {:run present :with {:from 0}})
             (label#ribbon-collapse :style barIcon
               :runs [{:icon "{{state.ribbon ? 'panel-top-close' : 'panel-top-open'}}" :size 16}]
               :tooltip "{{state.ribbon ? 'Hide the commands (Ctrl+F1)' : 'Show the commands (Ctrl+F1)'}}"
               :onClick [{:toggle state.ribbon}])
             (node#command-pad-3 :width 6 :height 1 :color transparent)
             (label#present-button :style pillButton
               :runs [{:icon play :size 13} {:text " Present" :size 12 :bold true}]
               :tooltip "Present from this slide (Shift+F5)"
               :onClick {:run present :with {:from "{{state.current}}"}})
             (node#command-pad-end :width 8 :height 1 :color transparent))
           (node#ribbon :height 100 :color @rb :when "{{state.ribbon}}"
             (node#ribbon-body :y 2 :anchors {:left 8 :right 8} :height 92 :color transparent
               (node :use tabHome :when "{{state.tab == 'home'}}")
               (node :use tabInsert :when "{{state.tab == 'insert'}}")
               (node :use tabDesign :when "{{state.tab == 'design'}}")
               (node :use tabTransitions :when "{{state.tab == 'transitions'}}")
               (node :use tabView :when "{{state.tab == 'view'}}")
               (node :use tabFile :when "{{state.tab == 'file'}}")
               (node :use tabMotion :when "{{state.tab == 'animations'}}")
               (node :use tabPresent :when "{{state.tab == 'slideShow'}}")
               (node :use tabInk :when "{{state.tab == 'draw'}}")
               (node :use tabRecord :when "{{state.tab == 'record'}}")
               (node :use tabCheck :when "{{state.tab == 'review'}}")
               (node :use tabHelp :when "{{state.tab == 'help'}}")))
           (node#ribbon-line :height 1 :color @barLine)
           (row#main :grow 1 :color @work :alignItems stretch
             (column#thumbnails :when "{{state.view == 'normal'}}"
               :width 200
               :color @rb
               :scroll vertical
               :gap 0
               :focusable true
               :role list
               :label "Slides"
               :onContextMenu {:run openContext :with {:x "{{event.x}}" :y "{{event.y}}" :kind slide}}
               :keys {:up {:run pickSlide :with {:index "{{state.current - 1}}"}}
                      :down {:run pickSlide :with {:index "{{state.current + 1}}"}}
                      :pageup {:run pickSlide :with {:index "{{state.current - 1}}"}}
                      :pagedown {:run pickSlide :with {:index "{{state.current + 1}}"}}
                      :home {:run pickSlide :with {:index 0}}
                      :end {:run pickSlide :with {:index "{{length(state.deck.slides) - 1}}"}}
                      :enter {:run newSlide :with {:layout content}}
                      :delete deleteSlide
                      :backspace deleteSlide}
               (row#thumbs-top :height 36 :width 200 :color transparent :alignItems center
                 (node#thumbs-top-pad :width 12 :height 1 :color transparent)
                 (label#thumbs-title :fitText true
                   :height 20
                   :textColor @rbInk
                   :runs [{:text "Slides" :size 12 :bold true}
                          {:text "{{'  ' + length(state.deck.slides)}}" :size 11 :color @rbMuted}])
                 (node#thumbs-top-gap :grow 1 :height 1 :color transparent)
                 (label#thumbs-add :style barIcon
                   :width 26
                   :height 26
                   :runs [{:icon plus :size 15}]
                   :tooltip "New Slide (Ctrl+M)"
                   :onClick {:run newSlide :with {:layout content}})
                 (node#thumbs-top-pad-end :width 8 :height 1 :color transparent))
               (column :id "{{'thumb-' + s.id}}"
                 :repeat {:for s :in "{{state.deck.slides}}" :index i :key "{{s.id}}"}
                 :fitContent height
                 :width 200
                 :color transparent
                 (label :id "{{'thumb-' + s.id + '-section'}}"
                   :when "{{computed.sectionAt[i] != null}}"
                   :width 200
                   :height 24
                   :padding 8
                   :textColor @rbMuted
                   :cursor hand
                   :role button
                   :label "{{computed.sectionAt[i].name}}"
                   :tooltip "{{computed.folded[i] ? 'Expand the section' : 'Collapse the section'}}"
                   :runs [{:icon "{{computed.folded[i] ? 'chevron-right' : 'chevron-down'}}" :size 11}
                          {:text "{{' ' + upper(computed.sectionAt[i].name)}}" :size 9 :bold true}
                          {:text "{{computed.folded[i] ? '  ' + plural(count(computed.sectionOf, it == computed.sectionAt[i].id), 'slide') : ''}}"
                           :size 9}]
                   :onClick {:set state.foldedSections
                             :to "{{contains(state.foldedSections, computed.sectionAt[i].id) ? filter(state.foldedSections, it != computed.sectionAt[i].id) : concat(state.foldedSections, [computed.sectionAt[i].id])}}"})
                 (column :id "{{'thumb-' + s.id + '-row'}}"
                   :width 200
                   :when "{{!computed.folded[i]}}"
                   :height 126
                   :color transparent
                   :cursor hand
                   :role listitem
                   :alignItems center
                   :selected "{{i == state.current}}"
                   :label "{{'Slide ' + (i + 1)}}"
                   :drag {:data {:slide "{{i}}"}}
                   :onDrop {:run moveSlide :with {:from "{{event.data.slide ?? -1}}" :to "{{i}}"}}
                   :onClick [{:run pickSlide :with {:index "{{i}}"}} {:focus "#thumbnails"}]
                   :onContextMenu [{:run pickSlide :with {:index "{{i}}"}}
                                   {:run openContext :with {:x "{{event.x}}" :y "{{event.y}}" :kind slide}}]
                   (node :id "{{'thumb-' + s.id + '-frame'}}"
                     :width 178
                     :height 104
                     :radius 10
                     :color transparent
                     :borderWidth 2
                     :borderColor "{{i == state.current ? '@brandLine' : 'transparent'}}"
                     :hover {:borderColor "{{i == state.current ? '@brandLine' : '@thumbHover'}}"}
                     :dragOver {:borderColor @brandLine}
                     (node :id "{{'thumb-' + s.id + '-paper'}}"
                       :x 5
                       :y 5
                       :width 168
                       :height 94
                       :radius 4
                       :color "#ffffff"
                       :borderWidth 1
                       :borderColor @thumbLine
                       (image :id "{{'thumb-' + s.id + '-page'}}"
                         :x 0
                         :y 0
                         :width 168
                         :height 94
                         :source "{{'slide-' + s.id}}"
                         :fit fill)
                       (node :id "{{'thumb-' + s.id + '-veil'}}"
                         :when "{{s.hidden ?? false}}"
                         :fill 0
                         :color "#ffffff99")))
                   (row :id "{{'thumb-' + s.id + '-gutter'}}"
                     :width 170
                     :height 18
                     :gap 4
                     :color transparent
                     :alignItems center
                     (label :id "{{'thumb-' + s.id + '-number'}}"
                       :fitText true
                       :height 16
                       :textColor "{{i == state.current ? '@brandLine' : '@rbMuted'}}"
                       :runs [{:text "{{text(i + 1)}}" :size 11 :bold "{{i == state.current}}"}])
                     (label :id "{{'thumb-' + s.id + '-star'}}"
                       :when "{{(s.transition.kind ?? 'none') != 'none' || length(s.animations ?? []) > 0}}"
                       :width 14
                       :height 16
                       :align center
                       :textColor @rbMuted
                       :runs [{:icon wand-sparkles :size 10}]
                       :tooltip "Has a transition or animations")
                     (label :id "{{'thumb-' + s.id + '-hidden'}}"
                       :when "{{s.hidden ?? false}}"
                       :width 14
                       :height 16
                       :align center
                       :textColor @rbMuted
                       :runs [{:icon eye-off :size 10}]
                       :tooltip "Hidden in the show"))))
               (node#thumbs-bottom :height 20 :width 200 :color transparent))
             (node#thumbs-line :when "{{state.view == 'normal'}}" :width 1 :color @barLine)
             (node :use outlineView :when "{{state.view == 'outline'}}")
             (node :use sorterView :when "{{state.view == 'sorter'}}")
             (column#centre :when "{{state.view != 'sorter'}}" :grow 1 :color @work :alignItems stretch
               (node#board-area :grow 1 :color @work
                 (canvas#board :fill 0
                   :dir ltr
                   :focusable true
                   :onContextMenu [{:if "{{!(event.keyboard ?? false)}}" :then [{:set state.board.selection :to []}]}
                                   {:run openContext :with {:x "{{event.x}}" :y "{{event.y}}" :kind canvas}}]
                   :color @work
                   :accentColor @handles
                   :view "{{computed.view}}"
                   :scaleChildren true
                   :snap true
                   :rulers "{{state.ruler}}"
                   :grid "{{state.gridlines ? 12 : 0}}"
                   :label "{{'Slide ' + (state.current + 1) + (computed.slideTitle == '' ? '' : ', ' + computed.slideTitle)}}"
                   :onChange {:run viewChanged :with {:view "{{event.value}}"}}
                   :onTransform {:run placeObjects :with {:moved "{{event.items}}"}}
                   (node :x 0
                     :y 0
                     :width 960
                     :height 540
                     :color "#ffffff"
                     :shadow {:x 0 :y 1 :blur 4 :color "#00000033"}
                     (node :use slideBackground :with {:slide "{{computed.slide}}" :prefix e-}))
                   (node :repeat {:for o :in "{{computed.objects}}" :key "{{o.id}}"}
                     :use slideObject
                     :with {:o "{{o}}" :prefix "" :layout "{{computed.layout}}" :edit true}
                     :onContextMenu [{:if "{{!contains(state.board.selection, o.id)}}"
                                      :then [{:set state.board.selection :to ["{{o.id}}"]}]}
                                     {:run openContext :with {:x "{{event.x}}" :y "{{event.y}}" :kind object}}]
                     :onDoubleClick {:run editText :with {:id "{{o.id}}"}}))
                 (node :use inkLayer :when "{{state.inkTool != null && !state.show.on}}"))
               (node#notes-line :height 1 :color @barLine :when "{{state.notes}}")
               (node#notes-pane :height 76 :color @notes :when "{{state.notes}}"
                 (label#notes-placeholder :x 14
                   :y 8
                   :width 400
                   :height 20
                   :textColor @faint
                   :runs [{:text "{{(computed.slide.notes ?? '') == '' ? 'Click to add notes' : ''}}" :size 13}])
                 (editor#notes :fill 4
                   :color transparent
                   :wrap true
                   :textColor @rbInk
                   :lineColor transparent
                   :label "Notes"
                   :bind "{{'state.deck.slides[' + state.current + '].notes'}}")))
             (node :use formatPane :when "{{state.formatPane && state.view != 'sorter'}}")
             (node :use sidePane :when "{{!state.formatPane && state.sidePane != null && state.view != 'sorter'}}"))
           (node#status-line :height 1 :color @barLine)
           (row#status-bar :height 26 :color @rb :alignItems center :gap 2
             (node#status-pad :width 8 :height 1 :color transparent)
             (row#status-views :fitContent width
               :height 22
               :radius 11
               :color @card
               :borderWidth 1
               :borderColor @rbLine
               :alignItems center
               (label#status-normal :style "{{state.view == 'normal' ? 'barButtonOn' : 'barButton'}}"
                 :height 20
                 :runs [{:icon panel-left :size 13}]
                 :tooltip "Normal"
                 :onClick [{:set state.view :to normal}])
               (label#status-outline :style "{{state.view == 'outline' ? 'barButtonOn' : 'barButton'}}"
                 :height 20
                 :runs [{:icon list-tree :size 13}]
                 :tooltip "Outline"
                 :onClick [{:set state.view :to outline}])
               (label#status-sorter :style "{{state.view == 'sorter' ? 'barButtonOn' : 'barButton'}}"
                 :height 20
                 :runs [{:icon layout-grid :size 13}]
                 :tooltip "Slide Sorter"
                 :onClick [{:set state.view :to sorter}])
               (label#status-show :style barButton
                 :height 20
                 :runs [{:icon play :size 13}]
                 :tooltip "Present from this slide"
                 :onClick {:run present :with {:from "{{state.current}}"}}))
             (node#status-gap-1 :width 6 :height 1 :color transparent)
             (label#status-slide :style barButton
               :runs [{:text "{{'Slide ' + (state.current + 1) + ' of ' + length(state.deck.slides)}}" :size 11}])
             (label#status-notes :style "{{state.notes ? 'barButtonOn' : 'barButton'}}"
               :runs [{:icon notebook-pen :size 13} {:text " Notes" :size 11}]
               :onClick [{:toggle state.notes}])
             (label#status-comments :style "{{state.sidePane == 'comments' && !state.formatPane ? 'barButtonOn' : 'barButton'}}"
               :runs [{:icon message-square :size 13}
                      {:text "{{length(computed.comments) > 0 ? ' Comments (' + length(computed.comments) + ')' : ' Comments'}}"
                       :size 11}]
               :onClick {:run openSidePane :with {:pane comments}})
             (node#status-gap :grow 1 :height 1 :color transparent)
             (label#zoom-fit :style "{{state.fit ? 'barButtonOn' : 'barButton'}}"
               :runs [{:icon scan :size 13} {:text " Fit" :size 11}]
               :tooltip "Fit slide to current window"
               :onClick fit)
             (label#zoom-out :style barButton
               :runs [{:icon zoom-out :size 13}]
               :tooltip "Zoom Out"
               :onClick {:run zoomBy :with {:by 0.9}})
             (slider#zoom-slider :width 110
               :height 16
               :trackColor @rbFieldLine
               :activeColor @brand
               :knobColor @rbFieldBg
               :min 10
               :max 400
               :value "{{round(computed.zoom * 100)}}"
               :label "Zoom"
               :onChange {:run zoomBy :with {:by "{{event.value / 100 / computed.zoom}}"}})
             (label#zoom-in :style barButton
               :runs [{:icon zoom-in :size 13}]
               :tooltip "Zoom In"
               :onClick {:run zoomBy :with {:by 1.1}})
             (label#zoom-percent :style barButton
               :width 42
               :align center
               :runs [{:text "{{round(computed.zoom * 100) + '%'}}" :size 11}])
             (node#status-pad-end :width 8 :height 1 :color transparent)))
         (node :repeat {:for s :in "{{state.deck.slides}}" :key "{{s.id}}"} :use slideImage :with {:s "{{s}}"})
         (node :use showStage :when "{{state.show.on}}"))}
