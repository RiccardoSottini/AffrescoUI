;;; files.lisp --- what reaches outside Modello: a new scene, opening and saving one, exporting glTF, rendering
;;;
;;; A saved scene is `{format: "modello-scene", version: 1, scene, added}`; one opened is checked whole
;;; (computed.openedIsScene) before it replaces the scene. What happened shows in the status bar (state.fileStatus).

;;; What happened

(defn saved "A file saved: its name in the status bar." [result]
  (set "state.fileStatus" (t "file.saved" {:name (:name result)})))

(defn notSaved "A file not saved: why, in the status bar." [error]
  (set "state.fileStatus" (t "file.notSaved" {:reason (:message error)})))

(defn exported "A scene exported: its name in the status bar." [result]
  (set "state.fileStatus" (t "file.exported" {:name (:name result)})))

(defn notExported "A scene not exported: why, in the status bar." [error]
  (set "state.fileStatus" (t "file.notExported" {:reason (:message error)})))

(defn notOpened "A file not opened: why, in the status bar." [error]
  (set "state.fileStatus" (t "file.notOpened" {:reason (:message error)})))

;;; Scenes

(defcommand newScene "Starts again from the scene Modello opens with."
  :title ~(t "command.newScene") :description ~(t "command.newScene.about")
  :category ~(t "category.file") :icon "file-plus" []
  (dismiss)
  (snapshot)
  (set "state.scene.nodes" state.startScene.nodes))

(defcommand saveScene "Saves the scene to a file."
  :title ~(t "command.saveScene") :description ~(t "command.saveScene.about")
  :category ~(t "category.file") :icon "save" []
  (dismiss)
  (saveFile {:format "modello-scene" :version 1 :scene state.scene :added state.added}
            :name "scene.json" :title (t "file.saveTitle") :onDone saved :onError notSaved))

(defn sceneOpened "A file opened: it replaces the scene if it is a Modello scene, and says so either way." []
  (if computed.openedIsScene
    (do (when (not= state.mode "object") (set "state.mode" "object"))
        (snapshot)
        (set "state.scene" state.opened.scene)
        (set "state.added" (max state.added state.opened.added))
        (set "state.view.selection" [])
        (set "state.active" nil)
        (set "state.fileStatus" (t "file.opened" {:name state.openedName})))
    (set "state.fileStatus" (t "file.notAScene" {:name (or state.openedName (t "file.theFile"))})))
  (set "state.opened" nil)
  (set "state.openedName" nil))

(defn opened "The file chosen to open, read into state.opened." [result]
  (set "state.openedName" (:name result))
  (sceneOpened))

(defcommand openScene "Opens a scene from a file."
  :title ~(t "command.openScene") :description ~(t "command.openScene.about")
  :category ~(t "category.file") :icon "folder-open" []
  (dismiss)
  (openFile "state.opened" :extensions ["json"] :title (t "file.openTitle") :onDone opened :onError notOpened))

(defcommand exportGltf "Exports the viewport's scene, with its animation, as glTF."
  :title ~(t "command.exportGltf") :description ~(t "command.exportGltf.about")
  :category ~(t "category.file") :icon "file-output" :enablement ~(contains (vals state.areas) "viewport") []
  (dismiss)
  (when (contains (vals state.areas) "viewport")
    (export "gltf"
      :pages ["viewport"]
      :animations computed.gltfClips
      :name "scene"
      :onDone exported
      :onError notExported)))

;;; Rendering

(defcommand renderImage "Renders the scene through its camera, in a window of its own."
  :title ~(t "command.renderImage") :description ~(t "command.renderImage.about")
  :category ~(t "category.render") :icon "image" :enablement ~(some? computed.cameraNode) []
  (dismiss)
  (when (and (some? computed.cameraNode) (nil? state.modal)) (set "state.rendering" true)))

(defcommand saveRender "Saves the render as a PNG image."
  :title ~(t "command.saveRender") :description ~(t "command.saveRender.about")
  :category ~(t "category.render") :icon "save" :enablement ~state.rendering []
  (export "png"
    :pages ["render-image"]
    :title (t "file.renderTitle")
    :name "render.png"
    :onDone saved
    :onError notSaved))
