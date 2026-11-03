;;; sidebar.lisp --- TODO

(defcomponent viewportSidebar "The viewportSidebar component."
  {}
  `(node#sidebar :anchors {:right 6 :top 40 :bottom 6} :width 236 :color @header :radius 8 :role region :label "Sidebar"
     ~(sidebar-tabs)
     (column :anchors {:left 6 :right 6 :top 34 :bottom 0} :gap 4 :color transparent
       (node :when ~(= state.sidebarTab "Item") :use transformPanel :with {:narrow true})
       (node :when ~(= state.sidebarTab "Tool") :use panelHeader :with {:title "Active Tool" :key activeTool})
       ~(tool-panel)
       (node :when ~(= state.sidebarTab "View") :use panelHeader :with {:title "View" :key viewPanel})
       ~(view-panel)
       (node :when ~(= state.sidebarTab "View") :use panelHeader :with {:title "3D Cursor" :key cursorPanel})
       ~(cursor-panel))))

(defcomponent transformPanel "The transformPanel component."
  {:narrow false}
  `(column :width "100%" :height ~(if state.panels.transform 266 24) :gap 0 :color transparent
     (node :use panelHeader :with {:title "Transform" :key transform})
     (node :when ~(and state.panels.transform (some? computed.activeNode))
       :width "100%"
       :height 242
       :color @panel
       :radius {:bottomLeft 4 :bottomRight 4}
       ~(transform-panel-column))))

(defview sidebar-tabs "#sidebar-tabs." []
  `(row#sidebar-tabs :anchors {:left 6 :right 6 :top 6}
     :height 22
     :color @panelBack
     :radius 6
     :gap 0
     :role tablist
     :arrows horizontal
     ~(repeat [tab ["Item" "Tool" "View"]]
       `(node :grow 1
          :height 22
          :role tab
          :focusable true
          :label ~tab
          :selected ~(= state.sidebarTab tab)
          :color ~(if (= state.sidebarTab tab) "@widget" "transparent")
          :hover {:color @widgetHover}
          :radius 6
          :onClick {:set state.sidebarTab :to ~tab}
          (label :text ~tab :textColor ~(if (= state.sidebarTab tab) "@text" "@dim") :align center :fill true)))))

(defview tool-panel "#tool-panel." []
  `(node#tool-panel :when ~(and (= state.sidebarTab "Tool") state.panels.activeTool)
     :width "100%"
     :height 96
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 2 :color transparent
       (label#tool-panel-name :text ~computed.toolName
         :textColor @text
         :height 20
         :width "100%"
         :align {:horizontal start :vertical center})
       (node :use checkRow :with {:id tool-snap :label "Snap" :path state.snap :checked ~state.snap})
       (node :use checkRow
         :with {:id tool-gizmos :label "Tool gizmos" :path state.toolGizmos :checked ~(not= state.toolGizmos false)})
       (label :text ~(str "Orientation: " state.orientation "   Pivot: " state.pivot)
         :textColor @dim
         :height 20
         :width "100%"
         :align {:horizontal start :vertical center}))))

(defview view-panel "#view-panel." []
  `(node#view-panel :when ~(and (= state.sidebarTab "View") state.panels.viewPanel)
     :width "100%"
     :height 56
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 4 :color transparent
       (node :use numberField
         :with {:label "Focal Length"
                :key lens
                :value ~computed.lens
                :step 0.5
                :text ~(str (fixed computed.lens 0) " mm")
                :first true
                :last true})
       (node :use checkRow
         :with {:id view-ortho
                :label "Orthographic"
                :path state.view.camera.orthographic
                :checked ~(= state.view.camera.orthographic true)}))))

(defview cursor-panel "#cursor-panel." []
  `(node#cursor-panel :when ~(and (= state.sidebarTab "View") state.panels.cursorPanel)
     :width "100%"
     :height 102
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 6 :color transparent
       (node :use vectorField :with {:title "Location" :values ~state.cursor :key cursor :unit " m" :step 0.01})
       (node#cursor-reset :height 20
         :width "100%"
         :color @widget
         :hover {:color @widgetHover}
         :radius 4
         :role button
         :focusable true
         :label "Reset the 3D cursor"
         :tooltip "Put the 3D cursor back at the origin"
         :onClick {:set state.cursor :to ~[0 0 0]}
         (label :text "Reset to World Origin" :textColor @text :align center :fill true)))))

(defview rotation-mode "#rotation-mode." []
  `(node#rotation-mode :anchors {:left "41%" :right 22 :top 0 :bottom 0}
     :color @menuWidget
     :radius 4
     :role button
     :label "Rotation mode: XYZ Euler"
     :disabled true
     :tooltip "Other rotation modes (quaternion, axis angle) aren't available yet"
     (label :text "XYZ Euler"
       :textColor @dim
       :align {:horizontal start :vertical center}
       :anchors {:left 7 :top 0 :bottom 0 :right 14})
     (label :font
            iconsSmall
            :icon
            chevron-down
            :text
            ""
            :textColor
            @dim
            :anchors
            {:right 2 :top 0 :bottom 0}
            :width
            12)))

(defview transform-panel-column "#transform-panel-column." []
  `(column :fill 6 :gap 6 :color transparent
     (node :use vectorField
       :with {:title "Location"
              :values ~computed.activeNode.translation
              :key translation
              :unit " m"
              :step 0.01
              :lockable true})
     (node :use vectorField
       :with {:title "Rotation"
              :values ~computed.activeNode.rotationDegrees
              :key rotationDegrees
              :unit "°"
              :step 0.5
              :digits 1
              :lockable true})
     (node :height 20 :width "100%" :color transparent
       (label :text "Mode"
         :textColor @dim
         :align {:horizontal end :vertical center}
         :anchors {:left 0 :top 0 :bottom 0}
         :width "38%")
       ~(rotation-mode))
     (node :use vectorField
       :with {:title "Scale"
              :values ~computed.activeNode.scale
              :key scale
              :unit ""
              :step 0.005
              :digits 3
              :exact true
              :lockable true})))

