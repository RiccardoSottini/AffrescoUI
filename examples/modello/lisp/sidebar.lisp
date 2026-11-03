;;; sidebar.lisp --- the viewport's sidebar (N): the active object's transform, the active tool, the view and the
;;; 3D cursor, in three tabs

;;; The sidebar

(defcomponent viewportSidebar "The sidebar: its tabs, then the panels of the tab chosen (state.sidebarTab)." {}
  `(node#sidebar
     :anchors {:right 6 :top 40 :bottom 6}
     :width 236
     :color @header
     :radius 8
     :role region
     :label ~(t "sidebar.label")
     ~(sidebar-tabs)
     (column :anchors {:left 6 :right 6 :top 34 :bottom 0} :gap 4 :color transparent
       (node :when ~(= state.sidebarTab "Item") :use transformPanel :with {:narrow true})
       (node
         :when ~(= state.sidebarTab "Tool")
         :use panelHeader
         :with {:title ~(t "sidebar.activeTool") :key activeTool})
       ~(tool-panel)
       (node :when ~(= state.sidebarTab "View") :use panelHeader :with {:title ~(t "sidebar.view") :key viewPanel})
       ~(view-panel)
       (node :when ~(= state.sidebarTab "View") :use panelHeader :with {:title ~(t "sidebar.cursor") :key cursorPanel})
       ~(cursor-panel))))

(defview sidebar-tabs "The tabs: Item, Tool and View." []
  `(row#sidebar-tabs
     :anchors {:left 6 :right 6 :top 6}
     :height 22
     :color @panelBack
     :radius 6
     :gap 0
     :role tablist
     :arrows horizontal
     ~(repeat [tab ["Item" "Tool" "View"]] (sidebar-tab tab))))

(defview sidebar-tab "A tab of the sidebar; tab is what state.sidebarTab holds while it is chosen." [tab]
  `(node :grow 1
     :height 22
     :role tab
     :focusable true
     :label ~(t (str "sidebar.tab." tab))
     :selected ~(= state.sidebarTab tab)
     :color ~(if (= state.sidebarTab tab) "@widget" "transparent")
     :hover {:color @widgetHover}
     :radius 6
     :onClick ~(handler chooseSidebarTab tab)
     (label
       :text ~(t (str "sidebar.tab." tab))
       :textColor ~(if (= state.sidebarTab tab) "@text" "@dim")
       :align center
       :fill true)))

;;; The Tool and View tabs

(defview tool-panel "The active tool: its name, snapping, its gizmos, and the orientation and pivot it uses." []
  `(node#tool-panel :when ~(and (= state.sidebarTab "Tool") state.panels.activeTool)
     :width "100%"
     :height 96
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 2 :color transparent
       (label#tool-panel-name
         :text ~computed.toolName
         :textColor @text
         :height 20
         :width "100%"
         :align {:horizontal start :vertical center})
       (node :use checkRow :with {:id tool-snap :label ~(t "sidebar.snap") :path state.snap :checked ~state.snap})
       (node :use checkRow
         :with {:id tool-gizmos :label ~(t "sidebar.toolGizmos") :path state.toolGizmos :checked ~(not= state.toolGizmos false)})
       (label :text ~(t "sidebar.toolInfo" {:orientation state.orientation :pivot state.pivot})
         :textColor @dim
         :height 20
         :width "100%"
         :align {:horizontal start :vertical center}))))

(defview view-panel "The view: its focal length, and whether it is orthographic." []
  `(node#view-panel :when ~(and (= state.sidebarTab "View") state.panels.viewPanel)
     :width "100%"
     :height 56
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 4 :color transparent
       (node :use numberField
         :with {:label ~(t "sidebar.focalLength")
                :key lens
                :value ~computed.lens
                :step 0.5
                :text ~(t "unit.mm" {:value (fixed computed.lens 0)})
                :first true
                :last true})
       (node :use checkRow
         :with {:id view-ortho
                :label ~(t "sidebar.orthographic")
                :path state.view.camera.orthographic
                :checked ~(= state.view.camera.orthographic true)}))))

(defview cursor-panel "The 3D cursor: where it is, and a button that puts it back at the origin." []
  `(node#cursor-panel :when ~(and (= state.sidebarTab "View") state.panels.cursorPanel)
     :width "100%"
     :height 102
     :color @panel
     :radius {:bottomLeft 4 :bottomRight 4}
     (column :fill 6 :gap 6 :color transparent
       (node
         :use vectorField
         :with {:title ~(t "transform.location") :values ~state.cursor :key cursor :unit ~(t "unit.m") :step 0.01})
       (node#cursor-reset :height 20
         :width "100%"
         :color @widget
         :hover {:color @widgetHover}
         :radius 4
         :role button
         :focusable true
         :label ~(t "sidebar.resetCursor")
         :tooltip ~(t "sidebar.resetCursorTip")
         :onClick {:set state.cursor :to [0 0 0]}
         (label :text ~(t "sidebar.resetCursorButton") :textColor @text :align center :fill true)))))

;;; The Item tab: the transform

(defcomponent transformPanel "The active object's location, rotation (and its mode) and scale, each item lockable." {:narrow false}
  `(column :width "100%" :height ~(if state.panels.transform 266 24) :gap 0 :color transparent
     (node :use panelHeader :with {:title ~(t "transform.title") :key transform})
     (node :when ~(and state.panels.transform (some? computed.activeNode))
       :width "100%"
       :height 242
       :color @panel
       :radius {:bottomLeft 4 :bottomRight 4}
       ~(transform-fields))))

(defview transform-fields "The location, rotation, rotation mode and scale fields." []
  `(column :fill 6 :gap 6 :color transparent
     (node :use vectorField
       :with {:title ~(t "transform.location")
              :values ~computed.activeNode.translation
              :key translation
              :unit ~(t "unit.m")
              :step 0.01
              :lockable true})
     (node :use vectorField
       :with {:title ~(t "transform.rotation")
              :values ~computed.activeNode.rotationDegrees
              :key rotationDegrees
              :unit "°"
              :step 0.5
              :digits 1
              :lockable true})
     (node :height 20 :width "100%" :color transparent
       ~(caption "transform.mode")
       ~(rotation-mode))
     (node :use vectorField
       :with {:title ~(t "transform.scale")
              :values ~computed.activeNode.scale
              :key scale
              :unit ""
              :step 0.005
              :digits 3
              :exact true
              :lockable true})))

(defview rotation-mode "The rotation mode: XYZ Euler, the one there is for now." []
  `(node#rotation-mode :anchors {:left "41%" :right 22 :top 0 :bottom 0}
     :color @menuWidget
     :radius 4
     :role button
     :label ~(t "transform.rotationMode")
     :disabled true
     :tooltip ~(t "transform.rotationModeTip")
     (label
       :text ~(t "transform.euler")
       :textColor @dim
       :align {:horizontal start :vertical center}
       :anchors {:left 7 :top 0 :bottom 0 :right 14})
     (label
       :font iconsSmall
       :icon chevron-down
       :text ""
       :textColor @dim
       :anchors {:right 2 :top 0 :bottom 0}
       :width 12)))
