;;; lsp.lisp --- Forge's language server: hovers, completion, signatures, navigation, renames and diagnostics
;;;
;;; The lsp service runs clangd (granted by the host when it finds it) for C and C++ files and speaks the protocol;
;;; this file asks it about the active file, shows its answers, and applies the edits a rename or a format proposes
;;; after a review. Its diagnostics squiggle the editors and fill Problems.

;;; The server

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

(defcomputed lspLanguage
  (if (or (nil? computed.activeDoc) (not= computed.activeDoc.kind "file"))
        nil
        (or-else (get state.lsp.languages (lower-case (get (reverse (split computed.activeDoc.id ".")) 0))) nil)))

(defpart lsp-service
  "The language server, started when first asked; and the symbol picker's query, sent as it is typed."
  :services {:lsp {:provider lsp
                   :process clangd
                   :root workspace
                   :language cpp
                   :start onDemand
                   :restart {:times 3 :after 1000}}}
  :watch {:state.symbolPick.query searchWorkspaceSymbols})

(defn lspAsk
  "Asks the server about the active file, its text sent along; line and column from 0, or nil for the caret's."
  [kind line column newName query]
  (when (not= kind "outline") (increment "state.lsp.request" :by 1))
  (when-some [language computed.lspLanguage]
    (lspListen)
    (lspCall kind
             state.lsp.request
             (if (= kind "workspaceSymbols") "" computed.activeDoc.id)
             (if (= kind "workspaceSymbols")
               {:query (or-else query "")}
               (merge {:path computed.activeDoc.id :text computed.activeDoc.text :language language}
                      (if (contains? #{:hover :completion :signature :definition :references :rename} kind)
                        {:line (or-else line (max (dec state.caret.line) 0))
                         :column (or-else column (max (dec state.caret.column) 0))}
                        {})
                      (if (= kind "rename") {:newName newName} {})
                      (if (= kind "format") {:tabSize (or-else settings.editor.tabSize 4) :insertSpaces true} {}))))
    (when (and (not= kind "workspaceSymbols") (not (contains state.lsp.open computed.activeDoc.id)))
      (set "state.lsp.open" (slice (conj state.lsp.open computed.activeDoc.id) (- 256))))))

(defn lspCall
  "Asks one kind of question, each a step naming its method; the silent kinds cancel the last. Answers go to lspAnswer."
  [kind request path arguments]
  (let [done [lspAnswered kind request path]
        failed [lspFailed kind request path]]
    (case kind
      :hover (invoke "lsp.hover" :with arguments :id "lsp-hover" :onDone done :onError failed)
      :completion (invoke "lsp.completion" :with arguments :id "lsp-completion" :onDone done :onError failed)
      :signature (invoke "lsp.signature" :with arguments :id "lsp-signature" :onDone done :onError failed)
      :tokens (invoke "lsp.tokens" :with arguments :id "lsp-tokens" :onDone done :onError failed)
      :definition (invoke "lsp.definition" :with arguments :onDone done :onError failed)
      :references (invoke "lsp.references" :with arguments :onDone done :onError failed)
      :rename (invoke "lsp.rename" :with arguments :onDone done :onError failed)
      :format (invoke "lsp.format" :with arguments :onDone done :onError failed)
      :outline (invoke "lsp.symbols" :with arguments :id "lsp-outline" :onDone done :onError failed)
      :symbols (invoke "lsp.symbols" :with arguments :onDone done :onError failed)
      :workspaceSymbols (invoke "lsp.workspaceSymbols" :with arguments :onDone done :onError failed)
      nil)))

(defn lspAnswered "The server's answer to a question." [kind request path result]
  (lspAnswer {:kind kind :request request :path path :result result :error nil}))

(defn lspFailed "The server's refusal of a question." [kind request path error]
  (lspAnswer {:kind kind :request request :path path :result nil :error error}))

(defn lspPlay
  "Plays an answer the service gave (`event`: kind, result, path, request, error), as the scenarios do."
  []
  (if (= event.kind "diagnostics")
    (lspPublished event.result)
    (lspAnswer {:kind event.kind
                :request (or-else event.request state.lsp.request)
                :path (or-else event.path "")
                :result (or-else event.result nil)
                :error (or-else event.error nil)})))

;;; Its answers

(defn lspAnswer
  "An answer (`answer`: kind, request, path, result, error; its positions from 0), shown as its kind asks."
  [answer]
  (if (some? answer.error)
    (lspRefused answer)
    (case answer.kind
      :hover (when (= answer.request state.lsp.hover.request) (lspShowHover answer))
      :completion (when (= answer.request state.lsp.completion.request) (lspShowCompletions answer.result.items))
      :signature (lspShowSignature answer.result)
      :definition
        (if (= (count answer.result.locations) 1)
          (lspOpenLocation (first answer.result.locations))
          (lspPeek answer))
      :references (lspPeek answer)
      (:rename :format) (lspReview answer)
      :outline (when (= answer.path (or-else computed.activeDoc.id "")) (lspShowOutline answer))
      (:symbols :workspaceSymbols) (set "state.lsp.symbols" answer.result.symbols)
      :tokens (lspShowTokens answer.path answer.result.tokens)
      nil)))

(defn lspRefused "Says why the server didn't answer, but for the kinds asked as the person types." [answer]
  (case answer.kind
    (:rename :format) (notify "warning" (t "lsp.notApplied" {:error answer.error.message}))
    (:hover :completion :signature :tokens :outline) nil
    (notify "info" (t "lsp.refused" {:error answer.error.message}))))

(defn lspShowHover "The hover's text, at the place asked about." [answer]
  (set "state.lsp.hover"
       {:open (not (empty? answer.result.text))
        :text answer.result.text
        :x state.lsp.at.x
        :y state.lsp.at.y
        :request answer.request}))

(defn lspShowCompletions "The completions, the first chosen." [items]
  (set "state.lsp.completion.items" items)
  (set "state.lsp.completion.index" 0)
  (set "state.lsp.completion.open" (pos? (count items))))

(defn lspShowSignature "The signature of the call at the caret, if there is one." [result]
  (set "state.lsp.signature"
       (if (some? result)
         {:open true :label result.label :documentation result.documentation}
         {:open false :label "" :documentation ""})))

(defn lspPeek "Definitions or references, listed in the peek." [answer]
  (set "state.lsp.peek"
       {:open true
        :index 0
        :items answer.result.locations
        :title (t (if (= answer.kind "definition") "lsp.definitions" "lsp.references")
                  {:n (count answer.result.locations) :more (if answer.result.truncated "+" "")})}))

(defn lspReview "A rename's or a format's edits, to review before they apply." [answer]
  (if (empty? answer.result.edits)
    (log-line (t "lsp.nothingToChange"))
    (set "state.lsp.review"
         {:open true
          :kind answer.kind
          :edits answer.result.edits
          :files answer.result.files
          :truncated answer.result.truncated})))

(defn lspShowOutline "The active file's symbols, as the outline lists them (lines from 1)." [answer]
  (set "state.outline"
       ; style: keep, #(...) can't give a map
       (map (fn [it] {:line (inc it.line) :depth it.depth :label it.name :kind it.kind}) answer.result.symbols))
  (set "state.outlineSymbols" answer.path))

(defn lspShowTokens "A file's semantic tokens, beside its diagnostics." [path tokens]
  (set "state.lsp.files"
       (conj (remove #(= (:id %) path) state.lsp.files)
             {:id path
              :decorations (or-else (:decorations (find #(= (:id %) path) state.lsp.files)) [])
              :tokens tokens})))

;;; Diagnostics

(defn lspListen
  "Listens to the server's diagnostics, file by file, from its first request on (again after the stream ends)."
  []
  (when-not (true? state.lsp.listening)
    (set "state.lsp.listening" true)
    (subscribe "lsp.diagnostics" :id "lsp-diagnostics" :do lspListenEach :onError lspListenFailed)))

(defn lspListenEach "A file's diagnostics, as they come." [event]
  (lspPublished event))

(defn lspListenFailed "The stream ended: the next request listens again." []
  (set "state.lsp.listening" false))

(defn lspPublished
  "A file's diagnostics (`published`, from 0): squiggles in its editor, and rows (from 1) in Problems."
  [published]
  (set "state.lsp.files"
       (conj (remove #(= (:id %) published.path) state.lsp.files)
             {:id published.path
              :tokens (or-else (:tokens (find #(= (:id %) published.path) state.lsp.files)) [])
              :decorations (map (fn [it]
                                  {:line it.line
                                   :column it.column
                                   :endLine it.endLine
                                   :endColumn (if (and (= it.endLine it.line) (<= it.endColumn it.column))
                                                (inc it.column)
                                                it.endColumn)
                                   :kind "squiggle"
                                   :color (case it.severity :error "#f85149" :warning "#cca700" "#3794ff")
                                   :text it.message})
                                published.diagnostics)}))
  (set "state.problems"
       (concat (remove #(and (or-else (:server %) false) (= (:file %) published.path)) state.problems)
               (map (fn [it]
                      {:severity (if (= it.severity "hint") "info" it.severity)
                       :message (if (empty? it.source) it.message (str it.message " (" it.source ")"))
                       :file published.path
                       :label (baseName published.path)
                       :line (inc it.line)
                       :column (inc it.column)
                       :source "lsp"
                       :server true})
                    published.diagnostics)))
  (when (and (some? computed.activeDoc) (= computed.activeDoc.id published.path)) (lspAsk "tokens" nil nil nil nil)))

;;; Keeping the server's files

(defn lspChanged
  "An edited file (`id`): one the server has open gets its new text once typing pauses, so its diagnostics follow."
  [id]
  (when (contains state.lsp.open id) (debounce 500 lspChangedLater id)))

(defn lspChangedLater "Sends an edited file's text to the server." [id]
  (doseq [doc (filter #(and (= (:id %) id) (= (:kind %) "file")) state.docs)]
    (invoke "lsp.sync"
            :with {:path doc.id
                   :text doc.text
                   ; style: keep, the host's split keeps the empty text after a last dot: a name ending in one has no language
                   :language (or-else (get state.lsp.languages (str/lower-case (last (split doc.id ".")))) "cpp")}
            :onError lspChangedLaterFailed)))

(defn lspChangedLaterFailed "A text the server didn't take: nothing to do." []
  nil)

(defn lspClosed "A file no longer open in Forge (`id`): the server reads it from disk again." [id]
  (when (contains state.lsp.open id)
    (set "state.lsp.open" (remove #{id} state.lsp.open))
    (invoke "lsp.close" :with {:path id} :onError lspClosedFailed)))

(defn lspClosedFailed "A file the server didn't close: nothing to do." []
  nil)

;;; Typing

(defcomputed lspCompletions (fuzzySort state.lsp.completion.items state.lsp.completion.word "filter" 50))

(defn lspHoverAt "The pointer rests over a word: asks for its hover, or hides it." []
  (if (and (true? event.over) (some? computed.activeDoc))
    (do
      (set "state.lsp.at" {:x event.x :y event.y :top event.top})
      (lspAsk "hover" event.line event.column nil nil)
      (set "state.lsp.hover.request" state.lsp.request))
    (set "state.lsp.hover.open" false)))

(defn lspTyped "A word typed: asks for completions, and for the signature after an open bracket or a comma." []
  (set "state.lsp.at" {:x event.x :y event.y :top event.top})
  (set "state.lsp.hover.open" false)
  (set "state.lsp.completion.word" event.word)
  (set "state.lsp.completion.index" 0)
  (when (#{"(" ","} event.text) (lspAsk "signature" event.line event.column nil nil))
  (when (= event.text ")") (set "state.lsp.signature.open" false))
  (cond
    (and (empty? event.word) (not= event.text ".")) (set "state.lsp.completion.open" false)
    (or (= event.text ".") (= (count event.word) 1) (not (true? state.lsp.completion.open)))
      (do
        (lspAsk "completion" event.line event.column nil nil)
        (set "state.lsp.completion.request" state.lsp.request))))

(defn lspPopupKey "A key in the completion popup: Up and Down choose, Enter and Tab insert, Escape closes." []
  (when (or (= event.key "escape") (empty? computed.lspCompletions)) (set "state.lsp.completion.open" false))
  (let [index state.lsp.completion.index
        last-index (dec (count computed.lspCompletions))]
    (case event.key
      :down (set "state.lsp.completion.index" (min (inc index) last-index))
      :up (set "state.lsp.completion.index" (max (dec index) 0))
      :pageDown (set "state.lsp.completion.index" (min (+ index 10) last-index))
      :pageUp (set "state.lsp.completion.index" (max (- index 10) 0))
      (:enter :tab)
        (when (and (true? state.lsp.completion.open) (seq computed.lspCompletions)) (lspComplete index))
      nil)))

(defn lspComplete "Inserts a completion, by its place in the list." [index]
  (set "state.lsp.insert"
       {:text (get-in computed.lspCompletions [index :item :insert])
        :before (count state.lsp.completion.word)
        :request (inc state.lsp.insert.request)})
  (set "state.lsp.completion.open" false))

(defcommand triggerSuggest "Trigger Suggest"
  :title ~(t "command.triggerSuggest")
  :description ~(t "command.triggerSuggest") :category ~(t "category.edit") :icon "list"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "completion" nil nil nil nil)
  (set "state.lsp.completion.request" state.lsp.request)
  (set "state.lsp.completion.word" ""))

;;; Navigating

(defstate symbolPick {:open false :query ""})

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
                    :cells [(str (get (reverse (split it.path "/")) 0) ":" (inc it.line)) it.path (inc it.line)]})
                 state.lsp.peek.items))

(defcommand goToDefinition "Go to Definition"
  :title ~(t "command.goToDefinition")
  :description ~(t "command.goToDefinition") :category ~(t "category.go") :icon "arrow-right"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "definition" nil nil nil nil))

(defcommand findReferences "Find All References"
  :title ~(t "command.findReferences")
  :description ~(t "command.findReferences") :category ~(t "category.go") :icon "list"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "references" nil nil nil nil))

(defn lspOpenLocation "Opens a location (`at`: path, line, column, from 0) at its place." [at]
  (openPath at.path (baseName at.path))
  (set "state.goTo.line" (inc at.line))
  (set "state.goTo.column" (inc at.column))
  (increment "state.goTo.request" :by 1))

(defn lspPeekOpen "A row of the peek, opened." []
  (lspOpenLocation (get state.lsp.peek.items (number event.id)))
  (set "state.lsp.peek.open" false))

(defcommand goToSymbol "Go to Symbol in Editor..."
  :title ~(t "command.goToSymbol") :description ~(t "command.goToSymbol") :category ~(t "category.go") :icon "hash"
  :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.symbols" [])
  (set "state.symbolPick.query" "@")
  (set "state.symbolPick.open" true)
  (lspAsk "symbols" nil nil nil nil))

(defcommand goToWorkspaceSymbol "Go to Symbol in Workspace..."
  :title ~(t "command.goToWorkspaceSymbol")
  :description ~(t "command.goToWorkspaceSymbol") :category ~(t "category.go") :icon "hash"
  :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.symbols" [])
  (set "state.symbolPick.query" "#")
  (set "state.symbolPick.open" true))

(defn lspSymbolPicked "A symbol picked: its place opens." [item]
  (lspOpenLocation item))

(defn searchWorkspaceSymbols "A query typed in the symbol picker after #: the workspace's symbols are asked for." []
  (when (and (true? state.symbolPick.open) (str/starts-with? event.value "#") (> (count event.value) 1))
    (lspAsk "workspaceSymbols" nil nil nil (slice event.value 1))))

;;; Renaming and formatting

(defcommand renameSymbol "Rename Symbol"
  :title ~(t "command.renameSymbol")
  :description ~(t "command.renameSymbol") :category ~(t "category.edit") :icon "pencil"
  :enablement ~(some? computed.activeDoc) []
  (set "state.lsp.rename" {:open true :name ""}))

(defn lspRenameSend "Renames the symbol at the caret to the name typed." []
  (set "state.lsp.rename.open" false)
  (let [trimmed (str/trim state.lsp.rename.name)]
    (when (seq trimmed) (lspAsk "rename" nil nil trimmed nil))))

(defcommand formatDocument "Format Document"
  :title ~(t "command.formatDocument")
  :description ~(t "command.formatDocument") :category ~(t "category.edit") :icon "align-left"
  :enablement ~(some? computed.activeDoc) []
  (lspAsk "format" nil nil nil nil))

(defn lspApply
  "Applies the edits reviewed: in an open file's editor (one undo step, unsaved), else read, changed and written."
  []
  (doseq [[file edits] (group-by :path state.lsp.review.edits)]
    (lspApplyTo file (map #(select-keys % [:line :column :endLine :endColumn :text]) edits)))
  (log-line (t "lsp.applied" {:edits (count state.lsp.review.edits) :files state.lsp.review.files}))
  (set "state.lsp.review.open" false))

(defn lspApplyTo "Applies edits to a file, in its editor if it is open." [file edits]
  (if (some #(and (= (:id %) file) (= (:kind %) "file")) state.docs)
    (doseq [doc (filter #(and (= (:id %) file) (= (:kind %) "file")) state.docs)]
      (if (some? (applyEdits doc.text edits))
        (set "state.docs"
             (map (fn [it]
                    (if (= it.id file)
                      (let [edited (applyEdits it.text edits)] (merge it {:text edited :dirty (not= edited it.saved)}))
                      it))
                  state.docs))
        (notify "error" (t "lsp.overlap" {:file file}))))
    (invoke "files.read" :with {:path file} :onError [lspApplyToFailed file] :onDone [lspApplyToDone edits file])))

(defn lspApplyToDone "A file read: its edits applied, and written." [edits file result]
  (if-some [edited (applyEdits result.text edits)]
    (invoke "files.write" :with {:path file :text edited} :onError [lspApplyToDoneFailed file])
    (notify "error" (t "lsp.overlap" {:file file}))))

(defn lspApplyToFailed "Says why a file couldn't be changed." [file error]
  (notify "error" (t "lsp.notAppliedTo" {:file file :error error.message})))

(defn lspApplyToDoneFailed "Says why a changed file couldn't be written." [file error]
  (notify "error" (t "lsp.notAppliedTo" {:file file :error error.message})))

;;; Keys, in the order the keyboard shortcuts page lists them

(defkey "f12" goToDefinition)
(defkey "shift+f12" findReferences)
(defkey "f2" renameSymbol)
(defkey "shift+alt+f" formatDocument)
(defkey "ctrl+space" triggerSuggest)
(defkey "mod+shift+o" goToSymbol)
(defkey "mod+t" goToWorkspaceSymbol)
