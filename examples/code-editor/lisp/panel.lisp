;;; panel.lisp --- the bottom panel: the problems and the console

;;; Views

(defview bottom-panel "The bottom panel: its tabs, then the problems or the console." []
  `(node#bottomPanel :when ~(and state.panelOpen (not state.zen)) :color @panel :keys {:escape togglePanel}
     (node#panelEdge :anchors {:left 0 :right 0 :top 0} :height 1 :color @border)
     ~(panel-problems)
     (label#panelConsole :text ~(t "panel.console")
       :font small
       :anchors {:left 92 :top 4}
       :height 22
       :width 72
       :align center
       :radius 4
       :cursor hand
       :color ~(if (= state.panelTab "terminal") "@rowSelected" "transparent")
       :textColor ~(if (= state.panelTab "terminal") "@text" "@muted")
       :hover {:textColor @text}
       :onClick showTerminal)
     (label#panelClose :text "×"
       :font default
       :anchors {:right 10 :top 4}
       :width 22
       :height 22
       :align center
       :radius 4
       :textColor @muted
       :cursor hand
       :hover {:textColor @text :color @hover}
       :tooltip ~(t "panel.hideThePanelEscape")
       :onClick togglePanel)
     ~(problems-table)
     (node#noProblems :when ~(and (= state.panelTab "problems") (= (count state.problems) 0))
       :style caption
       :text ~(t "panel.noProblemsNoTodoOr")
       :anchors {:left 20 :top 70})
     ~(terminal-view)))

(defview panel-problems "The problems tab: the table, or a line saying there are none." []
  `(label#panelProblems :text ~(t "panel.problems")
     :font small
     :anchors {:left 10 :top 4}
     :height 22
     :width 78
     :align center
     :radius 4
     :cursor hand
     :color ~(if (= state.panelTab "problems") "@rowSelected" "transparent")
     :textColor ~(if (= state.panelTab "problems") "@text" "@muted")
     :hover {:textColor @text}
     :onClick showProblems))

(defview problems-table "The TODO and FIXME lines, a row each: a click opens the file there." []
  `(table#problemsTable :when ~(= state.panelTab "problems")
     :anchors {:left 8 :right 8 :top 30 :bottom 4}
     :rowHeight 24
     :font code
     :color @panel
     :items ~state.problems
     :value ~state.problemPick
     :columns [{:id severity :title ~(t "panel.severity") :width 96}
               {:id file :title ~(t "bar.file") :width 170}
               {:id line :title ~(t "panel.line") :width 70}
               {:id message :title ~(t "panel.message") :width 560}]
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @rowSelected
     :accentColor @accent
     :onChange openProblem))

(defview terminal-view "The console: its lines, and the command line." []
  `(node#terminal :when ~(= state.panelTab "terminal") :anchors {:left 0 :right 0 :top 30 :bottom 0} :color transparent
     (stack#terminalOutput :anchors {:left 14 :right 14 :top 4 :bottom 30} :gap 0 :justify end :scroll vertical
       ~(repeat [out state.terminal]
         `(node#label-out-t :style terminalLine :text ~out.t :textColor ~(str "@" out.c) :anchors {:left 0 :right 0}
            :height 20)))
     (textarea#command :font code
       :anchors {:left 0 :right 0 :bottom 0}
       :height 26
       :padding 8
       :color @input
       :focused {:color @chrome}
       :textColor @text
       :bind state.command
       :onSubmit runCommand)))
