;;; offers.lisp --- what Tempera offers a feature from another app: its exports and its points
;;;
;;; A feature binds to these: Sinopia's slide sorter, docked under the Layers group (sorter/sorter.adapter.lisp), shows
;;; the layers and moves, chooses and duplicates them. An export's value is an expression: the layers, the active one,
;;; or an action's name (those three written as expressions, which no form gives).

{:exports {:doc.layers (array :kind list
                         :value ~state.doc.layers
                         :description "The layers, top first: id, name, depth (0 at the top level), kind and a thumbnail image."
                         :item {:id string :name string :depth integer :kind string :thumbnail image})
           :doc.active (string :kind state :value ~state.doc.active :description "The active layer's id.")
           :layers.move (string :kind action
                          :value "{{'moveLayer'}}"
                          :description "Moves a layer: args.id takes the place of args.onto."
                          :arguments {:args object})
           :layers.select (string :kind action
                            :value "{{'selectLayer'}}"
                            :description "Makes args.id the active layer."
                            :arguments {:args object})
           :layers.duplicate (string :kind action
                               :value "{{'duplicateLayer'}}"
                               :description "Duplicates layer args.id."
                               :arguments {:args object})}
 :points {:panels.dock {:slot dock
                        :receives [doc.layers doc.active layers.move layers.select layers.duplicate]
                        :description "The foot of the panels column, under the Layers group."}}}
