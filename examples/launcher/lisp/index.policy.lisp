{:screen index.screen.lisp
 :cases [{:name "the studio changes the Launcher"
          :revision {:author studio :set {"/program/look.lisp/styles.hint" "(defstyle hint \"A small, muted hint.\"\n  (label :font small :textColor \"#000000\" :height 18))"}}
          :expect allow}
         {:name "an agent's change always waits for the person's review"
          :revision {:author "agent/claude-code" :set {"/program/look.lisp/styles.hint" "(defstyle hint \"A small, muted hint.\"\n  (label :font small :textColor \"#000000\" :height 18))"}}
          :expect ask
          :reason "the rule \"agent\" asks"}
         {:name "a change to the live state waits too"
          :revision {:author agent :set {"/state/page" settings}}
          :expect ask}
         {:name "another author's change waits"
          :revision {:author features :set {"/program/look.lisp/styles.hint" "(defstyle hint \"A small, muted hint.\"\n  (label :font small :textColor \"#000000\" :height 18))"}}
          :expect ask}
         {:name "no agent adds an accept to a button"
          :revision {:author "agent/claude-code" :set {"/program/sidebar.lisp/new-app-button" "(defview new-app-button \"New app: a button with its name, or only its icon while the sidebar is narrow.\" []\n  `{:id sideNew\n    :style primary\n    :icon plus\n    :text ~(if computed.wide (t \"nav.newApp\") \"\")\n    :font ~(if computed.wide \"strong\" \"bigIcons\")\n    :label ~(t \"nav.newApp\")\n    :tooltip ~(if computed.wide \"\" (t \"nav.newApp\"))\n    :anchors {:left 12 :right 12 :bottom 20}\n    :height 42\n    :onClick [{:accept latest}]})"}}
          :expect deny
          :reason "can't add, change or move accept"}
         {:name "no one widens what the Launcher may touch"
          :revision {:author studio :set {"/program/launcher.lisp/part.host" "(defpart host \"The services, the read of the apps folder on load, and the actions the host and scenarios run.\"\n  :services {:examples {:provider fs :root examples :readOnly true}\n             :apps {:provider fs :root apps :copyFrom [examples]}\n             :launch {:provider process :process affresco}\n             :home {:provider fs :root workspace}}\n  :onLoad {:run launcher.refresh}\n  :actions {:launcher.refresh [{:lisp refreshMine}]\n            :launcher.show [{:lisp showMineList}]})"}}
          :expect deny
          :reason "can't change \"services\""}
         {:name "no one changes the permissions"
          :revision {:author studio :set {"/program/index.screen.lisp/main/:permissions/:allow" ["*"]}}
          :expect deny
          :reason permissions}]}
