;;; collections.lisp --- the collections the outliner sorts objects into: new, renamed, hidden, deleted, moved to
;;;
;;; A scene without its own list of collections has the one, "Collection" (computed.collections); the first change
;;; writes the list into the scene.

;;; Collections

(defn own-collections "Writes the collections into the scene, the first time one changes." []
  (when (nil? state.scene.collections) (set "state.scene.collections" computed.collections)))

(defn toggleCollection "A collection's eye, arrow or camera in the outliner: hides its objects, or keeps them from being picked or rendered."
  [id key]
  (own-collections)
  (let [i (indexOf (ids state.scene.collections) id)]
    (when (>= i 0)
      (let [now (= (get (get state.scene.collections i) key) false)]
        (set (str "state.scene.collections[" i "]." key) now)
        (when (and (not= key "render") (= now false))
          (set "state.view.selection"
               (ids (filter (fn [n] (and (selected? n) (not= (or (:collection n) "collection") id)))
                            state.scene.nodes))))))))

(defn collection-name "A name no collection has: Collection 1, Collection 2..." []
  (loop [n 1]
    (let [name (t "collection.numbered" {:n n})]
      (if (some (fn [c] (= (:name c) name)) state.scene.collections) (recur (inc n)) name))))

(defcommand newCollection "Adds a collection, and moves the selected objects into it when move is true."
  :title ~(t "command.newCollection") :category ~(t "category.collection") :icon "folder-plus"
  [move]
  (dismiss)
  (snapshot)
  (own-collections)
  (increment "state.added" :by 1)
  (let [id (str "collection-" state.added)]
    (push "state.scene.collections" {:id id :name (collection-name)})
    (set "state.outlinerOpen" (assoc state.outlinerOpen id true))
    (if (= move true)
      (moveToCollection id false)
      (set "state.activeCollection" id))))

(defn renameCollection "Names a collection, if the name isn't empty." [id name]
  (when (and (not= (trim name) "") (contains computed.collectionIds id))
    (snapshot)
    (own-collections)
    (set (str "state.scene.collections[" (indexOf computed.collectionIds id) "].name") (slice (trim name) 0 63))))

(defcommand startCollectionRename "Names the active collection in the outliner."
  :title ~(t "command.startCollectionRename") :category ~(t "category.collection") :icon "pencil"
  :enablement ~(contains (vals state.areas) "outliner")
  []
  (dismiss)
  (when (contains (vals state.areas) "outliner")
    (set "state.outlinerFilter" "")
    (set "state.outlinerOpen.root" true)
    (set "state.renaming" (str "coll:" computed.activeCollectionData.id))
    (after 50 focusRename)))

(defcommand deleteCollection "Deletes the active collection; its objects go to the first other one."
  :title ~(t "command.deleteCollection") :category ~(t "category.collection") :icon "trash-2"
  :enablement ~(> (count computed.collections) 1)
  []
  (dismiss)
  (when (>= (count computed.collections) 2)
    (snapshot)
    (let [gone computed.activeCollectionData.id
          heir (:id (find (fn [c] (not= (:id c) gone)) computed.collections))]
      (change-nodes (fn [n] (= (or (:collection n) "collection") gone)) (fn [n] (merge n {:collection heir})))
      (set "state.scene.collections" (remove (fn [c] (= (:id c) gone)) computed.collections))
      (set "state.activeCollection" heir)
      (set "state.last" (t "command.deleteCollection")))))

(defn moveToCollection "Moves the selected objects into a collection, with an undo step when undoable is true."
  [id undoable]
  (dismiss)
  (when (not (empty? state.view.selection))
    (when (= undoable true) (snapshot))
    (change-nodes selected? (fn [n] (merge n {:collection id})))
    (set "state.outlinerOpen" (assoc state.outlinerOpen id true))
    (set "state.last" (t "command.moveToCollectionMenu"))))

(defn collection-choices "The Move to Collection menu: each collection, then a new one." []
  (concat (map (fn [c] {:title (:name c) :icon "package" :run "moveToCollection" :with {:id (:id c) :undoable true}})
               computed.collections)
          [{:separator true}
           {:title (t "command.newCollection") :icon "plus" :run "newCollection" :with {:move true}}]))

(defcommand moveToCollectionMenu "Opens a menu of the collections to move the selected objects into."
  :title ~(t "command.moveToCollectionMenu") :category ~(t "category.object") :icon "package"
  :enablement ~(> (count state.view.selection) 0)
  []
  (when (and (not (empty? state.view.selection)) (nil? state.modal))
    (set "state.ctx" {:open true
                      :menu "moveTo"
                      :x computed.centre.x
                      :y computed.centre.y
                      :items (collection-choices)})))
