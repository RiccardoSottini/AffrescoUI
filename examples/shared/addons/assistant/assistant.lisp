;;; assistant.lisp --- The assistant add-on: a chat with the agents connected to the app, to include
;;;
;;; A launcher at the bottom right opens a panel where the person asks in plain words (the ask step), the agents
;;; answer (ask:reply), and the proposals they open come back as cards to preview, accept or reject, as does every
;;; other proposal waiting for review: this is where the person reviews. Everything here is named assistant..., so it
;;; merges into any screen; an app restyles it by overriding the assistant.* tokens. See README.md.

(include "locales/en.lisp"
         "locales/es.lisp"
         "locales/fr.lisp")

;;; Definitions

(defdefaults assistant
  ;; The colours, the key and the default language an app that includes the assistant replaces with its own.
  "The colours, the key and the default language an app that includes the assistant replaces with."
  :tokens {:assistant.accent {:light "#0969da" :dark "#4493f8"}
           :assistant.accentHover {:light "#0550ae" :dark "#2f81f7"}
           :assistant.onAccent {:light "#ffffff" :dark "#ffffff"}
           :assistant.surface {:light "#ffffff" :dark "#161b22"}
           :assistant.raised {:light "#f6f8fa" :dark "#21262d"}
           :assistant.border {:light "#d0d7de" :dark "#30363d"}
           :assistant.text {:light "#1f2328" :dark "#e6edf3"}
           :assistant.dim {:light "#57606a" :dark "#8d96a0"}
           :assistant.dot {:light "#8c959f" :dark "#6e7681"}
           :assistant.success {:light "#1f883d" :dark "#3fb950"}
           :assistant.danger {:light "#cf222e" :dark "#f85149"}
           :assistant.warningBackground {:light "#fff8c5" :dark "#3b2e0a"}}
  :keys {"mod+shift+a" assistant.toggle}
  :strings {:default en})

(defstate assistant
  {:show true
   :lift 0
   :open false
   :anim 1
   :draft ""
   :messages []
   :unread 0
   :pending ""
   :decided {}
   :phase 0
   :simulate nil
   :model ""
   :models nil
   :modelMenu false
   :canReview false
   :attachments []
   :picked nil
   :copied nil})

(defcomponent assistantChip "A small chip in the assistant: an icon and a text."
  {:text ""}
  `(label :text ~text
     :textColor @assistant.text
     :fitText {:maxWidth 320}
     :padding 7
     :radius 15
     :color @assistant.raised
     :borderWidth 1
     :borderColor @assistant.border
     :hover {:borderColor @assistant.accent}
     :focused {:borderColor @assistant.accent}
     :role button
     :focusable true
     :cursor hand
     :justifySelf start
     :onClick ~(handler assistant-ask text)))

(defcomponent assistantFile
  ;; A file as a card, as chat apps show attachments: an icon of its kind (a picture, a sound, any other file), its
  ;; name, and its length (a sound) or size; removable from the files of the next message
  "A file as a card, as chat apps show attachments: an icon of its kind (a picture, a sound."
  {:file {} :removable false}
  `(row :fitContent width
     :height 44
     :alignItems center
     :gap 8
     :radius 10
     :color @assistant.surface
     :borderWidth 1
     :borderColor @assistant.border
     :role listitem
     :label ~file.name
     :let {:assistantKind ~(slice (or-else file.type "") 0 6)}
     ~(assistant-file-icon)
     ~(assistant-file-caption file)
     ~(assistant-file-node file removable)
     {:width 2 :height 1 :color transparent}))

(defview assistant-file-icon "A file's icon, by its kind: a picture, a sound, or any other file." []
  `(node :width 38 :height 44 :color transparent
     (node :anchors {:left 6 :top 6} :width 32 :height 32 :radius 8 :color @assistant.raised
       (path :anchors {:left 8 :top 8}
         :width 16
         :height 16
         :viewBox "0 0 24 24"
         :d ~(cond (= assistantKind "image/") "M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2ZM11 9a2 2 0 1 1-4 0a2 2 0 1 1 4 0M21 15l-3.086-3.086a2 2 0 0 0-2.828 0L6 21"
                   (= assistantKind "audio/") "M9 18V5l12-2v13M9 18a3 3 0 1 1-6 0a3 3 0 1 1 6 0M21 16a3 3 0 1 1-6 0a3 3 0 1 1 6 0"
                   :else "M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7ZM14 2v4a2 2 0 0 0 2 2h4")
         :stroke @assistant.dim
         :strokeWidth 2
         :strokeCap round
         :strokeJoin round))))

(defview assistant-file-caption "A file's name, and its length (a sound) or size." [file]
  `(column :fitContent true :gap 1
     (label :text ~file.name :textColor @assistant.text :fitText {:maxWidth 180})
     (label :runs [{:text ~(cond (some? file.duration) (str (floor (/ file.duration 60))
                                                            ":"
                                                            (pad (floor (mod file.duration 60)) 2))
                                 (>= (or-else file.size 0) 1048576) (str (formatNumber (/ file.size 1048576) 0 1)
                                                                         " MB")
                                 (>= (or-else file.size 0) 1024) (str (formatNumber (floor (/ file.size 1024)))
                                                                      " KB")
                                 :else (str (or-else file.size 0) " B"))
                    :size 12}]
       :textColor @assistant.dim
       :fitText {:maxWidth 180})))

(defcomponent assistantExtras
  ;; What a proposal adds beside its edits: its assets (a file card each, with a picture's thumbnail while it is
  ;; previewed) and its run-once steps, a line each
  "What a proposal adds beside its edits: its assets (a file card each."
  {:id 0 :assets [] :run []}
  `(column :fitContent height :width 340 :gap 6 :color transparent :role list
     ~(repeat [asset assets :index a]
       `(row :id ~(str "assistantAsset" a) :fitContent height :alignSelf stretch :gap 8 :alignItems center
          (image :when ~(and (some? preview)
                             (= preview.id id)
                             (contains ["image/png" "image/jpeg" "image/svg+xml"] (or-else asset.type "")))
            :file ~(str "asset:" asset.name)
            :width 44
            :height 44
            :fit cover
            :radius 10
            :color @assistant.raised)
          (node :use assistantFile :with {:file ~asset})))
     ~(repeat [step run :index r]
       (assistant-run id step r))))

;;; What it does: each a function, run by its action's name where a key, a component or the app names one

(defn assistant-open "Opens the panel, the field focused, the conversation at its end." []
  (set "state.assistant.open" true)
  (set "state.assistant.unread" 0)
  (set "state.assistant.anim" 0)
  (animate "state.assistant.anim" :to 1 :ms 180 :easing "out")
  (focus "#assistantComposer")
  (scrollTo "#assistantEnd"))

(defn assistant-close "Closes the panel, the launcher focused." []
  (set "state.assistant.open" false)
  (focus "#assistantLauncher"))

(defn assistant-toggle "Opens the panel, or closes it." []
  (if state.assistant.open (assistant-close) (assistant-open)))

(defn assistant-trim "Keeps the last 200 messages." []
  (when (> (count state.assistant.messages) 200)
    (set "state.assistant.messages"
         (slice state.assistant.messages (- (count state.assistant.messages) 200)))))

(defn assistant-said "Adds a message to the conversation, keeping the last 200, at its end." [message]
  (push "state.assistant.messages" :value message)
  (assistant-trim)
  (scrollTo "#assistantEnd"))

(defn assistant-model-options "The model the person chose, or their default, for the host's agent." []
  (if (and (some? state.assistant.models) (not= state.assistant.models.default ""))
    {:model (if (some (fn [model] (= (:id model) state.assistant.model)) state.assistant.models.models)
              state.assistant.model
              state.assistant.models.default)}
    {}))

(defn assistant-ask "Asks the agents, with the files attached: the person's message, then the request." [text]
  (when (> (count (or text "")) 0)
    (when (> (count state.assistant.attachments) 0)
      (push "state.assistant.messages"
            :value {:kind "files" :mine true :files state.assistant.attachments :time (now)}))
    (push "state.assistant.messages" :value {:mine true :from (t "assistant.you") :text text :time (now) :error false})
    (assistant-trim)
    (ask text
         :attachments (map (fn [file] (:id file)) state.assistant.attachments)
         :into "state.assistant.pending"
         :options (assistant-model-options))
    (set "state.assistant.attachments" [])
    (scrollTo "#assistantEnd")))

(defn assistant-start "Opens the panel and asks what the app was given (event.text): the host's own way in." []
  (assistant-open)
  (assistant-ask (trim (or event.text ""))))

(defn assistant-send "Asks what the person wrote, and empties the field." []
  (assistant-ask (trim state.assistant.draft))
  (set "state.assistant.draft" ""))

(defn assistant-attach-failed
  "Says why a file couldn't be attached (its type, its size), in the conversation: the attach step's error, or a
message an app gives."
  [error message]
  (assistant-said {:mine false
                   :from (t "assistant.title")
                   :text (if (map? error) (or (:message error) "") (or message ""))
                   :time (now)
                   :error true}))

(defn assistant-pick-files "The file dialog: what the person picks lands in picked, which the watch adds." []
  (attach {:multiple true :title (t "assistant.attach")}
          :into "state.assistant.picked"
          :onError assistant-attach-failed))

(defn assistant-drop "Attaches files dropped on the panel from the OS, as picked ones." [files]
  (when (> (count (or files [])) 0)
    (attach {:from files :multiple true} :into "state.assistant.picked" :onError assistant-attach-failed)))

(defn assistant-attach-picked "Adds the files picked to the next message's, eight at most, each once." []
  (let [ids (map (fn [file] (:id file)) state.assistant.attachments)]
    (set "state.assistant.attachments"
         (slice (concat state.assistant.attachments
                        (filter (fn [file] (not (contains ids (:id file)))) (or state.assistant.picked [])))
                0
                8)))
  (set "state.assistant.picked" nil)
  (focus "#assistantComposer"))

(defn assistant-picked "When files are picked: attached." []
  (when (some? state.assistant.picked) (assistant-attach-picked)))

(defn assistant-unattach "Takes a file from the next message's." [id]
  (set "state.assistant.attachments" (remove (fn [file] (= (:id file) id)) state.assistant.attachments))
  (focus "#assistantComposer"))

(defn assistant-asked "What the request a reply answers asked, or nothing." [id]
  (or (:text (find (fn [request] (= (:id request) id)) requests)) ""))

(defn assistant-add-card "A proposal as a message: a card, then what it adds beside its edits, if anything."
  [proposal time]
  (push "state.assistant.messages"
        :value {:kind "card"
                :id (:id proposal)
                :time time
                :title (:message proposal)
                :summary (t "assistant.touches" {:n (count (or (:touches proposal) []))})
                :lines (slice (or (:changes proposal) []) 0 3)})
  (when (> (+ (count (or (:assets proposal) [])) (count (or (:run proposal) []))) 0)
    (push "state.assistant.messages"
          :value {:kind "extras"
                  :id (:id proposal)
                  :time time
                  :assets (slice (or (:assets proposal) []) 0 8)
                  :run (slice (or (:run proposal) []) 0 8)})))

(defn assistant-reply "An agent's answer (ask:reply): its text or error, then a card for each proposal it opened." []
  (let [text (or event.text "")
        error (or event.error "")
        opened (or event.proposals [])]
    (when (or (> (count text) 0) (> (count error) 0) (> (count opened) 0))
      (push "state.assistant.messages"
            :value {:mine false
                    :from event.from
                    :text (if (> (count error) 0) error text)
                    :time event.time
                    :error (> (count error) 0)
                    :asked (assistant-asked event.request)})
      (map (fn [proposal] (assistant-add-card proposal event.time))
           (filter (fn [proposal] (contains opened (:id proposal))) (or state.assistant.simulate.proposals proposals)))
      (assistant-trim)
      (when (not state.assistant.open) (increment "state.assistant.unread"))
      (scrollTo "#assistantEnd"))))

(defn assistant-stop
  "Stops the requests in progress (cancelAsk): the agents are told, and what they send after is dropped."
  []
  (assistant-said {:mine false
                   :from (t "assistant.title")
                   :text (t "assistant.stopped")
                   :time (now)
                   :error false
                   :note true
                   :asked (assistant-asked state.assistant.pending)})
  (cancelAsk (map (fn [request] (:id request))
                  (filter (fn [request] (or (= (:state request) "sent") (= (:state request) "working"))) requests)))
  (when (some? state.assistant.simulate) (set "state.assistant.simulate.working" false))
  (focus "#assistantComposer"))

(defn assistant-ask-again "Asks again what a reply answered (or a stopped request), as it was written." [text]
  (assistant-ask text))

(defn assistant-copy-done "The Copy button says Copy again." [] (set "state.assistant.copied" nil))

(defn assistant-copy-text "Copies a message, and its button says so for a moment." [text at]
  (copy text)
  (set "state.assistant.copied" at)
  (after 1500 assistant-copy-done))

(defn assistant-preview "Shows a proposal in the app, or stops showing it." [id]
  (if (and (some? preview) (= preview.id id)) (endPreview) (preview id)))

(defn assistant-review "Asks the host for its own window on the change (ChangesWindow): the file by file diff." [id]
  (call "changes.open" :with {:proposal id} :onError assistant-ignored))

(defn assistant-ignored "Nothing: a host without the call changes nothing." [])

(defn assistant-accept "Accepts a proposal, as the person saw it (its head)." [id]
  (accept id :head (:head (find (fn [proposal] (= (:id proposal) id)) proposals)))
  (set (str "state.assistant.decided[" id "]") "accepted"))

(defn assistant-reject "Rejects a proposal." [id]
  (reject id)
  (set (str "state.assistant.decided[" id "]") "rejected"))

(defn assistant-no-models "Without the host's own agent: no picker." [] (set "state.assistant.models" nil))

(defn assistant-load-models
  "The host's read-only assistant.models: the provider in use, the person's default and the models to choose from.
Only the hosts whose own agent is connected (the peer 'agent') have it; without it (MCP-only setups, other hosts, the
rendering check) models stays nil and the picker hides."
  []
  (if (some (fn [peer] (= (:author peer) "agent")) peers)
    (call "assistant.models" :into "state.assistant.models" :onError assistant-no-models)
    (assistant-no-models)))

(defn assistant-pick-model "Chooses a model: the person's default is kept as \"\", so it follows a later change of it."
  [id]
  (set "state.assistant.model"
       (if (= id (:default (or state.assistant.simulate.models state.assistant.models))) "" id))
  (set "state.assistant.modelMenu" false))

(defn assistant-clear "Empties the conversation." []
  (set "state.assistant.messages" [])
  (set "state.assistant.decided" {}))

(defpart assistant "The assistant's watch, persist, actions (each a function, by the name keys, components and apps
run) and overlays: its launcher and its panel."
  :watch {:state.assistant.picked assistant-picked}
  :persist [state.assistant.messages state.assistant.decided state.assistant.model]
  :actions {:assistant.toggle [{:lisp assistant-toggle}]
            :assistant.open [{:lisp assistant-open}]
            :assistant.close [{:lisp assistant-close}]
            :assistant.start [{:lisp assistant-start}]
            :assistant.send [{:lisp assistant-send}]
            :assistant.ask [{:lisp assistant-ask}]
            :assistant.attach [{:lisp assistant-pick-files}]
            :assistant.drop [{:lisp assistant-drop}]
            :assistant.attachFailed [{:lisp assistant-attach-failed}]
            :assistant.attachPicked [{:lisp assistant-attach-picked}]
            :assistant.unattach [{:lisp assistant-unattach}]
            "ask:reply" [{:lisp assistant-reply}]
            :assistant.stop [{:lisp assistant-stop}]
            :assistant.retry [{:lisp assistant-ask-again}]
            :assistant.copy [{:lisp assistant-copy-text}]
            :assistant.trim [{:lisp assistant-trim}]
            :assistant.preview [{:lisp assistant-preview}]
            :assistant.review [{:lisp assistant-review}]
            :assistant.accept [{:lisp assistant-accept}]
            :assistant.reject [{:lisp assistant-reject}]
            :assistant.loadModels [{:lisp assistant-load-models}]
            :assistant.pickModel [{:lisp assistant-pick-model}]
            :assistant.clear [{:lisp assistant-clear}]}
  :overlays {:assistantLauncher (node#assistantLauncher :when ~(or (or-else state.assistant.simulate.online
                                                                            (> (count peers) 0))
                                                                   (> (count state.assistant.messages) 0)
                                                                   state.assistant.show)
                                  :use chatLauncher
                                  :anchors {:right 24 :bottom ~(+ 24 state.assistant.lift)}
                                  :with {:isOpen ~state.assistant.open
                                         :run assistant.toggle
                                         :unread ~state.assistant.unread
                                         :label ~(t (if state.assistant.open "assistant.close" "assistant.open"))
                                         :tooltip ~(t "assistant.shortcut")
                                         :color @assistant.accent
                                         :hoverColor @assistant.accentHover
                                         :iconColor @assistant.onAccent
                                         :badgeColor @assistant.danger
                                         :badgeTextColor @assistant.onAccent})
             :assistantPanel ~(assistant-panel)})

;;; Views

(defview assistant-file-node "A file of a message, as a card." [file removable]
  `(node :when ~removable
     :width 22
     :height 22
     :radius 11
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :label ~(t "assistant.remove" {:name file.name})
     :tooltip ~(t "assistant.remove" {:name file.name})
     :hover {:color @assistant.raised}
     :focused {:borderWidth 1 :borderColor @assistant.accent}
     :onClick ~(handler assistant-unattach file.id)
     (path :anchors {:left 4 :top 4}
       :width 14
       :height 14
       :viewBox "0 0 24 24"
       :d "M18 6 6 18M6 6l12 12"
       :stroke @assistant.dim
       :strokeWidth 2.5
       :strokeCap round)))

(defview assistant-run "A run-once step a proposal holds, as a line." [id step r]
  `(label :id ~(str "assistantRun" r)
     :textColor @assistant.dim
     :wrap true
     :alignSelf stretch
     :height 32
     :padding 8
     :radius 8
     :color @assistant.raised
     :role listitem
     :text ~(or-else step.note
                     (if (or (= step.image "open") (= step.image "openAsset"))
                         (t "assistant.opens" {:name (or-else step.with.asset step.asset "")})
                         (t "assistant.runs"
                            {:step (slice (str (or-else (get (keys step) 0) "")
                                               " "
                                               (text (or-else (get (vals step) 0) "")))
                                          0
                                          80)})))))

(defview assistant-header "The panel's header: the title, who is connected, the model and the buttons." []
  `(node#assistantHeader :height 56
     :alignSelf stretch
     :color transparent
     :onShow [{:call changes.available :into state.assistant.canReview :onError []} {:lisp assistant-load-models}]
     (label :runs [{:text ~(t "assistant.title") :bold true :size 16}]
       :textColor @assistant.text
       :role heading
       :anchors {:left 16 :top 9}
       :fitText true)
     {:anchors {:left 16 :top 35}
      :width 8
      :height 8
      :radius 4
      :color ~(if assistantOnline "@assistant.success" "@assistant.dot")}
     (label#assistantStatus :role status
       :live polite
       :text ~(if assistantOnline (t "assistant.online" {:n (max 1 (count peers))}) (t "assistant.offline"))
       :textColor @assistant.dim
       :anchors {:left 30 :top 30}
       :fitText true)
     (node#assistantClose :anchors {:right 12 :top 12}
       :width 32
       :height 32
       :radius 6
       :color transparent
       :role button
       :focusable true
       :cursor hand
       :label ~(t "assistant.close")
       :tooltip ~(t "assistant.close")
       :hover {:color @assistant.raised}
       :focused {:borderWidth 1 :borderColor @assistant.accent}
       :onClick assistant-close
       {:anchors {:left 7 :top 15} :width 18 :height 2 :radius 1 :rotate 45 :color @assistant.dim}
       {:anchors {:left 7 :top 15} :width 18 :height 2 :radius 1 :rotate -45 :color @assistant.dim})
     {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @assistant.border}))

(defview assistant-copy "A message's Copy button." [message m]
  `(node :id ~(str "assistantCopy" m)
     :when ~(not= message.note true)
     :width 26
     :height 26
     :radius 6
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :label ~(t (if (= state.assistant.copied m) "assistant.copied" "assistant.copy"))
     :tooltip ~(t (if (= state.assistant.copied m) "assistant.copied" "assistant.copy"))
     :hover {:color @assistant.raised}
     :focused {:borderWidth 1 :borderColor @assistant.accent}
     :onClick ~(handler assistant-copy-text message.text m)
     (path :anchors {:left 6 :top 6}
       :width 14
       :height 14
       :viewBox "0 0 24 24"
       :d ~(if (= state.assistant.copied m)
               "M20 6 9 17l-5-5"
               "M10 8h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H10a2 2 0 0 1-2-2V10a2 2 0 0 1 2-2ZM4 16a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2")
       :stroke ~(if (= state.assistant.copied m) "@assistant.success" "@assistant.dim")
       :strokeWidth 2
       :strokeCap round
       :strokeJoin round)))

(defview assistant-retry "A message's Retry button." [message m]
  `(node :id ~(str "assistantRetry" m)
     :when ~(> (count (or-else message.asked "")) 0)
     :width 26
     :height 26
     :radius 6
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :label ~(t "assistant.retry")
     :tooltip ~(t "assistant.retry")
     :hover {:color @assistant.raised}
     :focused {:borderWidth 1 :borderColor @assistant.accent}
     :onClick ~(handler assistant-ask-again message.asked)
     (path :anchors {:left 6 :top 6}
       :width 14
       :height 14
       :viewBox "0 0 24 24"
       :d "M3 12a9 9 0 1 0 9-9a9.75 9.75 0 0 0-6.74 2.74L3 8M3 3v5h5"
       :stroke @assistant.dim
       :strokeWidth 2
       :strokeCap round
       :strokeJoin round)))

(defview assistant-message "A message of the conversation: the person's, an agent's or a note." [message m]
  `(column :id ~(str "assistantMessage" m)
     :when ~(= (or-else message.kind "message") "message")
     :fitContent height
     :gap 2
     :justifySelf stretch
     :color transparent
     :cell {:column 1 :row ~(+ m 1)}
     (node :id ~(str "assistantBubble" m)
       :use chatBubble
       :with {:text ~message.text
              :mine ~message.mine
              :from ~message.from
              :time ~(formatTime message.time "short")
              :error ~message.error
              :maxWidth ~(if message.mine 280 340)
              :padding ~(if (or message.mine message.error) 10 2)
              :radius 14
              :color transparent
              :textColor ~(if (= message.note true) "@assistant.dim" "@assistant.text")
              :mineColor @assistant.raised
              :mineTextColor @assistant.text
              :errorColor @assistant.danger})
     (row :id ~(str "assistantActions" m)
       :when ~(not message.mine)
       :fitContent width
       :height 26
       :gap 2
       :alignSelf start
       :color transparent
       :role toolbar
       ~(assistant-copy message m)
       ~(assistant-retry message m))))

(defview assistant-card "A proposal as a card: its title, its changes, and Preview, Review, Accept and Reject."
  [message m]
  `(node :id ~(str "assistantCard" message.id)
     :when ~(= message.kind "card")
     :use proposalCard
     :cell {:column 1 :row ~(+ m 1)}
     :with {:title ~message.title
            :summary ~message.summary
            :lines ~(or-else message.lines [])
            :status ~(cond (and (some? preview) (= preview.id message.id)) "previewing"
                           (contains assistantPending message.id) "pending"
                           :else (or-else (get state.assistant.decided message.id) "closed"))
            :preview assistant.preview
            :accept assistant.accept
            :reject assistant.reject
            :review ~(if (= state.assistant.canReview true) "assistant.review" nil)
            :with {:id ~message.id}
            :width 340
            :labels {:preview ~(t "assistant.preview")
                     :accept ~(t "assistant.accept")
                     :reject ~(t "assistant.reject")
                     :accepted ~(t "assistant.accepted")
                     :review ~(t "assistant.review")
                     :rejected ~(t "assistant.rejected")
                     :closed ~(t "assistant.closed")}
            :color @assistant.surface
            :textColor @assistant.text
            :dimColor @assistant.dim
            :borderColor @assistant.border
            :accent @assistant.success
            :accentTextColor @assistant.onAccent
            :buttonColor @assistant.raised}))

(defview assistant-waiting "A proposal waiting for review that no message brought." [proposal l]
  `(node :id ~(str "assistantWaiting" proposal.id)
     :use proposalCard
     :cell {:column 1 :row ~(+ assistantCount (if (= assistantCount 0) 4 0) l 1)}
     :with {:title ~proposal.message
            :summary ~(t "assistant.touches" {:n (count (or-else proposal.touches []))})
            :lines ~(slice (concat (map (fn [it] (str "+ " it.name)) (or-else proposal.assets []))
                                   (or-else proposal.changes []))
                           0
                           3)
            :status ~(if (and (some? preview) (= preview.id proposal.id)) "previewing" "pending")
            :preview assistant.preview
            :accept assistant.accept
            :reject assistant.reject
            :review ~(if (= state.assistant.canReview true) "assistant.review" nil)
            :with {:id ~proposal.id}
            :width 340
            :labels {:preview ~(t "assistant.preview")
                     :accept ~(t "assistant.accept")
                     :reject ~(t "assistant.reject")
                     :accepted ~(t "assistant.accepted")
                     :review ~(t "assistant.review")
                     :rejected ~(t "assistant.rejected")
                     :closed ~(t "assistant.closed")}
            :color @assistant.surface
            :textColor @assistant.text
            :dimColor @assistant.dim
            :borderColor @assistant.border
            :accent @assistant.success
            :accentTextColor @assistant.onAccent
            :buttonColor @assistant.raised}))

(defview assistant-messages "The conversation, newest last, scrolled to the end." []
  `(grid#assistantMessages :columns ["1fr" 8]
     :columnGap 0
     :rowGap 8
     :scroll vertical
     :anchors {:left 12 :right 12 :top 12 :bottom 0}
     :role list
     :label ~(t "assistant.messages")
     :let {:assistantCount ~(count state.assistant.messages)
           :assistantLoose ~(filter (fn [it] (not (contains assistantCarded it.id)))
                                    (or-else state.assistant.simulate.proposals proposals))}
     (label#assistantEmpty :when ~(= assistantCount 0)
       :cell {:column 1 :row 1}
       :text ~(t "assistant.empty")
       :textColor @assistant.dim
       :wrap true
       :height 44)
     ~(assistant-suggestion 1)
     ~(assistant-suggestion 2)
     ~(assistant-suggestion 3)
     ~(repeat [message state.assistant.messages :index m]
       (assistant-message message m))
     ~(repeat [message state.assistant.messages :index m]
       (assistant-sent-files message m))
     ~(repeat [message state.assistant.messages :index m]
       (assistant-additions message m))
     ~(repeat [message state.assistant.messages :index m]
       (assistant-card message m))
     ~(repeat [proposal assistantLoose :index l]
       (assistant-waiting proposal l))
     ~(assistant-typing)
     (node#assistantEnd :height 1
       :color transparent
       :cell {:column 1 :row ~(+ assistantCount (if (= assistantCount 0) 4 0) (count assistantLoose) 2)})))

(defview assistant-suggestion "Suggestion `n` of three, a chip to ask with, while the conversation is empty." [n]
  `(node :id ~(str "assistantChip" n)
     :when ~(= assistantCount 0)
     :use assistantChip
     :cell {:column 1 :row ~(+ n 1)}
     :with {:text ~(t (str "assistant.chip" n))}))

(defview assistant-sent-files "The files message `m` attached, as cards, where it is a message of files." [message m]
  `(row :id ~(str "assistantFiles" m)
     :when ~(= message.kind "files")
     :wrap true
     :fitContent height
     :gap 6
     :justify end
     :cell {:column 1 :row ~(+ m 1)}
     :role list
     :label ~(t "assistant.attached")
     ~(repeat [file message.files :index f]
       `(node :id ~(str "assistantSent" m "-" f) :use assistantFile :with {:file ~file}))))

(defview assistant-additions "What a proposal adds beside its edits, where message `m` holds it: assets and run-once steps."
  [message m]
  `(node :id ~(str "assistantAdds" message.id)
     :when ~(= message.kind "extras")
     :use assistantExtras
     :cell {:column 1 :row ~(+ m 1)}
     :justifySelf start
     :with {:id ~message.id :assets ~(or-else message.assets []) :run ~(or-else message.run [])}))

(defview assistant-typing "Three dots that blink while the agent works, under the conversation." []
  `(node#assistantTyping :when ~assistantWorking
     :cell {:column 1 :row ~(+ assistantCount (if (= assistantCount 0) 4 0) (count assistantLoose) 1)}
     :use typingIndicator
     :every {:ms 400 :do [{:increment state.assistant.phase}]}
     :with {:label ~(t "assistant.typing")
            :phase ~state.assistant.phase
            :color transparent
            :dotColor @assistant.dot
            :activeColor @assistant.dim}))

(defview assistant-attach "The button that attaches files to the next message." []
  `(node#assistantAttach :width 32
     :height 32
     :radius 8
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :label ~(t "assistant.attach")
     :tooltip ~(t "assistant.attach")
     :hover {:color @assistant.raised}
     :focused {:borderWidth 1 :borderColor @assistant.accent}
     :onClick assistant-pick-files
     (path :anchors {:left 7 :top 7}
       :width 18
       :height 18
       :viewBox "0 0 24 24"
       :d "M12 5v14M5 12h14"
       :stroke @assistant.dim
       :strokeWidth 2
       :strokeCap round)))

(defview assistant-model-picker "The menu of the models the host's agent can use." []
  `(row#assistantModelPicker :when ~(> (count (or-else assistantModels.models [])) 0)
     :fitContent width
     :height 32
     :gap 4
     :alignItems center
     :radius 8
     :color transparent
     :role button
     :focusable true
     :cursor hand
     :isOpen ~state.assistant.modelMenu
     :label ~(t "assistant.chooseModel" {:provider (or-else assistantModels.provider "")})
     :tooltip ~(t "assistant.chooseModel" {:provider (or-else assistantModels.provider "")})
     :hover {:color @assistant.raised}
     :focused {:borderWidth 1 :borderColor @assistant.accent}
     :onClick [{:toggle state.assistant.modelMenu}]
     {:width 4 :height 1 :color transparent}
     (label#assistantModelName :fitText {:maxWidth 150}
       :textColor @assistant.dim
       :text ~(or-else (:name (find (fn [it] (= it.id assistantModel)) assistantModels.models)) assistantModel))
     ~(assistant-model-chevron)
     {:width 4 :height 1 :color transparent}
     ~(assistant-model-menu)))

(defview assistant-model-chevron "The chevron that says the model's name opens a menu." []
  `(path#assistantModelChevron :width 14
     :height 14
     :viewBox "0 0 24 24"
     :d "m6 9 6 6 6-6"
     :stroke @assistant.dim
     :strokeWidth 2.25
     :strokeCap round
     :strokeJoin round))

(defview assistant-model-menu "The models to choose from, the one in use ticked and the default named." []
  `(node#assistantModelMenu :use menu
     :with {:open state.assistant.modelMenu
            :placement above
            :width 260
            :items ~(map (fn [it]
                           {:title it.name
                            :checked (= it.id assistantModel)
                            :keyLabel (if (= it.id assistantModels.default) (t "assistant.defaultModel") "")
                            :run "assistant.pickModel"
                            :with {:id it.id}})
                         assistantModels.models)
            :color @assistant.raised
            :textColor @assistant.text
            :dimColor @assistant.dim
            :highlight @assistant.border}))

(defview assistant-composer "The field the person writes in: Enter sends." []
  `(node#assistantComposer :use chatComposer
     :anchors {:left 1 :right 1 :top 1 :bottom 1}
     :with {:bind state.assistant.draft
            :value ~state.assistant.draft
            :send assistant.send
            :autofocus true
            :close assistant.close
            :label ~(t "assistant.message")
            :placeholder ~(t "assistant.placeholder")
            :sendLabel ~(t "assistant.send")
            :working ~assistantWorking
            :stop assistant.stop
            :stopLabel ~(t "assistant.stop")
            :borderWidth 0
            :radius 16
            :color @assistant.surface
            :textColor @assistant.text
            :dimColor @assistant.dim
            :borderColor @assistant.border
            :accent @assistant.accent
            :accentTextColor @assistant.onAccent}
     ~(assistant-attach)
     ~(assistant-model-picker)))

(defview assistant-input-card "The bottom of the panel: the files to send, the field and its buttons." []
  `(column#assistantInputCard :anchors {:left 12 :right 12 :top 0 :bottom 12}
     :color @assistant.surface
     :radius 16
     :borderWidth 1
     :borderColor @assistant.border
     :shadow {:x 0 :y 2 :blur 8 :color "#0000001a"}
     (node#assistantAttachments :when ~(> (count state.assistant.attachments) 0)
       :alignSelf stretch
       :height 54
       :color transparent
       (row#assistantAttachedList :scroll horizontal
         :gap 6
         :alignItems center
         :anchors {:left 10 :right 10 :top 10 :bottom 0}
         :role list
         :label ~(t "assistant.attached")
         ~(repeat [file state.assistant.attachments :index f]
           `(node :id ~(str "assistantAttached" f) :use assistantFile :with {:file ~file :removable true}))))
     (node :alignSelf stretch :height 100 :color transparent
       ~(assistant-composer))))

(defview assistant-panel "The assistant's panel over the app: the header, the conversation and the field." []
  `(column#assistantPanel :dir auto
     :width 380
     :height ~(min 560 (- (- window.height 120) state.assistant.lift))
     :anchors {:right 24 :bottom ~(+ 96 state.assistant.lift)}
     :overlay {:placement window :open state.assistant.open :modal true}
     :color @assistant.surface
     :radius 12
     :borderWidth 1
     :borderColor @assistant.border
     :shadow {:x 0 :y 8 :blur 24 :color "#00000040"}
     :translate ~[0 (* (- 1 state.assistant.anim) 16)]
     :scale ~(+ 0.96 (* 0.04 state.assistant.anim))
     :transformOrigin bottomRight
     :role dialog
     :label ~(t "assistant.title")
     :keys {:escape assistant.close}
     :onDrop ~(handler assistant-drop (or-else event.files []))
     :let {:assistantOnline ~(or-else state.assistant.simulate.online (> (count peers) 0))
           :assistantPending ~(or-else state.assistant.simulate.pending (map (fn [it] it.id) proposals))
           :assistantWorking ~(or-else state.assistant.simulate.working
                                       (some (fn [it] (or (= it.state "sent") (= it.state "working"))) requests))
           :assistantCarded ~(map (fn [it] it.id) (filter (fn [it] (= it.kind "card")) state.assistant.messages))
           :assistantModels ~(or-else state.assistant.simulate.models state.assistant.models)
           :assistantModel ~(if (some (fn [it] (= it.id state.assistant.model)) (or-else assistantModels.models []))
                                state.assistant.model
                                (or-else assistantModels.default ""))}
     ~(assistant-header)
     ~(assistant-offline-note)
     (node :grow 1 :alignSelf stretch :color transparent
       ~(assistant-messages))
     (node#assistantInput :alignSelf stretch
       :color transparent
       :height ~(if (> (count state.assistant.attachments) 0) 166 112)
       ~(assistant-input-card))))

(defview assistant-offline-note "A note while no agent is connected: choose a provider in Settings, or connect one." []
  `(node#assistantOffline :when ~(not assistantOnline)
     :alignSelf stretch
     :height 52
     :color @assistant.warningBackground
     :role alert
     (label :text ~(t "assistant.offlineNote")
       :textColor @assistant.text
       :wrap true
       :anchors {:left 16 :right 16 :top 8 :bottom 8})))
