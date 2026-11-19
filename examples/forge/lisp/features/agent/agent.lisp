;;; agent.lisp --- Forge's agent pane, its review of proposals, and the Features view
;;;
;;; A request in plain words goes to the connected agents through the host's request/reply relay, on a channel to
;;; Forge's own studio.sock (as any client of the person does); an agent's change comes back as a proposal, never
;;; applied by this pane: it is reviewed here (its changes, its evidence: screenshots and scenario results, a live
;;; preview) and accepted at the head the person reviewed, or rejected. The Features view lists the host's feature
;;; catalogue (the features service: catalog, install), and switches an installed feature on or off with a revision on
;;; /features/<name> (RFC 0004), so each is reviewable and can be undone. What a peer sends is bounded before it is
;;; kept.

(ns forge.features.agent.agent)

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
  :title ~(t "command.showAgent") :description ~(t "command.showAgent") :category ~(t "category.view") :icon "bot" []
  (forge-show-view "agent"))

(defcomputed agentLogRows
  (map-indexed (fn [index it]
                   {:id (text index)
                    :label it.text
                    :icon (case it.kind :you "user" :agent "bot" :error "circle-x" :review "git-pull-request" "info")
                    :iconColor (case it.kind :error "#f85149" :agent "#1fae9c" "#8b949e")})
                 state.agent.log))

(defn agent-log "A line of the conversation (you, agent, error or review), its text bounded; the last 200 are kept."
  [kind message]
  (push "state.agent.log" {:kind kind :text (slice (text message) 0 4000)})
  (when (> (count state.agent.log) 200) (set "state.agent.log" (slice state.agent.log (- 200)))))

(defcommand agentAsk "Send the request to the connected agents; what they change comes back as a proposal to review."
  :title ~(t "command.agentAsk") :category ~(t "category.agent") :icon "send"
  :description ~(t "command.agentAsk.about") :enablement ~(not (empty? (trim state.agent.draft))) []
  (let [request (str/trim state.agent.draft)]
    (when (seq request)
      (send "forgeAgent" :data {:type "request" :id (str "forge-" state.agent.asked) :text (slice request 0 8000)})
      (increment "state.agent.asked" :by 1)
      (agent-log "you" (t "agent.you" {:text request}))
      (set "state.agent.draft" ""))))

(defn forge-agent-open "The channel is open; the host hasn't said hello yet." []
  (set "state.agent.status" (t "agent.waiting")))

(defn forge-agent-hello "The host said hello: agents can reach Forge." []
  (set "state.agent.connected" true)
  (set "state.agent.status" (t "agent.connected")))

(defn forge-agent-closed "The channel closed: it is tried again." []
  (set "state.agent.connected" false)
  (set "state.agent.status" (t "agent.retrying")))

(defn forge-agent-reply "An agent's reply, with who sent it." []
  (agent-log "agent" (str (slice (text (or-else event.from "agent")) 0 80) ": " (text (or-else event.text "")))))

(defn forge-agent-error "A request that wasn't sent, and why." []
  (agent-log "error" (or-else event.message (t "agent.notSent"))))

(defkey "mod+alt+i" showAgent)

;;; Reviewing proposals

(defcomputed agentCurrent
  (if (empty? proposals) nil (or-else (find #(= (text (:id %)) state.agent.selected) proposals) proposals.0)))

(defcomputed agentEvidence (if (some? computed.agentCurrent) computed.agentCurrent.evidence nil))

(defcomputed agentProposalRows
  (map (fn [it]
           {:id (text it.id)
            :label (str "#" it.id "  " it.message "  · " it.author)
            :icon (if (= it.id preview) "eye" "git-pull-request")})
         proposals))

(defcomputed agentChangeRows
  (if (some? computed.agentCurrent)
        (map-indexed (fn [index it] {:id (text index) :label it :icon "git-commit"})
                     (slice (or-else computed.agentCurrent.changes []) 0 200))
        []))

(defcomputed agentScenarioRows
  (if (some? computed.agentEvidence)
        (map-indexed (fn [index it]
                       {:id (text index)
                        :label (+ (text it.name)
                                  (if (= it.passed true) "" (str ": " (text (or-else it.message "failed")))))
                        :icon (if (= it.passed true) "circle-check" "circle-x")
                        :iconColor (if (= it.passed true) "#2ea043" "#f85149")})
                     (slice (or-else computed.agentEvidence.scenarios []) 0 200))
        []))

(defcomputed previewing
  "The proposal previewed now: the host's `preview`, which a function can't name, as the step has that name."
  preview)

; style: keep, a view or data holds it where it may be text
(defn forgeAgentReview "The proposals waiting: the conversation says when a new one arrives." []
  (let [waiting (or-else event.proposals [])
        newest (last waiting)]
    (when (> (count waiting) state.agent.seen)
      (agent-log "review" (t "agent.waitingReview" {:id (text (:id newest)) :author (text (:author newest))})))
    (set "state.agent.seen" (count waiting))))

(defcommand agentPreview "Show the chosen proposal live, as accepting it would, until the preview ends."
  :title ~(t "command.agentPreview") :category ~(t "category.agent") :icon "eye"
  :description ~(t "command.agentPreview.about") :enablement ~(some? computed.agentCurrent) []
  (when-some [proposal computed.agentCurrent]
    (if (= proposal.id computed.previewing) (endPreview) (preview proposal.id))))

(defcommand agentAccept "Merge the chosen proposal at the head reviewed here; refused if it changed since."
  :title ~(t "command.agentAccept") :category ~(t "category.agent") :icon "check"
  :description ~(t "command.agentAccept.about") :enablement ~(some? computed.agentCurrent) []
  (when-some [proposal computed.agentCurrent]
    (accept proposal.id :head proposal.head)
    (agent-log "review" (t "agent.accepting" {:id proposal.id}))
    (set "state.agent.rejecting" nil)))

(defcommand agentReject "Drop the chosen proposal; the first click asks, the second rejects."
  :title ~(t "command.agentReject") :category ~(t "category.agent") :icon "x"
  :description ~(t "command.agentReject.about") :enablement ~(some? computed.agentCurrent) []
  (when-some [proposal computed.agentCurrent]
    (if (= state.agent.rejecting proposal.id)
      (do
        (reject proposal.id :reason (t "agent.rejectedReason"))
        (agent-log "review" (t "agent.rejected" {:id proposal.id}))
        (set "state.agent.rejecting" nil))
      (set "state.agent.rejecting" proposal.id))))

(defcommand agentOpenChanges "Open Proposal Changes"
  :title ~(t "command.agentOpenChanges")
  :description ~(t "command.agentOpenChanges") :category ~(t "category.agent") :icon "git-compare"
  :enablement ~(some? computed.agentCurrent) []
  (when-some [proposal computed.agentCurrent]
    (set "state.docs" (remove #(= (:id %) (str "proposal:" proposal.id)) state.docs))
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
    (showTab (str "proposal:" proposal.id) (t "agent.proposal" {:id proposal.id}))))

;;; Features

(defstate features {:catalog [] :selected nil :filter "" :error "" :busy false})

(defcomputed featureCards
  (filter (fn [it]
              (or (empty? (trim state.features.filter))
                  (some? (fuzzy (trim state.features.filter) (str it.title " " it.name)))))
            state.features.catalog))

(defcomputed featureCurrent (find #(= (:name %) state.features.selected) state.features.catalog))

(defcommand showFeatures "Show Features"
  :title ~(t "command.showFeatures")
  :description ~(t "command.showFeatures") :category ~(t "category.view") :icon "blocks" []
  (forge-show-view "features")
  (featuresRefresh))

(defcommand featuresRefresh "Refresh Features"
  :title ~(t "command.featuresRefresh")
  :description ~(t "command.featuresRefresh") :category ~(t "category.features") :icon "refresh-cw" []
  (invoke "features.catalog"
          :id "features-catalog"
          :loading "state.features.busy"
          :with {}
          :onError features-refresh-failed
          :onDone features-refresh-done))

(defn features-refresh-done "The catalogue, listed." [result]
  (features-store result))

(defn features-refresh-failed "Says why the catalogue isn't there." [error]
  (set "state.features.error" (t "features.unavailable" {:error error.message})))

; style: keep, named in examples/forge/lisp/agent.scenarios.json
(defn featuresApplyCatalog "Plays a catalogue given in the event, as the scenarios do." []
  (features-store event.features))

(defn features-store
  "The catalogue's features, each checked and bounded: a name that can't be a path, a title, a release."
  [features]
  (set "state.features.catalog"
       (map (fn [it]
              {:name (slice it.name 0 100)
               :title (slice (text (or-else it.title it.name)) 0 200)
               :description (slice (text (or-else it.description "")) 0 1000)
               :release (slice (text (or-else it.release it.version "")) 0 40)
               :installed (true? it.installed)
               :enabled (true? it.enabled)})
            (filter #(and (= (type %) "object")
                          (= (type (:name %)) "string")
                          (seq (:name %))
                          (not (str/includes? (:name %) "/"))
                          (not (str/includes? (:name %) "~")))
                    (slice (if (= (type features) "list") features []) 0 500))))
  (set "state.features.error" ""))

(defcommand featuresInstall
  "Ask the host to install the chosen feature from its catalogue; the install is a revision to review."
  :title ~(t "command.featuresInstall") :category ~(t "category.features") :icon "download"
  :description ~(t "command.featuresInstall.about")
  :enablement ~(and (some? computed.featureCurrent) (not computed.featureCurrent.installed)) []
  (let [feature computed.featureCurrent]
    (when (and (some? feature) (not feature.installed))
      (invoke "features.install"
              :loading "state.features.busy"
              :with {:name feature.name :version feature.release}
              :onError features-install-failed
              :onDone features-install-done))))

(defn features-install-done "A feature installed: the catalogue listed again." []
  (featuresRefresh))

(defn features-install-failed "Says why the feature wasn't installed." [error]
  (set "state.features.error" (t "features.notInstalled" {:error error.message})))

(defn features-choose "A feature of the catalogue, chosen." [name]
  (set "state.features.selected" name))

(defn features-switch "Switches the chosen installed feature on or off, by a revision on /features/<name>." [on]
  (let [feature computed.featureCurrent]
    (when (and (some? feature) feature.installed (not= feature.enabled on))
      (patch [{:op "replace" :path (str "/features/" feature.name "/enabled") :value on}]
             :message (t (if on "revision.enableFeature" "revision.disableFeature") {:name feature.name}))
      (set "state.features.catalog"
           (map #(if (= (:name %) feature.name) (merge % {:enabled on}) %) state.features.catalog)))))

(defcommand featuresEnable "Enable Feature"
  :title ~(t "command.featuresEnable") :description ~(t "command.featuresEnable") :category ~(t "category.features")
  :icon "circle-check"
  :enablement ~(and (some? computed.featureCurrent)
                    computed.featureCurrent.installed
                    (not computed.featureCurrent.enabled))
  []
  (features-switch true))

(defcommand featuresDisable "Disable Feature"
  :title ~(t "command.featuresDisable") :description ~(t "command.featuresDisable")
  :category ~(t "category.features") :icon "circle-pause"
  :enablement ~(and (some? computed.featureCurrent) computed.featureCurrent.installed computed.featureCurrent.enabled) []
  (features-switch false))

(defkey "mod+shift+x" showFeatures)

;;; Events

(defpart agent-events "What the channel's events run: each a function."
  :actions {"forgeAgent:open" ~(handler forge-agent-open)
            "forgeAgent:hello" ~(handler forge-agent-hello)
            "forgeAgent:closed" ~(handler forge-agent-closed)
            "forgeAgent:reply" ~(handler forge-agent-reply)
            "forgeAgent:error" ~(handler forge-agent-error)
            "forgeAgent:review" ~(handler forgeAgentReview)})
