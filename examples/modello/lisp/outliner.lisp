;;; outliner.lisp --- TODO

(defcomputed objectRows (sort-by (fn [it] it.name) state.scene.nodes))

(defcomputed outlinerItems
  (cond (= state.outlinerMode "list") (map-indexed (fn [index it] {:node it :k index}) computed.outlinerRows)
          (not= state.outlinerOpen.root true) []
          :else (sort-by (fn [it] it.k)
                   (concat (map-indexed (fn [index it] {:coll it :k (- (* index 100000) 1)}) computed.collections)
                           (map-indexed (fn [index it]
                                          {:node it
                                           :k (+ (* (indexOf computed.collectionIds
                                                             (or-else it.collection "collection"))
                                                    100000)
                                                 index)})
                                        (filter (fn [it]
                                                  (= (get state.outlinerOpen (or-else it.collection "collection"))
                                                     true))
                                                computed.outlinerRows))))))

(defcomputed outlinerRows
  (filter (fn [it]
              (and (or (nil? state.outlinerFilter)
                       (= state.outlinerFilter "")
                       (contains (lower-case it.name) (lower-case state.outlinerFilter)))
                   (not (contains state.outlinerHide it.kind))
                   (or (not state.outlinerSelectedOnly) (contains state.view.selection it.id))))
            computed.objectRows))

(defcomputed outlinerDepth (if (= state.outlinerMode "list") 0 2))

(defcomponent outlinerRow "The outlinerRow component."
  {:depth 0
   :name ""
   :icon ""
   :iconColor @text
   :data ""
   :node nil
   :coll nil
   :stripe false
   :key ""
   :expand ""
   :onSelect nil}
  `(node :id ~(str "outliner-" (if node node.id key))
     :width "100%"
     :height 20
     :color ~(cond (and node (= node.id state.active) (contains state.view.selection node.id)) "@sel"
                   (or (and node (contains state.view.selection node.id))
                       (and coll (= coll.id computed.activeCollectionData.id))) "@selRow"
                   stripe "@rowAlt"
                   :else "transparent")
     :hover {:color ~(if (and node (contains state.view.selection node.id)) "@sel" "@widget")}
     :role treeitem
     :focusable true
     :label ~name
     :selected ~(if node (contains state.view.selection node.id) nil)
     :let {:open ~(= (get state.outlinerOpen expand) true)
           :editing ~(or (and (some? node) (not= data "") (= state.renaming node.id))
                         (and (some? coll) (= state.renaming (str "coll:" coll.id))))}
     :onClick [{:if ~(some? node) :then [{:run selectNode :with {:id ~node.id :shift ~event.shift}}]}
               {:if ~(some? coll) :then [{:set state.activeCollection :to ~coll.id}]}
               {:if ~(some? onSelect) :then [{:set state.propTab :to ~onSelect}]}]
     :onDoubleClick [{:if ~(and (some? node) (not= data ""))
                      :then [{:set state.renaming :to ~node.id} {:focus "#outliner-rename"}]}
                     {:if ~(some? coll)
                      :then [{:set state.renaming :to ~(str "coll:" coll.id)} {:focus "#outliner-rename"}]}]
     :onContextMenu [{:if ~(some? coll)
                      :then [{:set state.activeCollection :to ~coll.id}
                             {:run openPopup :with {:menu collectionContext :x ~event.x :y ~event.y}}]}
                     {:if ~(some? node)
                      :then [{:if ~(not (contains state.view.selection node.id))
                              :then [{:run selectNode :with {:id ~node.id :shift false}}]}
                             {:run openPopup :with {:menu outlinerContext :x ~event.x :y ~event.y}}]}]
     ~(outliner--chevron depth name icon node key expand)
     (label :font icons
       :icon ~icon
       :text ""
       :textColor ~iconColor
       :x ~(+ (* depth 18) 18)
       :width 18
       :anchors {:top 0 :bottom 0}
       :align center)
     (label :when ~(not editing)
       :text ~name
       :textColor ~(if (and node (= node.visible false)) "@dim" "@text")
       :x ~(+ (* depth 18) 40)
       :anchors {:top 0 :bottom 0}
       :fitText true)
     (textarea#outliner-rename :when ~editing
       :x ~(+ (* depth 18) 38)
       :width 120
       :anchors {:top 1 :bottom 1}
       :color @field
       :radius 3
       :text ~name
       :textColor @text
       :label ~(str "Rename " name)
       :keys {:escape {:set state.renaming :to nil}}
       :onBlur {:set state.renaming :to nil}
       :onSubmit [{:if ~(some? coll)
                   :then [{:run renameCollection :with {:id ~coll.id :name ~event.text}}]
                   :else [{:run rename :with {:id ~node.id :name ~event.text}}]}
                  {:set state.renaming :to nil}])
     (label :when ~(and (not= data "") (not editing))
       :font iconsSmall
       :icon ~data
       :text ""
       :textColor @data
       :x ~(+ (* depth 18) 46 (* (count name) 7))
       :width 16
       :anchors {:top 0 :bottom 0}
       :align center)
     (row :when ~(or (not= data "") (some? coll))
       :anchors {:right 4 :top 0 :bottom 0}
       :width 66
       :gap 4
       :alignItems center
       :color transparent
       ~(repeat [t [{:key "selectable" :on "mouse-pointer-2" :off "mouse-pointer-2" :title "Disable Selection"} {:key "visible" :on "eye" :off "eye-off" :title "Hide in Viewport"} {:key "render" :on "camera" :off "camera-off" :title "Disable in Renders"}]]
         (outliner-view icon node coll key t)))))

(defcomponent outlinerArea "The outlinerArea component."
  {}
  `(node#outliner :color @region :radius 8
     ~(outliner-header)
     (column#outliner-rows :anchors {:left 0 :right 0 :top 28 :bottom 0}
       :gap 0
       :color transparent
       :role tree
       :label "Outliner"
       :arrows vertical
       :scroll vertical
       (node :when ~(not= state.outlinerMode "list")
         :use outlinerRow
         :with {:depth 0 :name "Scene Root" :key root :icon folder-tree :iconColor @text :expand root})
       ~(repeat [item computed.outlinerItems :index i :key (if item.node item.node.id (str "c:" item.coll.id))]
         (outliner-rows-column item i)))))

(defview outliner--chevron "#outliner--chevron." [depth name icon node key expand]
  `(node :id ~(str "outliner-" (if node node.id key) "-chevron")
     :when ~(not= expand "")
     :x ~(+ (* depth 18) 2)
     :width 14
     :anchors {:top 0 :bottom 0}
     :color transparent
     :role button
     :label ~(str (if open "Collapse" "Expand") " " name)
     :isOpen ~open
     :onClick {:toggle "state.outlinerOpen[expand]"}
     (label :font iconsSmall
       :icon ~(if open "chevron-down" "chevron-right")
       :text ""
       :textColor @dim
       :align center
       :fill true)))

(defview outliner-view "#outliner--." [icon node coll key t]
  `(node :id ~(str "outliner-" (if node node.id coll.id) "-" t.key)
     :let {:on ~(if node (not= (get node t.key) false) (not= (get coll t.key) false))}
     :width 18
     :height 18
     :radius 3
     :color transparent
     :hover {:color @widgetHover}
     :role checkbox
     :focusable true
     :label ~t.title
     :tooltip ~t.title
     :selected ~on
     :onClick {:if ~(some? node)
               :then [{:run toggleFlag :with {:id ~node.id :key ~t.key}}]
               :else [{:run toggleCollection :with {:id ~coll.id :key ~t.key}}]}
     (label :font
            iconsSmall
            :icon
            ~(if on t.on t.off)
            :text
            ""
            :textColor
            ~(if on "@text" "@faint")
            :align
            center
            :fill
            true)))

(defview outliner-header "#outliner-header." []
  `(row#outliner-header :anchors {:left 0 :right 0 :top 0}
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
              :tooltip "Editor Type"
              :open state.open.editorOutliner
              :items ~(map (fn [it]
                             (merge it
                                    {:checked (= it.id "outliner")
                                     :run "swapEditor"
                                     :with {:from "outliner" :to it.id}}))
                           computed.editorTypes)})
     (node#outliner-display :use hButton
       :with {:icon ~(if (= state.outlinerMode "list") "list" "layers-2")
              :chevron true
              :width 38
              :tooltip "Display Mode"
              :open state.open.outlinerDisplay
              :items ~computed.outlinerDisplayItems})
     (node#outliner-search :use searchField :with {:bind state.outlinerFilter :label "Filter the outliner"})
     (node#outliner-filter :use hButton
       :with {:icon filter
              :chevron true
              :width 38
              :tooltip "Filter"
              :on ~(or (> (count state.outlinerHide) 0) state.outlinerSelectedOnly)
              :open state.open.outlinerFilter
              :items ~computed.outlinerFilterItems})
     (node#outliner-new :use hButton
       :with {:icon folder-plus :width 24 :tooltip "New Collection" :run newCollection :with {:move false}})
     (node :use gap)))

(defview outliner-rows-column "#outliner-rows-column." [item i]
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
     (node :when ~(and (some? item.node) (= (get state.outlinerOpen n.id) true))
       :use outlinerRow
       :with {:depth ~(+ computed.outlinerDepth 1)
              :key ~(str n.id "-data")
              :name ~(+ (upper-case (slice (or-else n.mesh "") 0 1)) (slice (or-else n.mesh "") 1))
              :icon triangle
              :iconColor @data
              :onSelect data})
     (node :when ~(and (= (get state.outlinerOpen n.id) true) (some? n.material))
       :use outlinerRow
       :with {:depth ~(+ computed.outlinerDepth 1)
              :key ~(str n.id "-material")
              :name ~(or-else (:name (find (fn [it] (= it.id n.material)) state.scene.materials)) n.material)
              :icon circle
              :iconColor @data
              :onSelect material})))

