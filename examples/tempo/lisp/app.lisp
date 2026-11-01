(node :$schema "../../../schema/affrescoui-app.schema.json"
  :format affrescoui-app
  :version 1
  #tempo
  :name Tempo
  :description "A music player: playlists, a library, albums, a queue and an equaliser."
  :entry index.screen.lisp
  :capabilities {:audio "Plays the songs."
    :assistant "Answers your questions about your music, with the assistant you set up in Settings."}
  :assets ["../assets/audio" "../assets/images"])
