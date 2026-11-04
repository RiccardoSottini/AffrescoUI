{:screen calamaio.screen.lisp
 :cases [{:name "the studio changes the look"
          :revision {:author studio :set {"/program/calamaio.screen.lisp/main/:theme" inchiostro}}
          :expect allow}
         {:name "an agent's change waits for review"
          :revision {:author "agent/claude-code" :set {"/program/calamaio.screen.lisp/main/:theme" inchiostro}}
          :expect ask
          :reason "agent(/program)"}
         {:name "an agent's new key binding is always asked"
          :revision {:author "agent/claude-code"
                     :add {"/program/keys.lisp/defkey ctrl+c t" "(defkey \"ctrl+c t\" toggle-splash)"}}
          :expect ask}
         {:name "an agent's new command is always asked"
          :revision {:author "agent/claude-code"
                     :add {"/program/options.lisp/policyExample" "(defcommand policyExample \"An agent's new command.\" :title \"Policy\" [] (set \"settings.custom-enabled-themes\" \"inchiostro\"))"}}
          :expect ask}
         {:name "an agent's edit of a command's title, by its keyword, waits for review"
          :revision {:author "agent/claude-code" :set {"/program/killring.lisp/yank/:title" "Paste from Kill Ring"}}
          :expect ask}
         {:name "no one changes the echo area"
          :revision {:author studio :set {"/program/echo.lisp/echo-area" "(defview echo-area \"The echo area, across the foot of the frame.\" []\n  `(node#echoArea :use echoArea :alignSelf stretch :height 40))"}}
          :expect deny
          :reason "#echoArea"}
         {:name "an agent's new function is asked by its rule, in every mode"
          :revision {:author "agent/claude-code"
                     :add {"/program/killring.lisp/policyFunction" "(defn policyFunction \"An agent's new function.\" [] (set \"state.kills\" []))"}}
          :expect ask
          :reason "agent(/actions)"}
         {:name "no one changes the permissions"
          :revision {:author studio :set {"/program/calamaio.screen.lisp/main/:permissions/:allow" ["*"]}}
          :expect deny
          :reason permissions}]}
