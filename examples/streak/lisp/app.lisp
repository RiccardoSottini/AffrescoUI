(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #streak
  :name Streak
  :description "A habit tracker: habits checked day by day, with scores, charts and notes."
  :entry index.screen.lisp
  :capabilities {:network {:"ws://127.0.0.1:7420" "Syncs your habits with a document server on this computer."
      :"https://id.example.com" "Signs you in to the document server with Example ID."}
    :files "Saves a backup of your habits to a file you choose, and restores one."
    :assistant "Answers your questions about your habits, with the assistant you set up in Settings."}
  :data {:models models.lisp :seed {:habits "seed/habits.lisp"}})
