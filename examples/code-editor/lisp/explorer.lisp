;;; explorer.lisp --- the side bar: the project's files as a tree

;;; Views

(defview explorer "The side bar: the open files, then the project's folders and files." []
  `(node#explorer :when ~(and state.sidebarOpen (not state.zen)) :color @sidebar
     (node#label-openFiles :style caption :text ~(t "side.openFiles") :anchors {:left 14 :top 10})
     (column#openFiles :anchors {:left 0 :right 0 :top 30} :height ~(* 24 (count state.tabs))
       ~(repeat [tab state.tabs :key tab]
         `(label#openFile
            :text ~(str (if (get-in state.files [tab :dirty]) "●  " "") (get-in state.files [tab :name]))
            :font small
            :height 24
            :anchors {:left 8 :right 8}
            :padding 14
            :radius 5
            :align {:horizontal start :vertical center}
            :color ~(if (= tab state.active) "@rowSelected" "transparent")
            :hover {:color @hover}
            :textColor ~(if (= tab state.active) "@text" "@muted")
            :cursor hand
            :onClick ~(handler showFile tab))))
     (node#label-folders :style caption :text ~(t "bar.project")
       :anchors {:left 14 :top ~(+ 44 (* 24 (count state.tabs)))})
     ~(file-tree)))

(defview file-tree "The project's folders and files as a tree: a click opens a file." []
  `(tree#fileTree :anchors {:left 0 :right 0 :top ~(+ 64 (* 24 (count state.tabs))) :bottom 8}
     :rowHeight 24
     :font small
     :color @sidebar
     :items ~computed.tree
     :expanded ~state.expanded
     :value ~(str "f" state.active)
     :textColor @muted
     :selectedTextColor @text
     :selectedBackground @rowSelected
     :accentColor @accent
     :guides false
     :onChange pickTreeRow
     :onExpand ~(handler expandFolder event.id event.expanded)))
