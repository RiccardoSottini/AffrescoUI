{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Modello's Shader Editor: the active material's node graph, which the scene node compiles into its shader. A material without one shows its output node alone; the first change gives it a graph. The nodes keep only { id, op, x, y, value }: their titles and ports come from computed.shaderOps, the same operations the renderer knows."
  :computed {:shaderOps "{{ { output: { title: 'Material Output', inputs: [{ id: 'baseColor', label: 'Base Color', kind: 'color', links: 1 }, { id: 'metallic', label: 'Metallic', kind: 'float', links: 1 }, { id: 'roughness', label: 'Roughness', kind: 'float', links: 1 }, { id: 'emissive', label: 'Emission', kind: 'color', links: 1 }], out: '' }, color: { title: 'Color', inputs: [], out: 'color' }, number: { title: 'Value', inputs: [], out: 'float' }, mix: { title: 'Mix', inputs: [{ id: 'a', label: 'A', kind: 'color', links: 1 }, { id: 'b', label: 'B', kind: 'color', links: 1 }, { id: 'factor', label: 'Factor', kind: 'float', links: 1 }], out: 'color' }, multiply: { title: 'Multiply', inputs: [{ id: 'a', label: 'A', kind: 'color', links: 1 }, { id: 'b', label: 'B', kind: 'color', links: 1 }], out: 'color' }, add: { title: 'Add', inputs: [{ id: 'a', label: 'A', kind: 'color', links: 1 }, { id: 'b', label: 'B', kind: 'color', links: 1 }], out: 'color' }, scale: { title: 'Scale', inputs: [{ id: 'color', label: 'Color', kind: 'color', links: 1 }, { id: 'factor', label: 'Factor', kind: 'float', links: 1 }], out: 'color' }, invert: { title: 'Invert', inputs: [{ id: 'value', label: 'Value', kind: 'float', links: 1 }], out: 'float' }, fresnel: { title: 'Fresnel', inputs: [{ id: 'power', label: 'Power', kind: 'float', links: 1 }], out: 'float' }, checker: { title: 'Checker Texture', inputs: [{ id: 'scale', label: 'Scale', kind: 'float', links: 1 }], out: 'float' }, noise: { title: 'Noise Texture', inputs: [{ id: 'scale', label: 'Scale', kind: 'float', links: 1 }], out: 'float' }, height: { title: 'Height', inputs: [], out: 'float' } } }}"
    :shaderAddItems "{{map([['color', 'Input', 'palette'], ['number', 'Input', 'hash'], ['fresnel', 'Input', 'aperture'], ['height', 'Input', 'arrow-up-down'], ['checker', 'Texture', 'grid-3x3'], ['noise', 'Texture', 'waves'], ['mix', 'Color', 'blend'], ['multiply', 'Color', 'x'], ['add', 'Color', 'plus'], ['scale', 'Color', 'scaling'], ['invert', 'Converter', 'contrast']], { id: it[0], title: computed.shaderOps[it[0]].title, icon: it[2], group: it[1], enabled: computed.activeMaterialIndex >= 0, run: 'addShaderNode', with: { args: { op: it[0] } } })}}"
    :shaderGraph "{{computed.activeMaterialIndex < 0 ? null : state.scene.materials[computed.activeMaterialIndex].graph ?? { nodes: [{ id: 'output', op: 'output', x: 320, y: 40 }], links: [] }}}"
    :shaderNodes "{{computed.shaderGraph == null ? [] : map(computed.shaderGraph.nodes, merge({ id: it.id, x: it.x, y: it.y, width: it.op == 'output' ? 170 : 150, title: computed.shaderOps[it.op].title ?? it.op, inputs: computed.shaderOps[it.op].inputs ?? [], outputs: (computed.shaderOps[it.op].out ?? '') == '' ? [] : [{ id: 'out', label: computed.shaderOps[it.op].out == 'color' ? 'Color' : 'Value', kind: computed.shaderOps[it.op].out }] }, { bodyHeight: contains(['color', 'number'], it.op) ? 26 : 0 }))}}"
    :shaderDeletable "{{computed.shaderGraph == null ? [] : map(filter(computed.shaderGraph.nodes, it.op != 'output'), it.id)}}"
    :shaderNode "{{computed.shaderGraph == null || length(state.shaderSelected) == 0 ? null : find(computed.shaderGraph.nodes, it.id == state.shaderSelected[0])}}"
    :shaderOp "{{computed.shaderNode == null ? '' : computed.shaderNode.op}}"
    :shaderNodeIndex "{{computed.shaderNode == null ? -1 : indexOf(map(computed.shaderGraph.nodes, it.id), computed.shaderNode.id)}}"}
  :actions {:newMaterial {:title "New Material"
      :category Material
      :icon plus
      :enablement "{{computed.activeNode.kind == 'mesh'}}"
      :do [{:dismiss true}
        {:if "{{computed.activeNode.kind != 'mesh'}}" :then [{:stop true}]}
        {:run snapshot}
        {:increment state.added :by 1}
        {:set state.tmp :to "{{ { n: 0, name: 'Material', id: 'material-' + state.added } }}"}
        {:while "{{some(state.scene.materials, it.name == state.tmp.name)}}"
          :do [{:increment state.tmp.n :by 1} {:set state.tmp.name :to "{{'Material.' + pad(state.tmp.n, 3, '0')}}"}]}
        {:push state.scene.materials
          :value "{{ { id: state.tmp.id, name: state.tmp.name, baseColor: [0.8, 0.8, 0.8, 1], metallic: 0, roughness: 0.5 } }}"}
        {:set "state.scene.nodes[computed.activeIndex].material" :to "{{state.tmp.id}}"}
        {:set state.last :to "New Material"}]}
    :assignMaterial [{:dismiss true}
      {:if "{{computed.activeNode.kind != 'mesh' || computed.activeNode.material == args.id}}" :then [{:stop true}]}
      {:run snapshot}
      {:set "state.scene.nodes[computed.activeIndex].material" :to "{{args.id}}"}
      {:set state.last :to "Assign Material"}]
    :unlinkMaterial {:title "Unlink Material"
      :category Material
      :icon x
      :enablement "{{computed.activeMaterialIndex >= 0}}"
      :do [{:if "{{computed.activeMaterialIndex < 0}}" :then [{:stop true}]}
        {:run snapshot}
        {:set "state.scene.nodes[computed.activeIndex].material" :to nil}
        {:set state.last :to "Unlink Material"}]}
    :shaderGraphEnsure [{:if "{{computed.activeMaterialIndex >= 0 && state.scene.materials[computed.activeMaterialIndex].graph == null}}"
        :then [{:set "state.scene.materials[computed.activeMaterialIndex].graph" :to "{{computed.shaderGraph}}"}]}]
    :addShaderNode {:title "Add Shader Node"
      :category Shader
      :do [{:dismiss true}
        {:if "{{computed.activeMaterialIndex < 0 || computed.shaderOps[args.op] == null || args.op == 'output'}}"
          :then [{:stop true}]}
        {:run snapshot}
        {:run shaderGraphEnsure}
        {:increment state.added :by 1}
        {:set state.tmp :to "{{ { id: args.op + '-' + state.added, n: length(computed.shaderGraph.nodes) } }}"}
        {:push "state.scene.materials[computed.activeMaterialIndex].graph.nodes"
          :value "{{merge({ id: state.tmp.id, op: args.op, x: args.x ?? 40 + (state.tmp.n % 3) * 30, y: args.y ?? 20 + ((state.tmp.n - 1) % 6) * 70 }, args.op == 'color' ? { value: [0.8, 0.8, 0.8] } : args.op == 'number' ? { value: 0.5 } : {})}}"}
        {:set state.shaderSelected :to "{{[state.tmp.id]}}"}
        {:set state.last :to "{{'Add ' + computed.shaderOps[args.op].title}}"}]}
    :shaderConnect [{:run snapshot}
      {:run shaderGraphEnsure}
      {:set "state.scene.materials[computed.activeMaterialIndex].graph.links"
        :to "{{concat(filter(computed.shaderGraph.links, !(it.to == event.to && it.input == event.input)), [{ from: event.from, output: event.output, to: event.to, input: event.input }])}}"}
      {:set state.last :to "Link Nodes"}]
    :shaderDisconnect [{:run snapshot}
      {:set "state.scene.materials[computed.activeMaterialIndex].graph.links"
        :to "{{filter(computed.shaderGraph.links, index != event.index)}}"}
      {:set state.last :to "Unlink Nodes"}]
    :shaderMove [{:run shaderGraphEnsure}
      {:set "state.scene.materials[computed.activeMaterialIndex].graph.nodes"
        :to "{{map(computed.shaderGraph.nodes, contains(event.ids, it.id) ? merge(it, { x: round(it.x + event.dx), y: round(it.y + event.dy) }) : it)}}"}]
    :deleteShaderNodes {:title "Delete Nodes"
      :category Shader
      :enablement "{{some(state.shaderSelected, contains(computed.shaderDeletable, it))}}"
      :do [{:set state.tmp :to "{{filter(state.shaderSelected, contains(computed.shaderDeletable, it))}}"}
        {:if "{{length(state.tmp) == 0}}" :then [{:stop true}]}
        {:run snapshot}
        {:set "state.scene.materials[computed.activeMaterialIndex].graph"
          :to "{{ { nodes: filter(computed.shaderGraph.nodes, !contains(state.tmp, it.id)), links: filter(computed.shaderGraph.links, !contains(state.tmp, it.from) && !contains(state.tmp, it.to)) } }}"}
        {:set state.shaderSelected :to []}
        {:set state.last :to "Delete Nodes"}]}
    :setShaderColor [{:if "{{computed.shaderNodeIndex < 0 || colorList(args.value) == null}}" :then [{:stop true}]}
      {:set "state.scene.materials[computed.activeMaterialIndex].graph.nodes[computed.shaderNodeIndex].value"
        :to "{{slice(colorList(args.value), 0, 3)}}"}]}
  :components {:shaderArea {:node (node #shader-editor :color @region :radius 8
        (row #shader-header
          :anchors {:left 0 :right 0 :top 0}
          :height 26
          :color @header
          :radius {:topLeft 8 :topRight 8}
          :alignItems center
          :gap 2
          {:use gap}
          (node :use hButton
            #editor-type-shader
            :with {:icon git-fork
              :chevron true
              :width 38
              :tooltip "Editor Type"
              :open state.open.editorShader
              :items "{{map(computed.editorTypes, merge(it, { checked: it.id == 'shader', run: 'swapEditor', with: { args: { from: 'shader', to: it.id } } }))}}"})
          {:use gap :with {:width 6}}
          (node :use hButton
            #shader-add
            :with {:text Add
              :width 50
              :chevron true
              :back transparent
              :open state.open.shaderAdd
              :items "{{computed.shaderAddItems}}"
              :menuWidth 200
              :disabled "{{computed.activeMaterialIndex < 0}}"
              :tooltip "{{computed.activeMaterialIndex < 0 ? 'The active object has no material: add one in the Material tab' : 'Add a node (Shift+A)'}}"})
          (node :use hButton
            #shader-delete
            :with {:icon trash-2
              :width 24
              :back transparent
              :tooltip "Delete the selected nodes (Delete)"
              :run deleteShaderNodes
              :disabled "{{!contains(computed.shaderDeletable, state.shaderSelected[0] ?? '')}}"})
          {:grow 1 :height 1 :color transparent}
          (label #shader-material
            :fitText true
            :height 20
            :textColor @dim
            :text "{{computed.activeMaterialIndex < 0 ? 'No material' : 'Material: ' + state.scene.materials[computed.activeMaterialIndex].name}}")
          {:use gap})
        (node :when "{{computed.shaderGraph == null}}"
          :type label
          #shader-empty
          :anchors {:left 0 :right 0 :top 26 :bottom 0}
          :align {:horizontal center :vertical center}
          :textColor @dim
          :text "Select an object with a material to edit its nodes")
        (node :when "{{computed.shaderGraph != null}}"
          :type graph
          #shader-graph
          :anchors {:left 0 :right "{{contains(['color', 'number'], computed.shaderOp) ? 210 : 0}}" :top 26 :bottom 0}
          :focusable true
          :label "Shader nodes"
          :nodes "{{computed.shaderNodes}}"
          :links "{{computed.shaderGraph.links}}"
          :selectedIds "{{state.shaderSelected}}"
          :minimap false
          :color @view
          :nodeColor @panel
          :headerColor @header
          :textColor @text
          :accentColor @selected
          :kindColors {:color @socketColor :float @socketFloat}
          :onConnect {:run shaderConnect}
          :onDisconnect {:run shaderDisconnect}
          :onMove {:run shaderMove}
          :onSelect {:set state.shaderSelected :to "{{event.ids}}"}
          (node :repeat {:for n :in "{{computed.shaderGraph.nodes}}"} #"shader-body-{{n.id}}" :color transparent
            {:when "{{n.op == 'color'}}"
              :anchors {:left 8 :right 8 :top 3 :bottom 3}
              :radius 4
              :color "{{hexColor(n.value ?? [0.8, 0.8, 0.8]) ?? '#cccccc'}}"
              :borderWidth 1
              :borderColor @widget}
            {:when "{{n.op == 'number'}}"
              :type label
              :anchors {:left 8 :right 8 :top 3 :bottom 3}
              :color @widget
              :radius 4
              :align {:horizontal center :vertical center}
              :textColor @text
              :text "{{fixed(n.value ?? 0.5, 3)}}"}))
        (node :when "{{computed.shaderGraph != null && contains(['color', 'number'], computed.shaderOp)}}"
          #shader-sidebar
          :anchors {:right 0 :top 26 :bottom 0}
          :width 210
          :color @panelBack
          :radius {:bottomRight 8}
          {:type column
            :anchors {:left 8 :right 8 :top 6 :bottom 8}
            :gap 6
            :color transparent
            :children [(label #shader-node-title
                :height 20
                :width "100%"
                :textColor @text
                :text "{{computed.shaderNode == null ? '' : computed.shaderOps[computed.shaderOp].title + ' (' + computed.shaderNode.id + ')'}}")
              (node :when "{{computed.shaderOp == 'color'}}"
                :type colorPicker
                #shader-color
                :width "100%"
                :height 180
                :label Color
                :value "{{hexColor(computed.shaderOp == 'color' ? computed.shaderNode.value : [0.8, 0.8, 0.8]) ?? '#cccccc'}}"
                :fields false
                :alpha false
                :focusable true
                :textColor @text
                :accentColor @kit.accent
                :onChange {:run setShaderColor :with {:args {:value "{{event.value}}"}}})
              {:when "{{computed.shaderOp == 'number'}}"
                :use numberField
                :with {:label Value
                  :path "{{'scene.materials[' + computed.activeMaterialIndex + '].graph.nodes[' + computed.shaderNodeIndex + '].value'}}"
                  :value "{{computed.shaderOp == 'number' ? computed.shaderNode.value : 0.5}}"
                  :step 0.01
                  :min -100
                  :max 100
                  :first true
                  :last true
                  :text "{{fixed(computed.shaderOp == 'number' ? computed.shaderNode.value : 0.5, 3)}}"}}]}))}}}
