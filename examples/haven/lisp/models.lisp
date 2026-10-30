{:$schema "../../../schema/affrescoui-screen.schema.json"
  :note "Haven's records: the logbook of what happened, each thing's on and off hours over the day, and the energy used each day of the week. Their first records are in seed/, all invented."
  :models {:logbook {:fields {:day {:type enum :values [today yesterday]}
        :time {:type string :maxLength 5 :description "HH:MM."}
        :icon {:type string}
        :name {:type string :required true}
        :rule {:type string :default "" :description "The automation that did it, or empty."}
        :what {:type enum :values [on off opened locked playing docked triggered]}}}
    :timeline {:fields {:name {:type string :required true}
        :k {:type string :description "The strings its states are named by: <k>.on, <k>.off."}
        :segs {:type list
          :maxItems 48
          :description "On or off, for h hours each, from midnight."
          :of {:type object :fields {:on {:type boolean} :h {:type number :minimum 0 :maximum 24}}}}}}
    :energy {:fields {:day {:type enum :values [Mon Tue Wed Thu Fri Sat Sun]} :kwh {:type number :minimum 0}}}}}
