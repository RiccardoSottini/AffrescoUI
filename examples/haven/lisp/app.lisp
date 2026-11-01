(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #haven
  :name Haven
  :description "A smart-home dashboard: rooms, lights, climate, automations and energy."
  :entry index.screen.lisp
  :capabilities {:assistant "Answers your questions about your home, with the assistant you set up in Settings."}
  :data {:models models.lisp
    :seed {:logbook "seed/logbook.lisp" :timeline "seed/timeline.lisp" :energy "seed/energy.lisp"}}
  :assets ["../assets/images"])
