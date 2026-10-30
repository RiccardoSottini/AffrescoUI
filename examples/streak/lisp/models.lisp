{:$schema "../../../schema/affrescoui-screen.schema.json"
  :note "Streak's records: the habits, and the query that shows them in the person's order. The first ones are in seed/, all invented."
  :models {:habits {:version 1
      :title Habits
      :note "Each habit and its history: entries holds a character a day from \"from\" (y, s or -), or a number a day separated by spaces for a measurable habit; notes holds a note by day."
      :fields {:name {:type string :required true :maxLength 200}
        :question {:type string}
        :color {:type integer :minimum 0 :maximum 31}
        :kind {:type enum :values [yesNo measurable] :required true}
        :frequency {:type object :fields {:times {:type integer :minimum 1} :days {:type integer :minimum 1}}}
        :unit {:type string}
        :target {:type number}
        :targetType {:type enum :values [atLeast atMost]}
        :reminder {:type object :fields {:time {:type string} :days {:type list :of {:type boolean} :maxItems 7}}}
        :description {:type string}
        :archived {:type boolean :default false}
        :from {:type date :required true}
        :entries {:type string :default ""}
        :notes {:type map :of {:type string} :maxItems 10000}
        :position {:type number :default 0}}
      :importFrom {:path state.habits
        :with "{{merge(it, {position: index})}}"
        :note "Streak kept its habits in its state file before the data layer: they come in once."}}}
  :queries {:ordered {:from habits :sort [position] :note "The habits in their manual order"}}}
