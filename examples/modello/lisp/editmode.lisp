;;; editmode.lisp --- Edit Mode: a mesh's vertices, edges and faces picked and changed, and the modifiers stacked on it
;;;
;;; Tab enters Edit Mode on the active mesh, whose primitive becomes an editable mesh in the scene's meshes. 1, 2 and 3
;;; pick vertices, edges or faces; a click selects (Shift adds), A selects all. E extrudes the faces, I insets them,
;;; Ctrl R cuts a loop across the edge, X deletes; G, R and S move, turn and scale the selection after the pointer, with
;;; proportional editing pulling the vertices near it along. Every change keeps an undo snapshot first.

(ns modello.editmode)

(defstate edit {:select vertex :vertices [] :faces [] :edge nil :object nil :objects []}
  "Edit Mode: what a click picks (vertex, edge or face), the vertices, faces and edge selected, the object edited, and the objects selected before.")

(defstate proportional false "Whether moving vertices pulls the ones near them along (proportional editing).")

(defstate proportionalSize 1.5 "How far proportional editing reaches.")

;;; What Edit Mode edits

(defcomputed editNode "The object Edit Mode edits, or nil."
  (when (= state.mode :edit) (by-id state.scene.nodes state.edit.object)))

(defcomputed editMesh "The edited object's mesh, or nil." (by-id state.scene.meshes computed.editNode.mesh))

(defcomputed editCounts "The edited mesh's counts, for the status bar." (when computed.editMesh (meshStats computed.editMesh)))

(defcomputed editCentre "The middle of the selected vertices, in the object's own space, or nil."
  (when computed.editMesh (meshCentre computed.editMesh state.edit.vertices)))

(defcomputed proportionalRadius "How far a move reaches past the selection: 0 without proportional editing."
  (if state.proportional state.proportionalSize 0))

(defn primitive-mesh "An object's editable mesh: the one in the scene's meshes, else its primitive's." [n]
  (or (by-id state.scene.meshes (:mesh n)) (meshPrimitive (:mesh n))))

(defcomputed activeSmooth "Whether the active mesh is shaded smooth: its editable mesh says, else its primitive's kind."
  (let [mesh computed.activeNode.mesh
        found (find #(= (:id %) mesh) state.scene.meshes)]
    (if found (true? found.smooth) (contains ["sphere" "cylinder" "cone" "torus"] mesh))))

(defn editable? "Whether an object has a mesh Edit Mode can change." [n]
  (and (= (:kind n) :mesh) (some? (primitive-mesh n))))

;;; Entering and leaving

(defn enter-edit "Edit Mode on the active mesh: its primitive is copied into the scene's meshes the first time." []
  (let [n computed.activeNode]
    (when (and n (editable? n))
      (when-not (by-id state.scene.meshes (:mesh n))
        (let [id (str "mesh-" (:id n))]
          (set "state.scene.meshes"
               (concat (remove #(= (:id %) id) (or state.scene.meshes [])) [(merge (meshPrimitive (:mesh n)) {:id id})]))
          (change-nodes #(= (:id %) (:id n)) #(assoc % :mesh id))))
      (set "state.edit" (merge state.edit {:vertices [] :faces [] :edge nil :object (:id n) :objects state.view.selection}))
      (set "state.view.selection" [])
      (set "state.mode" "edit"))))

(defn leave-edit "Back to Object Mode, the objects selected before selected again." []
  (when (= state.mode :edit)
    (set "state.mode" "object")
    (set "state.view.selection" (filter #(by-id state.scene.nodes %) state.edit.objects))))

(defcommand toggleEditMode "Switches between Object Mode and Edit Mode on the active mesh."
  :title ~(t "command.toggleEditMode") :description ~(t "command.toggleEditMode.about")
  :category ~(t "category.object") :icon "pen-tool" :enablement ~(or (= state.mode :edit) (editable? computed.activeNode)) []
  (dismiss)
  (if (= state.mode :edit) (leave-edit) (enter-edit)))

(defcommand setSelectMode "Picks vertices, edges or faces in Edit Mode."
  :title ~(t "command.setSelectMode") :description ~(t "command.setSelectMode.about")
  :category ~(t "category.select") [mode]
  (dismiss)
  (set "state.edit" (merge state.edit {:select mode :vertices [] :faces [] :edge nil}))
  (set "state.view.selection" []))

;;; Selecting

(defn pivot-selection "The viewport's selection in Edit Mode: the pivot that moves the vertices, when there are some." []
  (set "state.view.selection" (if (seq state.edit.vertices) ["edit-pivot"] [])))

(defn toggled "A list with an item added, or taken out if it was there; without add, the item alone." [items item add]
  (cond (not add) [item]
        (contains items item) (remove #(= % item) items)
        :else (concat items [item])))

(defn select-elements "Sets the selected faces (their vertices too) or vertices, and the edge." [faces vertices edge]
  (set "state.edit" (merge state.edit {:faces faces
                                       :vertices (if (seq faces) (meshFaceVertices computed.editMesh faces) vertices)
                                       :edge edge}))
  (pivot-selection))

(defn edit-pick "A click in Edit Mode: the vertex, edge or face under it selected, or with Shift added." []
  (if-some [point event.point]
    (let [hit (meshPick computed.editMesh point (name state.edit.select) computed.editNode)]
      (case state.edit.select
        :face (select-elements (toggled state.edit.faces hit event.shift) [] nil)
        :edge (select-elements [] (if event.shift (distinct (concat state.edit.vertices hit)) hit) hit)
        (select-elements [] (toggled state.edit.vertices hit event.shift) nil)))
    (when-not event.shift (select-elements [] [] nil))))

(defn select-all-elements "A selects every vertex or face; Alt A nothing." [all]
  (let [m computed.editMesh]
    (cond (not all) (select-elements [] [] nil)
          (= state.edit.select :face) (select-elements (range (count m.faces)) [] nil)
          :else (select-elements [] (range (count m.vertices)) nil))))

;;; Changing the mesh

(defn put-mesh "The edited mesh replaced, undoable, with what stays selected." [mesh faces vertices title]
  (snapshot)
  (set "state.scene.meshes" (map #(if (= (:id %) computed.editMesh.id) (merge mesh {:id (:id %)}) %) state.scene.meshes))
  (set "state.edit" (merge state.edit {:faces faces :vertices vertices :edge nil}))
  (pivot-selection)
  (set "state.last" title))

(defcommand extrudeFaces "Extrudes the selected faces along their normals; the new faces stay selected."
  :title ~(t "command.extrudeFaces") :description ~(t "command.extrudeFaces.about")
  :category ~(t "category.mesh") :enablement ~(and (= state.mode :edit) (seq state.edit.faces)) []
  (let [mesh (meshExtrude computed.editMesh state.edit.faces 0.5)]
    (put-mesh mesh state.edit.faces (meshFaceVertices mesh state.edit.faces) (t "command.extrudeFaces"))))

(defcommand insetFaces "Insets each selected face; the inner faces stay selected."
  :title ~(t "command.insetFaces") :description ~(t "command.insetFaces.about")
  :category ~(t "category.mesh") :enablement ~(and (= state.mode :edit) (seq state.edit.faces)) []
  (let [mesh (meshInset computed.editMesh state.edit.faces 0.25)]
    (put-mesh mesh state.edit.faces (meshFaceVertices mesh state.edit.faces) (t "command.insetFaces"))))

(defcommand loopCut "Cuts a loop of faces across the selected edge."
  :title ~(t "command.loopCut") :description ~(t "command.loopCut.about")
  :category ~(t "category.mesh") :enablement ~(and (= state.mode :edit) (some? state.edit.edge)) []
  (put-mesh (meshLoopCut computed.editMesh state.edit.edge) [] [] (t "command.loopCut")))

(defcommand deleteElements "Deletes the selected faces, or the selected vertices and their faces."
  :title ~(t "command.deleteElements") :description ~(t "command.deleteElements.about")
  :category ~(t "category.mesh") :icon "trash-2" :enablement ~(and (= state.mode :edit) (seq state.edit.vertices)) []
  (put-mesh (if (seq state.edit.faces)
              (meshDeleteFaces computed.editMesh state.edit.faces)
              (meshDeleteVertices computed.editMesh state.edit.vertices))
            [] [] (t "command.deleteElements")))

(defcommand toggleProportional "Switches proportional editing: moving vertices pulls the ones near them along."
  :title ~(t "command.toggleProportional") :description ~(t "command.toggleProportional.about")
  :category ~(t "category.mesh") :icon "circle-dot-dashed" []
  (dismiss)
  (set "state.proportional" (not state.proportional)))

(defn edit-transformed "A move, turn or scale of the pivot confirmed: the selected vertices follow, the near ones too with proportional editing."
  []
  (let [moved (first event.nodes)
        centre computed.editCentre]
    (when (and moved centre)
      (case event.mode
        :translate (put-mesh (meshMove computed.editMesh state.edit.vertices
                                        (map-indexed (fn [i v] (- v (get centre i))) (:translation moved))
                                        computed.proportionalRadius)
                             state.edit.faces state.edit.vertices event.text)
        :scale (put-mesh (meshScale computed.editMesh state.edit.vertices (get (:scale moved) 0)
                                     computed.proportionalRadius)
                         state.edit.faces state.edit.vertices event.text)
        :rotate (put-mesh (meshRotate computed.editMesh state.edit.vertices (:rotation moved) computed.proportionalRadius)
                          state.edit.faces state.edit.vertices event.text)
        nil))))

(defn set-smooth "The active mesh shaded smooth or flat: its primitive becomes an editable mesh the first time." [smooth]
  (let [n computed.activeNode]
    (when (editable? n)
      (snapshot)
      (let [id (if (by-id state.scene.meshes (:mesh n)) (:mesh n) (str "mesh-" (:id n)))
            mesh (merge (primitive-mesh n) {:id id :smooth smooth})]
        (set "state.scene.meshes" (concat (remove #(= (:id %) id) (or state.scene.meshes [])) [mesh]))
        (change-nodes #(= (:id %) (:id n)) #(assoc % :mesh id))))))

;;; Modifiers: mirror, array and subdivision, applied when the scene is drawn

(defn add-modifier "A modifier added to the active mesh's stack; undoable." [kind]
  (when (and (>= computed.activeIndex 0) (editable? computed.activeNode))
    (dismiss)
    (snapshot)
    (push (str "state.scene.nodes[" computed.activeIndex "].modifiers")
          (case kind
            "mirror" {:type "mirror" :axis "x"}
            "array" {:type "array" :count 3 :offset [2.5 0 0]}
            {:type "subdivision" :levels 1}))))

(defn remove-modifier "A modifier taken off the active mesh's stack; undoable." [i]
  (snapshot)
  (remove-state (str "state.scene.nodes[" computed.activeIndex "].modifiers") :at i))

(defn set-modifier "One setting of a modifier of the active mesh; undoable." [i key value]
  (snapshot)
  (set (str "state.scene.nodes[" computed.activeIndex "].modifiers[" i "]." key) value))

(defn step-modifier "An array's count (1 to 64) or a subdivision's levels (0 to 3) one more or one less; undoable." [i step]
  (let [m (get computed.activeNode.modifiers i)]
    (if (= (:type m) "array")
      (set-modifier i "count" (min 64 (max 1 (+ (:count m) step))))
      (set-modifier i "levels" (min 3 (max 0 (+ (:levels m) step)))))))

;;; What the viewport draws in Edit Mode, over the scene

(defn stacked? "Whether an object's modifiers are applied: it has some, and isn't being edited." [n]
  (and (seq (:modifiers n)) (not= (:id n) (:id computed.editNode)) (editable? n)))

(defn modifiers-applied "The scene with each object's modifiers applied to a mesh of its own." [scene]
  (let [stacked (filter stacked? scene.nodes)]
    (if (empty? stacked)
      scene
      (merge scene
             {:meshes (concat (or scene.meshes [])
                              (map #(merge (meshModifiers (primitive-mesh %) (:modifiers %)) {:id (str "modified-" (:id %))})
                                   stacked))
              :nodes (map #(if (stacked? %) (assoc % :mesh (str "modified-" (:id %))) %) scene.nodes)}))))

(defn from-pivot "A point of the mesh as the pivot's child sees it: less the selection's middle." [v centre]
  (map-indexed (fn [axis x] (- x (get centre axis))) v))

(defn edit-overlay
  "The scene with the edited mesh's edges and vertices drawn over it, and the pivot that G and S move, carrying the
  selected faces and vertices along while it moves."
  [scene]
  (let [n computed.editNode
        m computed.editMesh
        centre (or computed.editCentre [0 0 0])
        part (meshFaces m state.edit.faces 0.006)
        corners (range (count part.vertices))
        back (map #(- %) centre)
        picked-edge (if state.edit.edge [state.edit.edge] [])
        dot (map #(/ 0.035 (if (zero? %) 1 %)) (:scale n))
        size (abs (first (:scale n)))
        bar (/ 0.012 (max 0.001 size))]
    (merge scene
           {:meshes (concat (or scene.meshes [])
                            [(merge (meshMove part corners back) {:id "edit-selection"})
                             (merge (meshEdges m bar) {:id "edit-edges"})
                             (merge (meshEdges m (* bar 2) picked-edge) {:id "edit-edge"})])
            :materials (concat scene.materials
                               [{:id "edit-selected" :baseColor [1 0.55 0.12 0.5] :emissive [0.9 0.4 0.05] :alphaMode "blend"}
                                {:id "edit-dot" :baseColor [0.04 0.04 0.05 1] :roughness 1}])
            :nodes (concat scene.nodes
                           [{:id "edit-pivot" :parent (:id n) :translation centre}
                            {:id "edit-wire" :parent (:id n) :mesh "edit-edges" :material "edit-dot"}
                            {:id "edit-edge" :parent (:id n) :mesh "edit-edge" :material "edit-selected"}
                            {:id "edit-faces" :parent "edit-pivot" :mesh "edit-selection" :material "edit-selected"}]
                           (when (<= (count m.vertices) 600)
                             (map-indexed (fn [i v]
                                            (if (contains state.edit.vertices i)
                                              {:id (str "edit-v" i) :parent "edit-pivot" :mesh "cube" :material "edit-selected"
                                               :translation (from-pivot v centre) :scale dot}
                                              {:id (str "edit-v" i) :parent (:id n) :mesh "cube" :material "edit-dot"
                                               :translation v :scale dot}))
                                          m.vertices)))})))

(defcomputed viewScene "The scene the viewport draws: modifiers applied, and in Edit Mode the mesh's elements over it."
  (let [scene (modifiers-applied computed.displayScene)]
    (if (and computed.editNode computed.editMesh) (edit-overlay scene) scene)))
