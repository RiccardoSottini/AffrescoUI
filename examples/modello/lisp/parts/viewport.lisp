{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Modello's 3D viewport: its header (editor type, mode, menus, orientation, pivot, snapping, visibility, gizmos, overlays, X-ray and shading), the scene, the toolbar on the left, the navigation gizmo and its buttons on the right, the view's name, the N sidebar, and hButton, the header button every editor uses."
  :computed {:nav "{{ { cy: cos(state.view.camera.yaw), sy: sin(state.view.camera.yaw), cp: cos(state.view.camera.pitch), sp: sin(state.view.camera.pitch) } }}"
    :navAxes "{{sort([{ name: 'x', label: 'X', x: computed.nav.cy, y: computed.nav.sp * computed.nav.sy, d: computed.nav.cp * computed.nav.sy, color: '#e8604c', view: 'right' }, { name: '-x', label: '', x: -computed.nav.cy, y: -computed.nav.sp * computed.nav.sy, d: -computed.nav.cp * computed.nav.sy, color: '#e8604c40', view: 'left' }, { name: 'y', label: 'Y', x: computed.nav.sy, y: -computed.nav.sp * computed.nav.cy, d: -computed.nav.cp * computed.nav.cy, color: '#4cbf7a', view: 'back' }, { name: '-y', label: '', x: -computed.nav.sy, y: computed.nav.sp * computed.nav.cy, d: computed.nav.cp * computed.nav.cy, color: '#4cbf7a40', view: 'front' }, { name: 'z', label: 'Z', x: 0, y: -computed.nav.cp, d: computed.nav.sp, color: '#5b74f0', view: 'top' }, { name: '-z', label: '', x: 0, y: computed.nav.cp, d: -computed.nav.sp, color: '#5b74f040', view: 'bottom' }], 'd')}}"
    :viewName "{{state.inCamera ? 'Camera Perspective' : (abs(abs(state.view.camera.pitch) - pi() / 2) < 0.001 ? (state.view.camera.pitch > 0 ? 'Top' : 'Bottom') : abs(state.view.camera.pitch) > 0.001 ? 'User' : abs(state.view.camera.yaw) < 0.001 ? 'Front' : abs(state.view.camera.yaw - pi() / 2) < 0.001 ? 'Right' : abs(state.view.camera.yaw + pi() / 2) < 0.001 ? 'Left' : abs(abs(state.view.camera.yaw) - pi()) < 0.001 ? 'Back' : 'User') + (state.view.camera.orthographic ? ' Orthographic' : ' Perspective')}}"
    :tools "{{[[{ id: 'select', icon: 'mouse-pointer-2', title: 'Select Box', key: 'W' }], [{ id: 'cursor', icon: 'crosshair', title: 'Cursor', key: '' }], [{ id: 'move', icon: 'move', title: 'Move', key: 'G' }, { id: 'rotate', icon: 'rotate-cw', title: 'Rotate', key: 'R' }, { id: 'scale', icon: 'scale-3-d', title: 'Scale', key: 'S' }, { id: 'transform', icon: 'move-3-d', title: 'Transform', key: '', disabled: true }], [{ id: 'annotate', icon: 'pencil', title: 'Annotate', key: '', disabled: true }, { id: 'measure', icon: 'ruler', title: 'Measure', key: '' }], [{ id: 'add', icon: 'box', title: 'Add Cube', key: '' }]]}}"
    :toolName "{{find(flat(computed.tools), it.id == state.tool).title}}"
    :lens "{{18 / tan(state.view.camera.fieldOfView / 2)}}"
    :modeItems "{{[{ title: 'Object Mode', icon: 'box', checked: state.mode == 'object', run: 'setMode', with: { args: { mode: 'object' } } }, { title: 'Edit Mode', icon: 'pen-tool', enabled: false, keyLabel: 'Not yet' }]}}"
    :orientationItems "{{map(['Global', 'Local', 'Normal', 'Gimbal', 'View', 'Cursor'], contains(['Gimbal', 'View'], it) ? { title: it, enabled: false, keyLabel: 'Not yet' } : { title: it, checked: state.orientation == lower(it), run: 'setOption', with: { args: { key: 'orientation', value: lower(it) } } })}}"
    :pivotItems "{{map([['bounds', 'Bounding Box Center'], ['cursor', '3D Cursor'], ['individual', 'Individual Origins'], ['median', 'Median Point'], ['active', 'Active Element']], it[0] == 'individual' ? { title: it[1], enabled: false, keyLabel: 'Not yet' } : { title: it[1], checked: state.pivot == it[0], run: 'setOption', with: { args: { key: 'pivot', value: it[0] } } })}}"
    :snapItems "{{concat(map(['Vertex', 'Edge', 'Face'], { title: it, enabled: false, keyLabel: 'Not yet' }), [{ separator: true }], map([['fine', 'Increment 0.1 m, 1°'], ['unit', 'Increment 1 m, 5°'], ['coarse', 'Increment 10 m, 15°']], { title: it[1], checked: (state.snapStep ?? 'unit') == it[0], run: 'setOption', with: { args: { key: 'snapStep', value: it[0] } } }))}}"
    :snapSteps "{{state.snapStep == 'fine' ? { translate: 0.1, rotate: 1, scale: 0.01 } : state.snapStep == 'coarse' ? { translate: 10, rotate: 15, scale: 0.5 } : { translate: 1, rotate: 5, scale: 0.1 }}}"
    :kinds "{{[['mesh', 'Meshes'], ['light', 'Lights'], ['camera', 'Cameras'], ['empty', 'Empties']]}}"
    :visibilityItems "{{concat(map(computed.kinds, { title: it[1] + ' Visible', checked: !contains(state.hiddenKinds, it[0]), run: 'toggleKind', with: { args: { list: 'hiddenKinds', kind: it[0] } } }), [{ separator: true }], map(computed.kinds, { title: it[1] + ' Selectable', checked: !contains(state.unselectableKinds, it[0]), run: 'toggleKind', with: { args: { list: 'unselectableKinds', kind: it[0] } } }))}}"
    :gizmoItems "{{[{ title: 'Navigate', checked: state.navGizmo != false, run: 'toggleOption', with: { args: { key: 'navGizmo' } } }, { title: 'Active Tools', checked: state.toolGizmos != false, run: 'toggleOption', with: { args: { key: 'toolGizmos' } } }, { separator: true }, { title: 'Object Gizmo: Move', checked: state.objectGizmo == 'translate', run: 'setObjectGizmo', with: { args: { mode: 'translate' } } }, { title: 'Object Gizmo: Rotate', checked: state.objectGizmo == 'rotate', run: 'setObjectGizmo', with: { args: { mode: 'rotate' } } }, { title: 'Object Gizmo: Scale', checked: state.objectGizmo == 'scale', run: 'setObjectGizmo', with: { args: { mode: 'scale' } } }]}}"
    :overlayItems "{{map([['grid', 'Grid'], ['axes', 'Floor Axes'], ['cursor', '3D Cursor'], ['text', 'Text Info'], ['stats', 'Statistics'], ['extras', 'Extras (Lights, Cameras)']], { title: it[1], checked: state.overlay[it[0]] == true, run: 'toggleOverlay', with: { args: { key: it[0] } } })}}"
    :shadingItems "{{[{ title: 'Scene Lights', checked: state.sceneLights == true, run: 'toggleOption', with: { args: { key: 'sceneLights' } }, keyLabel: 'Material Preview' }, { title: 'Scene World', checked: state.sceneWorld == true, run: 'toggleOption', with: { args: { key: 'sceneWorld' } }, keyLabel: 'Material Preview' }, { separator: true }, { title: 'X-Ray', checked: state.xray == true, run: 'toggleXray', keyLabel: 'Alt Z' }]}}"
    :lit "{{state.shading == 'rendered' || state.shading == 'material' && state.sceneLights == true}}"
    :worldShown "{{state.shading == 'rendered' || state.shading == 'material' && state.sceneWorld == true}}"
    :boundsCentre "{{length(computed.selectedNodes) == 0 ? null : [(max(map(computed.selectedNodes, it.translation[0])) + min(map(computed.selectedNodes, it.translation[0]))) / 2, (max(map(computed.selectedNodes, it.translation[1])) + min(map(computed.selectedNodes, it.translation[1]))) / 2, (max(map(computed.selectedNodes, it.translation[2])) + min(map(computed.selectedNodes, it.translation[2]))) / 2]}}"
    :extrasShown "{{state.overlays && state.overlay.extras}}"
    :displayScene "{{length(state.hiddenKinds) == 0 && computed.extrasShown && length(computed.collectionsOff.visible) == 0 && state.shading != 'rendered' ? state.scene : merge(state.scene, { nodes: map(state.scene.nodes, contains(state.hiddenKinds, it.kind) || !computed.extrasShown && (it.kind == 'light' || it.kind == 'camera') || contains(computed.collectionsOff.visible, it.collection ?? 'collection') || state.shading == 'rendered' && (it.render == false || contains(computed.collectionsOff.render, it.collection ?? 'collection')) ? merge(it, { visible: false }) : it) })}}"
    :pickable "{{map(filter(computed.displayScene.nodes, it.visible != false && it.selectable != false && !contains(state.unselectableKinds, it.kind) && !contains(computed.collectionsOff.selectable, it.collection ?? 'collection')), it.id)}}"
    :measureShown "{{state.tool == 'measure' && length(state.measure.screen) > 0 && equals(state.measure.camera, state.view.camera)}}"
    :measured "{{length(state.measure.points) == 2 ? sqrt(pow(state.measure.points[1][0] - state.measure.points[0][0], 2) + pow(state.measure.points[1][1] - state.measure.points[0][1], 2) + pow(state.measure.points[1][2] - state.measure.points[0][2], 2)) : null}}"
    :shadingModes "{{[{ id: 'wireframe', icon: 'globe', title: 'Wireframe' }, { id: 'solid', icon: 'circle', title: 'Solid' }, { id: 'material', icon: 'circle-dot', title: 'Material Preview' }, { id: 'rendered', icon: 'sun', title: 'Rendered' }]}}"}
  :components {:hButton {:params {:icon ""
        :text ""
        :width 24
        :on false
        :chevron false
        :tooltip ""
        :run nil
        :with nil
        :open ""
        :items []
        :menuWidth 200
        :placement below
        :back @menuWidget
        :radius 4
        :textColor @text
        :disabled false}
      :node {:width "{{width}}"
        :height 20
        :radius "{{radius}}"
        :color "{{on ? '@sel' : back}}"
        :hover {:color "{{on ? '@sel' : disabled ? back : '@widgetHover'}}"}
        :role button
        :focusable true
        :disabled "{{disabled}}"
        :tooltip "{{tooltip}}"
        :label "{{text || tooltip}}"
        :selected "{{on ? true : null}}"
        :onClick [{:if "{{open != ''}}"
            :then [{:toggle "{{open}}"}]
            :else [{:if "{{run != null}}" :then [{:run "{{run}}" :with "{{with}}"}]}]}]
        :children [{:when "{{icon != ''}}"
            :type label
            :font icons
            :icon "{{icon}}"
            :text ""
            :textColor "{{disabled ? '@faint' : textColor}}"
            :align center
            :anchors {:left "{{text == '' && !chevron ? 0 : 3}}" :top 0 :bottom 0}
            :width "{{text == '' && !chevron ? width : 18}}"}
          {:when "{{text != ''}}"
            :type label
            :text "{{text}}"
            :textColor "{{disabled ? '@faint' : textColor}}"
            :align {:horizontal start :vertical center}
            :anchors {:left "{{icon != '' ? 23 : 7}}" :right "{{chevron ? 14 : 4}}" :top 0 :bottom 0}}
          {:when "{{chevron}}"
            :type label
            :font iconsSmall
            :icon chevron-down
            :text ""
            :textColor @dim
            :align center
            :anchors {:right 2 :top 0 :bottom 0}
            :width 12}
          {:when "{{open != ''}}"
            :use menu
            :with {:open "{{open}}"
              :items "{{items}}"
              :width "{{menuWidth}}"
              :placement "{{placement}}"
              :icons "{{some(items, it.icon != null)}}"}}]}}
    :pieMenu {:runsActions items
      :params {:items [] :open state.pie.open :x 0 :y 0 :title ""}
      :node {:width 360
        :height 240
        :color transparent
        :role menu
        :label "{{title}}"
        :arrows vertical
        :overlay {:placement window :open "{{open}}" :dismiss outside :modal true}
        :x "{{x - 180}}"
        :y "{{y - 120}}"
        :children [{:anchors {:left 168 :top 108}
            :width 24
            :height 24
            :radius 12
            :color @toolGroup
            :borderWidth 1
            :borderColor @faint}
          {:type label :text "{{title}}" :textColor @dim :anchors {:left 0 :right 0 :top 138} :height 16 :align center}
          (node :repeat {:for item :in "{{items}}" :index i}
            #"pie-{{i}}"
            :role menuitem
            :label "{{item.title}}"
            :focusable true
            :disabled "{{item.enabled == false}}"
            :selected "{{item.checked == null ? null : item.checked == true}}"
            :anchors {:left "{{115 + [-115, 115, 0, 0, -100, 100, -100, 100][i]}}"
              :top "{{107 + [0, 0, 80, -80, -50, -50, 50, 50][i]}}"}
            :width 130
            :height 26
            :radius 13
            :color "{{item.checked == true ? '@sel' : '@toolGroup'}}"
            :hover {:color "{{item.enabled == false ? '@toolGroup' : '@widgetHover'}}"}
            :focused {:color @widgetHover}
            :onClick [{:dismiss true} {:run "{{item.run}}" :with "{{item.with}}"}]
            {:when "{{item.icon != null}}"
              :type label
              :text ""
              :icon "{{item.icon}}"
              :anchors {:left 10 :top 5}
              :fitText true
              :textColor "{{item.enabled == false ? '@faint' : '@text'}}"}
            {:type label
              :text "{{item.title}}"
              :anchors {:left "{{item.icon != null ? 32 : 14}}" :top 5}
              :fitText true
              :textColor "{{item.enabled == false ? '@faint' : '@text'}}"})]}}
    :gap {:params {:width 4} :node {:width "{{width}}" :height 1 :color transparent}}
    :viewportArea {:node (node #viewport-area :color @view :radius 8
        (scene #viewport
          :fill true
          :bind state.view
          :scene "{{computed.displayScene}}"
          :up z
          :glyphs true
          :cursor3d "{{state.overlays && state.overlay.cursor ? state.cursor : null}}"
          :shading "{{state.shading == 'rendered' ? 'material' : state.shading}}"
          :lighting "{{state.shading == 'material' && !computed.lit ? 'studio' : 'scene'}}"
          :xray "{{state.xray ? 0.5 : 0}}"
          :exposure "{{state.render.exposure}}"
          :grid "{{state.overlays && state.overlay.grid}}"
          :axes "{{state.overlays && state.overlay.axes}}"
          :background "{{computed.worldShown ? state.world.color : '@view'}}"
          :gridColor @grid
          :outlineColor @selected
          :gizmo "{{computed.gizmo}}"
          :controls {:selects false}
          :label "3D Viewport"
          :role img
          :onSelect pick
          :onTransform transformed)
        {:use viewportHeader}
        {:when "{{state.toolbar != false}}" :use viewportToolbar}
        {:when "{{computed.measureShown}}" :use measureLine}
        (node :when "{{state.overlays && state.overlay.text}}"
          :type label
          #view-name
          :text "{{computed.viewName}}"
          :textColor @text
          :anchors {:left 54 :top 42}
          :fitText true)
        (node :when "{{state.overlays && state.overlay.text}}"
          :type label
          #view-collection
          :text "({{state.frame}}) {{computed.activeNode ? (find(computed.collections, it.id == (computed.activeNode.collection ?? 'collection')).name ?? 'Collection') + ' | ' + computed.activeNode.name : computed.activeCollectionData.name}}"
          :textColor @text
          :anchors {:left 54 :top 58}
          :fitText true)
        (node :when "{{state.overlays && state.overlay.stats}}"
          :type label
          #view-stats
          :text "Objects {{length(state.view.selection)}} / {{length(state.scene.nodes)}}   Lights {{length(filter(state.scene.nodes, it.kind == 'light'))}}   Cameras {{length(filter(state.scene.nodes, it.kind == 'camera'))}}"
          :textColor @text
          :anchors {:left 54 :top "{{state.overlay.text ? 78 : 42}}"}
          :fitText true)
        (node :when "{{state.tool == 'measure'}}"
          :type label
          #measure-hint
          :live polite
          :text "{{computed.measured != null ? 'Distance ' + fixed(computed.measured, 3) + ' m' : length(state.measure.points) == 1 ? 'Click the second point' : 'Click two points to measure'}}"
          :textColor @text
          :anchors {:left 52 :bottom 40}
          :fitText true)
        {:when "{{state.gizmos && state.navGizmo != false}}" :use navigator}
        {:when "{{state.sidebar}}" :use viewportSidebar}
        (node :when "{{!state.sidebar}}"
          :use hButton
          #sidebar-open
          :anchors {:right 0 :top 44}
          :with {:icon chevron-left
            :width 14
            :back transparent
            :tooltip "Sidebar (N)"
            :run toggleSidebar
            :textColor @dim})
        (node :when "{{state.modal != null}}"
          :type label
          #modal-hint
          :live polite
          :text "{{state.modalText}}"
          :textColor @text
          :anchors {:left 52 :bottom 12}
          :fitText true)
        {:when "{{state.modal == null && state.lastOp != null}}" :use redoPanel})}
    :redoPanel {:node (node #redo-panel
        :anchors {:left 48 :bottom 8}
        :width 230
        :height "{{state.redoOpen && state.lastOp.values != null ? 94 : 24}}"
        :color @panel
        :radius 8
        :role region
        :label "Adjust Last Operation"
        (node #redo-header
          :anchors {:left 0 :right 0 :top 0}
          :height 24
          :color transparent
          :role button
          :focusable true
          :cursor hand
          :label "{{state.lastOp.title}}"
          :onClick {:toggle state.redoOpen}
          {:type label
            :font iconsSmall
            :icon "{{state.redoOpen ? 'chevron-down' : 'chevron-right'}}"
            :text ""
            :textColor @dim
            :align center
            :anchors {:left 4 :top 0 :bottom 0}
            :width 14}
          {:type label
            :text "{{state.lastOp.title}}"
            :textColor @text
            :align {:horizontal start :vertical center}
            :anchors {:left 22 :right 4 :top 0 :bottom 0}})
        {:when "{{state.redoOpen && state.lastOp.values != null}}"
          :anchors {:left 4 :right 0 :top 26}
          :height 62
          :color transparent
          :children [{:use vectorField
              :with {:title "{{state.lastOp.label ?? (state.lastOp.kind == 'scale' ? 'Scale' : state.lastOp.kind == 'rotate' ? 'Rotation' : 'Move')}}"
                :values "{{state.lastOp.values}}"
                :key redo
                :unit "{{state.lastOp.kind == 'scale' ? '' : state.lastOp.kind == 'rotate' ? '°' : ' m'}}"
                :step "{{state.lastOp.kind == 'scale' ? 0.005 : state.lastOp.kind == 'rotate' ? 0.5 : 0.01}}"
                :digits "{{state.lastOp.kind == 'rotate' ? 1 : 3}}"
                :fixed "{{state.lastOp.kind == 'scale'}}"}}]})}
    :viewportHeader {:node (row #viewport-header
        :anchors {:left 6 :right 6 :top 6}
        :height 28
        :color transparent
        :alignItems center
        :gap 0
        (row #header-modes
          :height 28
          :fitContent width
          :color @toolGroup
          :radius 8
          :alignItems center
          :gap 2
          :role toolbar
          :label "Mode and menus"
          {:use gap}
          (node :use hButton
            #editor-type
            :with {:icon axis-3-d
              :chevron true
              :width 38
              :tooltip "Editor Type"
              :open state.open.editor0
              :items "{{map(computed.editorTypes, merge(it, { checked: it.id == 'viewport', run: 'swapEditor', with: { args: { from: 'viewport', to: it.id } } }))}}"})
          {:use gap :with {:width 6}}
          (node :use hButton
            #mode
            :with {:icon "{{state.mode == 'edit' ? 'pen-tool' : 'box'}}"
              :text "{{state.mode == 'edit' ? 'Edit Mode' : 'Object Mode'}}"
              :chevron true
              :width 116
              :tooltip "Interaction Mode"
              :open state.open.mode
              :items "{{computed.modeItems}}"})
          {:use gap :with {:width 6}}
          (node :repeat {:for m
              :in "{{[['view', 'View'], ['select', 'Select'], ['add', 'Add'], ['object', 'Object']]}}"}
            :use hButton
            #"menu-{{m[0]}}"
            :with {:text "{{m[1]}}"
              :width "{{length(m[1]) * 7 + 14}}"
              :back transparent
              :open "{{'state.open.' + m[0]}}"
              :items "{{menus[m[0]]}}"
              :menuWidth 230})
          {:use gap})
        {:grow 1 :height 1 :color transparent}
        (row #header-transform
          :height 28
          :fitContent width
          :color @toolGroup
          :radius 8
          :alignItems center
          :gap 2
          :role toolbar
          :label "Transform settings"
          {:use gap}
          (node :use hButton
            #orientation
            :with {:icon axis-3-d
              :text "{{upper(slice(state.orientation, 0, 1)) + slice(state.orientation, 1)}}"
              :chevron true
              :width 86
              :tooltip "Transform Orientation"
              :open state.open.orientation
              :items "{{computed.orientationItems}}"
              :menuWidth 160})
          (node :use hButton
            #pivot
            :with {:icon "{{state.pivot == 'cursor' ? 'crosshair' : state.pivot == 'individual' ? 'dot' : state.pivot == 'active' ? 'circle-dot' : state.pivot == 'bounds' ? 'square-dashed-bottom' : 'git-commit-horizontal'}}"
              :chevron true
              :width 38
              :tooltip "Transform Pivot Point"
              :open state.open.pivot
              :items "{{computed.pivotItems}}"
              :menuWidth 190})
          {:use gap}
          (node :use hButton
            #snap
            :with {:icon magnet
              :on "{{state.snap}}"
              :width 24
              :tooltip "Snap (Shift Tab)"
              :run toggleSnap
              :radius {:topLeft 4 :bottomLeft 4}})
          (node :use hButton
            #snap-target
            :with {:icon grid-3-x-3
              :chevron true
              :width 34
              :tooltip "Snap To: Increment"
              :open state.open.snapTarget
              :items "{{computed.snapItems}}"
              :menuWidth 190
              :radius {:topRight 4 :bottomRight 4}})
          {:use gap}
          (node :use hButton
            #proportional
            :with {:icon circle-dot-dashed
              :width 24
              :tooltip "Proportional Editing isn't available yet: it moves a mesh's vertices"
              :disabled true})
          {:use gap})
        {:grow 1 :height 1 :color transparent}
        (row #header-display
          :height 28
          :fitContent width
          :color @toolGroup
          :radius 8
          :alignItems center
          :gap 2
          :role toolbar
          :label Display
          {:use gap}
          (node :use hButton
            #visibility
            :with {:icon eye
              :chevron true
              :width 38
              :tooltip "Selectability & Visibility"
              :open state.open.visibility
              :items "{{computed.visibilityItems}}"
              :menuWidth 180})
          {:use gap}
          (node :use hButton
            #gizmos
            :with {:icon move-3-d
              :on "{{state.gizmos}}"
              :width 24
              :tooltip "Show Gizmo"
              :run toggleOption
              :with {:args {:key gizmos}}
              :radius {:topLeft 4 :bottomLeft 4}})
          (node :use hButton
            #gizmos-menu
            :with {:icon ""
              :chevron true
              :width 16
              :tooltip Gizmos
              :open state.open.gizmos
              :items "{{computed.gizmoItems}}"
              :menuWidth 180
              :radius {:topRight 4 :bottomRight 4}})
          {:use gap}
          (node :use hButton
            #overlays
            :with {:icon blend
              :on "{{state.overlays}}"
              :width 24
              :tooltip "Show Overlays (Shift Alt Z)"
              :run toggleOverlays
              :radius {:topLeft 4 :bottomLeft 4}})
          (node :use hButton
            #overlays-menu
            :with {:icon ""
              :chevron true
              :width 16
              :tooltip Overlays
              :open state.open.overlays
              :items "{{computed.overlayItems}}"
              :menuWidth 190
              :radius {:topRight 4 :bottomRight 4}})
          {:use gap}
          (node :use hButton
            #xray
            :with {:icon square-dashed-bottom
              :on "{{state.xray}}"
              :width 24
              :tooltip "Toggle X-Ray (Alt Z)"
              :run toggleXray})
          {:use gap}
          (node :repeat {:for s :in "{{computed.shadingModes}}" :index i}
            :use hButton
            #"shading-{{s.id}}"
            :with {:icon "{{s.icon}}"
              :on "{{state.shading == s.id}}"
              :width 24
              :tooltip "Viewport Shading: {{s.title}}"
              :run setOption
              :with {:args {:key shading :value "{{s.id}}"}}
              :radius "{{i == 0 ? { topLeft: 4, bottomLeft: 4 } : 0}}"})
          (node :use hButton
            #shading-menu
            :with {:icon ""
              :chevron true
              :width 16
              :tooltip Shading
              :open state.open.shading
              :items "{{computed.shadingItems}}"
              :menuWidth 230
              :radius {:topRight 4 :bottomRight 4}})
          {:use gap}))}
    :viewportToolbar {:node (column #toolbar
        :anchors {:left 8 :top 42}
        :width 34
        :height "{{sum(computed.tools, length(it) * 30 + 9) - 3}}"
        :gap 0
        :color @toolGroup
        :radius 8
        :role toolbar
        :label Tools
        :arrows vertical
        {:repeat {:for group :in "{{computed.tools}}" :index g}
          :type column
          :width 34
          :height "{{length(group) * 30 + (g < length(computed.tools) - 1 ? 9 : 6)}}"
          :color transparent
          :gap 0
          :children [{:height "{{g == 0 ? 3 : 0}}" :width 1 :color transparent}
            (node :repeat {:for t :in "{{group}}"}
              :use hButton
              #"tool-{{t.id}}"
              :x 2
              :with {:icon "{{t.icon}}"
                :on "{{state.tool == t.id}}"
                :width 30
                :radius 6
                :tooltip "{{t.title}}{{t.disabled == true ? \" isn't available yet\" : t.key != '' ? ' (' + t.key + ')' : ''}}"
                :back transparent
                :disabled "{{t.disabled == true}}"
                :run setTool
                :with {:args {:tool "{{t.id}}"}}}
              :height 30)
            {:when "{{g < length(computed.tools) - 1}}"
              :height 9
              :width 34
              :color transparent
              :children [{:x 9 :y 4 :width 16 :height 1 :color @faint}]}]})}
    :measureLine {:node (node #measure
        :x 0
        :y 0
        :width 0
        :height 0
        :overflow visible
        :color transparent
        :role img
        :label "{{computed.measured != null ? 'Measured ' + fixed(computed.measured, 3) + ' m' : 'Measuring'}}"
        {:when "{{length(state.measure.screen) == 2}}"
          :x "{{state.measure.screen[0][0]}}"
          :y "{{state.measure.screen[0][1] - 1}}"
          :height 2
          :color @active
          :width "{{hypot(state.measure.screen[1][0] - state.measure.screen[0][0], state.measure.screen[1][1] - state.measure.screen[0][1])}}"
          :rotate "{{degrees(atan2(state.measure.screen[1][1] - state.measure.screen[0][1], state.measure.screen[1][0] - state.measure.screen[0][0]))}}"
          :transformOrigin [0 50]}
        {:repeat {:for p :in "{{state.measure.screen}}"}
          :x "{{p[0] - 4}}"
          :y "{{p[1] - 4}}"
          :width 8
          :height 8
          :radius 4
          :color @active}
        (node :when "{{computed.measured != null}}"
          :type label
          #measure-length
          :text "{{fixed(computed.measured, 3)}} m"
          :textColor @text
          :color @toolGroup
          :radius 4
          :x "{{(state.measure.screen[0][0] + state.measure.screen[1][0]) / 2 + 6}}"
          :y "{{(state.measure.screen[0][1] + state.measure.screen[1][1]) / 2 - 22}}"
          :fitText true
          :padding 4))}
    :navigator {:node (node #navigator
        :dir ltr
        :anchors {:right "{{state.sidebar ? 254 : 14}}" :top 44}
        :width 84
        :height 212
        :color transparent
        (node #nav-ball
          :width 84
          :height 84
          :radius 14
          :color transparent
          :hover {:color @navBall}
          :role slider
          :label "Navigation gizmo: drag to orbit"
          :onPress {:set state.dragLast :to [0 0]}
          :onDrag [{:camera "#viewport"
              :orbit ["{{(state.dragLast[0] - event.dx) * 0.5}}" "{{(state.dragLast[1] - event.dy) * 0.5}}"]}
            {:set state.dragLast :to "{{[event.dx, event.dy]}}"}]
          {:repeat {:for a :in "{{filter(computed.navAxes, it.label != '')}}" :key "{{a.name}}"}
            :x 42
            :y 41
            :width "{{hypot(a.x, a.y) * 30}}"
            :height 2
            :color "{{a.color}}"
            :rotate "{{degrees(atan2(a.y, a.x))}}"
            :transformOrigin [0 50]}
          {:repeat {:for a :in "{{computed.navAxes}}" :key "{{a.name}}"}
            :x "{{42 + a.x * 30 - 9}}"
            :y "{{42 + a.y * 30 - 9}}"
            :width 18
            :height 18
            :radius 5
            :color "{{a.color}}"
            :borderWidth "{{a.label == '' ? 1 : 0}}"
            :borderColor "{{slice(a.color, 0, 7)}}"
            :role button
            :label "View {{upper(slice(a.view, 0, 1)) + slice(a.view, 1)}}"
            :hover {:color "{{slice(a.color, 0, 7)}}"}
            :onClick {:run viewAxis :with {:args {:view "{{a.view}}"}}}
            :children [{:type label :text "{{a.label}}" :textColor "#151515" :font small :align center :fill true}]})
        (column #nav-buttons :anchors {:top 94} :x 28 :width 28 :height 112 :gap 0 :color @navButton :radius 8
          (node :repeat {:for b
              :in "{{[{ id: 'zoom', icon: 'zoom-in', title: 'Zoom in/out in the view' }, { id: 'pan', icon: 'hand', title: 'Move the view' }, { id: 'camera', icon: 'video', title: 'Toggle the camera view (Numpad 0)' }, { id: 'ortho', icon: 'grid-3-x-3', title: 'Switch the current view from perspective/orthographic projection (Numpad 5)' }]}}"}
            #"nav-{{b.id}}"
            :width 28
            :height 28
            :radius 8
            :color transparent
            :hover {:color @navHover}
            :role button
            :label "{{b.title}}"
            :tooltip "{{b.title}}"
            :focusable true
            :onPress {:set state.dragLast :to [0 0]}
            :onDrag [{:run navDrag
                :with {:args (node #"{{b.id}}"
                    :dx "{{event.dx - state.dragLast[0]}}"
                    :dy "{{event.dy - state.dragLast[1]}}")}}
              {:set state.dragLast :to "{{[event.dx, event.dy]}}"}]
            :onClick {:run navClick :with {:args (node #"{{b.id}}")}}
            {:type label :font icons :icon "{{b.icon}}" :text "" :textColor @text :align center :fill true})))}
    :viewportSidebar {:node (node #sidebar
        :anchors {:right 6 :top 40 :bottom 6}
        :width 236
        :color @header
        :radius 8
        :role region
        :label Sidebar
        (row #sidebar-tabs
          :anchors {:left 6 :right 6 :top 6}
          :height 22
          :color @panelBack
          :radius 6
          :gap 0
          :role tablist
          :arrows horizontal
          {:repeat {:for tab :in "{{['Item', 'Tool', 'View']}}"}
            :grow 1
            :height 22
            :role tab
            :focusable true
            :label "{{tab}}"
            :selected "{{state.sidebarTab == tab}}"
            :color "{{state.sidebarTab == tab ? '@widget' : 'transparent'}}"
            :hover {:color @widgetHover}
            :radius 6
            :onClick {:set state.sidebarTab :to "{{tab}}"}
            :children [{:type label
                :text "{{tab}}"
                :textColor "{{state.sidebarTab == tab ? '@text' : '@dim'}}"
                :align center
                :fill true}]})
        {:type column
          :anchors {:left 6 :right 6 :top 34 :bottom 0}
          :gap 4
          :color transparent
          :children [{:when "{{state.sidebarTab == 'Item'}}" :use transformPanel :with {:narrow true}}
            {:when "{{state.sidebarTab == 'Tool'}}" :use panelHeader :with {:title "Active Tool" :key activeTool}}
            (node :when "{{state.sidebarTab == 'Tool' && state.panels.activeTool}}"
              #tool-panel
              :width "100%"
              :height 96
              :color @panel
              :radius {:bottomLeft 4 :bottomRight 4}
              {:type column
                :fill 6
                :gap 2
                :color transparent
                :children [(label #tool-panel-name
                    :text "{{computed.toolName}}"
                    :textColor @text
                    :height 20
                    :width "100%"
                    :align {:horizontal start :vertical center})
                  {:use checkRow :with (node #tool-snap :label Snap :path state.snap :checked "{{state.snap}}")}
                  {:use checkRow
                    :with (node #tool-gizmos
                      :label "Tool gizmos"
                      :path state.toolGizmos
                      :checked "{{state.toolGizmos != false}}")}
                  {:type label
                    :text "{{'Orientation: ' + state.orientation + '   Pivot: ' + state.pivot}}"
                    :textColor @dim
                    :height 20
                    :width "100%"
                    :align {:horizontal start :vertical center}}]})
            {:when "{{state.sidebarTab == 'View'}}" :use panelHeader :with {:title View :key viewPanel}}
            (node :when "{{state.sidebarTab == 'View' && state.panels.viewPanel}}"
              #view-panel
              :width "100%"
              :height 56
              :color @panel
              :radius {:bottomLeft 4 :bottomRight 4}
              {:type column
                :fill 6
                :gap 4
                :color transparent
                :children [{:use numberField
                    :with {:label "Focal Length"
                      :key lens
                      :value "{{computed.lens}}"
                      :step 0.5
                      :text "{{fixed(computed.lens, 0) + ' mm'}}"
                      :first true
                      :last true}}
                  {:use checkRow
                    :with (node #view-ortho
                      :label Orthographic
                      :path state.view.camera.orthographic
                      :checked "{{state.view.camera.orthographic == true}}")}]})
            {:when "{{state.sidebarTab == 'View'}}" :use panelHeader :with {:title "3D Cursor" :key cursorPanel}}
            (node :when "{{state.sidebarTab == 'View' && state.panels.cursorPanel}}"
              #cursor-panel
              :width "100%"
              :height 102
              :color @panel
              :radius {:bottomLeft 4 :bottomRight 4}
              {:type column
                :fill 6
                :gap 6
                :color transparent
                :children [{:use vectorField
                    :with {:title Location :values "{{state.cursor}}" :key cursor :unit " m" :step 0.01}}
                  (node #cursor-reset
                    :height 20
                    :width "100%"
                    :color @widget
                    :hover {:color @widgetHover}
                    :radius 4
                    :role button
                    :focusable true
                    :label "Reset the 3D cursor"
                    :tooltip "Put the 3D cursor back at the origin"
                    :onClick {:set state.cursor :to "{{[0, 0, 0]}}"}
                    {:type label :text "Reset to World Origin" :textColor @text :align center :fill true})]})]})}}}
