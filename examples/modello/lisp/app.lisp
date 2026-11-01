(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #modello
  :name Modello
  :description "A 3D modeller written only as screen documents."
  :entry index.screen.lisp
  :capabilities {:files "Saves your scenes and opens them again, where you choose."
    :assistant "Answers your questions about your models, with the assistant you set up in Settings."}
  :assets ["../assets/samples"])
