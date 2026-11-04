;;; outliner.lisp --- the outliner: the scene's collections and objects as a tree (or a list), each row's eye, arrow
;;; and camera, renaming in place, and its header (editor type, display mode, search, filter, new collection)

;;; The rows

(defcomputed objectRows "The scene's objects, by name." (sort-by (fn [it] it.name) state.scene.nodes))

(defcomputed outlinerRows "The objects listed: those the search, the filter and Only Selected let through."
  (filter (fn [it]
            (and (or (nil? state.outlinerFilter) (= state.outlinerFilter "") (contains (lower-case it.name) (lower-case state.outlinerFilter)))
                 (not (contains state.outlinerHide it.kind))
                 (or (not state.outlinerSelectedOnly) (contains state.view.selection it.id))))
          computed.objectRows))

;; In the view layer, a collection's k puts it before its objects: each collection has 100000 places.
(defcomputed outlinerItems "The rows under the root: each object in the list mode; else the collections, each followed by its open objects."
  (cond (= state.outlinerMode "list") (map-indexed (fn [index it] {:node it :k index}) computed.outlinerRows)
        (not= state.outlinerOpen.root true) []
        :else (sort-by (fn [it] it.k)
                       (concat (map-indexed (fn [index it] {:coll it :k (- (* index 100000) 1)}) computed.collections)
                               (map-indexed (fn [index it]
                                              {:node it :k (+ (* (indexOf computed.collectionIds (or-else it.collection "collection")) 100000) index)})
                                            (filter (fn [it] (= (get state.outlinerOpen (or-else it.collection "collection")) true))
                                                    computed.outlinerRows))))))

(defcomputed outlinerDepth "How deep an object's row is: under its collection in the view layer, at the edge in the list." (if (= state.outlinerMode "list") 0 2))

(defcomputed outlinerFlags "The switches at a row's end: selectable, visible, rendered."
  [{:key "selectable" :on "mouse-pointer-2" :off "mouse-pointer-2" :title (t "outliner.disableSelection")}
   {:key "visible" :on "eye" :off "eye-off" :title (t "outliner.hide")}
   {:key "render" :on "camera" :off "camera-off" :title (t "outliner.disableRender")}])

;;; The area

(defcomponent outlinerArea "The outliner: its header, the scene's root, and the rows under it." {}
  `(node#outliner :color @region :radius 8
     ~(outliner-header)
     (column#outliner-rows :anchors {:left 0 :right 0 :top 28 :bottom 0}
       :gap 0
       :color transparent
       :role tree
       :label ~(t "editor.outliner")
       :arrows vertical
       :scroll vertical
       (node :when ~(not= state.outlinerMode "list")
         :use outlinerRow
         :with {:depth 0 :name ~(t "outliner.root") :key root :icon folder-tree :iconColor @text :expand root})
       ~(repeat [item computed.outlinerItems :index i :key (if item.node item.node.id (str "c:" item.coll.id))]
          (outliner-item item i)))))

(defview outliner-header "The header: editor type, display mode, the search, the filter, and a new collection." []
  `(row#outliner-header
     :anchors {:left 0 :right 0 :top 0}
     :height 26
     :color @header
     :radius {:topLeft 8 :topRight 8}
     :alignItems center
     :gap 2
     (node :use gap)
     (node#editor-type-outliner :use hButton
       :with {:icon list-tree
              :chevron true
              :width 38
              :tooltip ~(t "header.editorType")
              :open state.open.editorOutliner
              :items ~(editor-choices "outliner")})
     (node#outliner-display :use hButton
       :with {:icon ~(if (= state.outlinerMode "list") "list" "layers-2")
              :chevron true
              :width 38
              :tooltip ~(t "outliner.displayMode")
              :open state.open.outlinerDisplay
              :items ~computed.outlinerDisplayItems})
     (node#outliner-search :use searchField :with {:bind state.outlinerFilter :label ~(t "outliner.search")})
     (node#outliner-filter :use hButton
       :with {:icon filter
              :chevron true
              :width 38
              :tooltip ~(t "outliner.filter")
              :on ~(or (> (count state.outlinerHide) 0) state.outlinerSelectedOnly)
              :open state.open.outlinerFilter
              :items ~computed.outlinerFilterItems})
     (node#outliner-new :use hButton
       :with {:icon folder-plus :width 24 :tooltip ~(t "command.newCollection") :run newCollection :with {:move false}})
     (node :use gap)))

(defview outliner-item
  "An item of the tree: a collection's row, or an object's row and, while it is open, its mesh's and its material's."
  [item i]
  `(column :let {:n ~(or-else item.node {:id ""})}
     :width "100%"
     :height ~(if (and item.node (= (get state.outlinerOpen n.id) true)) (* 20 (if n.material 3 2)) 20)
     :gap 0
     :color transparent
     (node :when ~(some? item.coll)
       :use outlinerRow
       :with {:depth 1
              :name ~item.coll.name
              :key ~item.coll.id
              :coll ~item.coll
              :icon package-open
              :iconColor @text
              :stripe ~(= (mod i 2) 0)
              :expand ~item.coll.id
              :onSelect collection})
     (node :when ~(some? item.node)
       :use outlinerRow
       :with {:depth ~computed.outlinerDepth
              :name ~n.name
              :icon ~(get computed.kindIcons n.kind)
              :iconColor @object
              :data ~(get computed.kindIcons n.kind)
              :node ~n
              :stripe ~(= (mod (if (= state.outlinerMode "list") (+ i 1) i) 2) 0)
              :expand ~(if n.mesh n.id "")})
     ~@(object-data-rows)))

(defview object-data-rows "An open object's mesh and material, a level deeper: two rows." []
  [`(node :when ~(and (some? item.node) (= (get state.outlinerOpen n.id) true))
      :use outlinerRow
      :with {:depth ~(+ computed.outlinerDepth 1)
             :key ~(str n.id "-data")
             :name ~(str (upper-case (slice (or-else n.mesh "") 0 1)) (slice (or-else n.mesh "") 1))
             :icon triangle
             :iconColor @data
             :onSelect data})
   `(node :when ~(and (= (get state.outlinerOpen n.id) true) (some? n.material))
      :use outlinerRow
      :with {:depth ~(+ computed.outlinerDepth 1)
             :key ~(str n.id "-material")
             :name ~(or-else (:name (find (fn [it] (= it.id n.material)) state.scene.materials)) n.material)
             :icon circle
             :iconColor @data
             :onSelect material})])

;;; A row

;; node or coll is what the row shows (an object or a collection); key names a row that is neither; expand is the key
;; of state.outlinerOpen its chevron opens, if it has one; onSelect is the properties' tab a click shows.
(defcomponent outlinerRow "A row of the outliner: a chevron, an icon, the name (or a field renaming it), and its switches."
  {:depth 0 :name "" :icon "" :iconColor @text :data "" :node nil :coll nil :stripe false :key "" :expand "" :onSelect nil}
  `(node :id ~(str "outliner-" (if node node.id key))
     :width "100%"
     :height 20
     :color ~(cond (and node (= node.id state.active) (contains state.view.selection node.id)) "@sel"
                   (or (and node (contains state.view.selection node.id)) (and coll (= coll.id computed.activeCollectionData.id))) "@selRow"
                   stripe "@rowAlt"
                   :else "transparent")
     :hover {:color ~(if (and node (contains state.view.selection node.id)) "@sel" "@widget")}
     :role treeitem
     :focusable true
     :label ~name
     :selected ~(if node (contains state.view.selection node.id) nil)
     :let {:open ~(= (get state.outlinerOpen expand) true)
           :editing ~(or (and (some? node) (not= data "") (= state.renaming node.id)) (and (some? coll) (= state.renaming (str "coll:" coll.id))))}
     :onClick ~(handler pickRow node.id coll.id onSelect event.shift)
     :onDoubleClick ~(handler startRowRename node.id coll.id data)
     :onContextMenu ~(handler rowMenu node.id coll.id)
     ~(outliner-chevron depth name node key expand)
     ~(row-icon depth icon iconColor)
     (label :when ~(not editing)
       :text ~name
       :textColor ~(if (and node (= node.visible false)) "@dim" "@text")
       :x ~(+ (* depth 18) 40)
       :anchors {:top 0 :bottom 0}
       :fitText true)
     ~(rename-field depth name node coll)
     ~(row-data-icon depth name data)
     ~(row-flags node coll data)))

(defview row-icon "A row's icon, in its colour, indented by its depth." [depth icon iconColor]
  `(label
     :font icons
     :icon ~icon
     :text ""
     :textColor ~iconColor
     :x ~(+ (* depth 18) 18)
     :width 18
     :anchors {:top 0 :bottom 0}
     :align center))

(defview row-data-icon "The icon of what an object holds (a mesh, a light...), after its name." [depth name data]
  `(label :when ~(and (not= data "") (not editing))
     :font iconsSmall
     :icon ~data
     :text ""
     :textColor @data
     :x ~(+ (* depth 18) 46 (* (count name) 7))
     :width 16
     :anchors {:top 0 :bottom 0}
     :align center))

(defview row-flags "An object's or a collection's switches, on the right of its row." [node coll data]
  `(row
     :when ~(or (not= data "") (some? coll))
     :anchors {:right 4 :top 0 :bottom 0}
     :width 66
     :gap 4
     :alignItems center
     :color transparent
     ~(repeat [flag computed.outlinerFlags] (row-flag node coll flag))))

(defview outliner-chevron "A row's chevron: it opens or closes what is under the row." [depth name node key expand]
  `(node :id ~(str "outliner-" (if node node.id key) "-chevron")
     :when ~(not= expand "")
     :x ~(+ (* depth 18) 2)
     :width 14
     :anchors {:top 0 :bottom 0}
     :color transparent
     :role button
     :label ~(if open (t "outliner.collapse" {:name name}) (t "outliner.expand" {:name name}))
     :isOpen ~open
     :onClick ~(handler toggleOpen expand)
     (label
       :font iconsSmall
       :icon ~(if open "chevron-down" "chevron-right")
       :text ""
       :textColor @dim
       :align center
       :fill true)))

(defview rename-field "The field that renames the row's object or collection, while it is being renamed." [depth name node coll]
  `(textarea#outliner-rename :when ~editing
     :x ~(+ (* depth 18) 38)
     :width 120
     :anchors {:top 1 :bottom 1}
     :color @field
     :radius 3
     :text ~name
     :textColor @text
     :label ~(t "outliner.rename" {:name name})
     :keys {:escape {:set state.renaming :to nil}}
     :onBlur {:set state.renaming :to nil}
     :onSubmit ~(handler submitRowRename node.id coll.id event.text)))

(defview row-flag "One of a row's switches: whether its object or collection is selectable, visible or rendered." [node coll flag]
  `(node :id ~(str "outliner-" (if node node.id coll.id) "-" flag.key)
     :let {:on ~(if node (not= (get node flag.key) false) (not= (get coll flag.key) false))}
     :width 18
     :height 18
     :radius 3
     :color transparent
     :hover {:color @widgetHover}
     :role checkbox
     :focusable true
     :label ~flag.title
     :tooltip ~flag.title
     :selected ~on
     :onClick ~(handler toggleRowFlag node.id coll.id flag.key)
     (label
       :font iconsSmall
       :icon ~(if on flag.on flag.off)
       :text ""
       :textColor ~(if on "@text" "@faint")
       :align center
       :fill true)))
