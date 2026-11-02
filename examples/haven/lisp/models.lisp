{:$schema "../../../schema/affrescoui-screen.schema.json"
 :note "Haven's records: the logbook of what happened, each thing's on and off hours over the day, and the energy used each day of the week. Their first records are in seed/, all invented."
 :models {:logbook {:fields {:day (enum :values [today yesterday])
                             :time (string :maxLength 5 :description "HH:MM.")
                             :icon (string)
                             :name (string :required true)
                             :rule (string :default "" :description "The automation that did it, or empty.")
                             :what (enum :values [on off opened locked playing docked triggered])}}
          :timeline {:fields {:name (string :required true)
                              :k (string :description "The strings its states are named by: <k>.on, <k>.off.")
                              :segs (list :maxItems 48
                                      :description "On or off, for h hours each, from midnight."
                                      :of (object :fields {:on (boolean) :h (number :minimum 0 :maximum 24)}))}}
          :energy {:fields {:day (enum :values ["Mon" "Tue" "Wed" "Thu" "Fri" "Sat" "Sun"]) :kwh (number :minimum 0)}}}}
