(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #flow
  :name Flow
  :description "A Kanban board: boards, swimlanes, a table and a timeline of every card."
  :entry index.screen.lisp
  :capabilities {:assistant "Answers your questions about your boards, with the assistant you set up in Settings."
    :channels "Shows who else is on a board and what they changed, through the host's local socket."}
  :data {:models models.lisp :seed {:issues "seed/issues.lisp" :projects "seed/projects.lisp" :inbox "seed/inbox.lisp"}})
