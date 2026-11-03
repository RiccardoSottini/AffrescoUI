;;; tools.lisp --- the tools bar down the window's left: the tools, the colours, the quick mask and the screen mode
;;;
;;; One column of tool groups (state.tools): each shows its chosen tool, or its first; a right click opens the group's
;;; other tools to its right. Under them, the foreground and background colours, the quick mask, the screen mode.

;;; The bar

(defview tool-bar "The tools bar: the tool groups, then the colours and the modes." []
  `(column#tool-bar :role toolbar
     :label ~(t "tools.label")
     :width 40
     :height "100%"
     :style card
     :alignItems center
     :gap 1
     (node :height 6 :width 1 :style clear)
     ~(repeat [g state.tools :index n] (tool-group g n))
     (node :height 6 :width 1 :style clear)
     ~(colour-chips)
     (node :height 2 :width 1 :style clear)
     (node#quick-mask :use tIcon
       :width 32
       :height 24
       :with {:icon circle-dashed :tip ~(t "tools.quickMask") :run toggleQuickMask :on ~state.doc.options.quickMask})
     ~(screen-mode-button)
     (node#edit-toolbar :use tIcon
       :width 32
       :height 24
       :with {:icon ellipsis :tip ~(t "tools.editToolbar") :off true})))

(defview tool-group "A group of the bar, g its tools and n its place: a dot for an empty one, else its tool." [g n]
  `(node :id ~(str "tool-group-" n)
     :style clear
     :let {:tool ~(or-else (find (fn [it] (= it.id state.doc.tool)) g) (first g))}
     :width 32
     :height ~(if (empty? g) 7 26)
     (node :when ~(empty? g) :x 14 :y 2 :width 3 :height 3 :radius 999 :color @border)
     ~(tool-button g n)))

(defview tool-button "A group's tool, the one chosen or its first: chosen by a click; a right click opens the group's other tools." [g n]
  `(label :id ~(str "tool-" (or-else tool.id n))
     :when ~(not (empty? g))
     :role button
     :label ~(t (str "tool." tool.name))
     :tooltip ~(str (t (str "tool." tool.name))
                    (if tool.key (str " (" tool.key ")") "")
                    (if tool.id "" (t "tools.notAvailable")))
     :disabled ~(not tool.id)
     :selected ~(= tool.id state.doc.tool)
     :icon ~tool.icon
     :font iconsLarge
     :fill true
     :align center
     :radius 8
     :cursor hand
     :textColor ~(if tool.id "@text" "@muted")
     :color ~(if (= tool.id state.doc.tool) "@selected" "transparent")
     :hover {:color ~(if tool.id "@hover" "transparent")}
     :onClick ~(handler setTool {:tool tool.id})
     :onContextMenu ~(handler toggleOpen (str "state.flyout.g" n))
     (node :when ~(= tool.id state.doc.tool) :x 0 :anchors {:top 6 :bottom 6} :width 3 :radius 2 :color @accent)
     (node :when ~(> (count g) 1)
       :width 3
       :height 3
       :radius 999
       :color ~(if (= tool.id state.doc.tool) "@accent" "@muted")
       :anchors {:right 2 :bottom 2})
     (node :use tMenu
       :when ~(> (count g) 1)
       :with {:open ~(str "state.flyout.g" n)
              :placement right
              :icons true
              :mark square
              :width 230
              :items ~(map (fn [it]
                             {:title (t (str "tool." it.name))
                              :icon it.icon
                              :enabled (some? it.id)
                              :checked (= it.id state.doc.tool)
                              :keyLabel (slice (or-else it.key "") (max 0 (- (count (or-else it.key "")) 1)))
                              :run "setTool"
                              :with {:args {:tool it.id}}})
                           g)})))

;;; The colours and the modes

(defview colour-chips "The foreground over the background colour; the arrows swap them, the small pair resets them." []
  `(node#colours :role group :label ~(t "tools.colours") :style clear :width 32 :height 36
     ~(colour-chip "background-colour" 'state.doc.background 11 12 "colour.background")
     (node :x 1 :y 2 :width 21 :height 21 :color @panel :radius 999)
     ~(colour-chip "foreground-colour" 'state.doc.foreground 2 3 "colour.foreground")
     (label#colour-swap :role button
       :label ~(t "tools.swapColours")
       :icon arrow-left-right
       :font small
       :x 22
       :y 0
       :width 10
       :height 10
       :cursor hand
       :tooltip ~(t "tools.swapColours.tip")
       :textColor @muted
       :hover {:textColor @text}
       :onClick swapColours)
     (node#colour-reset :role button
       :label ~(t "tools.defaultColours")
       :style clear
       :x 1
       :y 26
       :width 9
       :height 9
       :cursor hand
       :tooltip ~(t "tools.defaultColours.tip")
       :onClick resetColours
       (node :x 3 :y 3 :width 6 :height 6 :radius 999 :color "#ffffff" :borderWidth 1 :borderColor @muted)
       (node :width 6 :height 6 :radius 999 :color "#000000" :borderWidth 1 :borderColor @muted))))

(defview colour-chip "A colour's round chip at x, y, value its path: a click shows the Colour panel." [id value x y title]
  `(node :id ~id
     :role button
     :label ~(t title)
     :x ~x
     :y ~y
     :width 19
     :height 19
     :radius 999
     :color ~value
     :borderWidth 1
     :borderColor @muted
     :tooltip ~(t title)
     :cursor hand
     :onClick {:run showPanel :with {:args {:slot colour :id colour}}}))

(defview screen-mode-button "The screen mode: a click changes it; a right click shows both modes." []
  `(label#screen-mode :role button
     :label ~(t "tools.screenMode")
     :tooltip ~(t "tools.screenMode.tip")
     :icon monitor
     :width 32
     :height 24
     :align center
     :radius 8
     :cursor hand
     :textColor @text
     :hover {:color @hover}
     :onClick cycleScreenMode
     :onContextMenu {:toggle state.screenMenu}
     (node :width 3 :height 3 :radius 999 :color @muted :anchors {:right 2 :bottom 2})
     (node :use tMenu
       :with {:open state.screenMenu
              :placement right
              :mark square
              :width 250
              :items ~[{:title (t "menu.screenMode.standard") :keyLabel "F" :checked (= state.screenMode "standard")
                        :run "setScreenMode" :with {:args {:mode "standard"}}}
                       {:title (t "menu.screenMode.full") :keyLabel "F" :checked (= state.screenMode "full")
                        :run "setScreenMode" :with {:args {:mode "full"}}}]})))
