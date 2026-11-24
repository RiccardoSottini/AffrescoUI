;;; settings.lisp --- The intelligence settings, to include: which provider of intelligence (Claude Code, GitHub
;;; Copilot, or a local LLM on this computer: Ollama, LM Studio or llama.cpp) and which model every app's assistant uses
;;;
;;; Put { "use": "intelligenceSettings" } in a container (the AffrescoUI Launcher's settingsIntelligence). It calls
;;; the host's intelligence.* natives (modules/intelligence/include/affresco/intelligence/Intelligence.hpp), which
;;; only the Launcher's host registers; without them it shows stand-in data and can't save. The API key field isn't
;;; bound to the state: what is typed goes to the host's memory (intelligence.stageKey), then to the system's keychain
;;; on save, and the field is emptied. Everything here is named intelligence..., state under state.intelligence. See
;;; README.md.

(ns shared.addons.intelligence.settings
  (:require "locales/en.lisp"
            "locales/es.lisp"
            "locales/fr.lisp"))

;;; Definitions

(defdefaults settings "The colours and the default language an app that includes this replaces with its own."
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
   :modelsError ""
   :modelsLoaded false
   :data {:current {:provider "" :model ""}
          :keyStore "Keychain"
          :persistent true
          :providers [{:id claude-code
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
                                               :fr "Le plus rapide, pour les petits changements rapides."}}]}
                      {:id copilot
                       :name "GitHub Copilot"
                       :kind copilot
                       :needsKey false
                       :takesKey false
                       :hasKey false
                       :available true
                       :baseUrl ""
                       :editableUrl false
                       :model "claude-sonnet-5.5"
                       :defaultModel "claude-sonnet-5.5"
                       :fixedModels true
                       :keyHelp "https://docs.github.com/en/copilot/how-tos/copilot-cli/set-up-copilot-cli/install-copilot-cli"
                       :models [{:id "claude-sonnet-5.5"
                                 :name "Claude Sonnet 5.5"
                                 :description {:en "Fast and strong at editing screens: the best for most asks."
                                               :es "Rápido y bueno editando pantallas: el mejor para casi todo."
                                               :fr "Rapide et doué pour modifier les écrans : le meilleur pour presque tout."}}
                                {:id "claude-opus-5.5"
                                 :name "Claude Opus 5.5"
                                 :description {:en "Deeper reasoning, for harder and longer changes."
                                               :es "Razona más a fondo, para cambios difíciles y largos."
                                               :fr "Raisonne plus en profondeur, pour les changements longs et difficiles."}}
                                {:id "gpt-6.2-sol"
                                 :name "GPT-6.2 Sol"
                                 :description {:en "OpenAI's model, for those who prefer GPT."
                                               :es "El modelo de OpenAI, para quien prefiera GPT."
                                               :fr "Le modèle d'OpenAI, pour qui préfère GPT."}}
                                {:id "claude-haiku-5.5"
                                 :name "Claude Haiku 5.5"
                                 :description {:en "The fastest, for quick small changes."
                                               :es "El más rápido, para cambios pequeños y rápidos."
                                               :fr "Le plus rapide, pour les petits changements rapides."}}
                                {:id "auto"
                                 :name "Auto"
                                 :description {:en "Copilot picks the model for each ask, among those your plan allows."
                                               :es "Copilot elige el modelo de cada petición, entre los que permite tu plan."
                                               :fr "Copilot choisit le modèle de chaque demande, parmi ceux que permet votre forfait."}}]}
                      {:id local
                       :name "Local LLM"
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
                       :keyHelp ""
                       :presets [{:id ollama :name "Ollama" :baseUrl "http://127.0.0.1:11434/v1"}
                                 {:id lm-studio :name "LM Studio" :baseUrl "http://127.0.0.1:1234/v1"}
                                 {:id llama-server :name "llama.cpp" :baseUrl "http://127.0.0.1:8080/v1"}]
                       :models []}]}})

(defcomponent intelligenceButton "A button of the settings: filled when it is the main one."
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

(defcomponent intelligenceSettings
  "The provider of intelligence the Launcher's Settings choose: the provider, its key and its model."
  {}
  `(node#intelligenceSettings :fill true
     :color transparent
     :scroll vertical
     :dir auto
     :onShow intelligence-load
     :let {:iData ~state.intelligence.data
           :iProvider ~(or-else (find #(= (:id %) state.intelligence.selected) state.intelligence.data.providers)
                                state.intelligence.data.providers.0)}
     ~(intelligence-column)))

;;; What it does: each a function, run by its action's name where a button or an app names one

(defn intelligence-provider-of "The provider of that id, of the host's list." [id]
  (find #(= (:id %) id) state.intelligence.data.providers))

(defn intelligence-refresh
  "Asks the host for the chosen provider's models (only the host has them), from the server the form shows: an
editable base URL is asked before it is saved."
  []
  (when state.intelligence.native
    (set "state.intelligence.modelsError" "")
    (set "state.intelligence.modelsLoaded" false)
    (call "intelligence.models"
          :with {:provider state.intelligence.selected :baseUrl state.intelligence.baseUrl}
          :into "state.intelligence.models"
          :loading "state.intelligence.loadingModels"
          :timeout 20000
          :then intelligence-models-came
          :onError intelligence-no-models)))

(defn intelligence-models-came "The server's models came, perhaps none." [] (set "state.intelligence.modelsLoaded" true))

(defn intelligence-no-models "No models to choose from: the server didn't answer, and why." [error]
  (set "state.intelligence.models" [])
  (set "state.intelligence.modelsError" (or error "")))

(defn intelligence-use-preset "Points the base URL at a local server, and asks it for its models." [url]
  (set "state.intelligence.baseUrl" url)
  (intelligence-refresh))

(defn intelligence-choose "Shows a provider's form: its address and model, its models asked for." [id]
  (let [{:keys [baseUrl model]} (intelligence-provider-of id)]
    (set "state.intelligence.selected" id)
    (set "state.intelligence.baseUrl" (or baseUrl ""))
    (set "state.intelligence.model" (or model "")))
  (set "state.intelligence.query" "")
  (set "state.intelligence.models" [])
  (set "state.intelligence.modelsError" "")
  (set "state.intelligence.modelsLoaded" false)
  (set "state.intelligence.test" nil)
  (set "state.intelligence.error" "")
  (set "state.intelligence.notice" "")
  (intelligence-refresh))

(defn intelligence-key-shown "The key field again, empty." [] (set "state.intelligence.keyShown" true))

(defn intelligence-reset-key "Empties the key field: it isn't bound to the state, so building it again empties it." []
  (set "state.intelligence.keyShown" false)
  (after 0 intelligence-key-shown))

(defn intelligence-loaded
  "The host's providers came: the one in use is chosen the first time, or else the one chosen, or else the first."
  []
  (set "state.intelligence.native" true)
  (when-not state.intelligence.loaded
    (set "state.intelligence.loaded" true)
    (intelligence-choose
     (cond (not= state.intelligence.data.current.provider "") state.intelligence.data.current.provider
           (some? (intelligence-provider-of state.intelligence.selected)) state.intelligence.selected
           :else (or (:id (first state.intelligence.data.providers)) "")))))

(defn intelligence-ignored "Nothing: without the host's natives (the rendering check), the stand-in data stays." [])

(defn intelligence-load
  "The first provider until the host says which is in use; then the host's list, which may leave out the stand-in's
first provider: then its own first is chosen, as the form shows it."
  []
  (when (= state.intelligence.selected "")
    (intelligence-choose (:id (first state.intelligence.data.providers))))
  (call "intelligence.providers"
        :into "state.intelligence.data"
        :then intelligence-loaded
        :onError intelligence-ignored))

(defn intelligence-pick "Chooses a provider, its key field emptied." [id]
  (intelligence-choose id)
  (intelligence-reset-key))

(defn intelligence-pick-model "Chooses a model." [id]
  (set "state.intelligence.model" id)
  (set "state.intelligence.query" ""))

(defn intelligence-failed "Says what the host said went wrong." [error] (set "state.intelligence.error" error))

(defn intelligence-saved "Says the provider is kept, and asks for its models again." []
  (set "state.intelligence.notice" (t "intelligence.saved"))
  (intelligence-reset-key)
  (intelligence-refresh))

(defn intelligence-apply "Keeps the provider, its model and its address, as every app's assistant uses them." []
  (set "state.intelligence.error" "")
  (set "state.intelligence.test" nil)
  (if-not state.intelligence.native
    (set "state.intelligence.error" (t "intelligence.launcherOnly"))
    (call "intelligence.save"
          :into "state.intelligence.data"
          :with {:provider state.intelligence.selected
                 :model state.intelligence.model
                 :baseUrl state.intelligence.baseUrl}
          :then intelligence-saved
          :onError intelligence-failed)))

(defn intelligence-not-checked "The check couldn't run: why, or that only the Launcher can check." [error]
  (set "state.intelligence.test"
       {:ok false :message (if state.intelligence.native error (t "intelligence.launcherOnly"))}))

(defn intelligence-check "Asks the provider something small, to see that it answers." []
  (set "state.intelligence.error" "")
  (call "intelligence.test"
        :into "state.intelligence.test"
        :loading "state.intelligence.testing"
        :timeout 100000
        :onError intelligence-not-checked))

(defn intelligence-remove-key "Forgets the provider's key, in the system's keychain." []
  (call "intelligence.forget"
        :with {:provider state.intelligence.selected}
        :into "state.intelligence.data"
        :onError intelligence-failed)
  (intelligence-reset-key))

(defn intelligence-get-key "Opens the provider's page for a key, or how to install it." []
  (call "intelligence.openKeyHelp" :with {:provider state.intelligence.selected} :onError intelligence-ignored))

(defpart settings "The settings' actions, each a function, by the names buttons and apps run."
  :actions {:intelligence.load [{:lisp intelligence-load}]
            :intelligence.pick [{:lisp intelligence-pick}]
            :intelligence.choose [{:lisp intelligence-choose}]
            :intelligence.refresh [{:lisp intelligence-refresh}]
            :intelligence.preset [{:lisp intelligence-use-preset}]
            :intelligence.pickModel [{:lisp intelligence-pick-model}]
            :intelligence.apply [{:lisp intelligence-apply}]
            :intelligence.check [{:lisp intelligence-check}]
            :intelligence.removeKey [{:lisp intelligence-remove-key}]
            :intelligence.getKey [{:lisp intelligence-get-key}]
            :intelligence.resetKey [{:lisp intelligence-reset-key}]})

;;; Views

(defview intelligence-provider "A provider to choose, as a row." [provider p]
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
     :onClick ~(handler intelligence-pick provider.id)
     :keys {:space ~(handler intelligence-pick provider.id)}
     (label#label-intelligence-provider-p :anchors {:left 16 :top 9}
       :fitText true
       :textColor @intelligence.text
       :runs [{:text ~provider.name :bold true}])
     (label#label-provider-kind-claude :anchors {:left 16 :top 31}
       :fitText true
       :textColor @intelligence.dim
       :text ~(case provider.kind :claude-code "claude" :copilot "copilot" provider.baseUrl))
     (row#row-intelligence-provider-p :anchors {:right 16 :top 17} :height 22 :fitContent width :gap 6 :alignItems center
       (node#panel-intelligence-provider :width 8
         :height 8
         :radius 4
         :color ~(case iStatus
                   (:inUse :keySaved :ready) "@intelligence.success"
                   :needsKey "@intelligence.warning"
                   "@intelligence.dim"))
       (label#label-t-intelligence-status :fitText true
         :textColor @intelligence.dim
         :text ~(t (str "intelligence.status." iStatus))))))

(defview intelligence-key "The provider's key, hidden as it is typed." []
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
     :onSubmit intelligence-apply))

(defview intelligence-model-search "The field that searches the provider's models." []
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
     :onSubmit [{:if ~(let [query (str/trim state.intelligence.query)]
                        (and (pos? (count query))
                             (or (not iProvider.fixedModels) (some #(= (:id %) query) iProvider.models))))
                 :then [{:run intelligence.pickModel :with {:id ~(str/trim state.intelligence.query)}}]}]))

(defview intelligence-model "A model to choose, as a row." [model m]
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
     :onClick ~(handler intelligence-pick-model model.id)
     :keys {:space ~(handler intelligence-pick-model model.id)}
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

(defview intelligence-use-model "The button that uses the model chosen." []
  `(node#intelligenceUseModel :when ~(let [query (str/trim state.intelligence.query)]
                                       (and (not iProvider.fixedModels)
                                            (pos? (count query))
                                            (not (some #(= (:id %) query) iMatches))))
     :height 30
     :radius 6
     :color transparent
     :hover {:borderColor @intelligence.accent :borderWidth 1}
     :role option
     :focusable true
     :cursor hand
     :label ~(t "intelligence.useModel" {:model (str/trim state.intelligence.query)})
     :onClick ~(handler intelligence-pick-model (str/trim state.intelligence.query))
     (label#label-t-intelligence-use :anchors {:left 8 :top 7}
       :fitText true
       :textColor @intelligence.accent
       :text ~(t "intelligence.useModel" {:model (str/trim state.intelligence.query)}))))

(defview intelligence-models "The provider's models, searched." []
  `(column#intelligenceModels :gap 2
     :alignItems stretch
     :fitContent height
     :anchors {:left 14 :right 14}
     :role listbox
     :label ~(t "intelligence.model")
     :let {:iMatches ~(map :item
                           (fuzzySort (if (pos? (count state.intelligence.models))
                                        state.intelligence.models
                                        (or-else iProvider.models []))
                                      (str/trim state.intelligence.query)
                                      "name"
                                      6))}
     (label#intelligenceModelsLoading :when ~state.intelligence.loadingModels
       :height 28
       :align "start center"
       :textColor @intelligence.dim
       :text ~(t "intelligence.loadingModels"))
     (label#intelligenceNoServer :when ~(and (not state.intelligence.loadingModels)
                                             (not= state.intelligence.modelsError ""))
       :wrap true
       :height 40
       :textColor @intelligence.warning
       :text ~(t "intelligence.noServer" {:url state.intelligence.baseUrl}))
     (label#intelligenceNoServerWhy :when ~(and (not state.intelligence.loadingModels)
                                                (not= state.intelligence.modelsError ""))
       :wrap true
       :height 20
       :textColor @intelligence.dim
       :text ~state.intelligence.modelsError)
     (label#intelligenceNoModelLoaded :when ~(and (not state.intelligence.loadingModels)
                                                  state.intelligence.modelsLoaded
                                                  (empty? state.intelligence.models)
                                                  (empty? (or-else iProvider.models [])))
       :wrap true
       :height 40
       :textColor @intelligence.warning
       :text ~(t "intelligence.noModelLoaded"))
     (label#intelligenceNoModels :when ~(and (not state.intelligence.loadingModels)
                                             (empty? iMatches)
                                             (empty? (str/trim state.intelligence.query))
                                             (= state.intelligence.modelsError "")
                                             (not (and state.intelligence.modelsLoaded
                                                       (empty? (or-else iProvider.models [])))))
       :height 28
       :align "start center"
       :textColor @intelligence.dim
       :text ~(t "intelligence.noModels"))
     ~(repeat [model iMatches :index m]
       (intelligence-model model m))
     ~(intelligence-use-model)))

(defview intelligence-form "The provider's form: its server and address, its key and its model." []
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
     ~(intelligence-no-key-note)
     ~(intelligence-local-note)
     (node#intelligenceInstall :when ~(not iProvider.available)
       :use intelligenceButton
       :anchors {:left 14}
       :with {:text ~(t "intelligence.install") :run intelligence.getKey :width 150})
     (label#intelligenceServerLabel :when ~(pos? (count (or-else iProvider.presets [])))
       :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(t "intelligence.server"))
     ~(intelligence-presets)
     (label#intelligenceBaseUrlLabel :when ~iProvider.editableUrl
       :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(t "intelligence.baseUrl"))
     ~(intelligence-base-url)
     (label#intelligenceKeyLabel :when ~iProvider.takesKey
       :height 18
       :anchors {:left 14 :right 14}
       :textColor @intelligence.text
       :text ~(t (if iProvider.needsKey "intelligence.key" "intelligence.keyOptional")))
     ~(intelligence-key-row)
     ~(intelligence-key-saved)
     ~(intelligence-not-kept)
     ~(intelligence-model-label)
     ~(intelligence-model-search)
     ~(intelligence-models)
     ~(intelligence-actions)
     ~(intelligence-feedback)))

(defview intelligence-no-key-note "For a provider that takes no key, a command line: it uses its own sign-in, with no tools." []
  `(label#intelligenceClaudeNote :when ~(not iProvider.takesKey)
     :wrap true
     :height 40
     :anchors {:left 14 :right 14}
     :textColor @intelligence.dim
     :text ~(t (if (= iProvider.kind "copilot") "intelligence.copilot" "intelligence.claudeCode"))))

(defview intelligence-local-note "For a local model: nothing leaves the computer, and what a model needs." []
  `(label#intelligenceLocalNote :when ~(pos? (count (or-else iProvider.presets [])))
     :wrap true
     :height 40
     :anchors {:left 14 :right 14}
     :textColor @intelligence.dim
     :text ~(t "intelligence.localNote")))

(defview intelligence-preset "A local server to choose, as a button that fills the base URL." [preset s]
  `(label :id ~(str "intelligencePreset" s)
     :text ~preset.name
     :width 130
     :height 32
     :radius 6
     :align center
     :color ~(if (= preset.baseUrl state.intelligence.baseUrl) "@intelligence.selected" "@intelligence.raised")
     :textColor @intelligence.text
     :borderWidth 1
     :borderColor ~(if (= preset.baseUrl state.intelligence.baseUrl) "@intelligence.accent" "@intelligence.border")
     :hover {:borderColor @intelligence.accent}
     :focused {:borderWidth 2 :borderColor @intelligence.accentHover}
     :role radio
     :checked ~(= preset.baseUrl state.intelligence.baseUrl)
     :focusable true
     :cursor hand
     :onClick ~(handler intelligence-use-preset preset.baseUrl)
     :keys {:space ~(handler intelligence-use-preset preset.baseUrl)}))

(defview intelligence-presets "The local servers (Ollama, LM Studio, llama.cpp), each filling the base URL." []
  `(row#intelligencePresets :when ~(pos? (count (or-else iProvider.presets [])))
     :height 32
     :gap 8
     :anchors {:left 14 :right 14}
     :role radiogroup
     :label ~(t "intelligence.server")
     ~(repeat [preset (or-else iProvider.presets []) :index s]
       (intelligence-preset preset s))))

(defview intelligence-key-row "The key's field, and a link to get a key where the provider gives one." []
  `(row#intelligenceKeyRow :when ~iProvider.takesKey :height 32 :gap 8 :anchors {:left 14 :right 14}
     ~(intelligence-key)
     (node#intelligenceGetKey :when ~(zero? (indexOf iProvider.keyHelp "https://"))
       :use intelligenceButton
       :with {:text ~(t "intelligence.getKey") :run intelligence.getKey :width 130})))

(defview intelligence-key-saved "That a key is kept, where, and a button that removes it." []
  `(row#intelligenceKeySaved :when ~(and iProvider.takesKey iProvider.hasKey)
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
       :with {:text ~(t "intelligence.removeKey") :run intelligence.removeKey :width 140})))

(defview intelligence-not-kept "A warning that the key can't be kept on this computer." []
  `(label#intelligenceNotKept :when ~(and iProvider.takesKey (not iData.persistent))
     :height 32
     :anchors {:left 14 :right 14}
     :align "start center"
     :wrap true
     :textColor @intelligence.danger
     :text ~(t "intelligence.notKept")))

(defview intelligence-base-url "The provider's address, for one whose address can be changed." []
  `(textarea#intelligenceBaseUrl :when ~iProvider.editableUrl
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
     :placeholderColor @intelligence.dim))

(defview intelligence-model-label "The model's heading: the model chosen, by its name." []
  `(label#intelligenceModelLabel :height 18
     :anchors {:left 14 :right 14}
     :textColor @intelligence.text
     :text ~(if (not= state.intelligence.model "")
                (t "intelligence.modelChosen"
                   {:model (or-else (:name (find #(= (:id %) state.intelligence.model) (or-else iProvider.models [])))
                                    state.intelligence.model)})
                (t "intelligence.model"))))

(defview intelligence-actions "Save, Test and Refresh the models." []
  `(row#intelligenceActions :height 32 :gap 8 :anchors {:left 14 :right 14}
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
     (node#intelligenceRefresh :when ~(and (not= iProvider.kind "claude-code") (not= iProvider.kind "copilot"))
       :use intelligenceButton
       :with {:text ~(t "intelligence.refresh") :run intelligence.refresh :width 170})))

(defview intelligence-feedback "What the last action said: an error, the test's result or a notice." []
  `(label#intelligenceFeedback :role status
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
                  :else state.intelligence.notice)))

(defview intelligence-column "The settings' column: the providers, then the form." []
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
                     {:provider (or-else (:name (find #(= (:id %) iData.current.provider) iData.providers))
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
     (node#panel-intelligence-column :height 24 :color transparent)))
