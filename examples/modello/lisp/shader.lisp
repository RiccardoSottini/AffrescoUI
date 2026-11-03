;;; shader.lisp --- the Shader Editor: the active material's node graph, which the scene compiles into its shader, and
;;; a sidebar for the selected colour or value node
;;;
;;; The nodes keep only {id, op, x, y, value}: their titles and sockets come from shaderOps, the operations the renderer
;;; knows. What editing the graph does is in materials.lisp.

;;; The operations and the graph

(defcomputed shaderOps "The operations a node can do: its title, its inputs, and the kind of what it gives (out)."
  {:output {:title (t "shader.output")
            :inputs [{:id "baseColor" :label (t "properties.baseColor") :kind "color" :links 1}
                     {:id "metallic" :label (t "properties.metallic") :kind "float" :links 1}
                     {:id "roughness" :label (t "properties.roughness") :kind "float" :links 1}
                     {:id "emissive" :label (t "socket.emission") :kind "color" :links 1}]
            :out ""}
   :color {:title (t "shader.color") :inputs [] :out "color"}
   :number {:title (t "shader.number") :inputs [] :out "float"}
   :mix {:title (t "shader.mix")
         :inputs [{:id "a" :label "A" :kind "color" :links 1}
                  {:id "b" :label "B" :kind "color" :links 1}
                  {:id "factor" :label (t "socket.factor") :kind "float" :links 1}]
         :out "color"}
   :multiply {:title (t "shader.multiply") :inputs [{:id "a" :label "A" :kind "color" :links 1} {:id "b" :label "B" :kind "color" :links 1}] :out "color"}
   :add {:title (t "shader.add") :inputs [{:id "a" :label "A" :kind "color" :links 1} {:id "b" :label "B" :kind "color" :links 1}] :out "color"}
   :scale {:title (t "shader.scale")
           :inputs [{:id "color" :label (t "shader.color") :kind "color" :links 1} {:id "factor" :label (t "socket.factor") :kind "float" :links 1}]
           :out "color"}
   :invert {:title (t "shader.invert") :inputs [{:id "value" :label (t "shader.number") :kind "float" :links 1}] :out "float"}
   :fresnel {:title (t "shader.fresnel") :inputs [{:id "power" :label (t "properties.power") :kind "float" :links 1}] :out "float"}
   :checker {:title (t "shader.checker") :inputs [{:id "scale" :label (t "shader.scale") :kind "float" :links 1}] :out "float"}
   :noise {:title (t "shader.noise") :inputs [{:id "scale" :label (t "shader.scale") :kind "float" :links 1}] :out "float"}
   :height {:title (t "shader.height") :inputs [] :out "float"}})

(defcomputed shaderGraph "The active material's node graph; a material without one has its output node alone; nil without a material."
  (if (< computed.activeMaterialIndex 0)
      nil
      (or-else (get-in state.scene.materials [computed.activeMaterialIndex :graph])
               {:nodes [{:id "output" :op "output" :x 320 :y 40}] :links []})))

(defcomputed shaderNodes "The graph's nodes as the graph widget draws them: titles, sockets and sizes from their operations."
  (if (nil? computed.shaderGraph)
      []
      (map (fn [it]
             (merge {:id it.id
                     :x it.x
                     :y it.y
                     :width (if (= it.op "output") 170 150)
                     :title (or-else (get-in computed.shaderOps [it.op :title]) it.op)
                     :inputs (or-else (get-in computed.shaderOps [it.op :inputs]) [])
                     :outputs (if (= (or-else (get-in computed.shaderOps [it.op :out]) "") "")
                                  []
                                  [{:id "out"
                                    :label (if (= (get-in computed.shaderOps [it.op :out]) "color") (t "shader.color") (t "shader.number"))
                                    :kind (get-in computed.shaderOps [it.op :out])}])}
                    {:bodyHeight (if (contains ["color" "number"] it.op) 26 0)}))
           computed.shaderGraph.nodes)))

(defcomputed shaderDeletable "The nodes that can be deleted: all but the output."
  (if (nil? computed.shaderGraph)
      []
      (map (fn [it] it.id) (filter (fn [it] (not= it.op "output")) computed.shaderGraph.nodes))))

(defcomputed shaderNode "The first selected node, or nil."
  (if (or (nil? computed.shaderGraph) (= (count state.shaderSelected) 0))
      nil
      (find (fn [it] (= it.id state.shaderSelected.0)) computed.shaderGraph.nodes)))

(defcomputed shaderOp "The first selected node's operation, or nothing."
  (if (nil? computed.shaderNode) "" computed.shaderNode.op))

(defcomputed shaderNodeIndex "Where the first selected node is in the graph, or -1."
  (if (nil? computed.shaderNode)
      -1
      (indexOf (map (fn [it] it.id) computed.shaderGraph.nodes) computed.shaderNode.id)))

;;; The area

(defcomponent shaderArea "The Shader Editor: its header, the graph, and the sidebar of a colour or a value node." {}
  `(node#shader-editor :color @region :radius 8
     ~(shader-header)
     (label#shader-empty :when ~(nil? computed.shaderGraph)
       :anchors {:left 0 :right 0 :top 26 :bottom 0}
       :align {:horizontal center :vertical center}
       :textColor @dim
       :text ~(t "shader.empty"))
     ~(shader-graph)
     (node#shader-sidebar :when ~(and (some? computed.shaderGraph) (contains ["color" "number"] computed.shaderOp))
       :anchors {:right 0 :top 26 :bottom 0}
       :width 210
       :color @panelBack
       :radius {:bottomRight 8}
       ~(shader-node-settings))))

(defview shader-header "The header: editor type, Add, Delete, and the material's name." []
  `(row#shader-header :anchors {:left 0 :right 0 :top 0} :height 26 :color @header :radius {:topLeft 8 :topRight 8} :alignItems center :gap 2
     (node :use gap)
     (node#editor-type-shader :use hButton
       :with {:icon git-fork
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open state.open.editorShader
              :items ~(editor-choices "shader")})
     (node :use gap :with {:width 6})
     (node#shader-add :use hButton
       :with {:text ~(t "header.add")
              :width 50
              :chevron true
              :back transparent
              :open state.open.shaderAdd
              :items ~computed.shaderAddItems
              :menuWidth 200
              :disabled ~(< computed.activeMaterialIndex 0)
              :tooltip ~(if (< computed.activeMaterialIndex 0) (t "shader.addNeeds") (t "shader.addTip"))})
     (node#shader-delete :use hButton
       :with {:icon trash-2
              :width 24
              :back transparent
              :tooltip ~(t "shader.deleteTip")
              :run deleteShaderNodes
              :disabled ~(not (contains computed.shaderDeletable (or-else state.shaderSelected.0 "")))})
     {:grow 1 :height 1 :color transparent}
     (label#shader-material :fitText true
       :height 20
       :textColor @dim
       :text ~(if (< computed.activeMaterialIndex 0)
                (t "properties.noMaterial")
                (t "shader.material" {:name (get-in state.scene.materials [computed.activeMaterialIndex :name])})))
     (node :use gap)))

(defview shader-graph "The graph: nodes dragged, linked and unlinked, selected; a colour or a value node shows it in its body." []
  `(graph#shader-graph :when ~(some? computed.shaderGraph)
     :anchors {:left 0 :right ~(if (contains ["color" "number"] computed.shaderOp) 210 0) :top 26 :bottom 0}
     :focusable true
     :label ~(t "shader.nodes")
     :nodes ~computed.shaderNodes
     :links ~computed.shaderGraph.links
     :selectedIds ~state.shaderSelected
     :minimap false
     :color @view
     :nodeColor @panel
     :headerColor @header
     :textColor @text
     :accentColor @selected
     :kindColors {:color @socketColor :float @socketFloat}
     :onConnect shaderConnect
     :onDisconnect shaderDisconnect
     :onMove shaderMove
     :onSelect selectShaderNodes
     ~(repeat [n computed.shaderGraph.nodes] (shader-node-body n))))

(defview shader-node-body "A node's body: a colour node's swatch, or a value node's number." [n]
  `(node :id ~(str "shader-body-" n.id) :color transparent
     {:when ~(= n.op "color")
      :anchors {:left 8 :right 8 :top 3 :bottom 3}
      :radius 4
      :color ~(or-else (hexColor (or-else n.value [0.8 0.8 0.8])) "#cccccc")
      :borderWidth 1
      :borderColor @widget}
     (label :when ~(= n.op "number")
       :anchors {:left 8 :right 8 :top 3 :bottom 3}
       :color @widget
       :radius 4
       :align {:horizontal center :vertical center}
       :textColor @text
       :text ~(fixed (or-else n.value 0.5) 3))))

(defview shader-node-settings "The selected node's title, and its colour picker or its value field." []
  `(column :anchors {:left 8 :right 8 :top 6 :bottom 8} :gap 6 :color transparent
     (label#shader-node-title :height 20
       :width "100%"
       :textColor @text
       :text ~(if (nil? computed.shaderNode)
                ""
                (t "shader.nodeTitle" {:title (get-in computed.shaderOps [computed.shaderOp :title]) :id computed.shaderNode.id})))
     (colorPicker#shader-color :when ~(= computed.shaderOp "color")
       :width "100%"
       :height 180
       :label ~(t "shader.color")
       :value ~(or-else (hexColor (if (= computed.shaderOp "color") computed.shaderNode.value [0.8 0.8 0.8])) "#cccccc")
       :fields false
       :alpha false
       :focusable true
       :textColor @text
       :accentColor @kit.accent
       :onChange ~(handler setShaderColor event.value))
     (node :when ~(= computed.shaderOp "number")
       :use numberField
       :with {:label ~(t "shader.number")
              :path ~(str "scene.materials[" computed.activeMaterialIndex "].graph.nodes[" computed.shaderNodeIndex "].value")
              :value ~(if (= computed.shaderOp "number") computed.shaderNode.value 0.5)
              :step 0.01
              :min -100
              :max 100
              :first true
              :last true
              :text ~(fixed (if (= computed.shaderOp "number") computed.shaderNode.value 0.5) 3)})))
