;;; shader.lisp --- TODO

(defcomputed shaderOps
  {:output {:title "Material Output"
              :inputs [{:id "baseColor" :label "Base Color" :kind "color" :links 1}
                       {:id "metallic" :label "Metallic" :kind "float" :links 1}
                       {:id "roughness" :label "Roughness" :kind "float" :links 1}
                       {:id "emissive" :label "Emission" :kind "color" :links 1}]
              :out ""}
     :color {:title "Color" :inputs [] :out "color"}
     :number {:title "Value" :inputs [] :out "float"}
     :mix {:title "Mix"
           :inputs [{:id "a" :label "A" :kind "color" :links 1}
                    {:id "b" :label "B" :kind "color" :links 1}
                    {:id "factor" :label "Factor" :kind "float" :links 1}]
           :out "color"}
     :multiply {:title "Multiply"
                :inputs [{:id "a" :label "A" :kind "color" :links 1} {:id "b" :label "B" :kind "color" :links 1}]
                :out "color"}
     :add {:title "Add"
           :inputs [{:id "a" :label "A" :kind "color" :links 1} {:id "b" :label "B" :kind "color" :links 1}]
           :out "color"}
     :scale {:title "Scale"
             :inputs [{:id "color" :label "Color" :kind "color" :links 1}
                      {:id "factor" :label "Factor" :kind "float" :links 1}]
             :out "color"}
     :invert {:title "Invert" :inputs [{:id "value" :label "Value" :kind "float" :links 1}] :out "float"}
     :fresnel {:title "Fresnel" :inputs [{:id "power" :label "Power" :kind "float" :links 1}] :out "float"}
     :checker {:title "Checker Texture" :inputs [{:id "scale" :label "Scale" :kind "float" :links 1}] :out "float"}
     :noise {:title "Noise Texture" :inputs [{:id "scale" :label "Scale" :kind "float" :links 1}] :out "float"}
     :height {:title "Height" :inputs [] :out "float"}})

(defcomputed shaderGraph
  (if (< computed.activeMaterialIndex 0)
        nil
        (or-else (get-in state.scene.materials [computed.activeMaterialIndex :graph])
                 {:nodes [{:id "output" :op "output" :x 320 :y 40}] :links []})))

(defcomputed shaderNodes
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
                                      :label (if (= (get-in computed.shaderOps [it.op :out]) "color") "Color" "Value")
                                      :kind (get-in computed.shaderOps [it.op :out])}])}
                      {:bodyHeight (if (contains ["color" "number"] it.op) 26 0)}))
             computed.shaderGraph.nodes)))

(defcomputed shaderDeletable
  (if (nil? computed.shaderGraph)
        []
        (map (fn [it] it.id) (filter (fn [it] (not= it.op "output")) computed.shaderGraph.nodes))))

(defcomputed shaderNode
  (if (or (nil? computed.shaderGraph) (= (count state.shaderSelected) 0))
        nil
        (find (fn [it] (= it.id state.shaderSelected.0)) computed.shaderGraph.nodes)))

(defcomputed shaderOp (if (nil? computed.shaderNode) "" computed.shaderNode.op))

(defcomputed shaderNodeIndex
  (if (nil? computed.shaderNode)
        (- 1)
        (indexOf (map (fn [it] it.id) computed.shaderGraph.nodes) computed.shaderNode.id)))

(defcomponent shaderArea "The shaderArea component."
  {}
  `(node#shader-editor :color @region :radius 8
     ~(shader-header)
     (label#shader-empty :when ~(nil? computed.shaderGraph)
       :anchors {:left 0 :right 0 :top 26 :bottom 0}
       :align {:horizontal center :vertical center}
       :textColor @dim
       :text "Select an object with a material to edit its nodes")
     ~(shader-graph)
     (node#shader-sidebar :when ~(and (some? computed.shaderGraph) (contains ["color" "number"] computed.shaderOp))
       :anchors {:right 0 :top 26 :bottom 0}
       :width 210
       :color @panelBack
       :radius {:bottomRight 8}
       ~(shader-sidebar-column))))

(defpart shader "What shader adds."
  :actions {:newMaterial {:title "New Material"
                          :category "Material"
                          :icon plus
                          :enablement ~(= computed.activeNode.kind "mesh")
                          :do [{:dismiss true}
                               {:if ~(not= computed.activeNode.kind "mesh") :then [{:stop true}]}
                               {:run snapshot}
                               {:increment state.added :by 1}
                               {:set state.tmp :to ~{:n 0 :name "Material" :id (str "material-" state.added)}}
                               {:while ~(some (fn [it] (= it.name state.tmp.name)) state.scene.materials)
                                :do [{:increment state.tmp.n :by 1}
                                     {:set state.tmp.name :to ~(str "Material." (pad state.tmp.n 3 "0"))}]}
                               {:push state.scene.materials
                                :value ~{:id state.tmp.id
                                         :name state.tmp.name
                                         :baseColor [0.8 0.8 0.8 1]
                                         :metallic 0
                                         :roughness 0.5}}
                               {:set "state.scene.nodes[computed.activeIndex].material" :to ~state.tmp.id}
                               {:set state.last :to "New Material"}]}
            :assignMaterial [{:dismiss true}
                             {:if ~(or (not= computed.activeNode.kind "mesh") (= computed.activeNode.material args.id))
                              :then [{:stop true}]}
                             {:run snapshot}
                             {:set "state.scene.nodes[computed.activeIndex].material" :to ~args.id}
                             {:set state.last :to "Assign Material"}]
            :unlinkMaterial {:title "Unlink Material"
                             :category "Material"
                             :icon x
                             :enablement ~(>= computed.activeMaterialIndex 0)
                             :do [{:if ~(< computed.activeMaterialIndex 0) :then [{:stop true}]}
                                  {:run snapshot}
                                  {:set "state.scene.nodes[computed.activeIndex].material" :to nil}
                                  {:set state.last :to "Unlink Material"}]}
            :shaderGraphEnsure [{:if ~(and (>= computed.activeMaterialIndex 0)
                                           (nil? (get-in state.scene.materials [computed.activeMaterialIndex :graph])))
                                 :then [{:set "state.scene.materials[computed.activeMaterialIndex].graph"
                                         :to ~computed.shaderGraph}]}]
            :addShaderNode {:title "Add Shader Node"
                            :category "Shader"
                            :do [{:dismiss true}
                                 {:if ~(or (< computed.activeMaterialIndex 0)
                                           (nil? (get computed.shaderOps args.op))
                                           (= args.op "output"))
                                  :then [{:stop true}]}
                                 {:run snapshot}
                                 {:run shaderGraphEnsure}
                                 {:increment state.added :by 1}
                                 {:set state.tmp :to ~{:id (str args.op "-" state.added) :n (count computed.shaderGraph.nodes)}}
                                 {:push "state.scene.materials[computed.activeMaterialIndex].graph.nodes"
                                  :value ~(merge {:id state.tmp.id
                                                  :op args.op
                                                  :x (or-else args.x (+ 40 (* (mod state.tmp.n 3) 30)))
                                                  :y (or-else args.y (+ 20 (* (mod (- state.tmp.n 1) 6) 70)))}
                                                 (cond (= args.op "color") {:value [0.8 0.8 0.8]}
                                                       (= args.op "number") {:value 0.5}
                                                       :else {}))}
                                 {:set state.shaderSelected :to ~[state.tmp.id]}
                                 {:set state.last :to ~(str "Add " (get-in computed.shaderOps [args.op :title]))}]}
            :shaderConnect [{:run snapshot}
                            {:run shaderGraphEnsure}
                            {:set "state.scene.materials[computed.activeMaterialIndex].graph.links"
                             :to ~(concat (filter (fn [it] (not (and (= it.to event.to) (= it.input event.input))))
                                                  computed.shaderGraph.links)
                                          [{:from event.from :output event.output :to event.to :input event.input}])}
                            {:set state.last :to "Link Nodes"}]
            :shaderDisconnect [{:run snapshot}
                               {:set "state.scene.materials[computed.activeMaterialIndex].graph.links"
                                :to ~(filter (fn [it index] (not= index event.index)) computed.shaderGraph.links)}
                               {:set state.last :to "Unlink Nodes"}]
            :shaderMove [{:run shaderGraphEnsure}
                         {:set "state.scene.materials[computed.activeMaterialIndex].graph.nodes"
                          :to ~(map (fn [it]
                                      (if (contains event.ids it.id)
                                          (merge it {:x (round (+ it.x event.dx)) :y (round (+ it.y event.dy))})
                                          it))
                                    computed.shaderGraph.nodes)}]
            :deleteShaderNodes {:title "Delete Nodes"
                                :category "Shader"
                                :enablement ~(some (fn [it] (contains computed.shaderDeletable it)) state.shaderSelected)
                                :do [{:set state.tmp
                                      :to ~(filter (fn [it] (contains computed.shaderDeletable it)) state.shaderSelected)}
                                     {:if ~(= (count state.tmp) 0) :then [{:stop true}]}
                                     {:run snapshot}
                                     {:set "state.scene.materials[computed.activeMaterialIndex].graph"
                                      :to ~{:nodes (filter (fn [it] (not (contains state.tmp it.id)))
                                                           computed.shaderGraph.nodes)
                                            :links (filter (fn [it]
                                                             (and (not (contains state.tmp it.from))
                                                                  (not (contains state.tmp it.to))))
                                                           computed.shaderGraph.links)}}
                                     {:set state.shaderSelected :to []}
                                     {:set state.last :to "Delete Nodes"}]}
            :setShaderColor [{:if ~(or (< computed.shaderNodeIndex 0) (nil? (colorList args.value))) :then [{:stop true}]}
                             {:set "state.scene.materials[computed.activeMaterialIndex].graph.nodes[computed.shaderNodeIndex].value"
                              :to ~(slice (colorList args.value) 0 3)}]})

(defview shader-header "#shader-header." []
  `(row#shader-header :anchors {:left 0 :right 0 :top 0}
     :height 26
     :color @header
     :radius {:topLeft 8 :topRight 8}
     :alignItems center
     :gap 2
     (node :use gap)
     (node#editor-type-shader :use hButton
       :with {:icon git-fork
              :chevron true
              :width 38
              :tooltip "Editor Type"
              :open state.open.editorShader
              :items ~(map (fn [it]
                             (merge it
                                    {:checked (= it.id "shader")
                                     :run "swapEditor"
                                     :with {:args {:from "shader" :to it.id}}}))
                           computed.editorTypes)})
     (node :use gap :with {:width 6})
     (node#shader-add :use hButton
       :with {:text "Add"
              :width 50
              :chevron true
              :back transparent
              :open state.open.shaderAdd
              :items ~computed.shaderAddItems
              :menuWidth 200
              :disabled ~(< computed.activeMaterialIndex 0)
              :tooltip ~(if (< computed.activeMaterialIndex 0)
                            "The active object has no material: add one in the Material tab"
                            "Add a node (Shift+A)")})
     (node#shader-delete :use hButton
       :with {:icon trash-2
              :width 24
              :back transparent
              :tooltip "Delete the selected nodes (Delete)"
              :run deleteShaderNodes
              :disabled ~(not (contains computed.shaderDeletable (or-else state.shaderSelected.0 "")))})
     {:grow 1 :height 1 :color transparent}
     (label#shader-material :fitText true
       :height 20
       :textColor @dim
       :text ~(if (< computed.activeMaterialIndex 0)
                  "No material"
                  (str "Material: " (get-in state.scene.materials [computed.activeMaterialIndex :name]))))
     (node :use gap)))

(defview shader-graph "#shader-graph." []
  `(graph#shader-graph :when ~(some? computed.shaderGraph)
     :anchors {:left 0 :right ~(if (contains ["color" "number"] computed.shaderOp) 210 0) :top 26 :bottom 0}
     :focusable true
     :label "Shader nodes"
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
     :onConnect {:run shaderConnect}
     :onDisconnect {:run shaderDisconnect}
     :onMove {:run shaderMove}
     :onSelect {:set state.shaderSelected :to ~event.ids}
     ~(repeat [n computed.shaderGraph.nodes]
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
            :text ~(fixed (or-else n.value 0.5) 3))))))

(defview shader-sidebar-column "#shader-sidebar-column." []
  `(column :anchors {:left 8 :right 8 :top 6 :bottom 8} :gap 6 :color transparent
     (label#shader-node-title :height 20
       :width "100%"
       :textColor @text
       :text ~(if (nil? computed.shaderNode)
                  ""
                  (str (get-in computed.shaderOps [computed.shaderOp :title]) " (" computed.shaderNode.id ")")))
     (colorPicker#shader-color :when ~(= computed.shaderOp "color")
       :width "100%"
       :height 180
       :label "Color"
       :value ~(or-else (hexColor (if (= computed.shaderOp "color") computed.shaderNode.value [0.8 0.8 0.8])) "#cccccc")
       :fields false
       :alpha false
       :focusable true
       :textColor @text
       :accentColor @kit.accent
       :onChange {:run setShaderColor :with {:args {:value ~event.value}}})
     (node :when ~(= computed.shaderOp "number")
       :use numberField
       :with {:label "Value"
              :path ~(str "scene.materials["
                          computed.activeMaterialIndex
                          "].graph.nodes["
                          computed.shaderNodeIndex
                          "].value")
              :value ~(if (= computed.shaderOp "number") computed.shaderNode.value 0.5)
              :step 0.01
              :min -100
              :max 100
              :first true
              :last true
              :text ~(fixed (if (= computed.shaderOp "number") computed.shaderNode.value 0.5) 3)})))

