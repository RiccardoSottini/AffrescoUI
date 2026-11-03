;;; lsp.lisp --- Forge's language server: the lsp service (clangd, granted by the host when it finds it) for C and C++ files; the service speaks the protocol (initialize, open documents, positions) and answers in small shapes counted from 0
;;;
;;; This file asks it at the caret, keeps it told of the text being edited, and shows the answers: diagnostics as
;;; squiggles and in Problems, a hover card, completion and signature help at the caret, a peek list for definitions
;;; and references, a reviewed list of edits for rename and formatting, and symbols in a quick pick ('@' this file,
;;; '#' the workspace). With no server, the editor works as before. Positions shown and gone to count from 1

;;; State and definitions

(defstate lsp
  {:request 0
   :languages {:c c :h cpp :cc cpp :cpp cpp :cxx cpp :hh cpp :hpp cpp :hxx cpp :m objective-c :mm objective-cpp}
   :open []
   :listening false
   :files []
   :hover {:open false :text "" :x 0 :y 0 :request 0}
   :at {:x 0 :y 0 :top 0}
   :completion {:open false :items [] :word "" :index 0 :request 0}
   :insert {:text "" :before 0 :request 0}
   :signature {:open false :label "" :documentation ""}
   :peek {:open false :title "" :items [] :index 0}
   :review {:open false :kind "" :edits [] :files 0 :truncated false}
   :rename {:open false :name ""}
   :symbols []})

(defstate symbolPick {:open false :query ""})

(defcomputed lspLanguage
  (if (or (nil? computed.activeDoc) (not= computed.activeDoc.kind "file"))
        nil
        (or-else (get state.lsp.languages (lower-case (get (reverse (split computed.activeDoc.id ".")) 0))) nil)))

(defcomputed lspCompletions (fuzzySort state.lsp.completion.items state.lsp.completion.word "filter" 50))

(defcomputed lspSymbolItems
  (map (fn [it]
           {:title (+ (slice "                " 0 (* 2 it.depth)) it.name)
            :detail (+ it.kind
                       (if (empty? it.detail) "" (str " " it.detail))
                       (if (= (slice state.symbolPick.query 0 1) "#") (str " — " it.path) ""))
            :name it.name
            :path it.path
            :line it.line
            :column it.column})
         state.lsp.symbols))

(defcomputed lspPeekRows
  (map-indexed (fn [index it]
                   {:id index
                    :cells [(str (get (reverse (split it.path "/")) 0) ":" (+ it.line 1)) it.path (+ it.line 1)]})
                 state.lsp.peek.items))

(defkey "f12" goToDefinition)

(defkey "shift+f12" findReferences)

(defkey "f2" renameSymbol)

(defkey "shift+alt+f" formatDocument)

(defkey "ctrl+space" triggerSuggest)

(defkey "mod+shift+o" goToSymbol)

(defkey "mod+t" goToWorkspaceSymbol)

(defpart lsp
         "What lsp adds."
         :services {:lsp {:provider lsp
                          :process clangd
                          :root workspace
                          :language cpp
                          :start onDemand
                          :restart {:times 3 :after 1000}}}
         :watch {:state.symbolPick.query searchWorkspaceSymbols})

;;; Functions and commands

(defn lspAsk "Asks the server about the active file, its text sent along (the service keeps the version when it is unchanged). line, column: from 0, or null for the caret's. The outline kind leaves the request count alone, as nothing waits on it"
  [kind line column newName query]
  (when (not= kind "outline") (increment "state.lsp.request" :by 1))
  (when (some? computed.lspLanguage)
    (lspListen)
    (lspCall kind
             state.lsp.request
             (if (= kind "workspaceSymbols") "" computed.activeDoc.id)
             (if (= kind "workspaceSymbols")
               {:query (or-else query "")}
               (merge {:path computed.activeDoc.id :text computed.activeDoc.text :language computed.lspLanguage}
                      (if (contains ["hover" "completion" "signature" "definition" "references" "rename"] kind)
                        {:line (or-else line (max (- state.caret.line 1) 0))
                         :column (or-else column (max (- state.caret.column 1) 0))}
                        {})
                      (if (= kind "rename") {:newName newName} {})
                      (if (= kind "format") {:tabSize (or-else settings.editor.tabSize 4) :insertSpaces true} {}))))
    (when (and (not= kind "workspaceSymbols") (not (contains state.lsp.open computed.activeDoc.id)))
      (set "state.lsp.open" (slice (concat state.lsp.open [computed.activeDoc.id]) (- 256))))))

(defn lspCall "One invoke per kind, as a step names its method written out; the silent kinds cancel their older call. Every answer goes to lspAnswer"
  [kind request path arguments]
  (when (= kind "hover")
    (invoke "lsp.hover"
            :with arguments
            :id "lsp-hover"
            :onDone [lspCallDone kind request path]
            :onError [lspCallFailed kind request path]))
  (when (= kind "completion")
    (invoke "lsp.completion"
            :with arguments
            :id "lsp-completion"
            :onDone [lspCallDone2 kind request path]
            :onError [lspCallFailed2 kind request path]))
  (when (= kind "signature")
    (invoke "lsp.signature"
            :with arguments
            :id "lsp-signature"
            :onDone [lspCallDone3 kind request path]
            :onError [lspCallFailed3 kind request path]))
  (when (= kind "tokens")
    (invoke "lsp.tokens"
            :with arguments
            :id "lsp-tokens"
            :onDone [lspCallDone4 kind request path]
            :onError [lspCallFailed4 kind request path]))
  (when (= kind "definition")
    (invoke "lsp.definition"
            :with arguments
            :onDone [lspCallDone5 kind request path]
            :onError [lspCallFailed5 kind request path]))
  (when (= kind "references")
    (invoke "lsp.references"
            :with arguments
            :onDone [lspCallDone6 kind request path]
            :onError [lspCallFailed6 kind request path]))
  (when (= kind "rename")
    (invoke "lsp.rename"
            :with arguments
            :onDone [lspCallDone7 kind request path]
            :onError [lspCallFailed7 kind request path]))
  (when (= kind "format")
    (invoke "lsp.format"
            :with arguments
            :onDone [lspCallDone8 kind request path]
            :onError [lspCallFailed8 kind request path]))
  (when (= kind "outline")
    (invoke "lsp.symbols"
            :with arguments
            :id "lsp-outline"
            :onDone [lspCallDone9 kind request path]
            :onError [lspCallFailed9 kind request path]))
  (when (= kind "symbols")
    (invoke "lsp.symbols"
            :with arguments
            :onDone [lspCallDone10 kind request path]
            :onError [lspCallFailed10 kind request path]))
  (when (= kind "workspaceSymbols")
    (invoke "lsp.workspaceSymbols"
            :with arguments
            :onDone [lspCallDone11 kind request path]
            :onError [lspCallFailed11 kind request path])))

(defn lspListen "The server's diagnostics, file by file, from its first request on (again after the stream ends)" []
  (when (not state.lsp.listening)
    (set "state.lsp.listening" true)
    (subscribe "lsp.diagnostics" :id "lsp-diagnostics" :do lspListenEach :onError lspListenFailed)))

(defn lspChanged "id: an edited file. A file the server has open gets its new text once typing pauses, so its diagnostics follow"
  [id]
  (when (contains state.lsp.open id) (debounce 500 lspChangedLater id)))

(defn lspClosed "id: a file no longer open in Forge. The server reads it from disk again" [id]
  (when (contains state.lsp.open id)
    (set "state.lsp.open" (filter (fn [it] (not= it id)) state.lsp.open))
    (invoke "lsp.close" :with {:path id} :onError lspClosedFailed)))

(defn lspHoverAt "TODO" []
  (if (or (not event.over) (nil? computed.activeDoc))
    (set "state.lsp.hover.open" false)
    (do
      (set "state.lsp.at" {:x event.x :y event.y :top event.top})
      (lspAsk "hover" event.line event.column nil nil)
      (set "state.lsp.hover.request" state.lsp.request))))

(defn lspTyped "TODO" []
  (set "state.lsp.at" {:x event.x :y event.y :top event.top})
  (set "state.lsp.hover.open" false)
  (set "state.lsp.completion.word" event.word)
  (set "state.lsp.completion.index" 0)
  (when (or (= event.text "(") (= event.text ",")) (lspAsk "signature" event.line event.column nil nil))
  (when (= event.text ")") (set "state.lsp.signature.open" false))
  (if (and (empty? event.word) (not= event.text "."))
    (set "state.lsp.completion.open" false)
    (do
      (when (or (= event.text ".") (= (count event.word) 1) (not state.lsp.completion.open))
        (lspAsk "completion" event.line event.column nil nil)
        (set "state.lsp.completion.request" state.lsp.request)))))

(defn lspPopupKey "TODO" []
  (when (or (= event.key "escape") (= (count computed.lspCompletions) 0)) (set "state.lsp.completion.open" false))
  (when (= event.key "down")
    (set "state.lsp.completion.index" (min (+ state.lsp.completion.index 1) (- (count computed.lspCompletions) 1))))
  (when (= event.key "up") (set "state.lsp.completion.index" (max (- state.lsp.completion.index 1) 0)))
  (when (= event.key "pageDown")
    (set "state.lsp.completion.index" (min (+ state.lsp.completion.index 10) (- (count computed.lspCompletions) 1))))
  (when (= event.key "pageUp") (set "state.lsp.completion.index" (max (- state.lsp.completion.index 10) 0)))
  (when (and (or (= event.key "enter") (= event.key "tab"))
             state.lsp.completion.open
             (> (count computed.lspCompletions) 0))
    (lspComplete state.lsp.completion.index)))

(defn lspComplete "TODO" [index]
  (set "state.lsp.insert"
       {:text (get-in computed.lspCompletions [index :item :insert])
        :before (count state.lsp.completion.word)
        :request (+ state.lsp.insert.request 1)})
  (set "state.lsp.completion.open" false))

(defn lspAnswer "answer: { kind, request, path, result, error }, its positions from 0" [answer]
  (if (some? answer.error)
    (do
      (if (or (= answer.kind "rename") (= answer.kind "format"))
        (notify "warning" (str "Not applied: " answer.error.message))
        (do
          (when (and (not= answer.kind "hover")
                     (not= answer.kind "completion")
                     (not= answer.kind "signature")
                     (not= answer.kind "tokens")
                     (not= answer.kind "outline"))
            (notify "info" (str "Language server: " answer.error.message))))))
    (do
      (when (and (= answer.kind "hover") (= answer.request state.lsp.hover.request))
        (set "state.lsp.hover"
             {:open (not (empty? answer.result.text))
              :text answer.result.text
              :x state.lsp.at.x
              :y state.lsp.at.y
              :request answer.request}))
      (when (and (= answer.kind "completion") (= answer.request state.lsp.completion.request))
        (set "state.lsp.completion.items" answer.result.items)
        (set "state.lsp.completion.index" 0)
        (set "state.lsp.completion.open" (> (count answer.result.items) 0)))
      (when (= answer.kind "signature")
        (set "state.lsp.signature"
             (if (nil? answer.result)
               {:open false :label "" :documentation ""}
               {:open true :label answer.result.label :documentation answer.result.documentation})))
      (when (and (= answer.kind "definition") (= (count answer.result.locations) 1))
        (lspOpenLocation answer.result.locations.0))
      (when (or (and (= answer.kind "definition") (not= (count answer.result.locations) 1))
                (= answer.kind "references"))
        (set "state.lsp.peek"
             {:open true
              :index 0
              :items answer.result.locations
              :title (str (if (= answer.kind "definition") "Definitions: " "References: ")
                          (count answer.result.locations)
                          (if answer.result.truncated "+" ""))}))
      (when (or (= answer.kind "rename") (= answer.kind "format"))
        (if (= (count answer.result.edits) 0)
          (log "Language server: nothing to change")
          (set "state.lsp.review"
               {:open true
                :kind answer.kind
                :edits answer.result.edits
                :files answer.result.files
                :truncated answer.result.truncated})))
      (when (and (= answer.kind "outline") (= answer.path (or-else computed.activeDoc.id "")))
        (set "state.outline"
             (map (fn [it] {:line (+ it.line 1) :depth it.depth :label it.name :kind it.kind}) answer.result.symbols))
        (set "state.outlineSymbols" answer.path))
      (when (or (= answer.kind "symbols") (= answer.kind "workspaceSymbols"))
        (set "state.lsp.symbols" answer.result.symbols))
      (when (= answer.kind "tokens")
        (set "state.lsp.files"
             (concat (filter (fn [it] (not= it.id answer.path)) state.lsp.files)
                     [{:id answer.path
                       :decorations (or-else (:decorations (or-else (find (fn [it] (= it.id answer.path))
                                                                          state.lsp.files)
                                                                    {}))
                                             [])
                       :tokens answer.result.tokens}]))))))

(defn lspPublished "published: { path, diagnostics, truncated }, positions from 0: squiggles in the file's editor, rows (from 1) in Problems"
  [published]
  (set "state.lsp.files"
       (concat (filter (fn [it] (not= it.id published.path)) state.lsp.files)
               [{:id published.path
                 :tokens (or-else (:tokens (or-else (find (fn [it] (= it.id published.path)) state.lsp.files) {})) [])
                 :decorations (map (fn [it]
                                     {:line it.line
                                      :column it.column
                                      :endLine it.endLine
                                      :endColumn (if (and (= it.endLine it.line) (<= it.endColumn it.column))
                                                   (+ it.column 1)
                                                   it.endColumn)
                                      :kind "squiggle"
                                      :color (cond
                                               (= it.severity "error") "#f85149"
                                               (= it.severity "warning") "#cca700"
                                               :else "#3794ff")
                                      :text it.message})
                                   published.diagnostics)}]))
  (set "state.problems"
       (concat (filter (fn [it] (or (not (or-else it.server false)) (not= it.file published.path))) state.problems)
               (map (fn [it]
                      {:severity (if (= it.severity "hint") "info" it.severity)
                       :message (if (empty? it.source) it.message (str it.message " (" it.source ")"))
                       :file published.path
                       :label (get (reverse (split published.path "/")) 0)
                       :line (+ it.line 1)
                       :column (+ it.column 1)
                       :source "lsp"
                       :server true})
                    published.diagnostics)))
  (when (and (some? computed.activeDoc) (= computed.activeDoc.id published.path)) (lspAsk "tokens" nil nil nil nil)))

(defn lspPlay "Plays an answer the service gave (event: { kind, result, path, request, error }; kind 'diagnostics' for a published file), as the scenarios do"
  []
  (if (= event.kind "diagnostics")
    (lspPublished event.result)
    (lspAnswer {:kind event.kind
                :request (or-else event.request state.lsp.request)
                :path (or-else event.path "")
                :result (or-else event.result nil)
                :error (or-else event.error nil)})))

(defn lspOpenLocation "at: { path, line, column } from 0" [at]
  (openPath at.path (get (reverse (split at.path "/")) 0))
  (set "state.goTo.line" (+ at.line 1))
  (set "state.goTo.column" (+ at.column 1))
  (increment "state.goTo.request" :by 1))

(defn lspPeekOpen "TODO" []
  (lspOpenLocation (get state.lsp.peek.items (number event.id)))
  (set "state.lsp.peek.open" false))

(defn lspApply "An open file changes in its editor (one undo step, left unsaved); any other is read, changed and written"
  []
  (doseq [file (keys (fromEntries (map (fn [it] {:key it.path :value true}) state.lsp.review.edits)))]
    (lspApplyTo file
                (map (fn [it]
                       {:line it.line :column it.column :endLine it.endLine :endColumn it.endColumn :text it.text})
                     (filter (fn [it] (= it.path file)) state.lsp.review.edits))))
  (log (str "Applied " (plural (count state.lsp.review.edits) "edit") " in " (plural state.lsp.review.files "file")))
  (set "state.lsp.review.open" false))

(defn lspApplyTo "TODO" [file edits]
  (if (some (fn [it] (and (= it.id file) (= it.kind "file"))) state.docs)
    (do
      (doseq [doc (filter (fn [it] (and (= it.id file) (= it.kind "file"))) state.docs)]
        (if (nil? (applyEdits doc.text edits))
          (notify "error" (str "Not applied to " file ": an edit is past its text, or two overlap"))
          (set "state.docs"
               (map (fn [it]
                      (if (= it.id file)
                        (merge it
                               {:text (applyEdits it.text edits) :dirty (not= (applyEdits it.text edits) it.saved)})
                        it))
                    state.docs)))))
    (invoke "files.read" :with {:path file} :onError [lspApplyToFailed file] :onDone [lspApplyToDone edits file])))

(defn lspRenameSend "TODO" []
  (set "state.lsp.rename.open" false)
  (when (not (empty? (trim state.lsp.rename.name))) (lspAsk "rename" nil nil (trim state.lsp.rename.name) nil)))

(defn lspSymbolPicked "TODO" [item]
  (lspOpenLocation item))

(defcommand goToDefinition "Go to Definition"
  :title ~(t "command.goToDefinition") :category ~(t "category.go") :icon "arrow-right"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "definition" nil nil nil nil))

(defcommand findReferences "Find All References"
  :title ~(t "command.findReferences") :category ~(t "category.go") :icon "list"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "references" nil nil nil nil))

(defcommand renameSymbol "Rename Symbol"
  :title ~(t "command.renameSymbol") :category ~(t "category.edit") :icon "pencil"
  :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.rename" {:open true :name ""}))

(defcommand formatDocument "Format Document"
  :title ~(t "command.formatDocument") :category ~(t "category.edit") :icon "align-left"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "format" nil nil nil nil))

(defcommand triggerSuggest "Trigger Suggest"
  :title ~(t "command.triggerSuggest") :category ~(t "category.edit") :icon "list"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "completion" nil nil nil nil)
  (set "state.lsp.completion.request" state.lsp.request)
  (set "state.lsp.completion.word" ""))

(defcommand goToSymbol "Go to Symbol in Editor..."
  :title ~(t "command.goToSymbol") :category ~(t "category.go") :icon "hash" :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.symbols" [])
  (set "state.symbolPick.query" "@")
  (set "state.symbolPick.open" true)
  (lspAsk "symbols" nil nil nil nil))

(defcommand goToWorkspaceSymbol "Go to Symbol in Workspace..."
  :title ~(t "command.goToWorkspaceSymbol") :category ~(t "category.go") :icon "hash"
  :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.symbols" [])
  (set "state.symbolPick.query" "#")
  (set "state.symbolPick.open" true))

(defn searchWorkspaceSymbols "TODO" []
  (when (and state.symbolPick.open (= (slice event.value 0 1) "#") (> (count event.value) 1))
    (lspAsk "workspaceSymbols" nil nil nil (slice event.value 1))))

(defn lspCallDone "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone2 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed2 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone3 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed3 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone4 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed4 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone5 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed5 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone6 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed6 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone7 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed7 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone8 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed8 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone9 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed9 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone10 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed10 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspCallDone11 "What lspCall does when it is done." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspCallFailed11 "What lspCall does when it fails." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspListenEach "What lspListen does when its time comes." [event]
  (lspPublished event))

(defn lspListenFailed "What lspListen does when it fails." []
  (set "state.lsp.listening" false))

(defn lspChangedLater "What lspChanged does when its time comes." [id]
  (doseq [doc (filter (fn [it] (and (= it.id id) (= it.kind "file"))) state.docs)]
    (invoke "lsp.sync"
            :with {:path doc.id
                   :text doc.text
                   :language (or-else (get state.lsp.languages (lower-case (get (reverse (split doc.id ".")) 0)))
                                      "cpp")}
            :onError lspChangedLaterFailed)))

(defn lspChangedLaterFailed "What lspChangedLater does when it fails." []
  nil)

(defn lspClosedFailed "What lspClosed does when it fails." []
  nil)

(defn lspApplyToFailed "What lspApplyTo does when it fails." [file error]
  (notify "error" (str "Not applied to " file ": " error.message)))

(defn lspApplyToDone "What lspApplyTo does when it is done." [edits file result]
  (if (nil? (applyEdits result.text edits))
    (notify "error" (str "Not applied to " file ": an edit is past its text, or two overlap"))
    (invoke "files.write"
            :with {:path file :text (applyEdits result.text edits)}
            :onError [lspApplyToDoneFailed file])))

(defn lspApplyToDoneFailed "What lspApplyToDone does when it fails." [file error]
  (notify "error" (str "Not applied to " file ": " error.message)))
