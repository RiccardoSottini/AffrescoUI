;;; actions.lisp --- TODO

(defpart commands "What commands adds."
  :actions {:pick [{:if ~(some? state.modal) :then [{:stop true}]}
                   {:if ~(= event.type "boxSelect")
                    :then [{:set state.view.selection
                            :to ~(concat (if event.shift
                                             (filter (fn [it] (not (contains event.ids it))) state.view.selection)
                                             [])
                                         (filter (fn [it] (contains computed.pickable it)) event.ids))}
                           {:if ~(not (contains state.view.selection state.active))
                            :then [{:set state.active
                                    :to ~(if (> (count state.view.selection) 0)
                                             (get state.view.selection (- (count state.view.selection) 1))
                                             nil)}]}
                           {:stop true}]}
                   {:if ~(not= event.type "pick") :then [{:stop true}]}
                   {:if ~(and (= event.button "right") event.shift)
                    :then [{:set state.cursor :to ~(map (fn [it] (/ (round (* it 10000)) 10000)) event.point)} {:stop true}]}
                   {:if ~(= event.button "right")
                    :then [{:run openPopup
                            :with {:args {:menu objectContext :x ~(+ nodes.viewport.x event.x) :y ~(+ nodes.viewport.y event.y)}}}
                           {:stop true}]}
                   {:if ~(= state.tool "cursor")
                    :then [{:set state.cursor :to ~(map (fn [it] (/ (round (* it 10000)) 10000)) event.point)} {:stop true}]}
                   {:if ~(= state.tool "measure")
                    :then [{:run measurePoint :with {:args {:point ~event.point :x ~event.x :y ~event.y}}} {:stop true}]}
                   {:if ~(= state.tool "add")
                    :then [{:run addObject
                            :with {:args {:kind mesh
                                          :mesh cube
                                          :name "Cube"
                                          :light nil
                                          :at ~(map (fn [it] (/ (round (* it 10000)) 10000)) event.point)}}}
                           {:stop true}]}
                   {:if ~(or (nil? event.id) (not (contains computed.pickable event.id)))
                    :then [{:if ~(not event.shift) :then [{:set state.view.selection :to []}]} {:stop true}]}
                   {:run selectNode :with {:args {:id ~event.id :shift ~event.shift}}}]
            :measurePoint [{:if ~(or (>= (count state.measure.points) 2) (not (= state.measure.camera state.view.camera)))
                            :then [{:set state.measure :to {:points [] :screen [] :camera ~state.view.camera}}]}
                           {:push state.measure.points :value ~(map (fn [it] (/ (round (* it 10000)) 10000)) args.point)}
                           {:push state.measure.screen :value ~[args.x args.y]}]
            :transformed [{:if ~(= event.phase "preview") :then [{:set state.modalText :to ~event.text} {:stop true}]}
                          {:set state.modal :to nil}
                          {:set state.modalText :to ""}
                          {:if ~(not= event.phase "end") :then [{:stop true}]}
                          {:run snapshot}
                          {:set state.lastBase :to []}
                          {:forEach ~event.nodes
                           :as m
                           :do [{:push state.lastBase
                                 :value ~(find (fn [it] (= it.id m.id))
                                               (get-in state.undoStack [(- (count state.undoStack) 1) :nodes]))}
                                {:set state.tmp
                                 :to ~(get state.scene.nodes (indexOf (map (fn [it] it.id) state.scene.nodes) m.id))}
                                {:set "state.scene.nodes[indexOf(map(state.scene.nodes, it.id), m.id)]"
                                 :to ~(merge state.tmp
                                             {:translation (map-indexed (fn [index it]
                                                                          (if (= (get state.tmp.lock_translation index) true)
                                                                              (get state.tmp.translation index)
                                                                              (/ (round (* it 10000)) 10000)))
                                                                        m.translation)
                                              :rotationDegrees (map-indexed (fn [index it]
                                                                              (if (= (get state.tmp.lock_rotationDegrees index)
                                                                                     true)
                                                                                  (get state.tmp.rotationDegrees index)
                                                                                  (/ (round (* it 1000)) 1000)))
                                                                            m.rotationDegrees)
                                              :scale (map-indexed (fn [index it]
                                                                    (if (= (get state.tmp.lock_scale index) true)
                                                                        (get state.tmp.scale index)
                                                                        (/ (round (* it 10000)) 10000)))
                                                                  m.scale)})}]}
                          {:set state.last :to ~event.text}
                          {:set state.lastOp
                           :to ~{:title (cond (= event.mode "translate") "Move"
                                              (= event.mode "rotate") "Rotate"
                                              :else "Resize")
                                 :kind event.mode
                                 :base state.lastBase
                                 :values (cond (= event.mode "translate") (map-indexed (fn [index it]
                                                              (/ (round (* (- it (get state.lastBase.0.translation index))
                                                                           10000))
                                                                 10000))
                                                            event.nodes.0.translation)
                                               (= event.mode "scale") (map-indexed (fn [index it]
                                                              (if (= (get state.lastBase.0.scale index) 0)
                                                                  1
                                                                  (/ (round (* (/ it (get state.lastBase.0.scale index)) 10000))
                                                                     10000)))
                                                            event.nodes.0.scale)
                                               (= (count state.lastBase) 1) (map-indexed (fn [index it]
                                                              (/ (round (* (- it (get state.lastBase.0.rotationDegrees index))
                                                                           100))
                                                                 100))
                                                            event.nodes.0.rotationDegrees)
                                               :else nil)}}
                          {:run autoKey}]
            :search {:title "Search…"
                     :category "Edit"
                     :icon search
                     :do [{:dismiss true} {:set state.quickPick :to {:open true :query ""}}]}
            :adjustLastPanel {:title "Adjust Last Operation" :category "Edit" :do [{:set state.redoOpen :to true}]}
            :adjustLast [{:if ~(or (nil? state.lastOp) (nil? state.lastOp.values)) :then [{:stop true}]}
                         {:set "state.lastOp.values[args.axis]" :to ~(/ (round (* args.value 10000)) 10000)}
                         {:forEach ~state.lastOp.base
                          :as b
                          :do [{:set "state.scene.nodes[indexOf(map(state.scene.nodes, it.id), b.id)]"
                                :to ~(merge b
                                            (cond (= state.lastOp.kind "scale") {:translation (map-indexed (fn [index it]
                                                                               (if (= (get b.lock_translation index) true)
                                                                                   it
                                                                                   (+ (get computed.lastCentre index)
                                                                                      (* (- it (get computed.lastCentre index))
                                                                                         (get state.lastOp.values index)))))
                                                                             b.translation)
                                                   :scale (map-indexed (fn [index it]
                                                                         (if (= (get b.lock_scale index) true)
                                                                             it
                                                                             (/ (round (* it
                                                                                          (get state.lastOp.values index)
                                                                                          10000))
                                                                                10000)))
                                                                       b.scale)}
                                                  (= state.lastOp.kind "rotate") {:rotationDegrees (map-indexed (fn [index it]
                                                                                   (if (= (get b.lock_rotationDegrees index)
                                                                                          true)
                                                                                       it
                                                                                       (/ (round (* (+ it
                                                                                                       (get state.lastOp.values
                                                                                                            index))
                                                                                                    100))
                                                                                          100)))
                                                                                 b.rotationDegrees)}
                                                  :else {:translation (map-indexed (fn [index it]
                                                                               (if (= (get b.lock_translation index) true)
                                                                                   it
                                                                                   (/ (round (* (+ it
                                                                                                   (get state.lastOp.values
                                                                                                        index))
                                                                                                10000))
                                                                                      10000)))
                                                                             b.translation)}))}]}
                         {:run autoKey}]
            :startTransform [{:if ~(= (count state.view.selection) 0) :then [{:stop true}]}
                             {:set state.modal :to {:mode ~args.mode :axis view :typed ""}}
                             {:set state.modalText :to ~computed.modalName}
                             {:transform "#viewport" :mode ~args.mode}]
            :grab {:title "Move" :category "Object" :icon move :do [{:run startTransform :with {:args {:mode translate}}}]}
            :rotate {:title "Rotate" :category "Object" :icon rotate-cw :do [{:run startTransform :with {:args {:mode rotate}}}]}
            :resize {:title "Scale" :category "Object" :icon scale-3-d :do [{:run startTransform :with {:args {:mode scale}}}]}
            :lockAxis [{:set state.modal.axis :to ~(if (= state.modal.axis args.axis) "view" args.axis)}
                       {:transform "#viewport" :axis ~state.modal.axis}]
            :typeDigit [{:set state.modal.typed
                         :to ~(cond (= args.key "back") (slice state.modal.typed 0 (max 0 (- (count state.modal.typed) 1)))
                                    (= args.key "-") (if (= (slice state.modal.typed 0 1) "-")
                                        (slice state.modal.typed 1)
                                        (str "-" state.modal.typed))
                                    :else (+ state.modal.typed args.key))}
                        {:transform "#viewport" :value ~(number state.modal.typed)}]
            :confirmTransform [{:transform "#viewport" :end confirm}]
            :cancelTransform [{:transform "#viewport" :end cancel} {:set state.modal :to nil}]
            :openPopup [{:set state.ctx
                         :to {:open true
                              :menu ~args.menu
                              :items nil
                              :x ~(or-else args.x computed.centre.x)
                              :y ~(or-else args.y computed.centre.y)}}]
            :openPie [{:if ~(or (some? state.modal) (not= (get state.areas (or-else state.hoverSlot "a")) "viewport"))
                       :then [{:stop true}]}
                      {:set state.pie
                       :to {:open true
                            :menu ~args.menu
                            :x ~(+ nodes.viewport.x (/ nodes.viewport.width 2))
                            :y ~(+ nodes.viewport.y (/ nodes.viewport.height 2))}}]
            :addMenu {:title "Add"
                      :category "Object"
                      :icon plus
                      :do [{:run openPopup
                            :with {:args {:menu ~(if (= (get state.areas state.hoverSlot) "shader") "shaderAdd" "add")
                                          :x nil
                                          :y nil}}}]}
            :deleteMenu {:title "Delete…"
                         :category "Object"
                         :do [{:if ~(> (count state.view.selection) 0)
                               :then [{:run openPopup :with {:args {:menu delete :x nil :y nil}}}]}]}
            :uniqueName [{:set state.tmp :to {:n 0 :name ~args.name}}
                         {:while ~(some (fn [it] (= it.name state.tmp.name)) state.scene.nodes)
                          :do [{:increment state.tmp.n :by 1}
                               {:set state.tmp.name :to ~(str args.name "." (pad state.tmp.n 3 "0"))}]}]
            :addObject [{:dismiss true}
                        {:if ~(not= state.mode "object") :then [{:set state.mode :to object}]}
                        {:run snapshot}
                        {:increment state.added :by 1}
                        {:run uniqueName :with {:args {:name ~args.name}}}
                        {:push state.scene.nodes
                         :value {:id ~(str (lower-case args.kind) "-" state.added)
                                 :name ~state.tmp.name
                                 :kind ~args.kind
                                 :collection ~(if (and (not= state.activeCollection "collection")
                                                       (contains (map (fn [it] it.id) computed.collections)
                                                                 state.activeCollection))
                                                  state.activeCollection
                                                  nil)
                                 :mesh ~(or-else args.mesh nil)
                                 :material ~(if (= args.kind "mesh") "material" nil)
                                 :light ~(or-else args.light nil)
                                 :camera ~(if (= args.kind "camera") {:fieldOfView 0.69} nil)
                                 :translation ~(or-else args.at state.cursor)
                                 :rotationDegrees [0 0 0]
                                 :scale [1 1 1]}}
                        {:set state.view.selection :to ~[(str (lower-case args.kind) "-" state.added)]}
                        {:set state.active :to ~(str (lower-case args.kind) "-" state.added)}
                        {:set state.last :to ~(str "Add " args.name)}
                        {:set state.lastOp
                         :to ~{:title (str "Add " args.name)
                               :kind "translate"
                               :label "Location"
                               :base [(merge (get state.scene.nodes (- (count state.scene.nodes) 1)) {:translation [0 0 0]})]
                               :values (or-else args.at state.cursor)}}]
            :duplicate {:title "Duplicate Objects"
                        :category "Object"
                        :icon copy
                        :enablement ~(> (count state.view.selection) 0)
                        :do [{:run snapshot}
                             {:set state.dupes :to []}
                             {:forEach ~computed.selectedNodes
                              :as m
                              :do [{:increment state.added :by 1}
                                   {:run uniqueName :with {:args {:name ~(get (split m.name ".") 0)}}}
                                   {:push state.scene.nodes
                                    :value ~(merge m {:id (str (get (split m.id "-") 0) "-" state.added) :name state.tmp.name})}
                                   {:push state.dupes :value ~(str (get (split m.id "-") 0) "-" state.added)}]}
                             {:set state.view.selection :to ~state.dupes}
                             {:set state.active :to ~(get state.dupes (- (count state.dupes) 1))}
                             {:after 0 :do [{:run startTransform :with {:args {:mode translate}}}]}]}
            :selectAll {:title "Select All" :category "Select" :do [{:set state.view.selection :to ~computed.pickable}]}
            :selectNone {:title "Select None" :category "Select" :do [{:set state.view.selection :to []}]}
            :selectInvert {:title "Invert Selection"
                           :category "Select"
                           :do [{:set state.view.selection
                                 :to ~(filter (fn [it] (not (contains state.view.selection it))) computed.pickable)}]}
            :selectNode [{:if ~(not args.shift)
                          :then [{:set state.view.selection :to ~[args.id]} {:set state.active :to ~args.id} {:stop true}]}
                         {:if ~(and (contains state.view.selection args.id) (= state.active args.id))
                          :then [{:set state.view.selection :to ~(filter (fn [it] (not= it args.id)) state.view.selection)}]
                          :else [{:set state.view.selection
                                  :to ~(concat (filter (fn [it] (not= it args.id)) state.view.selection) [args.id])}
                                 {:set state.active :to ~args.id}]}]
            :toggleFlag [{:set "state.scene.nodes[indexOf(map(state.scene.nodes, it.id), args.id)][args.key]"
                          :to ~(= (get-in state.scene.nodes
                                          [(indexOf (map (fn [it] it.id) state.scene.nodes) args.id) args.key])
                                  false)}
                         {:if ~(and (not= args.key "render")
                                    (= (get-in state.scene.nodes
                                               [(indexOf (map (fn [it] it.id) state.scene.nodes) args.id) args.key])
                                       false))
                          :then [{:set state.view.selection :to ~(filter (fn [it] (not= it args.id)) state.view.selection)}]}]
            :setField [{:if ~(= args.key "redo")
                        :then [{:run adjustLast :with {:args {:axis ~args.axis :value ~args.value}}} {:stop true}]}
                       {:if ~(= args.key "whole")
                        :then [{:set ~(str "state." args.path) :to ~(round (clamp args.value args.min args.max))} {:stop true}]}
                       {:if ~(not= args.path "")
                        :then [{:set ~(str "state." args.path)
                                :to ~(/ (round (* (clamp args.value args.min args.max) 10000)) 10000)}
                               {:stop true}]}
                       {:if ~(= args.key "cameraLens")
                        :then [{:if ~(or (< computed.activeIndex 0) (not= computed.activeNode.kind "camera")) :then [{:stop true}]}
                               {:set "state.scene.nodes[computed.activeIndex].camera.fieldOfView"
                                :to ~(/ (round (* 2 (atan (/ 18 (clamp args.value args.min args.max))) 10000)) 10000)}
                               {:stop true}]}
                       {:if ~(= args.key "lens")
                        :then [{:set state.view.camera.fieldOfView
                                :to ~(/ (round (* 2 (atan (/ 18 (clamp (round args.value) 2 250))) 10000)) 10000)}
                               {:stop true}]}
                       {:if ~(= args.key "cursor")
                        :then [{:set "state.cursor[args.axis]" :to ~(/ (round (* args.value 10000)) 10000)} {:stop true}]}
                       {:if ~(< computed.activeIndex 0) :then [{:stop true}]}
                       {:set "state.scene.nodes[computed.activeIndex][args.key][args.axis]"
                        :to ~(/ (round (* args.value 10000)) 10000)}
                       {:run autoKey}]
            :setColor [{:if ~(nil? (colorList args.value)) :then [{:stop true}]}
                       {:if ~(= args.target "world")
                        :then [{:set state.world.color :to ~(slice (hexColor args.value) 0 7)} {:stop true}]}
                       {:if ~(and (= args.target "material") (>= computed.activeMaterialIndex 0))
                        :then [{:set "state.scene.materials[computed.activeMaterialIndex].baseColor" :to ~(colorList args.value)}
                               {:stop true}]}
                       {:if ~(and (= args.target "light") (= computed.activeNode.kind "light"))
                        :then [{:set "state.scene.nodes[computed.activeIndex].light.color" :to ~(slice (colorList args.value)
                                                                                                       0
                                                                                                       3)}]}]
            :setLight [{:if ~(or (< computed.activeIndex 0)
                                 (not= computed.activeNode.kind "light")
                                 (= (get computed.activeNode.light args.key) args.value))
                        :then [{:stop true}]}
                       {:run snapshot}
                       {:set "state.scene.nodes[computed.activeIndex].light[args.key]" :to ~args.value}]
            :toggleLock [{:if ~(< computed.activeIndex 0) :then [{:stop true}]}
                         {:set "state.scene.nodes[computed.activeIndex]['lock_' + args.key]"
                          :to ~(map-indexed (fn [index it]
                                              (if (= index args.axis)
                                                  (not= (get-in computed.activeNode [(str "lock_" args.key) index]) true)
                                                  (= (get-in computed.activeNode [(str "lock_" args.key) index]) true)))
                                            (range 3))}]
            :rename [{:if ~(or (= (trim args.name) "")
                               (not (contains (map (fn [it] it.id) state.scene.nodes) (or-else args.id state.active))))
                      :then [{:stop true}]}
                     {:run snapshot}
                     {:set "state.scene.nodes[indexOf(map(state.scene.nodes, it.id), args.id ?? state.active)].name"
                      :to ~(slice (trim args.name) 0 63)}]
            :startRename {:title "Rename"
                          :category "Object"
                          :icon pencil
                          :do [{:dismiss true}
                               {:if ~(or (nil? state.active) (not (contains state.view.selection state.active)))
                                :then [{:stop true}]}
                               {:if ~(not (contains (vals state.areas) "outliner"))
                                :then [{:run swapEditor :with {:args {:from properties :to outliner}}}]}
                               {:set state.outlinerHide
                                :to ~(filter (fn [it] (not= it computed.activeNode.kind)) state.outlinerHide)}
                               {:set state.outlinerFilter :to ""}
                               {:set state.outlinerOpen.root :to true}
                               {:set state.outlinerOpen.collection :to true}
                               {:set state.renaming :to ~state.active}
                               {:after 0 :do [{:focus "#outliner-rename"}]}]}
            :toggleCollection [{:if ~(nil? state.scene.collections)
                                :then [{:set state.scene.collections :to ~computed.collections}]}
                               {:set state.tmp :to ~(indexOf (map (fn [it] it.id) state.scene.collections) args.id)}
                               {:if ~(< state.tmp 0) :then [{:stop true}]}
                               {:set "state.scene.collections[state.tmp][args.key]"
                                :to ~(= (get-in state.scene.collections [state.tmp args.key]) false)}
                               {:if ~(and (not= args.key "render")
                                          (= (get-in state.scene.collections [state.tmp args.key]) false))
                                :then [{:set state.view.selection
                                        :to ~(map (fn [it] it.id)
                                                  (filter (fn [it]
                                                            (and (contains state.view.selection it.id)
                                                                 (not= (or-else it.collection "collection") args.id)))
                                                          state.scene.nodes))}]}]
            :newCollection {:title "New Collection"
                            :category "Collection"
                            :icon folder-plus
                            :do [{:dismiss true}
                                 {:run snapshot}
                                 {:if ~(nil? state.scene.collections)
                                  :then [{:set state.scene.collections :to ~computed.collections}]}
                                 {:increment state.added :by 1}
                                 {:set state.tmp :to ~{:n 1 :name "Collection 1" :id (str "collection-" state.added)}}
                                 {:while ~(some (fn [it] (= it.name state.tmp.name)) state.scene.collections)
                                  :do [{:increment state.tmp.n :by 1} {:set state.tmp.name :to ~(str "Collection " state.tmp.n)}]}
                                 {:push state.scene.collections :value ~{:id state.tmp.id :name state.tmp.name}}
                                 {:set "state.outlinerOpen[state.tmp.id]" :to true}
                                 {:if ~(= args.move true)
                                  :then [{:run moveToCollection :with {:args {:id ~state.tmp.id :snapshot false}}}]
                                  :else [{:set state.activeCollection :to ~state.tmp.id}]}]}
            :renameCollection [{:if ~(or (= (trim args.name) "") (not (contains computed.collectionIds args.id)))
                                :then [{:stop true}]}
                               {:run snapshot}
                               {:if ~(nil? state.scene.collections)
                                :then [{:set state.scene.collections :to ~computed.collections}]}
                               {:set "state.scene.collections[indexOf(computed.collectionIds, args.id)].name"
                                :to ~(slice (trim args.name) 0 63)}]
            :startCollectionRename {:title "Rename Collection"
                                    :category "Collection"
                                    :icon pencil
                                    :enablement ~(contains (vals state.areas) "outliner")
                                    :do [{:dismiss true}
                                         {:if ~(not (contains (vals state.areas) "outliner")) :then [{:stop true}]}
                                         {:set state.outlinerFilter :to ""}
                                         {:set state.outlinerOpen.root :to true}
                                         {:set state.renaming :to ~(str "coll:" computed.activeCollectionData.id)}
                                         {:after 50 :do [{:focus "#outliner-rename"}]}]}
            :deleteCollection {:title "Delete Collection"
                               :category "Collection"
                               :icon trash-2
                               :enablement ~(> (count computed.collections) 1)
                               :do [{:dismiss true}
                                    {:if ~(< (count computed.collections) 2) :then [{:stop true}]}
                                    {:run snapshot}
                                    {:set state.tmp
                                     :to ~{:gone computed.activeCollectionData.id
                                           :to (:id (find (fn [it] (not= it.id computed.activeCollectionData.id))
                                                          computed.collections))}}
                                    {:set state.scene.nodes
                                     :to ~(map (fn [it]
                                                 (if (= (or-else it.collection "collection") state.tmp.gone)
                                                     (merge it {:collection state.tmp.to})
                                                     it))
                                               state.scene.nodes)}
                                    {:set state.scene.collections
                                     :to ~(filter (fn [it] (not= it.id state.tmp.gone)) computed.collections)}
                                    {:set state.activeCollection :to ~state.tmp.to}
                                    {:set state.last :to "Delete Collection"}]}
            :moveToCollection [{:dismiss true}
                               {:if ~(= (count state.view.selection) 0) :then [{:stop true}]}
                               {:if ~(= args.snapshot true) :then [{:run snapshot}]}
                               {:set state.scene.nodes
                                :to ~(map (fn [it]
                                            (if (contains state.view.selection it.id) (merge it {:collection args.id}) it))
                                          state.scene.nodes)}
                               {:set "state.outlinerOpen[args.id]" :to true}
                               {:set state.last :to "Move to Collection"}]
            :moveToCollectionMenu {:title "Move to Collection"
                                   :category "Object"
                                   :icon package
                                   :enablement ~(> (count state.view.selection) 0)
                                   :do [{:if ~(and (> (count state.view.selection) 0) (nil? state.modal))
                                         :then [{:set state.ctx
                                                 :to {:open true
                                                      :menu moveTo
                                                      :x ~computed.centre.x
                                                      :y ~computed.centre.y
                                                      :items ~(concat (map (fn [it]
                                                                             {:title it.name
                                                                              :icon "package"
                                                                              :run "moveToCollection"
                                                                              :with {:args {:id it.id :snapshot true}}})
                                                                           computed.collections)
                                                                      [{:separator true}
                                                                       {:title "New Collection"
                                                                        :icon "plus"
                                                                        :run "newCollection"
                                                                        :with {:args {:move true}}}])}}]}]}
            :setMode {:title "Set Mode" :category "Object" :do [{:dismiss true} {:set state.mode :to ~args.mode}]}
            :setOption [{:dismiss true} {:set "state[args.key]" :to ~args.value}]
            :toggleOption [{:toggle "state[args.key]"}]
            :swapEditor [{:dismiss true}
                         {:set state.tmp
                          :to ~{:from (find (fn [it] (= (get state.areas it) args.from)) (keys state.areas))
                                :to (find (fn [it] (= (get state.areas it) args.to)) (keys state.areas))}}
                         {:if ~(nil? state.tmp.from) :then [{:stop true}]}
                         {:if ~(nil? state.tmp.to) :then [{:set "state.areas[state.tmp.from]" :to ~args.to} {:stop true}]}
                         {:set "state.areas[state.tmp.from]" :to ~args.to}
                         {:set "state.areas[state.tmp.to]" :to ~args.from}]
            :setWorkspace [{:dismiss true}
                           {:set state.workspace :to ~args.title}
                           {:set state.areas
                            :to {:a viewport :b ~(if (= args.id "shading") "shader" "timeline") :c outliner :d properties}}
                           {:set state.sizes.left
                            :to ~[(cond (= args.id "animation") 520
                                        (= args.id "shading") 470
                                        (= args.id "modeling") 790
                                        :else 720)]}
                           {:set state.shading
                            :to ~(cond (= args.id "shading") "material" (= args.id "rendering") "rendered" :else "solid")}
                           {:set state.xray :to false}
                           {:set state.overlays :to ~(not= args.id "rendering")}
                           {:set state.propTab
                            :to ~(cond (= args.id "shading") "material" (= args.id "rendering") "render" :else state.propTab)}]
            :toggleOverlay [{:toggle "state.overlay[args.key]"}]
            :toggleKind [{:set "state[args.list]"
                          :to ~(if (contains (get state args.list) args.kind)
                                   (filter (fn [it] (not= it args.kind)) (get state args.list))
                                   (concat (get state args.list) [args.kind]))}
                         {:set state.view.selection :to ~(filter (fn [it] (contains computed.pickable it))
                                                                 state.view.selection)}]
            :setObjectGizmo [{:set state.objectGizmo :to ~(if (= state.objectGizmo args.mode) nil args.mode)}]
            :toggleXray {:title "Toggle X-Ray" :category "View" :icon square-dashed-bottom :do [{:toggle state.xray}]}
            :toggleSnap {:title "Toggle Snapping" :category "Object" :icon magnet :do [{:toggle state.snap}]}
            :toggleOverlays {:title "Toggle Overlays" :category "View" :do [{:toggle state.overlays}]}
            :setTool [{:set state.tool :to ~args.tool} {:set state.measure :to {:points [] :screen [] :camera nil}}]
            :toggleSidebar {:title "Sidebar" :category "View" :do [{:toggle state.sidebar}]}
            :toggleToolbar {:title "Toolbar" :category "View" :do [{:set state.toolbar :to ~(= state.toolbar false)}]}
            :toggleTheme {:title "Toggle Light and Dark Theme"
                          :category "Window"
                          :icon sun-moon
                          :do [{:set state.theme :to ~(if (= state.theme "dark") "light" "dark")}]}
            :viewAxis [{:set state.inCamera :to false} {:camera "#viewport" :view ~args.view}]
            :viewFront {:title "Front" :category "View" :do [{:run viewAxis :with {:args {:view front}}}]}
            :viewBack {:title "Back" :category "View" :do [{:run viewAxis :with {:args {:view back}}}]}
            :viewRight {:title "Right" :category "View" :do [{:run viewAxis :with {:args {:view right}}}]}
            :viewLeft {:title "Left" :category "View" :do [{:run viewAxis :with {:args {:view left}}}]}
            :viewTop {:title "Top" :category "View" :do [{:run viewAxis :with {:args {:view top}}}]}
            :viewBottom {:title "Bottom" :category "View" :do [{:run viewAxis :with {:args {:view bottom}}}]}
            :viewCamera {:title "Camera"
                         :category "View"
                         :icon video
                         :enablement ~(some? computed.cameraNode)
                         :do [{:if ~state.inCamera :then [{:set state.inCamera :to false} {:stop true}]}
                              {:set state.view.camera :to ~computed.cameraView}
                              {:set state.inCamera :to true}]}
            :toggleProjection {:title "Perspective/Orthographic" :category "View" :do [{:camera "#viewport" :projection toggle}]}
            :frameSelected {:title "Frame Selected"
                            :category "View"
                            :do [{:set state.inCamera :to false} {:camera "#viewport" :frame selection}]}
            :frameAll {:title "Frame All" :category "View" :do [{:set state.inCamera :to false} {:camera "#viewport" :frame all}]}
            :orbitView [{:set state.inCamera :to false} {:camera "#viewport" :orbit ~args.orbit}]
            :zoomView [{:camera "#viewport" :zoom ~args.steps}]
            :navDrag [{:if ~(= args.id "zoom") :then [{:camera "#viewport" :zoom ~(* (- args.dy) 0.05)}]}
                      {:if ~(= args.id "pan")
                       :then [{:set state.view.camera.target
                               :to ~[(- state.view.camera.target.0
                                        (* (cos state.view.camera.yaw) args.dx state.view.camera.distance 0.002))
                                     (+ state.view.camera.target.1
                                        (* (sin state.view.camera.yaw) args.dx state.view.camera.distance 0.002))
                                     (+ state.view.camera.target.2 (* args.dy state.view.camera.distance 0.002))]}]}]
            :navClick [{:if ~(= args.id "camera") :then [{:run viewCamera}]}
                       {:if ~(= args.id "ortho") :then [{:run toggleProjection}]}]
            :insertKeyframe {:title "Insert Keyframe"
                             :category "Animation"
                             :icon diamond
                             :enablement ~(> (count state.view.selection) 0)
                             :do [{:dismiss true} {:run snapshot} {:run keySelected} {:set state.last :to "Insert Keyframe"}]}
            :keySelected [{:forEach ~state.view.selection
                           :as id
                           :do [{:set state.tmp :to ~(indexOf (map (fn [it] it.id) state.scene.nodes) id)}
                                {:if ~(>= state.tmp 0)
                                 :then [{:set "state.scene.nodes[state.tmp].keyframes"
                                         :to ~(sort-by (fn [it] it.frame)
                                                       (concat (filter (fn [it] (not= it.frame state.frame))
                                                                       (or-else (get-in state.scene.nodes
                                                                                        [state.tmp :keyframes])
                                                                                []))
                                                               [{:frame state.frame
                                                                 :translation (get-in state.scene.nodes
                                                                                      [state.tmp :translation])
                                                                 :rotationDegrees (get-in state.scene.nodes
                                                                                          [state.tmp :rotationDegrees])
                                                                 :scale (get-in state.scene.nodes [state.tmp :scale])}]))}]}]}]
            :autoKey [{:if ~(= state.autoKey true) :then [{:run keySelected}]}]
            :deleteKeyframe {:title "Delete Keyframes"
                             :category "Animation"
                             :icon diamond-minus
                             :enablement ~(contains computed.keyFrames state.frame)
                             :do [{:dismiss true}
                                  {:if ~(not (contains computed.keyFrames state.frame)) :then [{:stop true}]}
                                  {:run snapshot}
                                  {:set state.scene.nodes
                                   :to ~(map (fn [it]
                                               (if (and (contains state.view.selection it.id) (some? it.keyframes))
                                                   (merge it
                                                          {:keyframes (filter (fn [it] (not= it.frame state.frame))
                                                                              it.keyframes)})
                                                   it))
                                             state.scene.nodes)}
                                  {:set state.last :to "Delete Keyframes"}]}
            :clearKeyframes {:title "Clear Keyframes"
                             :category "Animation"
                             :enablement ~(> (count computed.keyFrames) 0)
                             :do [{:dismiss true}
                                  {:if ~(= (count computed.keyFrames) 0) :then [{:stop true}]}
                                  {:run snapshot}
                                  {:set state.scene.nodes
                                   :to ~(map (fn [it]
                                               (if (and (contains state.view.selection it.id) (some? it.keyframes))
                                                   (merge it {:keyframes []})
                                                   it))
                                             state.scene.nodes)}
                                  {:set state.last :to "Clear Keyframes"}]}
            :moveKeyframe [{:set state.tmp :to ~{:from (+ (round (* event.from 24)) 1) :to (+ (round (* event.time 24)) 1)}}
                           {:if ~(or (= state.tmp.from state.tmp.to)
                                     (contains computed.keyFrames state.tmp.to)
                                     (not (contains computed.keyFrames state.tmp.from)))
                            :then [{:stop true}]}
                           {:if ~(not= state.keyDragAt state.tmp.from) :then [{:run snapshot}]}
                           {:set state.keyDragAt :to ~state.tmp.to}
                           {:set state.scene.nodes
                            :to ~(map (fn [it]
                                        (if (and (contains state.view.selection it.id) (some? it.keyframes))
                                            (merge it
                                                   {:keyframes (sort-by (fn [it] it.frame)
                                                                        (map (fn [it]
                                                                               (if (= it.frame state.tmp.from)
                                                                                   (merge it {:frame state.tmp.to})
                                                                                   it))
                                                                             it.keyframes))})
                                            it))
                                      state.scene.nodes)}
                           {:run poseFrame}]
            :poseFrame [{:forEach ~(filter (fn [it] (> (count (or-else it.keyframes [])) 0)) state.scene.nodes)
                         :as n
                         :do [{:set state.pose
                               :to ~{:at (indexOf (map (fn [it] it.id) state.scene.nodes) n.id)
                                     :a (or-else (find (fn [it] (<= it.frame state.frame)) (reverse n.keyframes))
                                                 n.keyframes.0)
                                     :b (or-else (find (fn [it] (>= it.frame state.frame)) n.keyframes)
                                                 (get n.keyframes (- (count n.keyframes) 1)))}}
                              {:set state.pose.u
                               :to ~(if (= state.pose.b.frame state.pose.a.frame)
                                        0
                                        (/ (- state.frame state.pose.a.frame) (- state.pose.b.frame state.pose.a.frame)))}
                              {:set state.pose.u :to ~(* state.pose.u state.pose.u (- 3 (* 2 state.pose.u)))}
                              {:set "state.scene.nodes[state.pose.at]"
                               :to ~(merge n
                                           {:translation (map-indexed (fn [index it]
                                                                        (/ (round (* (+ it
                                                                                        (* (- (get state.pose.b.translation
                                                                                                   index)
                                                                                              it)
                                                                                           state.pose.u))
                                                                                     10000))
                                                                           10000))
                                                                      state.pose.a.translation)
                                            :rotationDegrees (map-indexed (fn [index it]
                                                                            (/ (round (* (+ it
                                                                                            (* (- (get state.pose.b.rotationDegrees
                                                                                                       index)
                                                                                                  it)
                                                                                               state.pose.u))
                                                                                         1000))
                                                                               1000))
                                                                          state.pose.a.rotationDegrees)
                                            :scale (map-indexed (fn [index it]
                                                                  (/ (round (* (+ it
                                                                                  (* (- (get state.pose.b.scale index) it)
                                                                                     state.pose.u))
                                                                               10000))
                                                                     10000))
                                                                state.pose.a.scale)})}]}]
            :setFrame [{:set state.frame :to ~(round (clamp args.value 0 1048574))} {:run poseFrame}]
            :setFrameField [{:if ~(= args.key "frame") :then [{:run setFrame}] :else [{:run setRange}]}]
            :setRange [{:set state.tmp :to ~(round (clamp args.value 0 1048574))}
                       {:if ~(= args.key "start")
                        :then [{:set ~(str "state." computed.rangeKeys.0) :to ~(min state.tmp computed.range.1)}]
                        :else [{:set ~(str "state." computed.rangeKeys.1) :to ~(max state.tmp computed.range.0)}]}]
            :play {:title "Play Animation"
                   :category "Playback"
                   :icon play
                   :do [{:dismiss true} {:set state.playing :to ~(if (not= state.playing "forward") "forward" false)}]}
            :playReverse {:title "Play Animation in Reverse"
                          :category "Playback"
                          :do [{:dismiss true} {:set state.playing :to ~(if (not= state.playing "reverse") "reverse" false)}]}
            :tick [{:set state.frame
                    :to ~(cond (= state.playing "reverse") (if (<= state.frame computed.range.0) computed.range.1 (- state.frame 1))
                               (>= state.frame computed.range.1) computed.range.0
                               :else (+ state.frame 1))}
                   {:run poseFrame}]
            :jumpStart {:title "Jump to Start"
                        :category "Playback"
                        :do [{:dismiss true} {:run setFrame :with {:args {:value ~computed.range.0}}}]}
            :jumpEnd {:title "Jump to End"
                      :category "Playback"
                      :do [{:dismiss true} {:run setFrame :with {:args {:value ~computed.range.1}}}]}
            :prevKey {:title "Jump to Previous Keyframe"
                      :category "Playback"
                      :do [{:dismiss true}
                           {:run setFrame
                            :with {:args {:value ~(max (concat [computed.range.0]
                                                               (filter (fn [it] (< it state.frame)) computed.keyFrames)))}}}]}
            :nextKey {:title "Jump to Next Keyframe"
                      :category "Playback"
                      :do [{:dismiss true}
                           {:run setFrame
                            :with {:args {:value ~(min (concat [computed.range.1]
                                                               (filter (fn [it] (> it state.frame)) computed.keyFrames)))}}}]}
            :togglePreviewRange {:title "Use Preview Range"
                                 :category "Playback"
                                 :icon clock-3
                                 :do [{:dismiss true}
                                      {:if ~(nil? state.previewRange)
                                       :then [{:set state.previewRange :to ~{:start state.start :end state.end}}]
                                       :else [{:set state.previewRange :to nil}]}]}
            :addMarker {:title "Add Marker"
                        :category "Marker"
                        :icon bookmark
                        :do [{:dismiss true}
                             {:if ~(some (fn [it] (= it.frame state.frame)) state.markers) :then [{:stop true}]}
                             {:set state.markers
                              :to ~(sort-by (fn [it] it.frame)
                                            (concat state.markers
                                                    [{:frame state.frame :label (str "F_" (pad state.frame 2 "0"))}]))}]}
            :deleteMarker {:title "Delete Marker"
                           :category "Marker"
                           :enablement ~(some (fn [it] (= it.frame state.frame)) state.markers)
                           :do [{:dismiss true}
                                {:set state.markers :to ~(filter (fn [it] (not= it.frame state.frame)) state.markers)}]}
            :prevMarker {:title "Jump to Previous Marker"
                         :category "Marker"
                         :enablement ~(some (fn [it] (< it.frame state.frame)) state.markers)
                         :do [{:dismiss true}
                              {:if ~(some (fn [it] (< it.frame state.frame)) state.markers)
                               :then [{:run setFrame
                                       :with {:args {:value ~(max (map (fn [it] it.frame)
                                                                       (filter (fn [it] (< it.frame state.frame))
                                                                               state.markers)))}}}]}]}
            :nextMarker {:title "Jump to Next Marker"
                         :category "Marker"
                         :enablement ~(some (fn [it] (> it.frame state.frame)) state.markers)
                         :do [{:dismiss true}
                              {:if ~(some (fn [it] (> it.frame state.frame)) state.markers)
                               :then [{:run setFrame
                                       :with {:args {:value ~(min (map (fn [it] it.frame)
                                                                       (filter (fn [it] (> it.frame state.frame))
                                                                               state.markers)))}}}]}]}
            :frameCurrent {:title "Go to Current Frame"
                           :category "View"
                           :do [{:dismiss true}
                                {:set state.timelineOffset :to ~(/ (round (* (- (/ (- state.frame 1) 24) 4) 1000)) 1000)}]}
            :clearLocation {:title "Clear Location"
                            :category "Object"
                            :enablement ~(> (count state.view.selection) 0)
                            :do [{:run clearTransform :with {:args {:key translation :value 0 :title "Clear Location"}}}]}
            :clearRotation {:title "Clear Rotation"
                            :category "Object"
                            :enablement ~(> (count state.view.selection) 0)
                            :do [{:run clearTransform :with {:args {:key rotationDegrees :value 0 :title "Clear Rotation"}}}]}
            :clearScale {:title "Clear Scale"
                         :category "Object"
                         :enablement ~(> (count state.view.selection) 0)
                         :do [{:run clearTransform :with {:args {:key scale :value 1 :title "Clear Scale"}}}]}
            :snapMenu {:title "Snap"
                       :category "Object"
                       :do [{:if ~(nil? state.modal) :then [{:run openPopup :with {:args {:menu snap :x nil :y nil}}}]}]}
            :cursorToSelected {:title "Cursor to Selected"
                               :category "Object"
                               :enablement ~(> (count state.view.selection) 0)
                               :do [{:dismiss true}
                                    {:set state.tmp
                                     :to ~(filter (fn [it] (contains state.view.selection it.id)) state.scene.nodes)}
                                    {:if ~(= (count state.tmp) 0) :then [{:stop true}]}
                                    {:set state.cursor
                                     :to ~[(/ (round (* (/ (sum state.tmp (fn [it] it.translation.0)) (count state.tmp)) 10000))
                                              10000)
                                           (/ (round (* (/ (sum state.tmp (fn [it] it.translation.1)) (count state.tmp)) 10000))
                                              10000)
                                           (/ (round (* (/ (sum state.tmp (fn [it] it.translation.2)) (count state.tmp)) 10000))
                                              10000)]}]}
            :cursorToActive {:title "Cursor to Active"
                             :category "Object"
                             :enablement ~(some? computed.activeNode)
                             :do [{:dismiss true}
                                  {:if ~(some? computed.activeNode)
                                   :then [{:set state.cursor :to ~computed.activeNode.translation}]}]}
            :cursorToOrigin {:title "Cursor to World Origin"
                             :category "Object"
                             :do [{:dismiss true} {:set state.cursor :to [0 0 0]}]}
            :toggleMaximize {:title "Toggle Maximize Area"
                             :category "View"
                             :do [{:dismiss true}
                                  {:set state.maximized :to ~(if (some? state.maximized) nil (or-else state.hoverSlot "a"))}]}
            :copyObjects {:title "Copy Objects"
                          :category "Edit"
                          :icon copy
                          :enablement ~(> (count state.view.selection) 0)
                          :do [{:if ~(= (count state.view.selection) 0) :then [{:stop true}]}
                               {:set state.copied :to ~computed.selectedNodes}
                               {:set state.last
                                :to ~(str "Copied " (plural (count state.copied) "selected object" "selected objects"))}]}
            :pasteObjects {:title "Paste Objects"
                           :category "Edit"
                           :icon clipboard-paste
                           :enablement ~(> (count state.copied) 0)
                           :do [{:dismiss true}
                                {:if ~(= (count state.copied) 0) :then [{:stop true}]}
                                {:run snapshot}
                                {:set state.dupes :to []}
                                {:forEach ~state.copied
                                 :as m
                                 :do [{:increment state.added :by 1}
                                      {:run uniqueName :with {:args {:name ~(get (split m.name ".") 0)}}}
                                      {:push state.scene.nodes
                                       :value ~(merge m
                                                      {:id (str (get (split m.id "-") 0) "-" state.added) :name state.tmp.name})}
                                      {:push state.dupes :value ~(str (get (split m.id "-") 0) "-" state.added)}]}
                                {:set state.view.selection :to ~state.dupes}
                                {:set state.active :to ~(get state.dupes (- (count state.dupes) 1))}
                                {:set state.last :to "Paste Objects"}]}
            :renderImage {:title "Render Image"
                          :category "Render"
                          :icon image
                          :enablement ~(some? computed.cameraNode)
                          :do [{:dismiss true}
                               {:if ~(and (some? computed.cameraNode) (nil? state.modal)) :then [{:set state.rendering :to true}]}]}
            :nextWorkspace {:title "Next Workspace"
                            :category "Window"
                            :icon arrow-right
                            :do [{:set state.tmp :to ~(filter (fn [it] (nil? it.needs)) computed.workspaces)}
                                 {:run setWorkspace
                                  :with {:args ~(get state.tmp
                                                     (mod (+ (indexOf (map (fn [it] it.title) state.tmp) state.workspace) 1)
                                                          (count state.tmp)))}}]}
            :previousWorkspace {:title "Previous Workspace"
                                :category "Window"
                                :icon arrow-left
                                :do [{:set state.tmp :to ~(filter (fn [it] (nil? it.needs)) computed.workspaces)}
                                     {:run setWorkspace
                                      :with {:args ~(get state.tmp
                                                         (mod (- (+ (indexOf (map (fn [it] it.title) state.tmp)
                                                                             state.workspace)
                                                                    (count state.tmp))
                                                                 1)
                                                              (count state.tmp)))}}]}
            :saveRender {:title "Save Render As…"
                         :category "Render"
                         :icon save
                         :enablement ~state.rendering
                         :do [{:export png
                               :pages [render-image]
                               :title "Save the render"
                               :name render.png
                               :onDone [{:set state.fileStatus :to ~(str "Saved " result.name)}]
                               :onError [{:set state.fileStatus :to ~(str "Not saved: " error.message)}]}]}
            :about {:title "About Modello" :category "Help" :icon info :do [{:dismiss true} {:set state.about :to true}]}
            :newScene {:title "New"
                       :category "File"
                       :icon file-plus
                       :do [{:dismiss true} {:run snapshot} {:set state.scene.nodes :to ~state.startScene.nodes}]}
            :saveScene {:title "Save"
                        :category "File"
                        :icon save
                        :do [{:dismiss true}
                             {:saveFile ~{:format "modello-scene" :version 1 :scene state.scene :added state.added}
                              :name scene.json
                              :title "Save the scene"
                              :onDone [{:set state.fileStatus :to ~(str "Saved " result.name)}]
                              :onError [{:set state.fileStatus :to ~(str "Not saved: " error.message)}]}]}
            :exportGltf {:title "Export glTF…"
                         :category "File"
                         :icon file-output
                         :enablement ~(contains (vals state.areas) "viewport")
                         :do [{:dismiss true}
                              {:if ~(not (contains (vals state.areas) "viewport")) :then [{:stop true}]}
                              {:export gltf
                               :pages [viewport]
                               :animations ~computed.gltfClips
                               :name scene
                               :onDone [{:set state.fileStatus :to ~(str "Exported " result.name)}]
                               :onError [{:set state.fileStatus :to ~(str "Not exported: " error.message)}]}]}
            :openScene {:title "Open…"
                        :category "File"
                        :icon folder-open
                        :do [{:dismiss true}
                             {:openFile state.opened
                              :extensions [json]
                              :title "Open a scene"
                              :onDone [{:set state.openedName :to ~result.name} {:run sceneOpened}]
                              :onError [{:set state.fileStatus :to ~(str "Not opened: " error.message)}]}]}
            :sceneOpened [{:if ~computed.openedIsScene
                           :then [{:if ~(not= state.mode "object") :then [{:set state.mode :to object}]}
                                  {:run snapshot}
                                  {:set state.scene :to ~state.opened.scene}
                                  {:set state.added :to ~(max state.added state.opened.added)}
                                  {:set state.view.selection :to []}
                                  {:set state.active :to nil}
                                  {:set state.fileStatus :to ~(str "Opened " state.openedName)}]
                           :else [{:set state.fileStatus
                                   :to ~(str "Not opened: " (or-else state.openedName "the file") " is not a Modello scene")}]}
                          {:set state.opened :to nil}
                          {:set state.openedName :to nil}]})

