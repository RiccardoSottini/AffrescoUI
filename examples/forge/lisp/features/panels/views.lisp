;;; views.lisp --- what panels.lisp shows

;;; Views

(defcomponent forgePanel "The forgePanel component."
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
       :onOpen {:run goToProblem})
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

(defview panel-tabs-view "#panel-tabs." []
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
              :label (str "Problems" (if (> (count state.problems) 0) (str " " (count state.problems)) ""))
              :closable false}
             {:id "output" :label "Output" :closable false}
             {:id "debugConsole" :label "Debug Console" :closable false}
             {:id "terminal" :label "Terminal" :closable false}]))

(defview panel-head "#panel-head." []
  `(row#panel-head :width "100%" :height 32 :alignItems center
     ~(panel-tabs-view)
     {:id panel-gap :grow 1}
     (node#panel-new-terminal :use forgeTitleButton
       :when ~(= state.panelTab "terminal")
       :with {:icon plus :tip ~(t "panel-new-terminal.tip")}
       :onClick {:run terminalNew})
     (node#panel-clear-output :use forgeTitleButton
       :when ~(= state.panelTab "output")
       :with {:icon list-x :tip ~(t "panel-clear-output.tip")}
       :onClick {:run clearOutput})
     (node#panel-maximize :use forgeTitleButton
       :with {:icon ~(if state.layout.panelMax "chevron-down" "chevron-up")
              :tip ~(if state.layout.panelMax "Restore Panel Size" "Maximize Panel Size")}
       :onClick {:run toggleMaximizedPanel})
     (node#panel-close :use forgeTitleButton :with {:icon x :tip ~(t "panel-close.tip")} :onClick {:run togglePanel})
     {:id panel-end :width 8}))
