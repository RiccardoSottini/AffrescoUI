;;; scenes.lisp --- several scenes in one file: the top bar's picker switches between them, adds one and deletes one
;;;
;;; The scene shown is state.scene, as everywhere; the others wait in state.sceneStore, each under its id, until the
;;; picker brings one back. Switching leaves Edit Mode, clears the selection and starts Undo afresh for that scene.

(ns modello.scenes)

(defstate scenes [{:id scene :name "Scene"}] "The scenes, in the picker's order: each one's id and name.")

(defstate activeScene scene "The id of the scene shown.")

(defstate sceneStore [] "The scenes not shown: each {id, scene}.")

(defcomputed sceneItems "The scene picker's menu: each scene, the one shown checked, then New Scene and Delete Scene."
  (concat (map #(hash-map :title (:name %) :checked (= (:id %) state.activeScene) :run "switchScene" :with {:id (:id %)})
               state.scenes)
          [{:title (t "scene.new") :icon "plus" :run "addScene" :group "2"}
           {:title (t "scene.delete") :icon "trash-2" :run "deleteScene" :group "2" :enabled (> (count state.scenes) 1)}]))

(defn show-scene "Puts the scene shown away and brings another out of the store, or a given one." [id scene]
  (leave-edit)
  (set "state.sceneStore" (concat (remove #(contains [id state.activeScene] (:id %)) state.sceneStore)
                                  [{:id state.activeScene :scene state.scene}]))
  (set "state.scene" scene)
  (set "state.activeScene" id)
  (set "state.sceneName" (:name (by-id state.scenes id)))
  (set "state.view.selection" [])
  (set "state.active" nil)
  (set "state.undoStack" [])
  (set "state.redoStack" []))

(defcommand switchScene "Shows another scene of the file."
  :title ~(t "command.switchScene") :description ~(t "command.switchScene.about")
  :category ~(t "category.file") :icon "shapes" [id]
  (dismiss)
  (when-some [stored (by-id state.sceneStore id)]
    (show-scene id (:scene stored))))

(defcommand addScene "Adds an empty scene, with its own collection and the materials of this one, and shows it."
  :title ~(t "scene.new") :description ~(t "command.addScene.about")
  :category ~(t "category.file") :icon "plus" []
  (dismiss)
  (increment "state.added" :by 1)
  (let [id (str "scene-" state.added)
        name (unique-name (t "scene.defaultName") (map :name state.scenes))]
    (push "state.scenes" {:id id :name name})
    (show-scene id {:nodes []
                    :materials state.scene.materials
                    :meshes []
                    :textures []
                    :collections [{:id "collection" :name (t "collection.default")}]})))

(defcommand deleteScene "Deletes the scene shown, and shows the one before it."
  :title ~(t "scene.delete") :description ~(t "command.deleteScene.about")
  :category ~(t "category.file") :icon "trash-2" :enablement ~(> (count state.scenes) 1) []
  (dismiss)
  (when (> (count state.scenes) 1)
    (let [gone state.activeScene
          next (:id (first (remove #(= (:id %) gone) state.scenes)))]
      (show-scene next (:scene (by-id state.sceneStore next)))
      (set "state.scenes" (remove #(= (:id %) gone) state.scenes))
      (set "state.sceneStore" (remove #(= (:id %) gone) state.sceneStore)))))
