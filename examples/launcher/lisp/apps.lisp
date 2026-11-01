{:$schema "../../../schema/affrescoui-screen.schema.json"
  :note "The Launcher's catalog of example apps, included by index.screen.json (as state.examples). Each: id, name, a one-line tagline in English (the screen shows its translation, tagline.<id> in locales/), its folder in examples/, the program that runs it (host: the stock affrescoui_launcher opens <folder>/json or <folder>/lisp, as Settings choose; else the program, which opens its own screen), and a cover in covers/. The Apps page tags each one Example, and the person's own apps Yours. Only the apps a host-run program shows can start a new app."
  :state {:examples [(node #flow
        :name Flow
        :tagline "A Kanban board for a team's issues"
        :folder flow
        :program host
        :cover "../assets/covers/kanban.jpg")
      (node #streak
        :name Streak
        :tagline "A habit tracker with goals and a year heatmap"
        :folder streak
        :program host
        :cover "../assets/covers/habit-tracker.jpg")
      (node #sign-in
        :name "Sign in"
        :tagline "Sign in, create an account, two-step codes"
        :folder sign-in
        :program host
        :cover "../assets/covers/sign-in.jpg")
      (node #sinopia
        :name Sinopia
        :tagline "Slides and vector documents"
        :folder sinopia
        :program host
        :cover "../assets/covers/sinopia.jpg")
      (node #tempera
        :name Tempera
        :tagline "An image editor with layers, masks and filters"
        :folder tempera
        :program host
        :cover "../assets/covers/tempera.jpg")
      (node #modello
        :name Modello
        :tagline "A 3D modeller and scene editor"
        :folder modello
        :program host
        :cover "../assets/covers/modello.jpg")
      (node #forge
        :name Forge
        :tagline "An IDE with a terminal, source control and a debugger"
        :folder forge
        :program host
        :starter false
        :cover "../assets/covers/forge.jpg")
      (node #quaderno
        :name Quaderno
        :tagline "A code editor with tabs, quick open and a terminal"
        :folder code-editor
        :program host
        :cover "../assets/covers/code-editor.jpg")
      (node #haven
        :name Haven
        :tagline "A smart-home dashboard with a floor plan"
        :folder haven
        :program host
        :cover "../assets/covers/smart-home.jpg")
      (node #tempo
        :name Tempo
        :tagline "A music player with an equaliser and a visualiser"
        :folder tempo
        :program host
        :cover "../assets/covers/music-player.jpg")
      (node #arcadia
        :name Arcadia
        :tagline "A game store and launcher"
        :folder arcadia
        :program host
        :cover "../assets/covers/arcadia.jpg")
      (node #orrery
        :name Orrery
        :tagline "A planetarium with a sky chart and the planets"
        :folder orrery
        :program host
        :cover "../assets/covers/orrery.jpg")]}}
