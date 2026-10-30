{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "The ribbon's tabs, each a row of groups built from parts/ribbon.json. Commands act on the current slide and its selected objects (parts/edit.json)."
  :components {:tabHome {:params {}
      :node (row :fitContent width #tab-home :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-clipboard :with {:title Clipboard}
          (node :use rbBig
            #b-paste
            :with {:icon clipboard-paste :title Paste :menu true :width 46 :drop b-paste}
            :tooltip "Paste (Ctrl+V)"
            :onClick pasteObjects)
          (node :use rbStack #s-clipboard
            (node :use rbSmall #b-cut :with {:icon scissors :title Cut} :tooltip "Cut (Ctrl+X)" :onClick cutObjects)
            (node :use rbSmall
              #b-copy
              :with {:icon copy :title Copy :menu true :drop b-copy}
              :tooltip "Copy (Ctrl+C)"
              :onClick copyObjects)
            (node :use rbSmall
              #b-painter
              :with {:icon paint-roller :title "Copy Style" :on "{{state.painter != null}}"}
              :tooltip "Copy Style: select an object, click here, then select the objects to give its look"
              :onClick copyStyle)))
        (node :use rbGroup #g-slides :with {:title Slides}
          (node :use rbBig
            #b-new-slide
            :with {:icon layout-panel-top :title "New\nSlide" :menu true :width 50 :drop b-new-slide}
            :tooltip "New Slide (Ctrl+M)"
            :onClick {:run newSlide :with {:layout content}})
          (node :use rbStack #s-slides
            (node :use rbSmall
              #b-layout
              :with {:icon layout-template :title Layout :menu true}
              :tooltip Layout
              :onClick {:run openDrop :with (node #b-layout)})
            (node :use rbSmall #b-reset :with {:icon rotate-ccw :title Reset} :tooltip Reset :onClick resetSlide)
            (node :use rbSmall
              #b-section
              :with {:icon layout-list :title Section :menu true}
              :tooltip Section
              :onClick {:run openDrop :with (node #b-section)})))
        (node :use rbGroup #g-font :with {:title Font :launcher true}
          (node :use rbStack #s-font
            (node :use rbRow #r-font-1
              (node :use rbField #f-font :with {:text "{{computed.fontTitle}}" :width 150 :drop f-font} :tooltip Font)
              (node :use rbField
                #f-size
                :with {:text "{{text(computed.textSize)}}" :width 46 :drop f-size}
                :tooltip "Font Size")
              (node :use rbSmall
                #b-grow
                :with {:icon a-arrow-up}
                :tooltip "Increase Font Size (Ctrl+Shift+>)"
                :onClick {:run growText :with {:by 1}})
              (node :use rbSmall
                #b-shrink
                :with {:icon a-arrow-down}
                :tooltip "Decrease Font Size (Ctrl+Shift+<)"
                :onClick {:run growText :with {:by -1}})
              (node :use rbSmall
                #b-clear
                :with {:icon remove-formatting}
                :tooltip "Clear All Formatting"
                :onClick clearFormatting))
            (node :use rbRow #r-font-2
              (node :use rbSmall
                #b-bold
                :with {:icon bold :on "{{computed.first != null && computed.allOn.bold}}"}
                :tooltip "Bold (Ctrl+B)"
                :onClick {:run toggleText :with {:key bold}})
              (node :use rbSmall
                #b-italic
                :with {:icon italic :on "{{computed.first != null && computed.allOn.italic}}"}
                :tooltip "Italic (Ctrl+I)"
                :onClick {:run toggleText :with {:key italic}})
              (node :use rbSmall
                #b-underline
                :with {:icon underline :on "{{computed.first != null && computed.allOn.underline}}"}
                :tooltip "Underline (Ctrl+U)"
                :onClick {:run toggleText :with {:key underline}})
              (node :use rbSmall
                #b-shadow
                :with {:icon type-outline :on "{{computed.first != null && computed.allOn.shadow}}"}
                :tooltip "Text Shadow"
                :onClick {:run toggleText :with {:key shadow}})
              (node :use rbSmall
                #b-strike
                :with {:icon strikethrough :on "{{computed.first != null && computed.allOn.strike}}"}
                :tooltip Strikethrough
                :onClick {:run toggleText :with {:key strike}})
              (node :use rbSmall
                #b-spacing
                :with {:icon move-horizontal :menu true :enabled false}
                :tooltip "Character Spacing isn't available yet")
              (node :use rbSmall
                #b-case
                :with {:icon case-sensitive :menu true}
                :tooltip "Change Case"
                :onClick {:run openDrop :with (node #b-case)})
              (node :use rbSmall
                #b-highlight
                :with {:icon highlighter
                  :menu true
                  :tint "{{state.lastColor['b-highlight']}}"
                  :drop b-highlight
                  :colors true}
                :tooltip "Text Highlight Color"
                :onClick {:run applyColor :with (node #b-highlight :value "{{state.lastColor['b-highlight']}}")})
              (node :use rbSmall
                #b-color
                :with {:icon baseline :menu true :tint "{{state.lastColor['b-color']}}" :drop b-color :colors true}
                :tooltip "Font Color"
                :onClick {:run applyColor :with (node #b-color :value "{{state.lastColor['b-color']}}")}))))
        (node :use rbGroup #g-paragraph :with {:title Paragraph :launcher true}
          (node :use rbStack #s-paragraph
            (node :use rbRow #r-para-1
              (node :use rbSmall
                #b-bullets
                :with {:icon list
                  :menu true
                  :drop b-bullets
                  :on "{{computed.first != null && computed.allOn.bullets && computed.first.numbering == null}}"}
                :tooltip Bullets
                :onClick {:run toggleText :with {:key bullets}})
              (node :use rbSmall
                #b-numbering
                :with {:icon list-ordered :menu true :drop b-numbering :on "{{computed.first.numbering != null}}"}
                :tooltip Numbering
                :onClick {:run setObjects
                  :with {:props "{{computed.first.numbering == null ? { bullets: true, numbering: '1.' } : { numbering: null }}}"}})
              (node :use rbSmall
                #b-outdent
                :with {:icon indent-decrease}
                :tooltip "Decrease List Level"
                :onClick {:run indentText :with {:by -1}})
              (node :use rbSmall
                #b-indent
                :with {:icon indent-increase}
                :tooltip "Increase List Level"
                :onClick {:run indentText :with {:by 1}})
              (node :use rbSmall
                #b-line-spacing
                :with {:icon list-chevrons-up-down :menu true}
                :tooltip "Line Spacing"
                :onClick {:run openDrop :with (node #b-line-spacing)})
              (node :use rbSmall
                #b-columns
                :with {:icon columns-2 :menu true}
                :tooltip "Add or Remove Columns"
                :onClick {:run openDrop :with (node #b-columns)}))
            (node :use rbRow #r-para-2
              (node :repeat {:for a
                  :in "{{[['left', 'align-left', 'Align Left (Ctrl+L)'], ['center', 'align-center', 'Center (Ctrl+E)'], ['right', 'align-right', 'Align Right (Ctrl+R)'], ['justify', 'align-justify', 'Justify (Ctrl+J)']]}}"}
                :use rbSmall
                #"{{'b-align-' + a[0]}}"
                :with {:icon "{{a[1]}}"
                  :on "{{computed.first != null && (computed.first.align ?? computed.firstPlaceholder.align ?? 'left') == a[0]}}"}
                :tooltip "{{a[2]}}"
                :onClick {:run setObjects :with {:props "{{ {align: a[0]} }}"}})
              (node :use rbSmall
                #b-direction
                :with {:icon arrow-down-up :menu true}
                :tooltip "Text Direction"
                :onClick {:run openDrop :with (node #b-direction)})
              (node :use rbSmall
                #b-anchor
                :with {:icon align-vertical-justify-center :menu true}
                :tooltip "Align Text"
                :onClick {:run openDrop :with (node #b-anchor)})
              (node :use rbSmall
                #b-smart
                :with {:icon network :menu true :enabled false}
                :tooltip "Convert to a Diagram isn't available yet"))))
        (node :use rbGroup #g-drawing :with {:title Drawing :launcher true}
          (node :use rbGallery #gal-shapes :with {:width 168 :more gal-shapes}
            (grid #gal-shapes-grid :columns 6 :width 148 :height 66 :gap 1 :color transparent
              (node :repeat {:for k
                  :in "{{['textBox', 'rect', 'ellipse', 'triangle', 'rtTriangle', 'line', 'lineArrow', 'roundRect', 'rightArrow', 'downArrow', 'star5', 'curvedRightArrow', 'leftBrace', 'rightBrace', 'smileyFace', 'heart', 'cloud', 'wedgeRoundRectCallout']}}"
                  :index n}
                :type panel
                #"{{'gal-shape-' + k}}"
                :width 24
                :height 21
                :radius 2
                :color transparent
                :cursor hand
                :hover {:color @rbHover}
                :tooltip "{{k == 'textBox' ? 'Text Box' : computed.shapes[k].name ?? k}}"
                :onClick [{:if "{{k == 'textBox'}}"
                    :then [{:run insertTextBox}]
                    :else [{:run insertShape :with {:shape "{{k}}"}}]}]
                (label #"{{'gal-shape-' + k + '-tb'}}"
                  :when "{{k == 'textBox'}}"
                  :fill 0
                  :align center
                  :textColor @rbInk
                  :runs [{:icon text-cursor-input :size 14}])
                (node #"{{'gal-shape-' + k + '-box'}}"
                  :color transparent
                  :when "{{k != 'textBox'}}"
                  :x 5
                  :y 4
                  :width 14
                  :height 13
                  :overflow visible
                  {:use shapePath
                    :with {:o "{{ {shape: k, w: 14, h: computed.shapes[k].line ? 0 : 13, fill: 'none', line: '#444444', lineWidth: 1, flipH: false, flipV: false, gradient: null, dash: null, effect: null} }}"
                      :name "{{'gal-shape-' + k}}"}}))))
          (node :use rbBig
            #b-arrange
            :with {:icon layers-2 :title Arrange :menu true :width 52}
            :tooltip Arrange
            :onClick {:run openDrop :with (node #b-arrange)})
          (node :use rbBig
            #b-quick-styles
            :with {:icon swatch-book :title "Shape\nStyles" :menu true :width 52}
            :tooltip "Shape Styles"
            :onClick {:run openColors :with (node #b-quick-styles)})
          (node :use rbStack #s-drawing
            (node :use rbSmall
              #b-fill
              :with {:icon paint-bucket :title "Shape Fill" :menu true :drop b-fill :colors true}
              :tooltip "Shape Fill"
              :onClick {:run applyColor :with (node #b-fill :value "{{state.lastColor['b-fill']}}")})
            (node :use rbSmall
              #b-outline
              :with {:icon pen-tool :title "Shape Outline" :menu true :drop b-outline :colors true}
              :tooltip "Shape Outline"
              :onClick {:run applyColor :with (node #b-outline :value "{{state.lastColor['b-outline']}}")})
            (node :use rbSmall
              #b-effects
              :with {:icon sparkles :title "Shape Effects" :menu true}
              :tooltip "Shape Effects"
              :onClick {:run openDrop :with (node #b-effects)})))
        (node :use rbGroup #g-editing :with {:title Editing}
          (node :use rbStack #s-editing
            (node :use rbSmall
              #b-find
              :with {:icon search :title Find}
              :tooltip "Find (Ctrl+F)"
              :onClick {:run openFind :with {:replace false}})
            (node :use rbSmall
              #b-replace
              :with {:icon replace :title Replace}
              :tooltip "Replace (Ctrl+H)"
              :onClick {:run openFind :with {:replace true}})
            (node :use rbSmall
              #b-select
              :with {:icon mouse-pointer-2 :title Select :menu true}
              :tooltip Select
              :onClick {:run openDrop :with (node #b-select)})))
        (node :use rbGroup #g-voice :with {:title Voice}
          (node :use rbBig
            #b-dictate
            :with {:icon audio-lines :title Speak :menu true :width 50 :enabled false}
            :tooltip "Speak to type isn't available yet")))}
    :tabInsert {:params {}
      :node (row :fitContent width #tab-insert :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-ins-slides :with {:title Slides}
          (node :use rbBig
            #b-ins-new-slide
            :with {:icon layout-panel-top :title "New\nSlide" :menu true :width 50}
            :onClick {:run newSlide :with {:layout content}}))
        (node :use rbGroup #g-ins-tables :with {:title Tables}
          (node :use rbBig
            #b-ins-table
            :with {:icon table :title Table :width 46 :enabled false}
            :tooltip "Inserting a table isn't available yet"))
        (node :use rbGroup #g-ins-images :with {:title Images}
          (node :use rbBig
            #b-ins-pictures
            :with {:icon image :title Pictures :width 52 :enabled false}
            :tooltip "Inserting pictures isn't available yet")
          (node :use rbBig
            #b-ins-album
            :with {:icon images :title "Photo\nAlbum" :width 50 :enabled false}
            :tooltip "A photo album isn't available yet"))
        (node :use rbGroup #g-ins-illustrations :with {:title Illustrations}
          (node :use rbBig
            #b-ins-shapes
            :with {:icon shapes :title Shapes :menu true :width 48}
            :onClick {:run openDrop :with (node #b-ins-shapes)})
          (node :use rbBig
            #b-ins-icons
            :with {:icon face-slightly-smiling :title Icons :menu true :width 44}
            :onClick {:run openDrop :with (node #b-ins-icons)})
          (node :use rbBig
            #b-ins-chart
            :with {:icon chart-column-stacked :title Chart :width 44 :enabled false}
            :tooltip "Inserting a chart isn't available yet"))
        (node :use rbGroup #g-ins-links :with {:title Links}
          (node :use rbBig
            #b-ins-link
            :with {:icon link
              :title Link
              :menu true
              :width 44
              :enabled "{{some(computed.selected, contains(['text', 'shape'], it.kind ?? 'text'))}}"}
            :tooltip "Link the selected text to a slide of this deck, which a click in the show goes to"
            :onClick {:run openDrop :with (node #b-ins-link)}))
        (node :use rbGroup #g-ins-comments :with {:title Comments}
          (node :use rbBig
            #b-ins-comment
            :with {:icon message-square-plus :title Comment :width 56}
            :tooltip "Comment on this slide"
            :onClick newComment))
        (node :use rbGroup #g-ins-text :with {:title Text}
          (node :use rbBig
            #b-ins-textbox
            :with {:icon text-cursor-input :title "Text\nBox" :width 44}
            :onClick insertTextBox)
          (node :use rbBig
            #b-ins-header
            :with {:icon panel-top :title "Header\n& Footer" :menu true :width 52}
            :onClick {:run openDrop :with (node #b-ins-header)})
          (node :use rbBig
            #b-ins-art-text
            :with {:icon type :title "Art\nText" :menu true :width 44}
            :onClick {:run openDrop :with (node #b-ins-art-text)}))
        (node :use rbGroup #g-ins-symbols :with {:title Symbols}
          (node :use rbBig
            #b-ins-equation
            :with {:icon sigma :title Equation :width 56 :enabled false}
            :tooltip "Inserting an equation isn't available yet")
          (node :use rbBig
            #b-ins-symbol
            :with {:icon omega :title Symbol :menu true :width 48}
            :onClick {:run openDrop :with (node #b-ins-symbol)}))
        (node :use rbGroup #g-ins-media :with {:title Media}
          (node :use rbBig
            #b-ins-video
            :with {:icon film :title Video :width 44 :enabled false}
            :tooltip "Inserting video isn't available yet")
          (node :use rbBig
            #b-ins-audio
            :with {:icon music :title Audio :width 44 :enabled false}
            :tooltip "Inserting audio isn't available yet")))}
    :tabDesign {:params {}
      :node (row :fitContent width #tab-design :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-themes :with {:title Themes}
          (node :use rbGallery #gal-themes :with {:width 490 :more gal-themes}
            (node :repeat {:for t :in "{{slice(computed.themes, 0, 6)}}" :key "{{t.id}}"}
              :type panel
              #"{{'theme-' + t.id}}"
              :width 76
              :height 60
              :radius 2
              :cursor hand
              :tooltip "{{t.name}}"
              :color "{{t.colors[t.background]}}"
              :borderWidth "{{state.deck.theme == t.id ? 2 : 1}}"
              :borderColor "{{state.deck.theme == t.id ? '@brand' : '@rbFieldLine'}}"
              :hover {:borderColor @brand}
              :onClick {:run setTheme :with {:theme "{{t.id}}"}}
              (label #"{{'theme-' + t.id + '-aa'}}"
                :x 8
                :y 10
                :width 40
                :height 24
                :textColor "{{t.colors.dark1}}"
                :runs [{:text Aa :size 18}])
              (row #"{{'theme-' + t.id + '-bar'}}" :x 8 :y 42 :width 60 :height 8 :color transparent
                (node :repeat {:for c :in "{{['accent1', 'accent2', 'accent3', 'accent4']}}"}
                  #"{{'theme-' + t.id + '-' + c}}"
                  :width 15
                  :height 8
                  :color "{{t.colors[c]}}")))))
        (node :use rbGroup #g-variants :with {:title Variants}
          (node :use rbGallery #gal-variants :with {:width 290}
            (node :repeat {:for v :in "{{computed.theme.variants}}" :index n}
              :type panel
              #"{{'variant-' + n}}"
              :width 62
              :height 60
              :radius 2
              :cursor hand
              :color "{{merge(computed.theme.colors, v)[computed.theme.background]}}"
              :borderWidth "{{(state.deck.variant ?? 0) == n ? 2 : 1}}"
              :borderColor "{{(state.deck.variant ?? 0) == n ? '@brand' : '@rbFieldLine'}}"
              :hover {:borderColor @brand}
              :onClick {:run setVariant :with {:variant "{{n}}"}}
              (node #"{{'variant-' + n + '-band'}}"
                :x 0
                :y 0
                :width 10
                :height 60
                :color "{{merge(computed.theme.colors, v).accent1}}")
              (node #"{{'variant-' + n + '-line'}}"
                :x 18
                :y 22
                :width 34
                :height 5
                :color "{{merge(computed.theme.colors, v).dark1}}")
              (node #"{{'variant-' + n + '-line2'}}"
                :x 18
                :y 32
                :width 24
                :height 3
                :color "{{merge(computed.theme.colors, v).accent2}}")))
          (column #variants-col :width 76 :height 72 :gap 2 :alignItems stretch :color transparent
            (node :use rbSmall
              #b-colors
              :with {:icon palette :title Colors :menu true}
              :tooltip "Another theme's colours for this deck"
              :onClick {:run openDrop :with (node #b-colors)})
            (node :use rbSmall
              #b-fonts
              :with {:icon type :title Fonts :menu true}
              :tooltip "The fonts of the titles and of the other text"
              :onClick {:run openDrop :with (node #b-fonts)})))
        (node :use rbGroup #g-customize :with {:title Customize}
          (node :use rbBig
            #b-slide-size
            :with {:icon rectangle-horizontal :title "Slide\nSize" :width 46 :enabled false}
            :tooltip "Changing the slide size isn't available yet")
          (node :use rbBig
            #b-format-bg
            :with {:icon paint-bucket :title "Format\nBackground" :width 70}
            :onClick formatBackground)))}
    :tabTransitions {:params {}
      :node (row :fitContent width #tab-transitions :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-preview :with {:title Preview}
          (node :use rbBig
            #b-preview
            :with {:icon play :title Preview :width 50}
            :tooltip "Play this slide's transition and builds"
            :onClick previewSlide))
        (node :use rbGroup #g-transition-to :with {:title "Transition to This Slide"}
          (node :use rbGallery #gal-transitions :with {:width 520}
            (node :repeat {:for k
                :in "{{[['none', 'ban', 'None'], ['glide', 'blend', 'Glide'], ['fade', 'sun-dim', 'Fade'], ['push', 'arrow-left-to-line', 'Push'], ['wipe', 'chevrons-left', 'Wipe'], ['split', 'columns-2', 'Split'], ['reveal', 'arrow-left-from-line', 'Reveal'], ['cover', 'square-stack', 'Cover']]}}"}
              :type label
              #"{{'tr-' + k[0]}}"
              :width 60
              :height 64
              :wrap true
              :align {:horizontal center :vertical center}
              :style "{{(computed.slide.transition.kind ?? 'none') == k[0] ? 'rbButtonOn' : 'rbButton'}}"
              :runs "{{[{ icon: k[1], size: 24 }, { text: '\n' + k[2], size: 11 }]}}"
              :onClick {:run setTransition :with {:props "{{ {kind: k[0]} }}"}}))
          (node :use rbBig
            #b-effect-options
            :with {:icon "{{computed.directionIcons[computed.slide.transition.direction ?? 'left']}}"
              :title "{{'From\n' + (computed.slide.transition.direction ?? 'left')}}"
              :width 52
              :enabled "{{contains(['push', 'wipe', 'reveal', 'cover'], computed.slide.transition.kind ?? 'none')}}"}
            :tooltip "The side the transition comes from"
            :onClick {:run setTransition
              :with {:props "{{ {direction: computed.directions[(indexOf(computed.directions, computed.slide.transition.direction ?? 'left') + 1) % 4]} }}"}}))
        (node :use rbGroup #g-timing :with {:title Timing}
          (column #timing-col :width 200 :height 72 :gap 2 :alignItems stretch :color transparent
            (row #timing-row :height 22 :gap 2 :alignItems center :color transparent
              (label #timing-duration
                :width 120
                :height 22
                :textColor @rbInk
                :runs "{{[{ icon: 'clock-3', size: 14 }, { text: ' Duration: ' + fixed((computed.slide.transition.ms ?? 500) / 1000, 2), size: 12 }]}}")
              (node :use rbSmall
                #timing-shorter
                :with {:icon minus}
                :tooltip Shorter
                :onClick {:run durationBy :with {:by -250}})
              (node :use rbSmall
                #timing-longer
                :with {:icon plus}
                :tooltip Longer
                :onClick {:run durationBy :with {:by 250}}))
            (label #timing-apply
              :height 22
              :style rbButton
              :padding 2
              :fitText true
              :runs [{:icon copy-check :size 14} {:text " Apply To All" :size 12}]
              :tooltip "Give every slide this transition"
              :onClick transitionToAll)
            (label #timing-advance
              :height 22
              :textColor @rbInk
              :runs [{:icon check-square :size 14} {:text " On Mouse Click" :size 12}]))))}
    :tabView {:params {}
      :node (row :fitContent width #tab-view :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-views :with {:title Views}
          (node :use rbBig
            #b-normal
            :with {:icon panel-left :title Normal :on "{{state.view == 'normal'}}" :width 46}
            :onClick [{:set state.view :to normal}])
          (node :use rbBig
            #b-outline-view
            :with {:icon list-tree :title "Outline\nView" :on "{{state.view == 'outline'}}" :width 48}
            :onClick [{:set state.view :to outline}])
          (node :use rbBig
            #b-sorter
            :with {:icon layout-grid :title "Slide\nSorter" :on "{{state.view == 'sorter'}}" :width 46}
            :onClick [{:set state.view :to sorter}])
          (node :use rbBig
            #b-notes-page
            :with {:icon notebook-text :title "Notes\nPage" :width 46 :enabled false}
            :tooltip "The notes page view isn't available yet")
          (node :use rbBig
            #b-reading
            :with {:icon book-open :title "Reading\nView" :width 50}
            :onClick {:run readingView :with {:from "{{state.current}}"}}))
        (node :use rbGroup #g-show :with {:title Show :launcher true}
          (column #show-col :width 90 :height 72 :gap 2 :alignItems stretch :color transparent
            (node :use rbCheck #cb-ruler :with {:title Ruler :at state.ruler :prefix cb-ruler})
            (node :use rbCheck #cb-grid :with {:title Gridlines :at state.gridlines :prefix cb-grid})
            (node :use rbCheck #cb-guides :with {:title Guides :at state.guides :prefix cb-guides})))
        (node :use rbGroup #g-zoom :with {:title Zoom}
          (node :use rbBig #b-zoom :with {:icon zoom-in :title Zoom :width 44} :onClick {:run zoomBy :with {:by 1.25}})
          (node :use rbBig #b-fit :with {:icon maximize :title "Fit to\nWindow" :width 50} :onClick fit))
        (node :use rbGroup #g-notes-show :with {:title Panes}
          (node :use rbBig
            #b-notes-toggle
            :with {:icon notebook-pen :title Notes :on "{{state.notes}}" :width 44}
            :onClick [{:toggle state.notes}])
          (node :use rbBig
            #b-format-pane
            :with {:icon paintbrush :title "Format\nPane" :on "{{state.formatPane}}" :width 48}
            :onClick [{:toggle state.formatPane} {:set state.sidePane :to nil}]))
        (node :use rbGroup #g-mode :with {:title Appearance}
          (node :use rbBig
            #b-dark
            :with {:icon "{{state.theme == 'dark' ? 'sun' : 'moon'}}"
              :title "{{state.theme == 'dark' ? 'Light\nMode' : 'Dark\nMode'}}"
              :width 48}
            :onClick toggleTheme)))}
    :tabFile {:params {}
      :node (row :fitContent width #tab-file :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-file :with {:title Deck}
          (node :use rbBig
            #b-open
            :with {:icon folder-open :title Open :width 46}
            :tooltip "Open a deck (Ctrl+O)"
            :onClick openDeck)
          (node :use rbBig
            #b-save
            :with {:icon save :title Save :width 46}
            :tooltip "Save the deck (Ctrl+S)"
            :onClick saveDeck))
        (node :use rbGroup #g-export :with {:title Export}
          (node :use rbBig
            #b-export-pdf
            :with {:icon file-text :title PDF :width 46}
            :tooltip "Every slide shown, a page each"
            :onClick exportPdf)
          (node :use rbBig
            #b-export-png
            :with {:icon file-image :title Picture :width 50}
            :tooltip "This slide as a PNG picture"
            :onClick exportPng))
        (node :use rbGroup #g-file-info :with {:title Last}
          (label #file-status
            :width 220
            :height 72
            :wrap true
            :padding 4
            :textColor @rbMuted
            :runs [{:text "{{state.fileStatus == '' ? 'Nothing saved or exported yet.' : state.fileStatus}}" :size 12}])))}
    :tabMotion {:params {}
      :node (row :fitContent width
        #tab-motion
        :height 92
        :gap 4
        :color transparent
        :let {:a "{{computed.animation}}" :on "{{length(computed.selected) > 0}}"}
        (node :use rbGroup #g-motion-preview :with {:title Preview}
          (node :use rbBig
            #b-motion-preview
            :with {:icon play :title Preview :width 50}
            :tooltip "Play this slide's transition and builds"
            :onClick previewSlide))
        (node :use rbGroup #g-effects :with {:title "{{a.kind == 'exit' ? 'Exit' : 'Entrance'}}"}
          (node :use rbGallery #gal-effects :with {:width 380}
            (node :repeat {:for e
                :in "{{[['none', 'ban', 'None'], ['appear', 'square', 'Appear'], ['fade', 'sun-dim', 'Fade'], ['wipe', 'chevrons-right', 'Wipe'], ['fly', 'move-up', 'Fly'], ['zoom', 'zoom-in', 'Zoom']]}}"}
              :type label
              #"{{'fx-' + e[0]}}"
              :width 60
              :height 64
              :wrap true
              :align {:horizontal center :vertical center}
              :style "{{!on ? 'rbButtonOff' : (a.effect ?? 'none') == e[0] ? 'rbButtonOn' : 'rbButton'}}"
              :runs "{{[{ icon: e[1], size: 24 }, { text: '\n' + e[2], size: 11 }]}}"
              :onClick {:run setAnimation
                :with {:effect "{{e[0]}}"
                  :kind "{{a.kind ?? 'entrance'}}"
                  :start "{{a.start ?? 'click'}}"
                  :ms "{{a.ms ?? 500}}"}}))
          (node :use rbBig
            #b-exit
            :with {:icon log-out :title "As\nExit" :on "{{a.kind == 'exit'}}" :enabled "{{a != null}}" :width 46}
            :tooltip "Leave the slide instead of coming in"
            :onClick {:run changeAnimation :with {:props "{{ {kind: a.kind == 'exit' ? 'entrance' : 'exit'} }}"}}))
        (node :use rbGroup #g-motion-timing :with {:title Timing}
          (column #motion-timing-col :width 200 :height 72 :gap 2 :alignItems stretch :color transparent
            (row #motion-start-row :height 22 :gap 2 :alignItems center :color transparent
              (label #motion-start-title :width 44 :height 22 :textColor @rbInk :runs [{:text Start :size 12}])
              (node :repeat {:for w :in "{{[['click', 'Click'], ['with', 'With'], ['after', 'After']]}}"}
                :type label
                #"{{'motion-start-' + w[0]}}"
                :height 22
                :fitText true
                :padding 4
                :style "{{a == null ? 'rbButtonOff' : a.start == w[0] ? 'rbButtonOn' : 'rbButton'}}"
                :runs [{:text "{{w[1]}}" :size 12}]
                :onClick {:run changeAnimation :with {:props "{{ {start: w[0]} }}"}}))
            (row #motion-duration-row :height 22 :gap 2 :alignItems center :color transparent
              (label #motion-duration
                :width 120
                :height 22
                :textColor @rbInk
                :runs "{{[{ icon: 'clock-3', size: 14 }, { text: ' Duration: ' + fixed((a.ms ?? 500) / 1000, 2), size: 12 }]}}")
              (node :use rbSmall
                #motion-shorter
                :with {:icon minus :enabled "{{a != null}}"}
                :tooltip Shorter
                :onClick {:run changeAnimation :with {:props "{{ {ms: max((a.ms ?? 500) - 250, 0)} }}"}})
              (node :use rbSmall
                #motion-longer
                :with {:icon plus :enabled "{{a != null}}"}
                :tooltip Longer
                :onClick {:run changeAnimation :with {:props "{{ {ms: min((a.ms ?? 500) + 250, 59000)} }}"}}))
            (label #motion-count
              :height 22
              :textColor @rbMuted
              :runs [{:text "{{length(computed.slide.animations ?? []) + ' on this slide, ' + count(computed.slide.animations ?? [], it.start == 'click') + ' on a click'}}"
                  :size 12}])))
        (node :use rbGroup #g-motion-advanced :with {:title Advanced}
          (node :use rbBig
            #b-motion-pane
            :with {:icon list-ordered
              :title "Animation\nPane"
              :on "{{state.sidePane == 'motion' && !state.formatPane}}"
              :width 64}
            :tooltip "The slide's builds in the order they play"
            :onClick {:run openSidePane :with {:pane motion}})))}
    :rbCheck {:params {:title "" :at "" :prefix ""}
      :note "A checkbox bound to the state path at, and its title, which toggles it too."
      :node {:type row
        :height 22
        :gap 6
        :alignItems center
        :color transparent
        :children [(checkbox #"{{prefix + '-box'}}" :width 14 :height 14 :bind "{{at}}" :label "{{title}}")
          (label #"{{prefix + '-caption'}}"
            :fitText true
            :height 20
            :cursor hand
            :textColor @rbInk
            :runs [{:text "{{title}}" :size 12}]
            :onClick [{:toggle "{{at}}"}])]}}
    :tabPresent {:params {}
      :node (row :fitContent width #tab-present :height 92 :gap 4 :color transparent
        (node :use rbGroup #g-start :with {:title "Start Slide Show"}
          (node :use rbBig
            #b-from-start
            :with {:icon presentation :title "From\nBeginning" :width 60}
            :tooltip "From the start (F5)"
            :onClick {:run present :with {:from 0}})
          (node :use rbBig
            #b-from-current
            :with {:icon projector :title "From This\nSlide" :width 60}
            :tooltip "From this slide (Shift+F5)"
            :onClick {:run present :with {:from "{{state.current}}"}}))
        (node :use rbGroup #g-set-up :with {:title "Set Up"}
          (node :use rbBig
            #b-hide-slide
            :with {:icon eye-off :title "Hide\nSlide" :on "{{computed.slide.hidden ?? false}}" :width 46}
            :tooltip "Leave this slide out of the show"
            :onClick hideSlide))
        (node :use rbGroup #g-monitors :with {:title Monitors}
          (column #monitors-col :width 150 :height 72 :gap 2 :alignItems stretch :color transparent
            (node :use rbCheck
              #cb-presenter
              :with {:title "Presenter View" :at state.presenterView :prefix cb-presenter}
              :tooltip "The show in a window of its own, this one with the notes, the next slide and a timer"))))}}}
