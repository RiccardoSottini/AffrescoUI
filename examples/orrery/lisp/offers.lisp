;;; offers.lisp --- what Orrery offers a feature from another app: its exports and its points
;;;
;;; A feature binds to these (orrery-sorter.adapter.lisp binds Sinopia's slide sorter to the tour). An export's value
;;; is an expression: the bodies, the saved views, what is selected, or an action's name (written as expressions).

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
                           :arguments {:args object})
           :tour.title (string :kind state :value ~(t "tour.title") :description "What the tour strip is called.")
           :tour.views (array :kind list
                         :value ~computed.tourViews
                         :description "The saved views, the stops of the tour in its order: id, name and the thumbnail of the body each looks at."
                         :item {:id string :name string :image image})
           :tour.current (string :kind state :value ~state.bookmark :description "The view picked last.")
           :tour.go (string :kind action
                      :value "{{'goToView'}}"
                      :description "Goes to view args.id."
                      :arguments {:args object})
           :tour.reorder (string :kind action
                           :value "{{'reorderView'}}"
                           :description "Moves view args.id to the place of view args.onto."
                           :arguments {:args object})
           :tour.copy (string :kind action
                        :value "{{'copyView'}}"
                        :description "Adds a copy of view args.id at the end of the tour."
                        :arguments {:args object})}
 :points {:bodies.dock {:slot dock
                        :receives [sky.bodies
                                   sky.selected
                                   bodies.reorder
                                   bodies.select
                                   bodies.clone
                                   tour.title
                                   tour.views
                                   tour.current
                                   tour.go
                                   tour.reorder
                                   tour.copy]
                        :description "The tour strip under the timeline, across the window: the saved views in order."}}}
