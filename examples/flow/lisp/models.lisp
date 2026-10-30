{:$schema "../../../schema/affrescoui-screen.schema.json"
  :note "Flow's records: every board's issues, the projects and the inbox, with the queries that show a board's lanes. Their first records are in seed/, all invented."
  :models {:issues {:version 1
      :title Issues
      :note "Every board's issues: its status is the column it is in, its order its place there"
      :fields {:ref {:type string :required true :unique true}
        :board {:type enum :values [launch website mobile] :required true}
        :status {:type integer :minimum 0 :maximum 3 :required true}
        :order {:type number :default 0}
        :title {:type string :required true :maxLength 500}
        :priority {:type integer :minimum 0 :maximum 4 :default 0}
        :tag {:type string}
        :who {:type string}
        :points {:type number :minimum 0}
        :notes {:type string}
        :linkText {:type string}
        :link {:type string}
        :start {:type date}
        :due {:type date}
        :labels {:type list :of {:type string} :maxItems 64}
        :assignees {:type list :of {:type string} :maxItems 64}
        :checklist {:type list :maxItems 256 :of {:type object :fields {:text {:type string} :done {:type boolean}}}}
        :comments {:type list :maxItems 1000 :of {:type object :fields {:who {:type string} :text {:type string}}}}
        :attachments {:type list :of {:type string} :maxItems 64}}}
    :projects {:version 1
      :title Projects
      :fields {:name {:type string :required true :maxLength 200}
        :icon {:type string}
        :color {:type string}
        :health {:type enum :values [onTrack atRisk offTrack completed] :default onTrack}
        :lead {:type string}
        :target {:type date}
        :progress {:type number :minimum 0 :maximum 1 :default 0}
        :issues {:type integer :minimum 0 :default 0}}}
    :inbox {:version 1
      :title Inbox
      :note "Notifications about issues: who did what to which (ref)."
      :fields {:ref {:type string :required true}
        :who {:type string}
        :kind {:type enum :values [assigned commented mentioned status due]}
        :text {:type string}
        :to {:type string :description "The status it moved to."}
        :when {:type string :description "ISO 8601, UTC."}
        :read {:type boolean :default false}
        :snoozed {:type boolean :default false}}}}
  :queries {:lane0 {:from issues :where {:board "{{state.boardId}}" :status 0} :sort [order]}
    :lane1 {:from issues :where {:board "{{state.boardId}}" :status 1} :sort [order]}
    :lane2 {:from issues :where {:board "{{state.boardId}}" :status 2} :sort [order]}
    :lane3 {:from issues :where {:board "{{state.boardId}}" :status 3} :sort [order]}}}
