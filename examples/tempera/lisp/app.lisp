(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #tempera
  :name Tempera
  :description "An image editor with layers, masks, adjustments and filters."
  :entry index.screen.lisp
  :capabilities {:files "Opens and saves your images and projects, and exports them, where you choose."
    :assistant "Changes your workspace when you ask, with the assistant you set up in Settings."})
