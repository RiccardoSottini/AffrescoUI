;;; agent.lisp --- Forge's agent pane, its review of proposals, and the Features view
;;;
;;; A request in plain words goes to the connected agents through the host's request/reply relay, on a channel to
;;; Forge's own studio.sock (as any client of the person does); an agent's change comes back as a proposal, never
;;; applied by this pane: it is reviewed here (its changes, its evidence: screenshots and scenario results, a live
;;; preview) and accepted at the head the person reviewed, or rejected. The Features view lists the host's feature
;;; catalogue (the features service: catalog, install), and switches an installed feature on or off with a revision on
;;; /features/<name> (RFC 0004), so each is reviewable and can be undone. What a peer sends is bounded before it is
;;; kept.

;;; The conversation

(defstate agent
  {:draft ""
   :log []
   :asked 0
   :seen 0
   :connected false
   :status ""
   :selected nil
   :rejecting nil})

(defpart agent-channel "The channel to the host's studio.sock, which agents answer; it reconnects every 2 s."
  :channels {:forgeAgent {:socket studio.sock :reconnect {:after 2000}}})

(defcommand showAgent "Show Agent"
  :title ~(t "command.showAgent") :category ~(t "category.view") :icon "bot" []
  (forgeShowView "agent"))

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

(defn agentLog "A line of the conversation (you, agent, error or review), its text bounded; the last 200 are kept."
  [kind message]
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
    (agentLog "you" (t "agent.you" {:text (trim state.agent.draft)}))
    (set "state.agent.draft" "")))

(defn forgeAgentOpen "The channel is open; the host hasn't said hello yet." []
  (set "state.agent.status" (t "agent.waiting")))

(defn forgeAgentHello "The host said hello: agents can reach Forge." []
  (set "state.agent.connected" true)
  (set "state.agent.status" (t "agent.connected")))

(defn forgeAgentClosed "The channel closed: it is tried again." []
  (set "state.agent.connected" false)
  (set "state.agent.status" (t "agent.retrying")))

(defn forgeAgentReply "An agent's reply, with who sent it." []
  (agentLog "agent" (str (slice (text (or-else event.from "agent")) 0 80) ": " (text (or-else event.text "")))))

(defn forgeAgentError "A request that wasn't sent, and why." []
  (agentLog "error" (or-else event.message (t "agent.notSent"))))

(defkey "mod+alt+i" showAgent)

;;; Reviewing proposals

(defcomputed agentCurrent
  (if (= (count proposals) 0)
        nil
        (or-else (find (fn [it] (= (text it.id) state.agent.selected)) proposals) proposals.0)))

(defcomputed agentEvidence (if (nil? computed.agentCurrent) nil computed.agentCurrent.evidence))

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

(defcomputed previewing
  "The proposal previewed now: the host's `preview`, which a function can't name, as the step has that name."
  preview)

(defn forgeAgentReview "The proposals waiting: the conversation says when a new one arrives." []
  (when (> (count (or-else event.proposals [])) state.agent.seen)
    (agentLog "review"
              (t "agent.waitingReview" {:id (text (:id (last event.proposals))) :author (text (:author (last event.proposals)))})))
  (set "state.agent.seen" (count (or-else event.proposals []))))

(defcommand agentPreview "Show the chosen proposal live, as accepting it would, until the preview ends."
  :title ~(t "command.agentPreview") :category ~(t "category.agent") :icon "eye"
  :description ~(t "command.agentPreview.about") :enablement ~(some? computed.agentCurrent) []
  (when (some? computed.agentCurrent)
    (if (= computed.agentCurrent.id computed.previewing) (endPreview) (preview computed.agentCurrent.id))))

(defcommand agentAccept "Merge the chosen proposal at the head reviewed here; refused if it changed since."
  :title ~(t "command.agentAccept") :category ~(t "category.agent") :icon "check"
  :description ~(t "command.agentAccept.about") :enablement ~(some? computed.agentCurrent) []
  (when (some? computed.agentCurrent)
    (accept computed.agentCurrent.id :head computed.agentCurrent.head)
    (agentLog "review" (t "agent.accepting" {:id computed.agentCurrent.id}))
    (set "state.agent.rejecting" nil)))

(defcommand agentReject "Drop the chosen proposal; the first click asks, the second rejects."
  :title ~(t "command.agentReject") :category ~(t "category.agent") :icon "x"
  :description ~(t "command.agentReject.about") :enablement ~(some? computed.agentCurrent) []
  (when (some? computed.agentCurrent)
    (if (= state.agent.rejecting computed.agentCurrent.id)
      (do
        (reject computed.agentCurrent.id :reason (t "agent.rejectedReason"))
        (agentLog "review" (t "agent.rejected" {:id computed.agentCurrent.id}))
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
             :label (t "agent.proposal" {:id proposal.id})
             :kind "diff"
             :text ""
             :dirty false
             :language ""
             :diff {:path (str "#" proposal.id "  " proposal.message)
                    :added (count computed.agentChangeRows)
                    :removed 0
                    :truncated (> (count (or-else proposal.changes [])) 200)
                    :items computed.agentChangeRows}})
      (showTab (str "proposal:" proposal.id) (t "agent.proposal" {:id proposal.id})))))

;;; Features

(defstate features {:catalog [] :selected nil :filter "" :error "" :busy false})

(defcomputed featureCards
  (filter (fn [it]
              (or (empty? (trim state.features.filter))
                  (some? (fuzzy (trim state.features.filter) (str it.title " " it.name)))))
            state.features.catalog))

(defcomputed featureCurrent (find (fn [it] (= it.name state.features.selected)) state.features.catalog))

(defcommand showFeatures "Show Features"
  :title ~(t "command.showFeatures") :category ~(t "category.view") :icon "blocks" []
  (forgeShowView "features")
  (featuresRefresh))

(defcommand featuresRefresh "Refresh Features"
  :title ~(t "command.featuresRefresh") :category ~(t "category.features") :icon "refresh-cw" []
  (invoke "features.catalog"
          :id "features-catalog"
          :loading "state.features.busy"
          :with {}
          :onError featuresRefreshFailed
          :onDone featuresRefreshDone))

(defn featuresRefreshDone "The catalogue, listed." [result]
  (featuresStore result))

(defn featuresRefreshFailed "Says why the catalogue isn't there." [error]
  (set "state.features.error" (t "features.unavailable" {:error error.message})))

(defn featuresApplyCatalog "Plays a catalogue given in the event, as the scenarios do." []
  (featuresStore event.features))

(defn featuresStore "The catalogue's features, each checked and bounded: a name that can't be a path, a title, a release."
  [features]
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
                    (slice (if (= (type features) "list") features []) 0 500))))
  (set "state.features.error" ""))

(defcommand featuresInstall "Ask the host to install the chosen feature from its catalogue; the install is a revision to review."
  :title ~(t "command.featuresInstall") :category ~(t "category.features") :icon "download"
  :description ~(t "command.featuresInstall.about")
  :enablement ~(and (some? computed.featureCurrent) (not computed.featureCurrent.installed)) []
  (let [feature computed.featureCurrent]
    (when (and (some? feature) (not feature.installed))
      (invoke "features.install"
            :loading "state.features.busy"
            :with {:name feature.name :version feature.release}
            :onError featuresInstallFailed
              :onDone featuresInstallDone))))

(defn featuresInstallDone "A feature installed: the catalogue listed again." []
  (featuresRefresh))

(defn featuresInstallFailed "Says why the feature wasn't installed." [error]
  (set "state.features.error" (t "features.notInstalled" {:error error.message})))

(defn featuresChoose "A feature of the catalogue, chosen." [name]
  (set "state.features.selected" name))

(defn featuresSwitch "Switches the chosen installed feature on or off, by a revision on /features/<name>." [on]
  (let [feature computed.featureCurrent]
    (when (and (some? feature) feature.installed (not= feature.enabled on))
      (patch [{:op "replace" :path (str "/features/" feature.name "/enabled") :value on}]
             :message (t (if on "revision.enableFeature" "revision.disableFeature") {:name feature.name}))
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

(defkey "mod+shift+x" showFeatures)

;;; Events

(defpart agent-events "What the channel's events run: each a function."
  :actions {"forgeAgent:open" ~(handler forgeAgentOpen)
            "forgeAgent:hello" ~(handler forgeAgentHello)
            "forgeAgent:closed" ~(handler forgeAgentClosed)
            "forgeAgent:reply" ~(handler forgeAgentReply)
            "forgeAgent:error" ~(handler forgeAgentError)
            "forgeAgent:review" ~(handler forgeAgentReview)})
