{:$schema "../../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Forge's agent pane and the Features view. A request in plain words goes to the connected agents through the host's request/reply relay, on a channel to Forge's own studio.sock (as any client of the person does); an agent's change comes back as a proposal, never applied by this pane: it is reviewed here (its changes, its evidence: screenshots and scenario results, a live preview) and accepted at the head the person reviewed, or rejected. The Features view lists the host's feature catalogue (the features service: catalog, install), and switches an installed feature on or off with a revision on /features/<name> (RFC 0004), so each is reviewable and can be undone. What a peer sends is bounded before it is kept."
  :channels {:forgeAgent {:socket studio.sock :reconnect {:after 2000}}}
  :services {:features {:provider features}}
  :state {:agent {:draft ""
      :log []
      :asked 0
      :seen 0
      :connected false
      :status "Not connected to Forge's host"
      :selected nil
      :rejecting nil}
    :features {:catalog [] :selected nil :filter "" :error "" :busy false}}
  :computed {:agentCurrent "{{length(proposals) == 0 ? null : find(proposals, text(it.id) == state.agent.selected) ?? proposals[0]}}"
    :agentEvidence "{{computed.agentCurrent == null ? null : computed.agentCurrent.evidence}}"
    :agentLogRows "{{map(state.agent.log, {id: text(index), label: it.text, icon: it.kind == 'you' ? 'user' : it.kind == 'agent' ? 'bot' : it.kind == 'error' ? 'circle-x' : it.kind == 'review' ? 'git-pull-request' : 'info', iconColor: it.kind == 'error' ? '#f85149' : it.kind == 'agent' ? '#1fae9c' : '#8b949e'})}}"
    :agentProposalRows "{{map(proposals, {id: text(it.id), label: '#' + it.id + '  ' + it.message + '  · ' + it.author, icon: it.id == preview ? 'eye' : 'git-pull-request'})}}"
    :agentChangeRows "{{computed.agentCurrent == null ? [] : map(slice(computed.agentCurrent.changes ?? [], 0, 200), {id: text(index), label: it, icon: 'git-commit'})}}"
    :agentScenarioRows "{{computed.agentEvidence == null ? [] : map(slice(computed.agentEvidence.scenarios ?? [], 0, 200), {id: text(index), label: text(it.name) + (it.passed == true ? '' : ': ' + text(it.message ?? 'failed')), icon: it.passed == true ? 'circle-check' : 'circle-x', iconColor: it.passed == true ? '#2ea043' : '#f85149'})}}"
    :featureCards "{{filter(state.features.catalog, empty(trim(state.features.filter)) || fuzzy(trim(state.features.filter), it.title + ' ' + it.name) != null)}}"
    :featureCurrent "{{find(state.features.catalog, it.name == state.features.selected)}}"}
  :actions {:showAgent {:title "Show Agent" :category View :icon bot :do [{:run forgeShowView :with {:view agent}}]}
    :showFeatures {:title "Show Features"
      :category View
      :icon blocks
      :do [{:run forgeShowView :with {:view features}} {:run featuresRefresh}]}
    :agentLog [{:push state.agent.log :value "{{ {kind: kind, text: slice(text(message), 0, 4000)} }}"}
      {:if "{{length(state.agent.log) > 200}}"
        :then [{:set state.agent.log :to "{{slice(state.agent.log, length(state.agent.log) - 200)}}"}]}]
    :agentAsk {:title "Ask the Agent"
      :category Agent
      :icon send
      :description "Send the request to the connected agents; what they change comes back as a proposal to review."
      :enablement "{{!empty(trim(state.agent.draft))}}"
      :do [{:if "{{empty(trim(state.agent.draft))}}" :then [{:stop true}]}
        {:send forgeAgent
          :data (request #"{{'forge-' + state.agent.asked}}" :text "{{slice(trim(state.agent.draft), 0, 8000)}}")}
        {:increment state.agent.asked :by 1}
        {:run agentLog :with {:kind you :message "{{'You: ' + trim(state.agent.draft)}}"}}
        {:set state.agent.draft :to ""}]}
    :"forgeAgent:open" [{:set state.agent.status :to "Connected, waiting for the host"}]
    :"forgeAgent:hello" [{:set state.agent.connected :to true}
      {:set state.agent.status :to "Connected: agents reach Forge on agent.sock"}]
    :"forgeAgent:closed" [{:set state.agent.connected :to false}
      {:set state.agent.status :to "Not connected to Forge's host (retrying every 2 s)"}]
    :"forgeAgent:reply" [{:run agentLog
        :with {:kind agent :message "{{slice(text(event.from ?? 'agent'), 0, 80) + ': ' + text(event.text ?? '')}}"}}]
    :"forgeAgent:error" [{:run agentLog :with {:kind error :message "{{event.message ?? 'The request was not sent'}}"}}]
    :"forgeAgent:review" [{:if "{{length(event.proposals ?? []) > state.agent.seen}}"
        :then [{:run agentLog
            :with {:kind review
              :message "{{'Proposal #' + text(event.proposals[length(event.proposals) - 1].id) + ' from ' + text(event.proposals[length(event.proposals) - 1].author) + ' is waiting for review'}}"}}]}
      {:set state.agent.seen :to "{{length(event.proposals ?? [])}}"}]
    :agentPreview {:title "Preview Proposal"
      :category Agent
      :icon eye
      :description "Show the chosen proposal live, as accepting it would, until the preview ends."
      :enablement "{{computed.agentCurrent != null}}"
      :do [{:if "{{computed.agentCurrent == null}}" :then [{:stop true}]}
        {:if "{{computed.agentCurrent.id == preview}}"
          :then [{:endPreview true}]
          :else [{:preview "{{computed.agentCurrent.id}}"}]}]}
    :agentAccept {:title "Accept Proposal"
      :category Agent
      :icon check
      :description "Merge the chosen proposal at the head reviewed here; refused if it changed since."
      :enablement "{{computed.agentCurrent != null}}"
      :do [{:if "{{computed.agentCurrent == null}}" :then [{:stop true}]}
        {:accept "{{computed.agentCurrent.id}}" :head "{{computed.agentCurrent.head}}"}
        {:run agentLog :with {:kind review :message "{{'Accepting proposal #' + computed.agentCurrent.id}}"}}
        {:set state.agent.rejecting :to nil}]}
    :agentReject {:title "Reject Proposal"
      :category Agent
      :icon x
      :description "Drop the chosen proposal; the first click asks, the second rejects."
      :enablement "{{computed.agentCurrent != null}}"
      :do [{:if "{{computed.agentCurrent == null}}" :then [{:stop true}]}
        {:if "{{state.agent.rejecting == computed.agentCurrent.id}}"
          :then [{:reject "{{computed.agentCurrent.id}}" :reason "Rejected in Forge"}
            {:run agentLog :with {:kind review :message "{{'Rejected proposal #' + computed.agentCurrent.id}}"}}
            {:set state.agent.rejecting :to nil}]
          :else [{:set state.agent.rejecting :to "{{computed.agentCurrent.id}}"}]}]}
    :agentOpenChanges {:title "Open Proposal Changes"
      :category Agent
      :icon git-compare
      :enablement "{{computed.agentCurrent != null}}"
      :do [{:forEach "{{filter([computed.agentCurrent], it != null)}}"
          :as proposal
          :do [{:set state.docs :to "{{filter(state.docs, it.id != 'proposal:' + proposal.id)}}"}
            {:push state.docs
              :value "{{ {id: 'proposal:' + proposal.id, label: 'Proposal #' + proposal.id, kind: 'diff', text: '', dirty: false, language: '', diff: {path: '#' + proposal.id + '  ' + proposal.message, added: length(computed.agentChangeRows), removed: 0, truncated: length(proposal.changes ?? []) > 200, items: computed.agentChangeRows}} }}"}
            {:run showTab :with (node #"{{'proposal:' + proposal.id}}" :label "{{'Proposal #' + proposal.id}}")}]}]}
    :featuresStore [{:set state.features.catalog
        :to "{{map(filter(slice(type(list) == 'list' ? list : [], 0, 500), type(it) == 'object' && type(it.name) == 'string' && !empty(it.name) && !contains(it.name, '/') && !contains(it.name, '~')), {name: slice(it.name, 0, 100), title: slice(text(it.title ?? it.name), 0, 200), description: slice(text(it.description ?? ''), 0, 1000), release: slice(text(it.release ?? it.version ?? ''), 0, 40), installed: it.installed == true, enabled: it.enabled == true})}}"}
      {:set state.features.error :to ""}]
    :featuresApplyCatalog [{:run featuresStore :with {:list "{{event.features}}"}}]
    :featuresRefresh {:title "Refresh Features"
      :category Features
      :icon refresh-cw
      :do [(node :invoke features.catalog
          #features-catalog
          :loading state.features.busy
          :with {}
          :onError [{:set state.features.error :to "{{'The feature catalogue is not available: ' + error.message}}"}]
          :onDone [{:run featuresStore :with {:list "{{result}}"}}])]}
    :featuresInstall {:title "Install Feature"
      :category Features
      :icon download
      :description "Ask the host to install the chosen feature from its catalogue; the install is a revision to review."
      :enablement "{{computed.featureCurrent != null && !computed.featureCurrent.installed}}"
      :do [{:forEach "{{filter([computed.featureCurrent], it != null && !it.installed)}}"
          :as feature
          :do [{:invoke features.install
              :loading state.features.busy
              :with {:name "{{feature.name}}" :version "{{feature.release}}"}
              :onError [{:set state.features.error :to "{{'Not installed: ' + error.message}}"}]
              :onDone [{:run featuresRefresh}]}]}]}
    :featuresSwitch [{:forEach "{{filter([computed.featureCurrent], it != null && it.installed && it.enabled != on)}}"
        :as feature
        :do [{:patch [{:op replace :path "{{'/features/' + feature.name + '/enabled'}}" :value "{{on}}"}]
            :message "{{(on ? 'Enable' : 'Disable') + ' the feature ' + feature.name}}"}
          {:set state.features.catalog
            :to "{{map(state.features.catalog, it.name == feature.name ? merge(it, {enabled: on}) : it)}}"}]}]
    :featuresEnable {:title "Enable Feature"
      :category Features
      :icon circle-check
      :enablement "{{computed.featureCurrent != null && computed.featureCurrent.installed && !computed.featureCurrent.enabled}}"
      :do [{:run featuresSwitch :with {:on true}}]}
    :featuresDisable {:title "Disable Feature"
      :category Features
      :icon circle-pause
      :enablement "{{computed.featureCurrent != null && computed.featureCurrent.installed && computed.featureCurrent.enabled}}"
      :do [{:run featuresSwitch :with {:on false}}]}}
  :keys {:"mod+alt+i" showAgent :"mod+shift+x" showFeatures}
  :menus {:view {:agent {:run showAgent :group "1_views" :order 3}
      :features {:run showFeatures :group "1_views" :order 4}}}
  :components {:forgeAgentButton {:params (node #"" :text "" :icon "" :action "" :on true)
      :node (label #"{{id}}"
        :icon "{{icon}}"
        :text "{{text}}"
        :height 26
        :padding 8
        :fitText true
        :align center
        :radius 2
        :cursor hand
        :focusable true
        :role button
        :textColor "{{on ? '#ffffff' : '@color.muted'}}"
        :color "{{on ? '@color.accent' : '@color.border'}}"
        :onClick {:run "{{action}}"})}
    :forgeAgentView {:node (column #agent-view :width "100%" :grow 1 :color @color.sideBar :gap 4
        (label #agent-title :text Agent :textColor @color.muted :width "100%" :height 32 :padding 12)
        (label #agent-status
          :icon "{{state.agent.connected ? 'plug' : 'unplug'}}"
          :text "{{state.agent.status}}"
          :wrap true
          :width "100%"
          :height 36
          :padding 8
          :textColor @color.muted)
        (column #agent-empty
          :when "{{length(state.agent.log) == 0}}"
          :width "100%"
          :grow 2
          :alignItems center
          :justify center
          :gap 8
          (label #agent-empty-icon :icon bot :font iconsLarge :textColor @color.accent :fitText true)
          (label #agent-empty-title :text "No conversation yet" :textColor @color.text :fitText true)
          (label #agent-empty-text
            :wrap true
            :width "100%"
            :height 72
            :padding 12
            :align center
            :textColor @color.muted
            :text "{{state.agent.connected ? 'Ask for a change below. The agent edits the documents of this window, and each change waits under Review until you accept it.' : 'An agent joins through the socket of the host, for example with the MCP server. Once it is connected, ask for a change below; each change it makes waits under Review.'}}"))
        (list #agent-log
          :when "{{length(state.agent.log) > 0}}"
          :width "100%"
          :grow 2
          :items "{{computed.agentLogRows}}"
          :selection none
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :accentColor @color.accent)
        (textarea #agent-draft
          :width "100%"
          :height 52
          :padding 6
          :wrap true
          :color @color.input
          :textColor @color.text
          :placeholderColor @color.muted
          :bind state.agent.draft
          :placeholder "Ask for a change (Enter to send)"
          :onSubmit {:run agentAsk})
        (row #agent-ask-row :width "100%" :height 30 :justify end
          {:use forgeAgentButton
            :with (node #agent-ask :text Ask :icon send :action agentAsk :on "{{!empty(trim(state.agent.draft))}}")})
        (label #agent-review-title
          :text "{{'REVIEW  ' + length(proposals)}}"
          :textColor @color.muted
          :width "100%"
          :height 22
          :padding 8)
        (label #agent-review-empty
          :when "{{length(proposals) == 0}}"
          :wrap true
          :width "100%"
          :height 44
          :padding 8
          :textColor @color.muted
          :text "No proposals to review. An agent's change waits here.")
        (list #agent-proposals
          :when "{{length(proposals) > 0}}"
          :width "100%"
          :grow 1
          :items "{{computed.agentProposalRows}}"
          :bind state.agent.selected
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :selectedTextColor @color.text
          :selectedBackground @color.selection
          :accentColor @color.accent
          :onOpen {:run agentOpenChanges})
        {:use forgeAgentReview :when "{{computed.agentCurrent != null}}"})}
    :forgeAgentReview {:node (column #agent-review :width "100%" :grow 2 :gap 4
        (label #agent-proposal-message
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.text
          :text "{{computed.agentCurrent.message + '  (' + computed.agentCurrent.operations + ' changes by ' + computed.agentCurrent.author + ')'}}")
        (list #agent-changes
          :width "100%"
          :grow 1
          :items "{{computed.agentChangeRows}}"
          :selection none
          :rowHeight 20
          :font code
          :color @color.sideBar
          :textColor @color.text
          :accentColor @color.accent)
        (label #agent-evidence-none
          :when "{{computed.agentEvidence == null}}"
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.muted
          :text "No evidence came with this proposal: preview it before accepting.")
        (label #agent-evidence-behaviour
          :when "{{computed.agentEvidence != null && !empty(computed.agentEvidence.behaviour)}}"
          :icon activity
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.text
          :text "{{slice(text(computed.agentEvidence.behaviour), 0, 2000)}}")
        (row #agent-evidence-shots
          :when "{{computed.agentEvidence != null && type(computed.agentEvidence.before) == 'string' && type(computed.agentEvidence.after) == 'string'}}"
          :width "100%"
          :height 120
          :gap 4
          (image #agent-evidence-before
            :source "{{computed.agentEvidence.before}}"
            :grow 1
            :height "100%"
            :fit contain
            :label Before)
          (image #agent-evidence-after
            :source "{{computed.agentEvidence.after}}"
            :grow 1
            :height "100%"
            :fit contain
            :label After))
        (list #agent-scenarios
          :when "{{length(computed.agentScenarioRows) > 0}}"
          :width "100%"
          :height 88
          :items "{{computed.agentScenarioRows}}"
          :selection none
          :rowHeight 22
          :color @color.sideBar
          :textColor @color.text
          :accentColor @color.accent)
        (row #agent-decide :width "100%" :gap 4
          {:use forgeAgentButton
            :with (node #agent-preview
              :action agentPreview
              :on false
              :icon "{{computed.agentCurrent.id == preview ? 'eye-off' : 'eye'}}"
              :text "{{computed.agentCurrent.id == preview ? 'End Preview' : 'Preview'}}")}
          {:use forgeAgentButton :with (node #agent-accept :text Accept :icon check :action agentAccept)}
          {:use forgeAgentButton
            :with (node #agent-reject
              :icon x
              :action agentReject
              :on false
              :text "{{state.agent.rejecting == computed.agentCurrent.id ? 'Confirm Reject' : 'Reject'}}")}))}
    :forgeFeaturesView {:node (column #features-view :width "100%" :grow 1 :color @color.sideBar :gap 4
        (row #features-head :width "100%" :height 35 :alignItems center
          (label #features-title :text Features :textColor @color.muted :grow 1 :height 35 :padding 12)
          (label #features-refresh
            :icon refresh-cw
            :padding 6
            :fitText true
            :cursor hand
            :textColor "{{state.features.busy ? '@color.accent' : '@color.muted'}}"
            :tooltip Refresh
            :hover {:textColor @color.text}
            :onClick {:run featuresRefresh}))
        (textarea #features-filter
          :width "100%"
          :height 26
          :padding 6
          :color @color.input
          :textColor @color.text
          :placeholderColor @color.muted
          :bind state.features.filter
          :placeholder "Search the catalogue")
        (label #features-error
          :when "{{!empty(state.features.error)}}"
          :icon circle-x
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.text
          :text "{{state.features.error}}")
        (label #features-empty
          :when "{{length(state.features.catalog) == 0 && empty(state.features.error)}}"
          :wrap true
          :width "100%"
          :padding 8
          :textColor @color.muted
          :text "The host's catalogue lists no features.")
        (column #features-list :role listbox :label Features :width "100%" :grow 1 :scroll vertical
          (row :repeat {:for f :in "{{computed.featureCards}}" :index fi :key "{{f.name}}"}
            #"{{'feature-' + f.name}}"
            :role option
            :label "{{f.title + '. ' + f.description}}"
            :width "100%"
            :height 48
            :alignItems center
            :gap 8
            :focusable true
            :cursor hand
            :color "{{state.features.selected == f.name ? '@color.selection' : '@color.sideBar'}}"
            :hover {:color @color.hover}
            :onClick [{:set state.features.selected :to "{{f.name}}"}]
            (label #"{{'feature-icon-' + f.name}}"
              :icon "{{!f.installed ? 'download' : f.enabled ? 'circle-check' : 'circle-pause'}}"
              :width 32
              :height 48
              :align center
              :textColor "{{f.installed && f.enabled ? '@color.accent' : '@color.muted'}}")
            (column #"{{'feature-text-' + f.name}}" :grow 1 :height 40
              (label #"{{'feature-title-' + f.name}}"
                :width "100%"
                :height 20
                :textColor @color.text
                :text "{{f.title + (empty(f.release) ? '' : '  ' + f.release)}}")
              (label #"{{'feature-description-' + f.name}}"
                :width "100%"
                :height 20
                :textColor @color.muted
                :text "{{f.description}}"))
            (node #"{{'feature-end-' + f.name}}" :width 8)))
        (column #features-detail :when "{{computed.featureCurrent != null}}" :width "100%" :gap 4
          (label #features-detail-title
            :width "100%"
            :padding 4
            :textColor @color.text
            :text "{{computed.featureCurrent.title + (computed.featureCurrent.installed ? (computed.featureCurrent.enabled ? '  (enabled)' : '  (disabled)') : '')}}")
          (label #features-detail-description
            :wrap true
            :width "100%"
            :padding 4
            :textColor @color.muted
            :text "{{computed.featureCurrent.description}}")
          (row #features-actions :width "100%" :gap 4
            {:use forgeAgentButton
              :when "{{!computed.featureCurrent.installed}}"
              :with (node #features-install :text Install :icon download :action featuresInstall)}
            {:use forgeAgentButton
              :when "{{computed.featureCurrent.installed && !computed.featureCurrent.enabled}}"
              :with (node #features-enable :text Enable :icon circle-check :action featuresEnable)}
            {:use forgeAgentButton
              :when "{{computed.featureCurrent.installed && computed.featureCurrent.enabled}}"
              :with (node #features-disable :text Disable :icon circle-pause :action featuresDisable :on false)})))}}}
