{:screen index.screen.lisp
  :cases [{:name "the studio changes the screen"
      :revision {:author studio :set {:"#body/color" "#000000"}}
      :expect allow}
    {:name "an agent's change waits for review"
      :revision {:author "agent/claude-code" :set {:"#body/color" "#000000"}}
      :expect ask
      :reason "no rule allows"}
    {:name "no one changes the app bar"
      :revision {:author studio :set {:"#appBar/color" "#000000"}}
      :expect deny
      :reason "#appBar"}
    {:name "an agent's new behaviour is always asked"
      :revision {:author "agent/claude-code" :add {:"/actions/policyExample" [{:set state.theme :to dark}]}}
      :expect ask}
    {:name "twenty new panels are within the agent's budget"
      :revision {:author "agent/claude-code"
        :patch [{:op add :path "#body/children/-" :value (node #policyPanel0)}
          {:op add :path "#body/children/-" :value (node #policyPanel1)}
          {:op add :path "#body/children/-" :value (node #policyPanel2)}
          {:op add :path "#body/children/-" :value (node #policyPanel3)}
          {:op add :path "#body/children/-" :value (node #policyPanel4)}
          {:op add :path "#body/children/-" :value (node #policyPanel5)}
          {:op add :path "#body/children/-" :value (node #policyPanel6)}
          {:op add :path "#body/children/-" :value (node #policyPanel7)}
          {:op add :path "#body/children/-" :value (node #policyPanel8)}
          {:op add :path "#body/children/-" :value (node #policyPanel9)}
          {:op add :path "#body/children/-" :value (node #policyPanel10)}
          {:op add :path "#body/children/-" :value (node #policyPanel11)}
          {:op add :path "#body/children/-" :value (node #policyPanel12)}
          {:op add :path "#body/children/-" :value (node #policyPanel13)}
          {:op add :path "#body/children/-" :value (node #policyPanel14)}
          {:op add :path "#body/children/-" :value (node #policyPanel15)}
          {:op add :path "#body/children/-" :value (node #policyPanel16)}
          {:op add :path "#body/children/-" :value (node #policyPanel17)}
          {:op add :path "#body/children/-" :value (node #policyPanel18)}
          {:op add :path "#body/children/-" :value (node #policyPanel19)}]}
      :expect ask
      :reason "no rule allows"}
    {:name "a twenty-first is over it"
      :revision {:author "agent/claude-code"
        :patch [{:op add :path "#body/children/-" :value (node #policyPanel0)}
          {:op add :path "#body/children/-" :value (node #policyPanel1)}
          {:op add :path "#body/children/-" :value (node #policyPanel2)}
          {:op add :path "#body/children/-" :value (node #policyPanel3)}
          {:op add :path "#body/children/-" :value (node #policyPanel4)}
          {:op add :path "#body/children/-" :value (node #policyPanel5)}
          {:op add :path "#body/children/-" :value (node #policyPanel6)}
          {:op add :path "#body/children/-" :value (node #policyPanel7)}
          {:op add :path "#body/children/-" :value (node #policyPanel8)}
          {:op add :path "#body/children/-" :value (node #policyPanel9)}
          {:op add :path "#body/children/-" :value (node #policyPanel10)}
          {:op add :path "#body/children/-" :value (node #policyPanel11)}
          {:op add :path "#body/children/-" :value (node #policyPanel12)}
          {:op add :path "#body/children/-" :value (node #policyPanel13)}
          {:op add :path "#body/children/-" :value (node #policyPanel14)}
          {:op add :path "#body/children/-" :value (node #policyPanel15)}
          {:op add :path "#body/children/-" :value (node #policyPanel16)}
          {:op add :path "#body/children/-" :value (node #policyPanel17)}
          {:op add :path "#body/children/-" :value (node #policyPanel18)}
          {:op add :path "#body/children/-" :value (node #policyPanel19)}
          {:op add :path "#body/children/-" :value (node #policyPanel20)}]}
      :expect deny
      :reason "over the budget"}
    {:name "the budget is the agent's only"
      :revision {:author studio
        :patch [{:op add :path "#body/children/-" :value (node #policyPanel0)}
          {:op add :path "#body/children/-" :value (node #policyPanel1)}
          {:op add :path "#body/children/-" :value (node #policyPanel2)}
          {:op add :path "#body/children/-" :value (node #policyPanel3)}
          {:op add :path "#body/children/-" :value (node #policyPanel4)}
          {:op add :path "#body/children/-" :value (node #policyPanel5)}
          {:op add :path "#body/children/-" :value (node #policyPanel6)}
          {:op add :path "#body/children/-" :value (node #policyPanel7)}
          {:op add :path "#body/children/-" :value (node #policyPanel8)}
          {:op add :path "#body/children/-" :value (node #policyPanel9)}
          {:op add :path "#body/children/-" :value (node #policyPanel10)}
          {:op add :path "#body/children/-" :value (node #policyPanel11)}
          {:op add :path "#body/children/-" :value (node #policyPanel12)}
          {:op add :path "#body/children/-" :value (node #policyPanel13)}
          {:op add :path "#body/children/-" :value (node #policyPanel14)}
          {:op add :path "#body/children/-" :value (node #policyPanel15)}
          {:op add :path "#body/children/-" :value (node #policyPanel16)}
          {:op add :path "#body/children/-" :value (node #policyPanel17)}
          {:op add :path "#body/children/-" :value (node #policyPanel18)}
          {:op add :path "#body/children/-" :value (node #policyPanel19)}
          {:op add :path "#body/children/-" :value (node #policyPanel20)}]}
      :expect allow}
    {:name "no one changes the permissions"
      :revision {:author studio :set {:"/permissions/allow" ["*"]}}
      :expect deny
      :reason "can't change \"permissions\""}
    {:name "no one moves where the habits sync"
      :revision {:author studio :set {:"/connections/streakServer/url" "ws://127.0.0.1:7421/live"}}
      :expect deny
      :reason "can't change \"connections\""}
    {:name "nor does an agent"
      :revision {:author "agent/claude-code" :set {:"/connections/streakServer/messages/open/document" "~elsewhere"}}
      :expect deny
      :reason "can't change \"connections\""}]}
