;;; scene.lisp --- what Modello works out from its scene: the active and selected objects, what the viewport shows and
;;; can pick, the camera, the collections, the gizmo, and whether a file opened is a scene

(ns modello.scene)

;;; Objects

(defcomputed activeNode "The active object, or nil." (find #(= (:id %) state.active) state.scene.nodes))

(defcomputed activeIndex "Where the active object is in the scene's nodes, or -1."
  (indexOf (map :id state.scene.nodes) state.active))

(defcomputed selectedNodes "The selected objects."
  (filter #(contains state.view.selection (:id %)) state.scene.nodes))

(defcomputed kindIcons "Each kind of object's icon."
  {:mesh "triangle" :camera "video" :light "lightbulb" :empty "axis-3-d"})

(defcomputed cameraNode "The scene's camera, or nil." (find #(= (:kind %) :camera) state.scene.nodes))

(defcomputed cameraView "The viewport's camera when it looks through the scene's camera."
  (when-some [cam computed.cameraNode]
    (let [pitch (radians cam.rotationDegrees.0)
          yaw (radians cam.rotationDegrees.2)]
      {:target [(- cam.translation.0 (* (sin pitch) (sin yaw) 10))
                (+ cam.translation.1 (* (sin pitch) (cos yaw) 10))
                (- cam.translation.2 (* (cos pitch) 10))]
       :yaw yaw
       :pitch (- (/ (pi) 2) pitch)
       :distance 10
       :fieldOfView (or-else cam.camera.fieldOfView 0.69)
       :orthographic false})))

;;; What the viewport shows and picks

(defcomputed extrasShown "Whether lights and cameras are drawn." (and state.overlays state.overlay.extras))

(defcomputed displayScene "The scene as the viewport draws it: hidden kinds, extras, collections and what isn't rendered left out."
  (if (and (empty? state.hiddenKinds)
           computed.extrasShown
           (empty? computed.collectionsOff.visible)
           (not= state.shading :rendered))
      state.scene
      (merge state.scene
             {:nodes (map (fn [it]
                            (if (or (contains state.hiddenKinds it.kind)
                                    (and (not computed.extrasShown) (contains? #{:light :camera} it.kind))
                                    (contains computed.collectionsOff.visible (or-else it.collection "collection"))
                                    (and (= state.shading :rendered)
                                         (or (false? it.render)
                                             (contains computed.collectionsOff.render
                                                       (or-else it.collection "collection")))))
                                (merge it {:visible false})
                                it))
                          state.scene.nodes)})))

(defcomputed pickable "The ids of the objects a click or a box can select."
  (map :id
       (filter (fn [it]
                 (and (not= it.visible false)
                      (not= it.selectable false)
                      (not (contains state.unselectableKinds it.kind))
                      (not (contains computed.collectionsOff.selectable (or-else it.collection "collection")))))
               computed.displayScene.nodes)))

(defcomputed lit "Whether the viewport lights the scene with its own lights."
  (or (= state.shading :rendered) (and (= state.shading :material) (true? state.sceneLights))))

(defcomputed worldShown "Whether the viewport's background is the world's colour."
  (or (= state.shading :rendered) (and (= state.shading :material) (true? state.sceneWorld))))

(defcomputed stats "The counts the status bar starts with, when the overlays are on."
  (cond (not state.overlays) ""
        computed.editCounts (str (t "edit.stats" computed.editCounts) " | ")
        :else (t "status.stats" {:selected (count state.view.selection) :objects (count state.scene.nodes)})))

;;; Moving

(defcomputed modalName "The transform running's name, or nothing."
  (if state.modal (case state.modal.mode :translate (t "op.move") :rotate (t "op.rotate") (t "op.resize")) ""))

(defcomputed boundsCentre "The middle of the selected objects' bounds, or nil."
  (when-not (empty? computed.selectedNodes)
    (let [xs (map #(get-in % [:translation 0]) computed.selectedNodes)
          ys (map #(get-in % [:translation 1]) computed.selectedNodes)
          zs (map #(get-in % [:translation 2]) computed.selectedNodes)]
      [(/ (+ (max xs) (min xs)) 2) (/ (+ (max ys) (min ys)) 2) (/ (+ (max zs) (min zs)) 2)])))

(defcomputed lastCentre "The middle of the last operation's objects, before it: what Adjust Last scales about."
  (if (or (nil? state.lastOp) (empty? state.lastOp.base))
      [0 0 0]
      (map #(/ % (count state.lastOp.base))
           [(sum (map #(get-in % [:translation 0]) state.lastOp.base))
            (sum (map #(get-in % [:translation 1]) state.lastOp.base))
            (sum (map #(get-in % [:translation 2]) state.lastOp.base))])))

(defcomputed snapSteps "How far snapping moves, turns and scales at a step."
  (case state.snapStep
    :fine {:translate 0.1 :rotate 1 :scale 0.01}
    :coarse {:translate 10 :rotate 15 :scale 0.5}
    {:translate 1 :rotate 5 :scale 0.1}))

(defcomputed gizmo "The viewport's gizmo: shown or not, its mode and space, its snapping, and the pivot it turns about."
  (merge {:visible (and state.gizmos
                        (or (and (contains? #{:move :rotate :scale} state.tool) (not= state.toolGizmos false))
                            (and (= state.tool :select) (some? state.objectGizmo))))
          :mode (cond (= state.tool :rotate) "rotate"
                      (= state.tool :scale) "scale"
                      (and (= state.tool :select) (some? state.objectGizmo)) state.objectGizmo
                      :else "translate")
          :space (cond (#{:local :normal} state.orientation) "local" (= state.orientation :view) "view" :else "world")
          :snap (if state.snap computed.snapSteps {})}
         (cond (= state.pivot :individual) {:individual true}
               (= state.pivot :cursor) {:pivot state.cursor}
               (and (= state.pivot :active) computed.activeNode) {:pivot computed.activeNode.translation}
               (and (= state.pivot :bounds) (some? computed.boundsCentre)) {:pivot computed.boundsCentre}
               :else {})))

(defcomputed centre "Where a menu opens in the middle of the viewport."
  {:x (- (+ nodes.viewport.x (/ nodes.viewport.width 2)) 100)
   :y (- (+ nodes.viewport.y (/ nodes.viewport.height 2)) 60)})

;;; Collections

(defcomputed collections "The scene's collections; a scene without its own has Collection."
  (or-else state.scene.collections [{:id "collection" :name (t "collection.default")}]))

(defcomputed collectionIds "The collections' ids." (map :id computed.collections))

(defcomputed activeCollectionData "The active collection, or the first."
  (or-else (find #(= (:id %) state.activeCollection) computed.collections) computed.collections.0))

(defcomputed collectionsOff "The ids of the collections hidden, not selectable, and not rendered."
  {:visible (map :id (filter #(false? (:visible %)) computed.collections))
   :selectable (map :id (filter #(false? (:selectable %)) computed.collections))
   :render (map :id (filter #(false? (:render %)) computed.collections))})

;;; Opening a file

(defmacro three-numbers?
  ;; A macro, not a function: what it writes stays an expression.
  "Whether a value is a list of three numbers."
  [x]
  `(if (= (type ~x) "list") (and (= (count ~x) 3) (every? #(= (type %) "number") ~x)) false))

(defcomputed openedIsScene "Whether the file opened is a Modello scene, every object of it checked: at most 500 objects."
  (if (= (type state.opened) "object")
    (and (= state.opened.format "modello-scene")
         (= (type state.opened.added) "number")
         (if (= (type state.opened.scene) "object")
           (and (= (type state.opened.scene.materials) "list")
                (if (= (type state.opened.scene.nodes) "list")
                  (and (<= (count state.opened.scene.nodes) 500)
                       (every? (fn [it]
                                 (if (= (type it) "object")
                                   (and (= (type it.id) "string")
                                        (= (type it.name) "string")
                                        (contains? #{:mesh :light :camera} it.kind)
                                        (three-numbers? it.translation)
                                        (three-numbers? it.rotationDegrees)
                                        (three-numbers? it.scale))
                                   false))
                               state.opened.scene.nodes))
                  false))
           false))
    false))
