{:$schema "../../../schema/affrescoui-screen.schema.json"
 :note "Streak's records: the habits, and the query that shows them in the person's order. The first ones are in seed/, all invented."
 :models {:habits {:version 1
                   :title "Habits"
                   :note "Each habit and its history: entries holds a character a day from \"from\" (y, s or -), or a number a day separated by spaces for a measurable habit; notes holds a note by day."
                   :fields {:name (string :required true :maxLength 200)
                            :question (string)
                            :color (integer :minimum 0 :maximum 31)
                            :kind (enum :values [yesNo measurable] :required true)
                            :frequency (object :fields {:times (integer :minimum 1) :days (integer :minimum 1)})
                            :unit (string)
                            :target (number)
                            :targetType (enum :values [atLeast atMost])
                            :reminder (object :fields {:time (string) :days (list :of (boolean) :maxItems 7)})
                            :description (string)
                            :archived (boolean :default false)
                            :from (date :required true)
                            :entries (string :default "")
                            :notes (node :type map :of (string) :maxItems 10000)
                            :position (number :default 0)}
                   :importFrom {:path state.habits
                                :with "{{merge(it, {position: index})}}"
                                :note "Streak kept its habits in its state file before the data layer: they come in once."}}}
 :queries {:ordered {:from habits :sort [position] :note "The habits in their manual order"}}}
