{:screen index.screen.lisp
  :cases [{:name "without permissions, a change applies"
      :revision {:author "agent/claude-code" :set {:"#panel-root/color" "#000000"}}
      :expect allow}
    {:name "so do new panels"
      :revision {:author "agent/claude-code"
        :patch [{:op add :path "#panel-root/children/-" :value (node #policyPanel0)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel1)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel2)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel3)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel4)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel5)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel6)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel7)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel8)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel9)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel10)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel11)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel12)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel13)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel14)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel15)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel16)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel17)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel18)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel19)}
          {:op add :path "#panel-root/children/-" :value (node #policyPanel20)}]}
      :expect allow}
    {:name "a revision that can't apply is denied"
      :revision {:author "agent/claude-code" :remove "#noSuchNode"}
      :expect deny
      :reason "no node with id"}
    {:name "no one adds permissions"
      :revision {:author "agent/claude-code" :add {:"/permissions" {:allow ["*"]}}}
      :expect deny
      :reason "can't change \"permissions\""}
    {:name "a node inside a screen changes too"
      :revision {:author "agent/claude-code" :set {:"#card/radius" 20}}
      :expect allow}
    {:name "so does a translation"
      :revision {:author "agent/claude-code" :set {:"/strings/it/signInButton" Entra}}
      :expect allow}
    {:name "the fake provider is a capability: no one changes its address"
      :revision {:author "agent/claude-code"
        :set {:"/connections/exampleId/auth/tokenUrl" "https://id.example.org/token"}}
      :expect deny
      :reason "can't change \"connections\""}
    {:name "nor adds a connection"
      :revision {:author "agent/claude-code" :add {:"/connections/other" {:url "https://api.example.org"}}}
      :expect deny
      :reason "can't change \"connections\""}
    {:name "nor points the sign-in step at another provider"
      :revision {:author "agent/claude-code" :set {:"/actions/providerSignIn/3/signIn" other}}
      :expect deny
      :reason "can't add, change or move"}
    {:name "nor adds a sign-out to a button"
      :revision {:author "agent/claude-code" :set {:"#themeToggle/onClick" {:signOut exampleId}}}
      :expect deny
      :reason "can't add, change or move"}
    {:name "nor stores more than the screen persists"
      :revision {:author "agent/claude-code" :set {:"/persist" [state.password]}}
      :expect deny
      :reason "can't change \"persist\""}]}
