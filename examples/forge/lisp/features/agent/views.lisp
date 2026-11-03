;;; views.lisp --- the agent pane: the conversation, the proposals to review; and the Features view

;;; The pane

(defcomponent forgeAgentView "The agent pane: its status, the conversation, the proposals to review, and the request field."
  {}
  `(column#agent-view :width "100%" :grow 1 :color @color.sideBar :gap 4
     (label#agent-title :text ~(t "agent-title") :textColor @color.muted :width "100%" :height 32 :padding 12)
     (label#agent-status :icon ~(if state.agent.connected "plug" "unplug")
       :text ~(if (empty? state.agent.status) (t "agent.notConnected") state.agent.status)
       :wrap true
       :width "100%"
       :height 36
       :padding 8
       :textColor @color.muted)
     ~(agent-empty)
     (list#agent-log :when ~(> (count state.agent.log) 0)
       :width "100%"
       :grow 2
       :items ~computed.agentLogRows
       :selection none
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :accentColor @color.accent)
     ~(agent-draft)
     ~(agent-ask-row)
     (label#agent-review-title :text ~(t "agent-review-title" {:n (count proposals)})
       :textColor @color.muted
       :width "100%"
       :height 22
       :padding 8)
     (label#agent-review-empty :when ~(= (count proposals) 0)
       :wrap true
       :width "100%"
       :height 44
       :padding 8
       :textColor @color.muted
       :text ~(t "agent-review-empty"))
     ~(agent-proposals)
     (node :use forgeAgentReview :when ~(some? computed.agentCurrent))))

(defview agent-empty "Before the first request: what the pane does, or how an agent joins." []
  `(column#agent-empty :when ~(= (count state.agent.log) 0) :width "100%" :grow 2 :alignItems center :justify center :gap 8
     (label#agent-empty-icon :icon bot :font iconsLarge :textColor @color.accent :fitText true)
     (label#agent-empty-title :text ~(t "agent-empty-title") :textColor @color.text :fitText true)
     (label#agent-empty-text :wrap true
       :width "100%"
       :height 72
       :padding 12
       :align center
       :textColor @color.muted
       :text ~(if state.agent.connected
                  (t "agent-empty.connected")
                  (t "agent-empty.notConnected")))))

(defview agent-proposals "The proposals waiting for review: one is chosen." []
  `(list#agent-proposals :when ~(> (count proposals) 0)
     :width "100%"
     :grow 1
     :items ~computed.agentProposalRows
     :bind state.agent.selected
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen agentOpenChanges))

(defview agent-ask-row "The request field and its Ask button." []
  `(row#agent-ask-row :width "100%" :height 30 :justify end
     (node :use forgeAgentButton
       :with {:id agent-ask
              :text ~(t "agentView.ask")
              :icon send
              :action agentAsk
              :on ~(not (empty? (trim state.agent.draft)))})))

(defview agent-draft "The request, typed: Enter sends it." []
  `(textarea#agent-draft :width "100%"
     :height 52
     :padding 6
     :wrap true
     :color @color.input
     :textColor @color.text
     :placeholderColor @color.muted
     :bind state.agent.draft
     :placeholder ~(t "agent-draft.placeholder")
     :onSubmit agentAsk))

(defcomponent forgeAgentButton "A button of the pane: an icon and a text, which runs `action`; accented while `on`."
  {:id "" :text "" :icon "" :action "" :on true}
  `(label :id ~id
     :icon ~icon
     :text ~text
     :height 26
     :padding 8
     :fitText true
     :align center
     :radius 2
     :cursor hand
     :focusable true
     :role button
     :textColor ~(if on "#ffffff" "@color.muted")
     :color ~(if on "@color.accent" "@color.border")
     :onClick {:run ~action}))

;;; Reviewing a proposal

(defcomponent forgeAgentReview "The proposal chosen: its message, its changes, its evidence, and Preview, Accept and Reject."
  {}
  `(column#agent-review :width "100%" :grow 2 :gap 4
     (label#agent-proposal-message :wrap true
       :width "100%"
       :padding 8
       :textColor @color.text
       :text ~(t "agent-proposal-message" {:message computed.agentCurrent.message
                                            :n computed.agentCurrent.operations
                                            :author computed.agentCurrent.author}))
     (list#agent-changes :width "100%"
       :grow 1
       :items ~computed.agentChangeRows
       :selection none
       :rowHeight 20
       :font code
       :color @color.sideBar
       :textColor @color.text
       :accentColor @color.accent)
     (label#agent-evidence-none :when ~(nil? computed.agentEvidence)
       :wrap true
       :width "100%"
       :padding 8
       :textColor @color.muted
       :text ~(t "agent-evidence-none"))
     ~(agent-evidence-behaviour)
     ~(agent-evidence-shots)
     (list#agent-scenarios :when ~(> (count computed.agentScenarioRows) 0)
       :width "100%"
       :height 88
       :items ~computed.agentScenarioRows
       :selection none
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :accentColor @color.accent)
     ~(agent-decide)))

(defview agent-evidence-behaviour "What the agent says its change does." []
  `(label#agent-evidence-behaviour :when ~(and (some? computed.agentEvidence)
                                               (not (empty? computed.agentEvidence.behaviour)))
     :icon activity
     :wrap true
     :width "100%"
     :padding 8
     :textColor @color.text
     :text ~(slice (text computed.agentEvidence.behaviour) 0 2000)))

(defview agent-evidence-shots "The screenshots before and after the change, side by side." []
  `(row#agent-evidence-shots :when ~(and (some? computed.agentEvidence)
                                         (= (type computed.agentEvidence.before) "string")
                                         (= (type computed.agentEvidence.after) "string"))
     :width "100%"
     :height 120
     :gap 4
     (image#agent-evidence-before :source ~computed.agentEvidence.before
       :grow 1
       :height "100%"
       :fit contain
       :label ~(t "agent-evidence-before.label"))
     (image#agent-evidence-after :source ~computed.agentEvidence.after
       :grow 1
       :height "100%"
       :fit contain
       :label ~(t "agent-evidence-after.label"))))

(defview agent-decide "Preview, Accept and Reject; Reject asks once before it rejects." []
  `(row#agent-decide :width "100%" :gap 4
     (node :use forgeAgentButton
       :with {:id agent-preview
              :action agentPreview
              :on false
              :icon ~(if (= computed.agentCurrent.id preview) "eye-off" "eye")
              :text ~(if (= computed.agentCurrent.id preview) (t "agent-preview.end") (t "agent-preview"))})
     (node :use forgeAgentButton :with {:id agent-accept :text ~(t "agentReview.accept") :icon check :action agentAccept})
     (node :use forgeAgentButton
       :with {:id agent-reject
              :icon x
              :action agentReject
              :on false
              :text ~(if (= state.agent.rejecting computed.agentCurrent.id) (t "agent-reject.confirm") (t "agent-reject"))})))

;;; Features

(defcomponent forgeFeaturesView "The Features view: the catalogue, searchable, and the feature chosen."
  {}
  `(column#features-view :width "100%" :grow 1 :color @color.sideBar :gap 4
     (row#features-head :width "100%" :height 35 :alignItems center
       (label#features-title :text ~(t "features-title") :textColor @color.muted :grow 1 :height 35 :padding 12)
       (label#features-refresh :icon refresh-cw
         :padding 6
         :fitText true
         :cursor hand
         :textColor ~(if state.features.busy "@color.accent" "@color.muted")
         :tooltip ~(t "features-refresh.tip")
         :hover {:textColor @color.text}
         :onClick featuresRefresh))
     (textarea#features-filter :width "100%"
       :height 26
       :padding 6
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :bind state.features.filter
       :placeholder ~(t "features-filter.placeholder"))
     (label#features-error :when ~(not (empty? state.features.error))
       :icon circle-x
       :wrap true
       :width "100%"
       :padding 8
       :textColor @color.text
       :text ~state.features.error)
     (label#features-empty :when ~(and (= (count state.features.catalog) 0) (empty? state.features.error))
       :wrap true
       :width "100%"
       :padding 8
       :textColor @color.muted
       :text ~(t "features-empty"))
     (column#features-list :role listbox :label ~(t "features-list.label") :width "100%" :grow 1 :scroll vertical
       ~(repeat [f computed.featureCards :index fi :key f.name]
         (feature-row f)))
     ~(features-detail)))

(defview feature-row "A feature of the catalogue: its icon says whether it is installed and on; a click chooses it." [f]
  `(row :id ~(str "feature-" f.name)
     :role option
     :label ~(str f.title ". " f.description)
     :width "100%"
     :height 48
     :alignItems center
     :gap 8
     :focusable true
     :cursor hand
     :color ~(if (= state.features.selected f.name) "@color.selection" "@color.sideBar")
     :hover {:color @color.hover}
     :onClick ~(handler featuresChoose f.name)
     (label :id ~(str "feature-icon-" f.name)
       :icon ~(cond (not f.installed) "download" f.enabled "circle-check" :else "circle-pause")
       :width 32
       :height 48
       :align center
       :textColor ~(if (and f.installed f.enabled) "@color.accent" "@color.muted"))
     ~(feature-text f)
     (node :id ~(str "feature-end-" f.name) :width 8)))

(defview feature-text "A feature's title and release, over its description." [f]
  `(column :id ~(str "feature-text-" f.name) :grow 1 :height 40
     (label :id ~(str "feature-title-" f.name)
       :width "100%"
       :height 20
       :textColor @color.text
       :text ~(+ f.title (if (empty? f.release) "" (str "  " f.release))))
     (label :id ~(str "feature-description-" f.name) :width "100%" :height 20 :textColor @color.muted :text ~f.description)))

(defview features-detail "The feature chosen: its title, its description, and Install, Enable or Disable." []
  `(column#features-detail :when ~(some? computed.featureCurrent) :width "100%" :gap 4
     (label#features-detail-title :width "100%"
       :padding 4
       :textColor @color.text
       :text ~(str computed.featureCurrent.title
                   (if computed.featureCurrent.installed
                       (if computed.featureCurrent.enabled (t "features.enabled") (t "features.disabled"))
                       "")))
     (label#features-detail-description :wrap true
       :width "100%"
       :padding 4
       :textColor @color.muted
       :text ~computed.featureCurrent.description)
     (row#features-actions :width "100%" :gap 4
       (node :use forgeAgentButton
         :when ~(not computed.featureCurrent.installed)
         :with {:id features-install :text ~(t "featuresView.install") :icon download :action featuresInstall})
       (node :use forgeAgentButton
         :when ~(and computed.featureCurrent.installed (not computed.featureCurrent.enabled))
         :with {:id features-enable :text ~(t "featuresView.enable") :icon circle-check :action featuresEnable})
       (node :use forgeAgentButton
         :when ~(and computed.featureCurrent.installed computed.featureCurrent.enabled)
         :with {:id features-disable :text ~(t "featuresView.disable") :icon circle-pause :action featuresDisable :on false}))))
