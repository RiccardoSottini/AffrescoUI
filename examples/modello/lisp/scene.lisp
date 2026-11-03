;;; scene.lisp --- TODO

(defcomputed activeNode (find (fn [it] (= it.id state.active)) state.scene.nodes))

(defcomputed selectedNodes (filter (fn [it] (contains state.view.selection it.id)) state.scene.nodes))

(defcomputed gizmo
  (merge {:visible (and state.gizmos
                          (or (and (contains ["move" "rotate" "scale"] state.tool) (not= state.toolGizmos false))
                              (and (= state.tool "select") (some? state.objectGizmo))))
            :mode (cond (= state.tool "rotate") "rotate"
                        (= state.tool "scale") "scale"
                        (and (= state.tool "select") (some? state.objectGizmo)) state.objectGizmo
                        :else "translate")
            :space (if (or (= state.orientation "local") (= state.orientation "normal")) "local" "world")
            :snap (if state.snap computed.snapSteps {})}
           (cond (= state.pivot "cursor") {:pivot state.cursor}
                 (and (= state.pivot "active") computed.activeNode) {:pivot computed.activeNode.translation}
                 (and (= state.pivot "bounds") (some? computed.boundsCentre)) {:pivot computed.boundsCentre}
                 :else {})))

(defcomputed openedIsScene
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
                                                  (or (= it.kind "mesh") (= it.kind "light") (= it.kind "camera"))
                                                  (if (= (type it.translation) "list")
                                                      (and (= (count it.translation) 3)
                                                           (every? (fn [it] (= (type it) "number")) it.translation))
                                                      false)
                                                  (if (= (type it.rotationDegrees) "list")
                                                      (and (= (count it.rotationDegrees) 3)
                                                           (every? (fn [it] (= (type it) "number"))
                                                                   it.rotationDegrees))
                                                      false)
                                                  (if (= (type it.scale) "list")
                                                      (and (= (count it.scale) 3)
                                                           (every? (fn [it] (= (type it) "number")) it.scale))
                                                      false))
                                             false))
                                       state.opened.scene.nodes))
                          false))
                 false))
        false))

(defcomputed centre
  {:x (- (+ nodes.viewport.x (/ nodes.viewport.width 2)) 100)
     :y (- (+ nodes.viewport.y (/ nodes.viewport.height 2)) 60)})

(defcomputed lastCentre
  (if (or (nil? state.lastOp) (= (count state.lastOp.base) 0))
        [0 0 0]
        (map (fn [it] (/ it (count state.lastOp.base)))
             [(sum (map (fn [it] it.translation.0) state.lastOp.base))
              (sum (map (fn [it] it.translation.1) state.lastOp.base))
              (sum (map (fn [it] it.translation.2) state.lastOp.base))])))

(defcomputed cameraNode (find (fn [it] (= it.kind "camera")) state.scene.nodes))

(defcomputed cameraView
  (if (nil? computed.cameraNode)
        nil
        {:target [(- computed.cameraNode.translation.0
                     (* (sin (radians computed.cameraNode.rotationDegrees.0))
                        (sin (radians computed.cameraNode.rotationDegrees.2))
                        10))
                  (+ computed.cameraNode.translation.1
                     (* (sin (radians computed.cameraNode.rotationDegrees.0))
                        (cos (radians computed.cameraNode.rotationDegrees.2))
                        10))
                  (- computed.cameraNode.translation.2 (* (cos (radians computed.cameraNode.rotationDegrees.0)) 10))]
         :yaw (radians computed.cameraNode.rotationDegrees.2)
         :pitch (- (/ (pi) 2) (radians computed.cameraNode.rotationDegrees.0))
         :distance 10
         :fieldOfView (or-else computed.cameraNode.camera.fieldOfView 0.69)
         :orthographic false}))

(defcomputed modalName
  (if state.modal
        (cond (= state.modal.mode "translate") "Move" (= state.modal.mode "rotate") "Rotate" :else "Resize")
        ""))

(defcomputed boundsCentre
  (if (= (count computed.selectedNodes) 0)
        nil
        [(/ (+ (max (map (fn [it] it.translation.0) computed.selectedNodes))
               (min (map (fn [it] it.translation.0) computed.selectedNodes)))
            2)
         (/ (+ (max (map (fn [it] it.translation.1) computed.selectedNodes))
               (min (map (fn [it] it.translation.1) computed.selectedNodes)))
            2)
         (/ (+ (max (map (fn [it] it.translation.2) computed.selectedNodes))
               (min (map (fn [it] it.translation.2) computed.selectedNodes)))
            2)]))

(defcomputed extrasShown (and state.overlays state.overlay.extras))

(defcomputed displayScene
  (if (and (= (count state.hiddenKinds) 0)
             computed.extrasShown
             (= (count computed.collectionsOff.visible) 0)
             (not= state.shading "rendered"))
        state.scene
        (merge state.scene
               {:nodes (map (fn [it]
                              (if (or (contains state.hiddenKinds it.kind)
                                      (and (not computed.extrasShown) (or (= it.kind "light") (= it.kind "camera")))
                                      (contains computed.collectionsOff.visible (or-else it.collection "collection"))
                                      (and (= state.shading "rendered")
                                           (or (= it.render false)
                                               (contains computed.collectionsOff.render
                                                         (or-else it.collection "collection")))))
                                  (merge it {:visible false})
                                  it))
                            state.scene.nodes)})))

(defcomputed pickable
  (map (fn [it] it.id)
         (filter (fn [it]
                   (and (not= it.visible false)
                        (not= it.selectable false)
                        (not (contains state.unselectableKinds it.kind))
                        (not (contains computed.collectionsOff.selectable (or-else it.collection "collection")))))
                 computed.displayScene.nodes)))

(defcomputed lit (or (= state.shading "rendered") (and (= state.shading "material") (= state.sceneLights true))))

(defcomputed worldShown (or (= state.shading "rendered") (and (= state.shading "material") (= state.sceneWorld true))))

(defcomputed snapSteps
  (cond (= state.snapStep "fine") {:translate 0.1 :rotate 1 :scale 0.01}
          (= state.snapStep "coarse") {:translate 10 :rotate 15 :scale 0.5}
          :else {:translate 1 :rotate 5 :scale 0.1}))

(defcomputed activeIndex (indexOf (map (fn [it] it.id) state.scene.nodes) state.active))

(defcomputed kindIcons {:mesh "triangle" :camera "video" :light "lightbulb" :empty "axis-3-d"})

(defcomputed collections (or-else state.scene.collections [{:id "collection" :name "Collection"}]))

(defcomputed collectionIds (map (fn [it] it.id) computed.collections))

(defcomputed activeCollectionData
  (or-else (find (fn [it] (= it.id state.activeCollection)) computed.collections) computed.collections.0))

(defcomputed collectionsOff
  {:visible (map (fn [it] it.id) (filter (fn [it] (= it.visible false)) computed.collections))
     :selectable (map (fn [it] it.id) (filter (fn [it] (= it.selectable false)) computed.collections))
     :render (map (fn [it] it.id) (filter (fn [it] (= it.render false)) computed.collections))})

(defcomputed stats
  (if state.overlays (str "Objects " (count state.view.selection) "/" (count state.scene.nodes) " | ") ""))

