;;; views.lisp --- what terminal.lisp shows

;;; Views

(defcomponent forgeTerminal "The forgeTerminal component."
  {}
  `(column#terminal-panel :width "100%" :grow 1 :color @color.panel
     ~(terminal-head)
     ~(repeat [t (filter (fn [it] (= it state.terminals.active)) (map (fn [it] it.id) state.terminals.list)) :key t]
       `(terminal#terminal :session ~t
          :font code
          :width "100%"
          :grow 1
          :color @color.editor
          :textColor @color.text
          :cursorColor @color.accent
          :scrollback 1000
          :focusable true
          :keys {:tab terminalKeepTab "shift+tab" terminalKeepTab}))))

(defview terminal-head "#terminal-head." []
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
