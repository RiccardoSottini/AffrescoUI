;;; views.lisp --- the panel under the editors: its tabs and buttons, Problems, Output, a terminal or the debug console

;;; The panel

(defcomponent forgePanel "The panel: its head, then the page of the tab chosen."
  {}
  `(column#panel :role region :label ~(t "panel.label") :color @color.panel
     ~(panel-head)
     (label#problems-empty :when ~(and (= state.panelTab "problems") (empty? state.problems))
       :width "100%"
       :padding 12
       :text ~(t "problems-empty")
       :textColor @color.text)
     (table#problems :when ~(and (= state.panelTab "problems") (not (empty? state.problems)))
       :label ~(t "problems.label")
       :width "100%"
       :grow 1
       :items ~computed.problemRows
       :rowHeight 22
       :color @color.panel
       :textColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :columns [{:id message :title ~(t "problems.message") :width 420}
                 {:id file :title ~(t "problems.file") :width 160}
                 {:id line :title ~(t "problems.line") :width 60}]
       :onOpen goToProblem)
     (list#output :when ~(= state.panelTab "output")
       :role log
       :label ~(t "output.label")
       :width "100%"
       :grow 1
       :items ~computed.outputRows
       :rowHeight 20
       :font code
       :color @color.panel
       :textColor @color.text
       :selectedBackground @color.selection)
     (node :when ~(= state.panelTab "terminal") :use forgeTerminal)
     (node :when ~(= state.panelTab "debugConsole") :use forgeDebugConsole)))

(defview panel-tabs "The panel's tabs; Problems says how many there are." []
  `(tabs#panel-tabs :role tablist
     :label ~(t "panel-tabs.label")
     :width 420
     :height 32
     :bind state.panelTab
     :color @color.panel
     :textColor @color.muted
     :activeTextColor @color.text
     :activeBackground @color.panel
     :accentColor @color.accent
     :tabs ~[{:id "problems"
              :label (str (t "panel.problems") (if (> (count state.problems) 0) (str " " (count state.problems)) ""))
              :closable false}
             {:id "output" :label (t "panel.output") :closable false}
             {:id "debugConsole" :label (t "panel.debugConsole") :closable false}
             {:id "terminal" :label (t "panel.terminal") :closable false}]))

(defview panel-head "The panel's head: its tabs, then its buttons: a new terminal, clear, maximize and hide." []
  `(row#panel-head :width "100%" :height 32 :alignItems center
     ~(panel-tabs)
     (node#panel-gap :grow 1)
     (node#panel-new-terminal :use forgeTitleButton
       :when ~(= state.panelTab "terminal")
       :with {:icon plus :tip ~(t "panel-new-terminal.tip")}
       :onClick terminalNew)
     (node#panel-clear-output :use forgeTitleButton
       :when ~(= state.panelTab "output")
       :with {:icon list-x :tip ~(t "panel-clear-output.tip")}
       :onClick clearOutput)
     (node#panel-maximize :use forgeTitleButton
       :with {:icon ~(if state.layout.panelMax "chevron-down" "chevron-up")
              :tip ~(if state.layout.panelMax (t "panel-maximize.restore") (t "panel-maximize.tip"))}
       :onClick toggleMaximizedPanel)
     (node#panel-close :use forgeTitleButton :with {:icon x :tip ~(t "panel-close.tip")} :onClick togglePanel)
     (node#panel-end :width 8)))
