;;; collections.lisp --- the collections the outliner sorts objects into: new, renamed, hidden, deleted, moved to
;;;
;;; A scene without its own list of collections has the one, "Collection" (computed.collections); the first change
;;; writes the list into the scene.

(ns modello.collections)

;;; Collections

(defn own-collections "Writes the collections into the scene, the first time one changes." []
  (when (nil? state.scene.collections) (set "state.scene.collections" computed.collections)))

(defn toggle-collection "A collection's eye, arrow or camera in the outliner: hides its objects, or keeps them from being picked or rendered."
  [id key]
  (own-collections)
  (let [i (indexOf (ids state.scene.collections) id)]
    (when (>= i 0)
      (let [now (false? (get-in state.scene.collections [i key]))]
        (set (str "state.scene.collections[" i "]." key) now)
        (when (and (not= key :render) (not now))
          (set "state.view.selection"
               (ids (filter #(and (selected? %) (not= (or (:collection %) "collection") id)) state.scene.nodes))))))))

(defn collection-name "A name no collection has: Collection 1, Collection 2..." []
  (loop [n 1]
    (let [name (t "collection.numbered" {:n n})]
      (if (some #(= (:name %) name) state.scene.collections) (recur (inc n)) name))))

(defcommand newCollection "Adds a collection, and moves the selected objects into it when move is true."
  :title ~(t "command.newCollection") :description ~(t "command.newCollection.about")
  :category ~(t "category.collection") :icon "folder-plus" [move]
  (dismiss)
  (snapshot)
  (own-collections)
  (increment "state.added" :by 1)
  (let [id (str "collection-" state.added)]
    (push "state.scene.collections" {:id id :name (collection-name)})
    (set "state.outlinerOpen" (assoc state.outlinerOpen id true))
    (if (true? move)
      (moveToCollection id false)
      (set "state.activeCollection" id))))

(defn rename-collection "Names a collection, if the name isn't empty." [id name]
  (let [trimmed (str/trim name)]
    (when (and (not (str/blank? trimmed)) (contains computed.collectionIds id))
      (snapshot)
      (own-collections)
      (set (str "state.scene.collections[" (indexOf computed.collectionIds id) "].name") (slice trimmed 0 63)))))

(defcommand startCollectionRename "Names the active collection in the outliner."
  :title ~(t "command.startCollectionRename") :description ~(t "command.startCollectionRename.about")
  :category ~(t "category.collection") :icon "pencil" :enablement ~(contains (vals state.areas) "outliner") []
  (dismiss)
  (when (contains (vals state.areas) "outliner")
    (set "state.outlinerFilter" "")
    (set "state.outlinerOpen.root" true)
    (set "state.renaming" (str "coll:" computed.activeCollectionData.id))
    (after 50 focus-rename)))

(defcommand deleteCollection "Deletes the active collection; its objects go to the first other one."
  :title ~(t "command.deleteCollection") :description ~(t "command.deleteCollection.about")
  :category ~(t "category.collection") :icon "trash-2" :enablement ~(> (count computed.collections) 1) []
  (dismiss)
  (when (>= (count computed.collections) 2)
    (snapshot)
    (let [gone computed.activeCollectionData.id
          heir (:id (find #(not= (:id %) gone) computed.collections))]
      (change-nodes #(= (or (:collection %) "collection") gone) #(assoc % :collection heir))
      (set "state.scene.collections" (remove #(= (:id %) gone) computed.collections))
      (set "state.activeCollection" heir)
      (set "state.last" (t "command.deleteCollection")))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn moveToCollection "Moves the selected objects into a collection, with an undo step when undoable is true."
  [id undoable]
  (dismiss)
  (when (seq state.view.selection)
    (when (true? undoable) (snapshot))
    (change-nodes selected? #(assoc % :collection id))
    (set "state.outlinerOpen" (assoc state.outlinerOpen id true))
    (set "state.last" (t "command.moveToCollectionMenu"))))

(defn collection-choices "The Move to Collection menu: each collection, then a new one." []
  (concat (map (fn [{:keys [id name]}] {:title name :icon "package" :run "moveToCollection" :with {:id id :undoable true}})
               computed.collections)
          [{:separator true}
           {:title (t "command.newCollection") :icon "plus" :run "newCollection" :with {:move true}}]))

(defcommand moveToCollectionMenu "Opens a menu of the collections to move the selected objects into."
  :title ~(t "command.moveToCollectionMenu") :description ~(t "command.moveToCollectionMenu.about")
  :category ~(t "category.object") :icon "package" :enablement ~(pos? (count state.view.selection)) []
  (when (and (seq state.view.selection) (nil? state.modal))
    (set "state.ctx" {:open true
                      :menu "moveTo"
                      :x computed.centre.x
                      :y computed.centre.y
                      :items (collection-choices)})))
