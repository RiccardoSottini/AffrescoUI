;;; offers.lisp --- what Orrery offers a feature from another app: its exports and its points
;;;
;;; A feature binds to these (orrery-sorter.adapter.lisp binds Sinopia's slide sorter). An export's value is an
;;; expression: the bodies, the selected body, or an action's name (those three are written as expressions).

{:exports {:sky.bodies (array :kind list
                         :value ~computed.catalogue
                         :description "Every body in the orrery, in order: id, name, the id of the body it orbits (empty for the Sun) and a thumbnail image."
                         :item {:id string :name string :parent string :thumbnail image})
           :sky.selected (string :kind state :value ~state.selected :description "The selected body's id.")
           :bodies.reorder (string :kind action
                             :value "{{'reorderBody'}}"
                             :description "Moves body args.id to the place of body args.onto."
                             :arguments {:args object})
           :bodies.select (string :kind action
                            :value "{{'selectBody'}}"
                            :description "Selects body args.id."
                            :arguments {:args object})
           :bodies.clone (string :kind action
                           :value "{{'cloneBody'}}"
                           :description "Adds a twin of body args.id on the same orbit, half a turn away."
                           :arguments {:args object})}
 :points {:bodies.dock {:slot dock
                        :receives [sky.bodies sky.selected bodies.reorder bodies.select bodies.clone]
                        :description "The strip under the timeline, across the window."}}}
