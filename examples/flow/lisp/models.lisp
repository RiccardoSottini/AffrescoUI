{:$schema "../../../schema/affrescoui-screen.schema.json"
 :note "Flow's records: every board's issues, the projects and the inbox, with the queries that show a board's lanes. Their first records are in seed/, all invented."
 :models {:issues {:version 1
                   :title "Issues"
                   :note "Every board's issues: its status is the column it is in, its order its place there"
                   :fields {:ref (string :required true :unique true)
                            :board (enum :values [launch website mobile] :required true)
                            :status (integer :minimum 0 :maximum 3 :required true)
                            :order (number :default 0)
                            :title (string :required true :maxLength 500)
                            :priority (integer :minimum 0 :maximum 4 :default 0)
                            :tag (string)
                            :who (string)
                            :points (number :minimum 0)
                            :notes (string)
                            :linkText (string)
                            :link (string)
                            :start (date)
                            :due (date)
                            :labels (list :of (string) :maxItems 64)
                            :assignees (list :of (string) :maxItems 64)
                            :checklist (list :maxItems 256 :of (object :fields {:text (string) :done (boolean)}))
                            :comments (list :maxItems 1000 :of (object :fields {:who (string) :text (string)}))
                            :attachments (list :of (string) :maxItems 64)}}
          :projects {:version 1
                     :title "Projects"
                     :fields {:name (string :required true :maxLength 200)
                              :icon (string)
                              :color (string)
                              :health (enum :values [onTrack atRisk offTrack completed] :default onTrack)
                              :lead (string)
                              :target (date)
                              :progress (number :minimum 0 :maximum 1 :default 0)
                              :issues (integer :minimum 0 :default 0)}}
          :inbox {:version 1
                  :title "Inbox"
                  :note "Notifications about issues: who did what to which (ref)."
                  :fields {:ref (string :required true)
                           :who (string)
                           :kind (enum :values [assigned commented mentioned status due])
                           :text (string)
                           :to (string :description "The status it moved to.")
                           :when (string :description "ISO 8601, UTC.")
                           :read (boolean :default false)
                           :snoozed (boolean :default false)}}}
 :queries {:lane0 {:from issues :where {:board ~state.boardId :status 0} :sort [order]}
           :lane1 {:from issues :where {:board ~state.boardId :status 1} :sort [order]}
           :lane2 {:from issues :where {:board ~state.boardId :status 2} :sort [order]}
           :lane3 {:from issues :where {:board ~state.boardId :status 3} :sort [order]}}}
