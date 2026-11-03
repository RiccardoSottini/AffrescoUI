;;; views.lisp --- the Terminal tab: the terminals' tabs, and the one shown

;;; The terminals

(defcomponent forgeTerminal "The Terminal tab: a tab per terminal, then the active one's shell."
  {}
  `(column#terminal-panel :width "100%" :grow 1 :color @color.panel
     ~(terminal-head)
     ~(repeat [session (filter (fn [it] (= it state.terminals.active)) (map (fn [it] it.id) state.terminals.list))
               :key session]
       `(terminal#terminal :session ~session
          :font code
          :width "100%"
          :grow 1
          :color @color.editor
          :textColor @color.text
          :cursorColor @color.accent
          :scrollback 1000
          :focusable true
          :keys {:tab terminalKeepTab "shift+tab" terminalKeepTab}))))

(defview terminal-head "The terminals' tabs, each closable, and a button for a new one." []
  `(row#terminal-head :width "100%" :height 28 :alignItems center
     (tabs#terminal-tabs :grow 1
       :height 28
       :bind state.terminals.active
       :color @color.panel
       :textColor @color.muted
       :activeTextColor @color.text
       :activeBackground @color.panel
       :accentColor @color.accent
       :tabs ~state.terminals.list
       :onClose ~(handler terminalClose event.id))
     (label#terminal-new :icon plus
       :textColor @color.muted
       :padding 8
       :fitText true
       :cursor hand
       :tooltip ~(t "terminal-new.tip")
       :hover {:color @color.hover}
       :onClick terminalNew)))
