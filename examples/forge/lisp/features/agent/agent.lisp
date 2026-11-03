;;; agent.lisp --- Forge's agent pane and the Features view
;;;
;;; A request in plain words goes to the connected agents through the host's request/reply relay, on a channel to
;;; Forge's own studio.sock (as any client of the person does); an agent's change comes back as a proposal, never
;;; applied by this pane: it is reviewed here (its changes, its evidence: screenshots and scenario results, a live
;;; preview) and accepted at the head the person reviewed, or rejected. The Features view lists the host's feature
;;; catalogue (the features service: catalog, install), and switches an installed feature on or off with a revision on
;;; /features/<name> (RFC 0004), so each is reviewable and can be undone. What a peer sends is bounded before it is
;;; kept.

;;; State and definitions

(defstate agent
  {:draft ""
   :log []
   :asked 0
   :seen 0
   :connected false
   :status "Not connected to Forge's host"
   :selected nil
   :rejecting nil})

(defstate features {:catalog [] :selected nil :filter "" :error "" :busy false})

(defcomputed agentCurrent
  (if (= (count proposals) 0)
        nil
        (or-else (find (fn [it] (= (text it.id) state.agent.selected)) proposals) proposals.0)))

(defcomputed agentEvidence (if (nil? computed.agentCurrent) nil computed.agentCurrent.evidence))

(defcomputed agentLogRows
  (map-indexed (fn [index it]
                   {:id (text index)
                    :label it.text
                    :icon (cond (= it.kind "you") "user"
                                (= it.kind "agent") "bot"
                                (= it.kind "error") "circle-x"
                                (= it.kind "review") "git-pull-request"
                                :else "info")
                    :iconColor (cond (= it.kind "error") "#f85149" (= it.kind "agent") "#1fae9c" :else "#8b949e")})
                 state.agent.log))

(defcomputed agentProposalRows
  (map (fn [it]
           {:id (text it.id)
            :label (str "#" it.id "  " it.message "  · " it.author)
            :icon (if (= it.id preview) "eye" "git-pull-request")})
         proposals))

(defcomputed agentChangeRows
  (if (nil? computed.agentCurrent)
        []
        (map-indexed (fn [index it] {:id (text index) :label it :icon "git-commit"})
                     (slice (or-else computed.agentCurrent.changes []) 0 200))))

(defcomputed agentScenarioRows
  (if (nil? computed.agentEvidence)
        []
        (map-indexed (fn [index it]
                       {:id (text index)
                        :label (+ (text it.name)
                                  (if (= it.passed true) "" (str ": " (text (or-else it.message "failed")))))
                        :icon (if (= it.passed true) "circle-check" "circle-x")
                        :iconColor (if (= it.passed true) "#2ea043" "#f85149")})
                     (slice (or-else computed.agentEvidence.scenarios []) 0 200))))

(defcomputed featureCards
  (filter (fn [it]
              (or (empty? (trim state.features.filter))
                  (some? (fuzzy (trim state.features.filter) (str it.title " " it.name)))))
            state.features.catalog))

(defcomputed featureCurrent (find (fn [it] (= it.name state.features.selected)) state.features.catalog))

(defkey "mod+alt+i" showAgent)

(defkey "mod+shift+x" showFeatures)

(defpart agent
         "What agent adds."
         :channels {:forgeAgent {:socket studio.sock :reconnect {:after 2000}}})

;;; Functions and commands

(defcommand showAgent "Show Agent"
  :title ~(t "command.showAgent") :category ~(t "category.view") :icon "bot" []
  (forgeShowView "agent"))

(defcommand showFeatures "Show Features"
  :title ~(t "command.showFeatures") :category ~(t "category.view") :icon "blocks" []
  (forgeShowView "features")
  (featuresRefresh))

(defn agentLog "TODO" [kind message]
  (push "state.agent.log" {:kind kind :text (slice (text message) 0 4000)})
  (when (> (count state.agent.log) 200)
    (set "state.agent.log" (slice state.agent.log (- (count state.agent.log) 200)))))

(defcommand agentAsk "Send the request to the connected agents; what they change comes back as a proposal to review."
  :title ~(t "command.agentAsk") :category ~(t "category.agent") :icon "send"
  :description ~(t "command.agentAsk.about") :enablement ~(not (empty? (trim state.agent.draft))) []
  (when (not (empty? (trim state.agent.draft)))
    (send "forgeAgent"
          :data {:type "request" :id (str "forge-" state.agent.asked) :text (slice (trim state.agent.draft) 0 8000)})
    (increment "state.agent.asked" :by 1)
    (agentLog "you" (str "You: " (trim state.agent.draft)))
    (set "state.agent.draft" "")))

(defn forgeAgentOpen "TODO" []
  (set "state.agent.status" "Connected, waiting for the host"))

(defn forgeAgentHello "TODO" []
  (set "state.agent.connected" true)
  (set "state.agent.status" "Connected: agents reach Forge on agent.sock"))

(defn forgeAgentClosed "TODO" []
  (set "state.agent.connected" false)
  (set "state.agent.status" "Not connected to Forge's host (retrying every 2 s)"))

(defn forgeAgentReply "TODO" []
  (agentLog "agent" (str (slice (text (or-else event.from "agent")) 0 80) ": " (text (or-else event.text "")))))

(defn forgeAgentError "TODO" []
  (agentLog "error" (or-else event.message "The request was not sent")))

(defn forgeAgentReview "TODO" []
  (when (> (count (or-else event.proposals [])) state.agent.seen)
    (agentLog "review"
              (str "Proposal #"
                   (text (get-in event.proposals [(- (count event.proposals) 1) :id]))
                   " from "
                   (text (get-in event.proposals [(- (count event.proposals) 1) :author]))
                   " is waiting for review")))
  (set "state.agent.seen" (count (or-else event.proposals []))))

(defcommand agentPreview "Show the chosen proposal live, as accepting it would, until the preview ends."
  :title ~(t "command.agentPreview") :category ~(t "category.agent") :icon "eye"
  :description ~(t "command.agentPreview.about") :enablement ~(some? computed.agentCurrent) [preview]
  (when (not (nil? computed.agentCurrent))
    (if (= computed.agentCurrent.id preview) (endPreview) (preview computed.agentCurrent.id))))

(defcommand agentAccept "Merge the chosen proposal at the head reviewed here; refused if it changed since."
  :title ~(t "command.agentAccept") :category ~(t "category.agent") :icon "check"
  :description ~(t "command.agentAccept.about") :enablement ~(some? computed.agentCurrent) []
  (when (not (nil? computed.agentCurrent))
    (accept computed.agentCurrent.id :head computed.agentCurrent.head)
    (agentLog "review" (str "Accepting proposal #" computed.agentCurrent.id))
    (set "state.agent.rejecting" nil)))

(defcommand agentReject "Drop the chosen proposal; the first click asks, the second rejects."
  :title ~(t "command.agentReject") :category ~(t "category.agent") :icon "x"
  :description ~(t "command.agentReject.about") :enablement ~(some? computed.agentCurrent) []
  (when (not (nil? computed.agentCurrent))
    (if (= state.agent.rejecting computed.agentCurrent.id)
      (do
        (reject computed.agentCurrent.id :reason "Rejected in Forge")
        (agentLog "review" (str "Rejected proposal #" computed.agentCurrent.id))
        (set "state.agent.rejecting" nil))
      (set "state.agent.rejecting" computed.agentCurrent.id))))

(defcommand agentOpenChanges "Open Proposal Changes"
  :title ~(t "command.agentOpenChanges") :category ~(t "category.agent") :icon "git-compare"
  :enablement ~(some? computed.agentCurrent) []
  (let [proposal computed.agentCurrent]
    (when (some? proposal)
      (set "state.docs" (filter (fn [it] (not= it.id (str "proposal:" proposal.id))) state.docs))
      (push "state.docs"
            {:id (str "proposal:" proposal.id)
             :label (str "Proposal #" proposal.id)
             :kind "diff"
             :text ""
             :dirty false
             :language ""
             :diff {:path (str "#" proposal.id "  " proposal.message)
                    :added (count computed.agentChangeRows)
                    :removed 0
                    :truncated (> (count (or-else proposal.changes [])) 200)
                    :items computed.agentChangeRows}})
      (showTab (str "proposal:" proposal.id) (str "Proposal #" proposal.id)))))

(defn featuresStore "TODO" [list]
  (set "state.features.catalog"
       (map (fn [it]
              {:name (slice it.name 0 100)
               :title (slice (text (or-else it.title it.name)) 0 200)
               :description (slice (text (or-else it.description "")) 0 1000)
               :release (slice (text (or-else it.release it.version "")) 0 40)
               :installed (= it.installed true)
               :enabled (= it.enabled true)})
            (filter (fn [it]
                      (and (= (type it) "object")
                           (= (type it.name) "string")
                           (not (empty? it.name))
                           (not (contains it.name "/"))
                           (not (contains it.name "~"))))
                    (slice (if (= (type list) "list") list []) 0 500))))
  (set "state.features.error" ""))

(defn featuresApplyCatalog "TODO" []
  (featuresStore event.features))

(defcommand featuresRefresh "Refresh Features"
  :title ~(t "command.featuresRefresh") :category ~(t "category.features") :icon "refresh-cw" []
  (invoke "features.catalog"
          :id "features-catalog"
          :loading "state.features.busy"
          :with {}
          :onError featuresRefreshFailed
          :onDone featuresRefreshDone))

(defcommand featuresInstall "Ask the host to install the chosen feature from its catalogue; the install is a revision to review."
  :title ~(t "command.featuresInstall") :category ~(t "category.features") :icon "download"
  :description ~(t "command.featuresInstall.about")
  :enablement ~(and (some? computed.featureCurrent) (not computed.featureCurrent.installed)) []
  (doseq [feature (filter (fn [it] (and (some? it) (not it.installed))) [computed.featureCurrent])]
    (invoke "features.install"
            :loading "state.features.busy"
            :with {:name feature.name :version feature.release}
            :onError featuresInstallFailed
            :onDone featuresInstallDone)))

(defn featuresSwitch "TODO" [on]
  (let [feature computed.featureCurrent]
    (when (and (some? feature) feature.installed (not= feature.enabled on))
      (patch [{:op "replace" :path (str "/features/" feature.name "/enabled") :value on}]
             :message (str (if on "Enable" "Disable") " the feature " feature.name))
      (set "state.features.catalog"
           (map (fn [it] (if (= it.name feature.name) (merge it {:enabled on}) it)) state.features.catalog)))))

(defcommand featuresEnable "Enable Feature"
  :title ~(t "command.featuresEnable") :category ~(t "category.features") :icon "circle-check"
  :enablement ~(and (some? computed.featureCurrent) computed.featureCurrent.installed (not computed.featureCurrent.enabled)) []
  (featuresSwitch true))

(defcommand featuresDisable "Disable Feature"
  :title ~(t "command.featuresDisable") :category ~(t "category.features") :icon "circle-pause"
  :enablement ~(and (some? computed.featureCurrent) computed.featureCurrent.installed computed.featureCurrent.enabled) []
  (featuresSwitch false))

(defn featuresRefreshFailed "What featuresRefresh does when it fails." [error]
  (set "state.features.error" (str "The feature catalogue is not available: " error.message)))

(defn featuresRefreshDone "What featuresRefresh does when it is done." [result]
  (featuresStore result))

(defn featuresInstallFailed "What featuresInstall does when it fails." [error]
  (set "state.features.error" (str "Not installed: " error.message)))

(defn featuresInstallDone "What featuresInstall does when it is done." []
  (featuresRefresh))

(defpart agent-events "What its channels' and services' events run: each a function."
  :actions {"forgeAgent:open" ~(handler forgeAgentOpen)
            "forgeAgent:hello" ~(handler forgeAgentHello)
            "forgeAgent:closed" ~(handler forgeAgentClosed)
            "forgeAgent:reply" ~(handler forgeAgentReply)
            "forgeAgent:error" ~(handler forgeAgentError)
            "forgeAgent:review" ~(handler forgeAgentReview)})
