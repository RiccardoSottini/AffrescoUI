{:note "Calamaio's further editing commands (the region's case, comments, transposing, the whole buffer as the region), the minor modes Options turns on and off, and Customize, the variables in a buffer of their own."
  :actions {:region-command [{:if "{{!computed.text}}" :then [{:stop true}]}
      {:if "{{computed.buf.readOnly}}" :then [{:run read-only} {:stop true}]}
      {:if "{{need && (state.win[state.sel].caret.selected ?? 0) == 0}}"
        :then [{:run message :with {:m "The region is empty: C-SPC sets the mark"}} {:stop true}]}
      {:edit "{{c}}" :in "{{computed.ed}}" :prefix "{{state.modes[computed.buf.mode].comment ?? '#'}}"}
      {:set state.mark.on :to false}]
    :upcase-region {:title "Upcase Region"
      :category Editing
      :description "Turns the region's letters into capitals."
      :do {:run region-command :with {:c upperCase :need true}}}
    :downcase-region {:title "Downcase Region"
      :category Editing
      :description "Turns the region's letters into small letters."
      :do {:run region-command :with {:c lowerCase :need true}}}
    :capitalize-region {:title "Capitalize Region"
      :category Editing
      :description "Starts each word of the region with a capital."
      :do {:run region-command :with {:c titleCase :need true}}}
    :sort-lines {:title "Sort Lines"
      :category Editing
      :description "Sorts the region's lines alphabetically."
      :do {:run region-command :with {:c sortLines :need true}}}
    :comment-line {:title "Comment Line"
      :category Editing
      :description "Comments or uncomments the line, or the region's lines, with the mode's comment marker."
      :do {:run region-command :with {:c toggleComment :need false}}}
    :transpose-chars {:title "Transpose Chars"
      :category Editing
      :description "Swaps the characters before and after point."
      :do {:run region-command :with {:c transpose :need false}}}
    :open-line {:title "Open Line"
      :category Editing
      :description "Puts a line break after point, leaving point where it is."
      :do [{:if "{{!computed.text}}" :then [{:stop true}]}
        {:if "{{computed.buf.readOnly}}" :then [{:run read-only} {:stop true}]}
        {:edit type :text "\n" :in "{{computed.ed}}"}
        {:press left :in "{{computed.ed}}"}]}
    :mark-whole-buffer {:title "Select All"
      :category Editing
      :description "Puts point at the start of the buffer and the mark at its end, so the region is all of it."
      :do [{:if "{{!computed.text}}" :then [{:stop true}]}
        {:press "cmd+down" :in "{{computed.ed}}"}
        {:press "shift+cmd+up" :in "{{computed.ed}}"}
        {:set state.mark
          :to "{{ {on: true, line: length(split(computed.buf.text ?? '', '\\n')), column: length(split(computed.buf.text ?? '', '\\n')[length(split(computed.buf.text ?? '', '\\n')) - 1]) + 1} }}"}
        {:run message :with {:m "Mark set"}}]}
    :exchange-point-and-mark {:title "Exchange Point and Mark"
      :category Editing
      :description "Puts point where the mark is and the mark where point was, and activates the region."
      :do [{:if "{{!computed.text}}" :then [{:stop true}]}
        {:run exchange-at
          :with {:m "{{ {line: state.mark.line, column: state.mark.column} }}"
            :p "{{ {line: state.win[state.sel].caret.line, column: state.win[state.sel].caret.column} }}"}}]}
    :exchange-at [{:edit select
        :in "{{computed.ed}}"
        :line "{{p.line}}"
        :column "{{p.column}}"
        :toLine "{{m.line}}"
        :toColumn "{{m.column}}"}
      {:set state.mark :to "{{ {on: true, line: p.line, column: p.column} }}"}]
    :display-line-numbers-mode {:title "Line Numbers"
      :category Options
      :description "Shows or hides the line numbers in the margin."
      :do [{:toggle settings.global-display-line-numbers-mode}
        {:run mode-said :with {:name "Line numbers" :on "{{settings['global-display-line-numbers-mode']}}"}}]}
    :visual-line-mode {:title "Wrap Long Lines"
      :category Options
      :description "Wraps the lines that don't fit the window, or lets them run past its edge."
      :do [{:toggle settings.visual-line-mode}
        {:run mode-said :with {:name "Line wrapping" :on "{{settings['visual-line-mode']}}"}}]}
    :show-paren-mode {:title "Match Brackets"
      :category Options
      :description "Highlights the bracket that pairs with the one at point, or stops."
      :do [{:toggle settings.show-paren-mode}
        {:run mode-said :with {:name "Bracket matching" :on "{{settings['show-paren-mode']}}"}}]}
    :menu-bar-mode {:title "Menu Bar"
      :category Options
      :description "Shows or hides the menu bar; F10 still opens its menus."
      :do [{:toggle settings.menu-bar-mode}
        {:run mode-said :with {:name "The menu bar" :on "{{settings['menu-bar-mode']}}"}}]}
    :tool-bar-mode {:title "Tool Bar"
      :category Options
      :description "Shows or hides the tool bar."
      :do [{:toggle settings.tool-bar-mode}
        {:run mode-said :with {:name "The tool bar" :on "{{settings['tool-bar-mode']}}"}}]}
    :mode-said [{:run message :with {:m "{{name + (on ? ' on' : ' off')}}"}}]
    :customize {:title Customize
      :category Options
      :description "Shows Calamaio's variables in a buffer, to change and reset them."
      :do {:run customize-show :with {:q ""}}}
    :customize-startup {:title "Customize Startup"
      :category Options
      :description "Shows the variables of what Calamaio does at start."
      :do {:run customize-show :with {:q startup}}}
    :customize-show [{:set state.customQuery :to "{{q}}"}
      {:run listing-show
        :with (node #bc :buffer {:name "*Customize*" :mode customize :kind customize :readOnly true :text ""})}]}}
