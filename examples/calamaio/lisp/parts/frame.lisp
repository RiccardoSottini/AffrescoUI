{:note "Calamaio's frame: a window (fringes, the buffer, a scroll bar and its mode line), the splash screen, the tool bar and the echo area with the minibuffer."
  :components {:window {:params {:w w1}
      :node {:type column
        :grow 1
        :basis 0
        :alignSelf stretch
        :let {:win "{{state.win[w]}}"
          :b "{{state.bufs[state.win[w].buf]}}"
          :on "{{state.sel == w}}"
          :v "{{state.win[w].view}}"}
        :keys {:f10 menu-bar-open
          :"*" {:when "{{state.sel == w && (state.is.on || state.qr.on)}}" :do search-key :with {:k "{{event.key}}"}}}
        :children [{:type row
            :grow 1
            :alignSelf stretch
            :color @bg
            :children [{:width 8 :alignSelf stretch :color @fringe}
              {:grow 1
                :basis 0
                :alignSelf stretch
                :color @bg
                :children [{:when "{{b.kind == 'splash'}}" :use splash :with {:w "{{w}}"}}
                  {:when "{{b.kind == 'customize'}}"
                    :use settingsPage
                    :anchors {:left 0 :top 0 :right 0 :bottom 0}
                    :color @bg
                    :with {:search "{{state.customQuery}}" :query state.customQuery}}
                  (node :when "{{b.kind != 'splash' && b.kind != 'customize'}}"
                    :type editor
                    #"{{'ed' + w}}"
                    :anchors {:left -7 :top -6 :right 0 :bottom 0}
                    :bind "{{'state.bufs.' + win.buf + '.text'}}"
                    :caret "{{'state.win.' + w + '.caret'}}"
                    :viewport "{{'state.win.' + w + '.view'}}"
                    :font code
                    :textColor @fg
                    :color @bg
                    :lineColor @line
                    :selectionColor "{{on && (state.is.on || state.qr.on) ? @isearch : @region}}"
                    :caretColor @cursor
                    :caretStyle "{{ {box: 'block', bar: 'line', hbar: 'underline'}[settings['cursor-type']] }}"
                    :lineNumbers "{{settings['global-display-line-numbers-mode']}}"
                    :lineNumberColor @kit.dim
                    :activeLineNumberColor @fg
                    :wrap "{{settings['visual-line-mode']}}"
                    :brackets "{{settings['show-paren-mode']}}"
                    :rulers "{{settings['display-fill-column-indicator-mode'] ? [settings['fill-column']] : []}}"
                    :guideColor @barLine
                    :readOnly "{{b.readOnly == true}}"
                    :grammar "{{state.modes[b.mode].grammar ?? ''}}"
                    :language "{{state.modes[b.mode].language ?? ''}}"
                    :matchColor "{{settings['search-highlight'] ? @lazy : 'transparent'}}"
                    :bracketColor @paren
                    :onFocus {:set state.sel :to "{{w}}"}
                    :onChange {:set "{{'state.bufs.' + win.buf + '.dirty'}}" :to true}
                    :find "{{on && state.is.on && state.is.text != '' ? {text: state.is.text, caseSensitive: lower(state.is.text) != state.is.text, step: state.is.step, request: state.is.req} : on && state.qr.on ? {text: state.qr.from, caseSensitive: lower(state.qr.from) != state.qr.from, step: state.qr.step, request: state.qr.req, replace: state.qr.replace} : null}}"
                    :keys {:enter {:when "{{state.sel == w && (b.mode == 'dired' || b.mode == 'buffer-menu') && !state.is.on}}"
                        :do listing-visit}
                      :g {:when "{{state.sel == w && (b.mode == 'dired' || b.mode == 'buffer-menu') && !state.is.on}}"
                        :do listing-revert}
                      :q {:when "{{state.sel == w && b.readOnly == true && b.kind != 'splash' && b.mode != 'messages' && !state.is.on && !state.qr.on}}"
                        :do quit-window}
                      :"0" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 0}}
                      :"1" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 1}}
                      :"2" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 2}}
                      :"3" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 3}}
                      :"4" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 4}}
                      :"5" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 5}}
                      :"6" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 6}}
                      :"7" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 7}}
                      :"8" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 8}}
                      :"9" {:when "{{state.arg != null && state.argDigits}}" :do digit-argument :with {:d 9}}})]}
              {:width 8 :alignSelf stretch :color @fringe}
              {:width 10
                :alignSelf stretch
                :color @track
                :let {:lines "{{max(1, v.lines ?? 1)}}"
                  :shown "{{min(1, max(0.05, ((v.last ?? 1) - (v.first ?? 1) + 1) / lines))}}"}
                :children [{:x 2
                    :width 6
                    :radius 3
                    :color @thumb
                    :y "{{fixed(100 * min(1 - shown, ((v.first ?? 1) - 1) / lines), 2)}}%"
                    :height "{{fixed(100 * shown, 2)}}%"}]}]}
          {:type row
            :height 26
            :alignSelf stretch
            :alignItems center
            :gap 8
            :color "{{on ? @ml : @mli}}"
            :let {:first "{{v.first ?? 1}}"
              :last "{{v.last ?? 1}}"
              :lines "{{v.lines ?? 1}}"
              :pos "{{first <= 1 && last >= lines ? 'All' : first <= 1 ? 'Top' : last >= lines ? 'Bot' : floor(100 * (first - 1) / lines) + '%'}}"
              :line "{{b.kind == 'splash' ? 1 : win.caret.line ?? 1}}"
              :column "{{b.kind == 'splash' ? 1 : win.caret.column ?? 1}}"}
            :label "{{'Mode line: ' + b.name + ', ' + state.modes[b.mode].title + (b.dirty ? ', modified' : '') + (b.readOnly ? ', read-only' : '')}}"
            :children [{:width 4 :height 1 :color transparent}
              {:type label
                :fitText true
                :height 18
                :padding 8
                :radius 9
                :color "{{on ? @mlChip : @mliBorder}}"
                :textColor "{{on ? @mlFg : @mliFg}}"
                :runs [{:text "{{b.dirty ? '● ' : ''}}" :color "{{on ? @mlMark : @mliFg}}"}
                  {:text "{{b.name}}" :bold true}]}
              {:when "{{b.readOnly == true}}"
                :type label
                :fitText true
                :runs [{:icon lock :font icons :size 14}]
                :textColor "{{on ? @mlFg : @mliFg}}"
                :tooltip Read-only}
              {:type label
                :fitText true
                :textColor "{{on ? @mlFg : @mliFg}}"
                :text "{{state.modes[b.mode].title + (settings['visual-line-mode'] ? ' · Wrap' : '')}}"}
              {:grow 1 :height 1 :color transparent}
              {:type label
                :fitText true
                :textColor "{{on ? @mlFg : @mliFg}}"
                :text "{{'L' + line + '  C' + (column - 1) + '   ' + pos}}"}
              {:width 6 :height 1 :color transparent}]}]
        :note "A window. Plain keys stay with the editor unless a node around it binds them: the function keys are here."}}
    :splash {:note "The start screen: Calamaio's mark, the places to begin as cards, then the first actions as buttons; every link runs a command."
      :params {:w w1}
      :node {:type column
        :fill 0
        :alignItems center
        :children [{:grow 2 :width 1 :color transparent}
          {:width 84
            :height 84
            :radius 24
            :color @tile
            :children [{:type label :font logo :icon feather :align center :fill 0 :textColor @logo}]}
          {:height 14 :color transparent}
          {:type label :fitText true :runs [{:text Calamaio :size 32 :bold true :color @fg}]}
          {:height 4 :color transparent}
          {:type label
            :fitText true
            :textColor @kit.dim
            :text "A keyboard-first text editor whose keys, menus and looks live in one document."}
          {:height 24 :color transparent}
          {:type grid
            :columns 2
            :gap 10
            :width 560
            :height "{{70 * ceil(length(computed.splashLinks) / 2) - 10}}"
            :color transparent
            :children [{:repeat {:for l :in "{{computed.splashLinks}}"}
                :height 60
                :radius 8
                :color @kit.surface
                :borderWidth 1
                :borderColor @barLine
                :focusable true
                :role button
                :label "{{l.title}}"
                :cursor hand
                :hover {:borderColor @link}
                :focused {:borderColor @link}
                :onClick {:run "{{l.run}}"}
                :children [{:type label
                    :anchors {:left 14 :top 10}
                    :fitText true
                    :runs [{:text "{{l.title}}" :bold true :color @link}]}
                  {:type label :anchors {:left 14 :top 32} :fitText true :textColor @kit.dim :text "{{l.about}}"}
                  {:type label
                    :font code
                    :anchors {:right 12 :top 10}
                    :fitText true
                    :padding 4
                    :radius 4
                    :color @fringe
                    :textColor @kit.dim
                    :text "{{keyLabel(l.key ?? '', 'letters')}}"
                    :visible "{{l.key != null}}"}]}]}
          {:height 18 :color transparent}
          {:type row
            :gap 8
            :fitContent true
            :color transparent
            :children [{:repeat {:for a :in "{{computed.splashActions}}"}
                :type label
                :fitText true
                :padding 9
                :radius 16
                :color @button
                :textColor @barFg
                :focusable true
                :role button
                :label "{{a.title}}"
                :cursor hand
                :hover {:color @barHover}
                :focused {:color @barHover}
                :onClick {:run "{{a.run}}"}
                :runs [{:icon "{{a.icon}}" :font icons :size 15} " " "{{a.title}}"]}]}
          {:height 18 :color transparent}
          {:type row
            :gap 8
            :alignItems center
            :fitContent true
            :color transparent
            :children [(node #splashAtStart
                :type checkbox
                :width 16
                :height 16
                :checked "{{!settings['inhibit-startup-screen']}}"
                :onChange {:set settings.inhibit-startup-screen :to "{{!event.value}}"}
                :label "Show this screen at start"
                :activeColor @link)
              {:type label :fitText true :textColor @fg :text "Show this screen at start"}]}
          {:grow 3 :width 1 :color transparent}
          {:type label
            :fitText true
            :textColor @kit.dim
            :runs ["Calamaio 1.0, an example of AffrescoUI under the Apache License 2.0.  C-g cancels a command you have begun.  "
              {:text "Close this screen" :link splash-close :color @link}]
            :onLink {:run "{{event.link}}"}}
          {:height 14}]}}
    :frameMenuBar {:note "The menu bar: titles as wide as their text, no Alt mnemonics (M-f is a motion here); F10 opens the first menu."
      :node {:type row
        :height 30
        :fitContent width
        :gap 2
        :alignItems center
        :color @bar
        :role menubar
        :arrows horizontal
        :children [{:width 2}
          {:repeat {:for m :in "{{computed.menuBar}}"}
            :type label
            :text "{{m.title}}"
            :textColor @barFg
            :padding 9
            :fitText true
            :height 28
            :radius 6
            :color @bar
            :focusable true
            :role menuitem
            :label "{{m.title}}"
            :isOpen "{{state.menuBar[m.name] == true}}"
            :hover {:color @barHover}
            :focused {:color @barHover}
            :onClick {:toggle "{{'state.menuBar.' + m.name}}"}
            :children [{:use menu :with {:items "{{m.items}}" :open "{{'state.menuBar.' + m.name}}" :width 260}}]}]}}
    :toolBar {:node {:type row
        :height 34
        :fitContent width
        :color @bar
        :role toolbar
        :label "Tool bar"
        :gap 2
        :alignItems center
        :children [{:width 4 :height 1 :color transparent}
          {:repeat {:for t :in "{{computed.toolItems}}"}
            :width "{{t.icon ? 32 : 9}}"
            :height 30
            :radius 6
            :color @bar
            :role "{{t.icon ? 'button' : 'separator'}}"
            :label "{{t.tip ?? ''}}"
            :tooltip "{{t.tip ?? ''}}"
            :focusable "{{t.icon != null}}"
            :disabled "{{t.icon == null}}"
            :hover {:color "{{t.icon ? @barHover : @bar}}"}
            :onClick [{:if "{{t.run != null}}" :then [{:run "{{t.run}}"}]}]
            :children [{:when "{{t.icon != null}}"
                :type label
                :font icons
                :icon "{{t.icon}}"
                :align center
                :fill 0
                :textColor @icon}
              {:when "{{t.icon == null}}" :x 4 :width 1 :anchors {:top 6 :bottom 6} :color @barLine}]}]}}
    :echoArea {:node {:height 26
        :color @bar
        :children [{:when "{{!state.mb.open}}"
            :type label
            :font code
            :textColor @fg
            :anchors {:left 4 :top 5}
            :fitText true
            :text "{{context.chord != null ? keyLabel(context.chord, 'letters') + '-' : state.is.on ? (state.is.text != '' && (state.win[state.sel].caret.matches ?? 0) == 0 ? 'Failing ' : '') + (state.is.back ? 'Search backward: ' : 'Search: ') + state.is.text : state.qr.on ? 'Replace ' + state.qr.from + ' with ' + state.qr.to + '? (y, n, !, q, . or C-h) ' : state.keyread.on ? 'Key to describe: ' + (state.keyread.seq == '' ? '' : keyLabel(state.keyread.seq, 'letters') + '-') : state.echo}}"}
          {:when "{{state.mb.open}}"
            :type row
            :anchors {:left 4 :right 0 :top 4}
            :height 19
            :children [{:type label :font code :textColor @prompt :fitText true :text "{{state.mb.prompt}}"}
              (node #mb
                :type textarea
                :grow 0
                :height 19
                :font code
                :textColor @fg
                :color @bar
                :bind state.mb.text
                :autofocus true
                :onSubmit minibuffer-exit
                :keys {:tab minibuffer-complete
                  :"alt+p" {:do minibuffer-history :with {:step 1}}
                  :up {:do minibuffer-history :with {:step 1}}
                  :"alt+n" {:do minibuffer-history :with {:step -1}}
                  :down {:do minibuffer-history :with {:step -1}}
                  :"ctrl+g" keyboard-quit}
                :onChange {:set state.mb.comp :to nil}
                :width "{{max(30, length(state.mb.text) * 8.4 + 10)}}")
              {:type label :font code :textColor @prompt :fitText true :text "{{state.mb.note}}"}]}
          (node :when "{{state.keyread.on}}"
            #keyReader
            :focusable true
            :width 1
            :height 1
            :label "Key reader"
            :keys {:"*" {:do key-read :with {:k "{{event.key}}"}}})]}}
    :completions {:note "The *Completions* window: the candidates in columns, the typed part in its own face; a click chooses one."
      :node {:type column
        :height "{{min(260, 46 + 19 * ceil(length(state.mb.comp) / 4))}}"
        :color @bg
        :let {:t "{{length(state.mb.text)}}"}
        :children [{:grow 1
            :alignSelf stretch
            :color @bg
            :children [{:type label
                :font code
                :textColor @fg
                :anchors {:left 12 :top 3}
                :fitText true
                :text "{{plural(length(state.mb.comp), 'match', 'matches') + ': Tab completes further, a click chooses'}}"}
              {:repeat {:for c :in "{{state.mb.comp}}" :index i}
                :x "{{fixed(25 * (i % 4), 2)}}%"
                :y "{{22 + 19 * floor(i / 4)}}"
                :width "25%"
                :height 19
                :hover {:color @hl}
                :cursor hand
                :onClick {:run minibuffer-choose :with {:value "{{c}}"}}
                :children [{:type label
                    :font code
                    :textColor @fg
                    :anchors {:left 12 :top 1}
                    :fitText true
                    :runs [{:text "{{slice(c, 0, t)}}" :color @compCommon}
                      {:text "{{slice(c, t, t + 1)}}" :bold true :color @compFirst}
                      "{{slice(c, t + 1)}}"]}]
                :color @bg}]}
          {:type row
            :height 26
            :alignSelf stretch
            :alignItems center
            :gap 8
            :color @mli
            :children [{:width 4 :height 1 :color transparent}
              {:type label
                :fitText true
                :height 18
                :padding 8
                :radius 9
                :color @mliBorder
                :textColor @mliFg
                :runs [{:text "*Completions*" :bold true}]}
              {:type label :fitText true :textColor @mliFg :text "Completion List"}
              {:grow 1 :height 1 :color transparent}
              {:type label :fitText true :textColor @mliFg :text All}
              {:width 6 :height 1 :color transparent}]}]}}}}
