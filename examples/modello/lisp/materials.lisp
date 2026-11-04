;;; materials.lisp --- the materials and their shader graphs: new, assigned and unlinked materials, and the Shader
;;; Editor's nodes and links
;;;
;;; A material without a graph shows its output node alone (computed.shaderGraph); the first change writes the graph
;;; into the material.

;;; Materials

(defn material-name "A name no material has: Material, then Material.001, Material.002..." []
  (loop [n 0 name "Material"]
    (if (some (fn [m] (= (:name m) name)) state.scene.materials)
      (recur (inc n) (str "Material." (pad (inc n) 3 "0")))
      name)))

(defn active-node-path "Where the active object is in the state, for a step: state.scene.nodes[i] and what follows." [what]
  (str "state.scene.nodes[" computed.activeIndex "]." what))

(defcommand newMaterial "Gives the active mesh a new material of its own."
  :title ~(t "command.newMaterial") :description ~(t "command.newMaterial.about")
  :category ~(t "category.material") :icon "plus" :enablement ~(= computed.activeNode.kind "mesh") []
  (dismiss)
  (when (= computed.activeNode.kind "mesh")
    (snapshot)
    (increment "state.added" :by 1)
    (let [id (str "material-" state.added)]
      (push "state.scene.materials" {:id id :name (material-name) :baseColor [0.8 0.8 0.8 1] :metallic 0 :roughness 0.5})
      (set (active-node-path "material") id)
      (set "state.last" (t "command.newMaterial")))))

(defn assignMaterial "A material chosen in the properties: the active mesh takes it." [id]
  (dismiss)
  (when (and (= computed.activeNode.kind "mesh") (not= computed.activeNode.material id))
    (snapshot)
    (set (active-node-path "material") id)
    (set "state.last" (t "op.assignMaterial"))))

(defcommand unlinkMaterial "Takes the active object's material away from it."
  :title ~(t "command.unlinkMaterial") :description ~(t "command.unlinkMaterial.about")
  :category ~(t "category.material") :icon "x" :enablement ~(>= computed.activeMaterialIndex 0) []
  (when (>= computed.activeMaterialIndex 0)
    (snapshot)
    (set (active-node-path "material") nil)
    (set "state.last" (t "command.unlinkMaterial"))))

;;; The shader graph

(defn graph-path "Where the active material's graph is in the state, for a step, and what follows." [what]
  (str "state.scene.materials[" computed.activeMaterialIndex "].graph" what))

(defn own-graph "Writes the shader graph into the active material, the first time it changes." []
  (when (and (>= computed.activeMaterialIndex 0)
             (nil? (:graph (get state.scene.materials computed.activeMaterialIndex))))
    (set (graph-path "") computed.shaderGraph)))

(defn first-value "What a new node of an operation holds: a colour, a number, or nothing." [op]
  (cond (= op "color") {:value [0.8 0.8 0.8]}
        (= op "number") {:value 0.5}
        :else {}))

(defcommand addShaderNode "Adds a node of an operation to the graph, where it is dropped or beside the others."
  :title ~(t "command.addShaderNode") :description ~(t "command.addShaderNode.about")
  :category ~(t "category.shader") [op x y]
  (dismiss)
  (when (and (>= computed.activeMaterialIndex 0) (some? (get computed.shaderOps op)) (not= op "output"))
    (snapshot)
    (own-graph)
    (increment "state.added" :by 1)
    (let [id (str op "-" state.added)
          n (count computed.shaderGraph.nodes)]
      (push (graph-path ".nodes")
            (merge {:id id :op op :x (or x (+ 40 (* (mod n 3) 30))) :y (or y (+ 20 (* (mod (dec n) 6) 70)))}
                   (first-value op)))
      (set "state.shaderSelected" [id])
      (set "state.last" (t "op.add" {:name (get-in computed.shaderOps [op :title])})))))

(defn shaderConnect "A link drawn from an output to an input, which keeps one link." []
  (snapshot)
  (own-graph)
  (set (graph-path ".links")
       (concat (remove (fn [l] (and (= (:to l) event.to) (= (:input l) event.input))) computed.shaderGraph.links)
               [{:from event.from :output event.output :to event.to :input event.input}]))
  (set "state.last" (t "op.linkNodes")))

(defn shaderDisconnect "A link taken away." []
  (snapshot)
  (set (graph-path ".links")
       (let [links computed.shaderGraph.links] (concat (take event.index links) (drop (inc event.index) links))))
  (set "state.last" (t "op.unlinkNodes")))

(defn shaderMove "Nodes dragged in the graph keep their new places, in whole pixels." []
  (own-graph)
  (set (graph-path ".nodes")
       (map (fn [n] (if (contains event.ids (:id n))
                      (merge n {:x (round (+ (:x n) event.dx)) :y (round (+ (:y n) event.dy))})
                      n))
            computed.shaderGraph.nodes)))

(defcommand deleteShaderNodes "Deletes the selected nodes, and their links; the output stays."
  :title ~(t "command.deleteShaderNodes") :description ~(t "command.deleteShaderNodes.about")
  :category ~(t "category.shader")
  :enablement ~(some (fn [id] (contains computed.shaderDeletable id)) state.shaderSelected) []
  (let [gone (filter (fn [id] (contains computed.shaderDeletable id)) state.shaderSelected)]
    (when (not (empty? gone))
      (snapshot)
      (set (graph-path "")
           {:nodes (remove (fn [n] (contains gone (:id n))) computed.shaderGraph.nodes)
            :links (remove (fn [l] (or (contains gone (:from l)) (contains gone (:to l)))) computed.shaderGraph.links)})
      (set "state.shaderSelected" [])
      (set "state.last" (t "command.deleteShaderNodes")))))

(defn setShaderColor "A colour node's colour, picked in the Shader Editor's sidebar." [value]
  (let [colour (colorList value)]
    (when (and (>= computed.shaderNodeIndex 0) (some? colour))
      (set (graph-path (str ".nodes[" computed.shaderNodeIndex "].value")) (slice colour 0 3)))))

(defn selectShaderNodes "Nodes selected in the graph." []
  (set "state.shaderSelected" event.ids))
