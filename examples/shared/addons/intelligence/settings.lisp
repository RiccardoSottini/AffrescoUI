;;; settings.lisp --- The intelligence settings, to include: which provider of intelligence (Anthropic, OpenAI, an OpenAI-compatible server, Claude Code) and which model every app's assistant uses
;;;
;;; Put { "use": "intelligenceSettings" } in a container (the AffrescoUI Launcher's settingsIntelligence). It calls
;;; the host's intelligence.* natives (modules/intelligence/include/affresco/intelligence/Intelligence.hpp), which
;;; only the Launcher's host registers; without them it shows stand-in data and can't save. The API key field isn't
;;; bound to the state: what is typed goes to the host's memory (intelligence.stageKey), then to the system's keychain
;;; on save, and the field is emptied. Everything here is named intelligence..., state under state.intelligence. See
;;; README.md.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp")

;;; Definitions

(defdefaults settings "What an app that includes this gives instead, if it gives its own."
  :tokens {:intelligence.accent {:light "#0969da" :dark "#4493f8"}
           :intelligence.accentHover {:light "#0550ae" :dark "#2f81f7"}
           :intelligence.onAccent {:light "#ffffff" :dark "#ffffff"}
           :intelligence.surface {:light "#ffffff" :dark "#161b22"}
           :intelligence.raised {:light "#f6f8fa" :dark "#21262d"}
           :intelligence.selected {:light "#ddf4ff" :dark "#0c2d6b"}
           :intelligence.border {:light "#d0d7de" :dark "#30363d"}
           :intelligence.text {:light "#1f2328" :dark "#e6edf3"}
           :intelligence.dim {:light "#57606a" :dark "#8d96a0"}
           :intelligence.success {:light "#1a7f37" :dark "#3fb950"}
           :intelligence.warning {:light "#9a6700" :dark "#d29922"}
           :intelligence.danger {:light "#cf222e" :dark "#f85149"}}
  :strings {:default en})

(defstate intelligence
  {:native false
   :loaded false
   :selected ""
   :baseUrl ""
   :model ""
   :query ""
   :models []
   :loadingModels false
   :testing false
   :test nil
   :notice ""
   :error ""
   :keyShown true
   :data {:current {:provider "" :model ""}
          :keyStore "Keychain"
          :persistent true
          :providers [{:id anthropic
                       :name "Anthropic (Claude)"
                       :kind anthropic
                       :needsKey true
                       :takesKey true
                       :hasKey false
                       :available true
                       :baseUrl "https://api.anthropic.com"
                       :editableUrl false
                       :model claude-sonnet-5-5
                       :defaultModel claude-sonnet-5-5
                       :fixedModels false
                       :keyHelp "https://console.anthropic.com/settings/keys"
                       :models [{:id claude-opus-5-5 :name "Claude Opus 5.5"}
                                {:id claude-sonnet-5-5 :name "Claude Sonnet 5.5"}
                                {:id claude-haiku-5-5 :name "Claude Haiku 5.5"}
                                {:id claude-fable-5-1 :name "Claude Fable 5.1"}]}
                      {:id openai
                       :name "OpenAI"
                       :kind openai
                       :needsKey true
                       :takesKey true
                       :hasKey false
                       :available true
                       :baseUrl "https://api.openai.com/v1"
                       :editableUrl false
                       :model gpt-5
                       :defaultModel gpt-5
                       :fixedModels false
                       :keyHelp "https://platform.openai.com/api-keys"
                       :models [{:id gpt-5 :name "GPT-5"} {:id gpt-5-mini :name "GPT-5 mini"}]}
                      {:id openai-compatible
                       :name "OpenAI-compatible server"
                       :kind openai-compatible
                       :needsKey false
                       :takesKey true
                       :hasKey false
                       :available true
                       :baseUrl "http://127.0.0.1:11434/v1"
                       :editableUrl true
                       :model ""
                       :defaultModel ""
                       :fixedModels false
                       :keyHelp "https://ollama.com/download"
                       :models []}
                      {:id claude-code
                       :name "Claude Code"
                       :kind claude-code
                       :needsKey false
                       :takesKey false
                       :hasKey false
                       :available true
                       :baseUrl ""
                       :editableUrl false
                       :model sonnet
                       :defaultModel sonnet
                       :fixedModels true
                       :keyHelp "https://claude.com/claude-code"
                       :models [{:id sonnet
                                 :name "Claude Sonnet 5.5"
                                 :description {:en "Fast and strong at editing screens: the best for most asks."
                                               :es "Rápido y bueno editando pantallas: el mejor para casi todo."
                                               :fr "Rapide et doué pour modifier les écrans : le meilleur pour presque tout."}}
                                {:id opus
                                 :name "Claude Opus 5.5"
                                 :description {:en "Deeper reasoning, for harder and longer changes."
                                               :es "Razona más a fondo, para cambios difíciles y largos."
                                               :fr "Raisonne plus en profondeur, pour les changements longs et difficiles."}}
                                {:id fable
                                 :name "Claude Fable 5.1"
                                 :description {:en "The most capable, for the hardest asks; the slowest."
                                               :es "El más capaz, para lo más difícil; el más lento."
                                               :fr "Le plus capable, pour le plus difficile ; le plus lent."}}
                                {:id haiku
                                 :name "Claude Haiku 5.5"
                                 :description {:en "The fastest, for quick small changes."
                                               :es "El más rápido, para cambios pequeños y rápidos."
                                               :fr "Le plus rapide, pour les petits changements rapides."}}]}]}})

(defcomponent intelligenceButton "The intelligenceButton component."
  {:text "" :run "" :primary false :enabled true :width 150}
  `(label#label-text :text ~text
     :width ~width
     :height 32
     :radius 6
     :align center
     :color ~(if primary "@intelligence.accent" "@intelligence.raised")
     :textColor ~(if primary "@intelligence.onAccent" "@intelligence.text")
     :borderWidth ~(if primary 0 1)
     :borderColor @intelligence.border
     :hover {:color ~(if primary "@intelligence.accentHover" "@intelligence.border")}
     :focused {:borderWidth 2 :borderColor @intelligence.accentHover}
     :role button
     :focusable true
     :cursor hand
     :disabled ~(not enabled)
     :onClick [{:run ~run}]))

(defcomponent intelligenceSettings "The intelligenceSettings component."
  {}
  `(node#intelligenceSettings :fill true
     :color transparent
     :scroll vertical
     :dir auto
     :onShow [{:run intelligence.load}]
     :let {:iData ~state.intelligence.data
           :iProvider ~(or-else (find (fn [it] (= it.id state.intelligence.selected))
                                      state.intelligence.data.providers)
                                state.intelligence.data.providers.0)}
     ~(intelligence-column)))

(defpart settings "What settings adds."
  :actions {:intelligence.load [{:note "The first provider until the host says which is in use; without the natives (the rendering check) the call fails and the stand-in data stays. The host's list may leave out the stand-in's first provider: then its own first is chosen, as the form shows it"}
                                {:if ~(= state.intelligence.selected "")
                                 :then [{:run intelligence.choose :with {:id ~state.intelligence.data.providers.0.id}}]}
                                {:call intelligence.providers
                                 :into state.intelligence.data
                                 :onError []
                                 :then [{:set state.intelligence.native :to true}
                                        {:if ~(not state.intelligence.loaded)
                                         :then [{:set state.intelligence.loaded :to true}
                                                {:run intelligence.choose
                                                 :with {:id ~(cond (not= state.intelligence.data.current.provider "") state.intelligence.data.current.provider
                                                                   (some (fn [it] (= it.id state.intelligence.selected))
                                                                         state.intelligence.data.providers) state.intelligence.selected
                                                                   :else (or-else state.intelligence.data.providers.0.id ""))}}]}]}]
            :intelligence.pick [{:run intelligence.choose :with {:id ~id}} {:run intelligence.resetKey}]
            :intelligence.choose [{:set state.intelligence.selected :to ~id}
                                  {:set state.intelligence.baseUrl
                                   :to ~(or-else (:baseUrl (find (fn [it] (= it.id id)) state.intelligence.data.providers)) "")}
                                  {:set state.intelligence.model
                                   :to ~(or-else (:model (find (fn [it] (= it.id id)) state.intelligence.data.providers)) "")}
                                  {:set state.intelligence.query :to ""}
                                  {:set state.intelligence.models :to []}
                                  {:set state.intelligence.test :to nil}
                                  {:set state.intelligence.error :to ""}
                                  {:set state.intelligence.notice :to ""}
                                  {:run intelligence.refresh}]
            :intelligence.refresh [{:if ~(not state.intelligence.native) :then [{:stop true}]}
                                   {:call intelligence.models
                                    :with {:provider ~state.intelligence.selected}
                                    :into state.intelligence.models
                                    :loading state.intelligence.loadingModels
                                    :timeout 20000
                                    :onError [{:set state.intelligence.models :to []}]}]
            :intelligence.pickModel [{:set state.intelligence.model :to ~id} {:set state.intelligence.query :to ""}]
            :intelligence.apply [{:set state.intelligence.error :to ""}
                                 {:set state.intelligence.test :to nil}
                                 {:if ~(not state.intelligence.native)
                                  :then [{:set state.intelligence.error :to ~(t "intelligence.launcherOnly")} {:stop true}]}
                                 {:call intelligence.save
                                  :into state.intelligence.data
                                  :with {:provider ~state.intelligence.selected
                                         :model ~state.intelligence.model
                                         :baseUrl ~state.intelligence.baseUrl}
                                  :then [{:set state.intelligence.notice :to ~(t "intelligence.saved")}
                                         {:run intelligence.resetKey}
                                         {:run intelligence.refresh}]
                                  :onError [{:set state.intelligence.error :to ~error}]}]
            :intelligence.check [{:set state.intelligence.error :to ""}
                                 {:call intelligence.test
                                  :into state.intelligence.test
                                  :loading state.intelligence.testing
                                  :timeout 100000
                                  :onError [{:set state.intelligence.test
                                             :to ~{:ok false
                                                   :message (if state.intelligence.native
                                                                error
                                                                (t "intelligence.launcherOnly"))}}]}]
            :intelligence.removeKey [{:call intelligence.forget
                                      :with {:provider ~state.intelligence.selected}
                                      :into state.intelligence.data
                                      :onError [{:set state.intelligence.error :to ~error}]}
                                     {:run intelligence.resetKey}]
            :intelligence.getKey [{:call intelligence.openKeyHelp :with {:provider ~state.intelligence.selected} :onError []}]
            :intelligence.resetKey [{:note "The key field isn't bound to the state: building it again empties it"}
                                    {:set state.intelligence.keyShown :to false}
                                    {:after 0 :do [{:set state.intelligence.keyShown :to true}]}]})

;;; Views

(defview intelligence-provider "#intelligenceProvider." [provider p]
  `(node :id ~(str "intelligenceProvider" p)
     :height 56
     :radius 8
     :borderWidth 1
     :color ~(if (= provider.id iProvider.id) "@intelligence.selected" "@intelligence.surface")
     :borderColor ~(if (= provider.id iProvider.id) "@intelligence.accent" "@intelligence.border")
     :hover {:borderColor @intelligence.accent}
     :focused {:borderColor @intelligence.accentHover :borderWidth 2}
     :role radio
     :checked ~(= provider.id iProvider.id)
     :focusable true
     :cursor hand
     :label ~provider.name
     :let {:iStatus ~(cond (= provider.id iData.current.provider) "inUse"
                           (not provider.available) "notInstalled"
                           (and provider.needsKey (not provider.hasKey)) "needsKey"
                           provider.hasKey "keySaved"
                           :else "ready")}
     :onClick [{:run intelligence.pick :with {:id ~provider.id}}]
     :keys {:space [{:run intelligence.pick :with {:id ~provider.id}}]}
     (label#label-intelligence-provider-p :anchors {:left 16 :top 9}
       :fitText true
       :textColor @intelligence.text
       :runs [{:text ~provider.name :bold true}])
     (label#label-provider-kind-claude :anchors {:left 16 :top 31}
       :fitText true
       :textColor @intelligence.dim
       :text ~(if (= provider.kind "claude-code") "claude" provider.baseUrl))
     (row#row-intelligence-provider-p :anchors {:right 16 :top 17} :height 22 :fitContent width :gap 6 :alignItems center
       {:id panel-intelligence-provider
        :width 8
        :height 8
        :radius 4
        :color ~(cond (or (= iStatus "inUse") (= iStatus "keySaved") (= iStatus "ready")) "@intelligence.success"
                      (= iStatus "needsKey") "@intelligence.warning"
                      :else "@intelligence.dim")}
       (label#label-t-intelligence-status :fitText true
         :textColor @intelligence.dim
         :text ~(t (str "intelligence.status." iStatus))))))

(defview intelligence-key "#intelligenceKey." []
  `(textarea#intelligenceKey :when ~state.intelligence.keyShown
     :secret true
     :grow 1
     :height 32
     :padding 8
     :radius 6
     :borderWidth 1
     :color @intelligence.raised
     :borderColor @intelligence.border
     :focused {:borderColor @intelligence.accent}
     :textColor @intelligence.text
     :placeholder ~(t "intelligence.keyPlaceholder")
     :placeholderColor @intelligence.dim
     :label ~(t (if iProvider.needsKey "intelligence.key" "intelligence.keyOptional"))
     :maxLength 4096
     :onChange [{:call intelligence.stageKey :with {:provider ~iProvider.id :key ~event.value} :onError []}]
     :onSubmit [{:run intelligence.apply}]))

(defview intelligence-model-search "#intelligenceModelSearch." []
  `(textarea#intelligenceModelSearch :height 32
     :padding 8
     :radius 6
     :anchors {:left 14 :right 14}
     :borderWidth 1
     :color @intelligence.raised
     :borderColor @intelligence.border
     :focused {:borderColor @intelligence.accent}
     :textColor @intelligence.text
     :bind state.intelligence.query
     :maxLength 200
     :label ~(t "intelligence.searchModels")
     :placeholder ~(t "intelligence.searchModels")
     :placeholderColor @intelligence.dim
     :onSubmit [{:if ~(and (> (count (trim state.intelligence.query)) 0)
                           (or (not iProvider.fixedModels)
                               (some (fn [it] (= it.id (trim state.intelligence.query))) iProvider.models)))
                 :then [{:run intelligence.pickModel :with {:id ~(trim state.intelligence.query)}}]}]))

(defview intelligence-model "#intelligenceModel." [model m]
  `(node :id ~(str "intelligenceModel" m)
     :height ~(if (some? model.description) 46 30)
     :radius 6
     :color ~(if (= model.id state.intelligence.model) "@intelligence.selected" "transparent")
     :hover {:borderColor @intelligence.accent :borderWidth 1}
     :role option
     :checked ~(= model.id state.intelligence.model)
     :focusable true
     :cursor hand
     :label ~model.name
     :onClick [{:run intelligence.pickModel :with {:id ~model.id}}]
     :keys {:space [{:run intelligence.pickModel :with {:id ~model.id}}]}
     (label#label-model-name :anchors {:left 8 :top 7} :fitText true :textColor @intelligence.text :text ~model.name)
     (label#label-model-id :when ~(or (not= model.name model.id) (= model.id iProvider.defaultModel))
       :anchors {:right 8 :top 7}
       :fitText true
       :textColor @intelligence.dim
       :text ~(+ (if (= model.id iProvider.defaultModel) (str (t "intelligence.recommended") " · ") "") model.id))
     (label#label-model-description :when ~(some? model.description)
       :anchors {:left 8 :right 8 :top 25}
       :height 16
       :textColor @intelligence.dim
       :text ~(or-else (get model.description (slice locale 0 2)) model.description.en ""))))

(defview intelligence-use-model "#intelligenceUseModel." []
  `(node#intelligenceUseModel :when ~(and (not iProvider.fixedModels)
                                          (> (count (trim state.intelligence.query)) 0)
                                          (not (some (fn [it] (= it.id (trim state.intelligence.query))) iMatches)))
     :height 30
     :radius 6
     :color transparent
     :hover {:borderColor @intelligence.accent :borderWidth 1}
     :role option
     :focusable true
     :cursor hand
     :label ~(t "intelligence.useModel" {:model (trim state.intelligence.query)})
     :onClick [{:run intelligence.pickModel :with {:id ~(trim state.intelligence.query)}}]
     (label#label-t-intelligence-use :anchors {:left 8 :top 7}
       :fitText true
       :textColor @intelligence.accent
       :text ~(t "intelligence.useModel" {:model (trim state.intelligence.query)}))))

(defview intelligence-models "#intelligenceModels." []
  `(column#intelligenceModels :gap 2
     :alignItems stretch
     :fitContent height
     :anchors {:left 14 :right 14}
     :role listbox
     :label ~(t "intelligence.model")
     :let {:iMatches ~(map (fn [it] it.item)
                           (fuzzySort (if (> (count state.intelligence.models) 0)
                                          state.intelligence.models
                                          (or-else iProvider.models []))
                                      (trim state.intelligence.query)
                                      "name"
                                      6))}
     (label#intelligenceModelsLoading :when ~state.intelligence.loadingModels
       :height 28
       :align "start center"
       :textColor @intelligence.dim
       :text ~(t "intelligence.loadingModels"))
     (label#intelligenceNoModels :when ~(and (not state.intelligence.loadingModels)
                                             (= (count iMatches) 0)
                                             (= (count (trim state.intelligence.query)) 0))
       :height 28
       :align "start center"
       :textColor @intelligence.dim
       :text ~(t "intelligence.noModels"))
     ~(repeat [model iMatches :index m]
       (intelligence-model model m))
     ~(intelligence-use-model)))

(defview intelligence-form "#intelligenceForm." []
  `(column#intelligenceForm :gap 10
     :fitContent height
     :color @intelligence.surface
     :radius 8
     :borderWidth 1
     :borderColor @intelligence.border
     (label#intelligenceFormTitle :height 40
       :anchors {:left 14 :right 14}
       :align "start center"
       :textColor @intelligence.text
       :runs [{:text ~iProvider.name :bold true :size 16}])
     (label#intelligenceClaudeNote :when ~(not iProvider.takesKey)
       :wrap true
       :height 40
       :anchors {:left 14 :right 14}
       :textColor @intelligence.dim
       :text ~(t "intelligence.claudeCode"))
     (node#intelligenceInstall :when ~(not iProvider.available)
       :use intelligenceButton
       :anchors {:left 14}
       :with {:text ~(t "intelligence.install") :run intelligence.getKey :width 150})
     (label#intelligenceKeyLabel :when ~iProvider.takesKey
       :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(t (if iProvider.needsKey "intelligence.key" "intelligence.keyOptional")))
     (row#intelligenceKeyRow :when ~iProvider.takesKey :height 32 :gap 8 :anchors {:left 14 :right 14}
       ~(intelligence-key)
       (node#intelligenceGetKey :when ~(= (indexOf iProvider.keyHelp "https://") 0)
         :use intelligenceButton
         :with {:text ~(t "intelligence.getKey") :run intelligence.getKey :width 130}))
     (row#intelligenceKeySaved :when ~(and iProvider.takesKey iProvider.hasKey)
       :height 32
       :gap 8
       :anchors {:left 14 :right 14}
       :alignItems center
       (label#label-t-intelligence-key :grow 1
         :height 32
         :align "start center"
         :textColor @intelligence.success
         :text ~(t "intelligence.keySaved" {:store iData.keyStore}))
       (node#intelligenceRemoveKey :use intelligenceButton
         :with {:text ~(t "intelligence.removeKey") :run intelligence.removeKey :width 140}))
     (label#intelligenceNotKept :when ~(and iProvider.takesKey (not iData.persistent))
       :height 32
       :anchors {:left 14 :right 14}
       :align "start center"
       :wrap true
       :textColor @intelligence.danger
       :text ~(t "intelligence.notKept"))
     (label#intelligenceBaseUrlLabel :when ~iProvider.editableUrl
       :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(t "intelligence.baseUrl"))
     (textarea#intelligenceBaseUrl :when ~iProvider.editableUrl
       :height 32
       :padding 8
       :radius 6
       :anchors {:left 14 :right 14}
       :borderWidth 1
       :color @intelligence.raised
       :borderColor @intelligence.border
       :focused {:borderColor @intelligence.accent}
       :textColor @intelligence.text
       :bind state.intelligence.baseUrl
       :maxLength 512
       :label ~(t "intelligence.baseUrl")
       :placeholder ~(t "intelligence.baseUrlHelp")
       :placeholderColor @intelligence.dim)
     (label#intelligenceModelLabel :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(if (not= state.intelligence.model "")
                  (t "intelligence.modelChosen"
                     {:model (or-else (:name (find (fn [it] (= it.id state.intelligence.model))
                                                   (or-else iProvider.models [])))
                                      state.intelligence.model)})
                  (t "intelligence.model")))
     ~(intelligence-model-search)
     ~(intelligence-models)
     (row#intelligenceActions :height 32 :gap 8 :anchors {:left 14 :right 14}
       (node#intelligenceSave :use intelligenceButton
         :with {:text ~(t "intelligence.save")
                :run intelligence.apply
                :primary true
                :enabled ~(and (not= state.intelligence.model "") iProvider.available)
                :width 170})
       (node#intelligenceTest :use intelligenceButton
         :with {:text ~(t (if state.intelligence.testing "intelligence.testing" "intelligence.test"))
                :run intelligence.check
                :enabled ~(and (not= iData.current.provider "") (not state.intelligence.testing))
                :width 170})
       (node#intelligenceRefresh :when ~(not= iProvider.kind "claude-code")
         :use intelligenceButton
         :with {:text ~(t "intelligence.refresh") :run intelligence.refresh :width 170}))
     (label#intelligenceFeedback :role status
       :live polite
       :wrap true
       :height 40
       :anchors {:left 14 :right 14}
       :textColor ~(if (or (not= state.intelligence.error "")
                           (and (some? state.intelligence.test) (not state.intelligence.test.ok)))
                       "@intelligence.danger"
                       "@intelligence.success")
       :text ~(cond (not= state.intelligence.error "") state.intelligence.error
                    (some? state.intelligence.test) state.intelligence.test.message
                    :else state.intelligence.notice))))

(defview intelligence-column "#intelligenceColumn." []
  `(column#intelligenceColumn :gap 14
     :alignItems stretch
     :fitContent height
     :anchors {:left 24 :right 24 :top 24}
     :maxWidth 720
     (label#intelligenceTitle :role heading
       :height 28
       :textColor @intelligence.text
       :runs [{:text ~(t "intelligence.title") :bold true :size 20}])
     (label#intelligenceIntro :wrap true :height 40 :textColor @intelligence.dim :text ~(t "intelligence.intro"))
     (label#intelligenceCurrent :role status
       :live polite
       :height 22
       :textColor ~(if (not= iData.current.provider "") "@intelligence.success" "@intelligence.warning")
       :text ~(if (not= iData.current.provider "")
                  (t "intelligence.inUse"
                     {:provider (or-else (:name (find (fn [it] (= it.id iData.current.provider)) iData.providers))
                                         iData.current.provider)
                      :model iData.current.model})
                  (t "intelligence.none")))
     (label#intelligenceProvidersTitle :height 20
       :textColor @intelligence.text
       :runs [{:text ~(t "intelligence.providers") :bold true}])
     (column#intelligenceProviders :gap 8
       :alignItems stretch
       :fitContent height
       :role radiogroup
       :label ~(t "intelligence.providers")
       ~(repeat [provider iData.providers :index p]
         (intelligence-provider provider p)))
     ~(intelligence-form)
     {:id panel-intelligence-column :height 24 :color transparent}))
