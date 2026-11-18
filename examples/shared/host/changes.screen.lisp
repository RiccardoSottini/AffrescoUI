;;; changes.screen.lisp --- the host's own view of a change, in a window of its own
;;;
;;; What a proposal or a commit changes, file by file (apps/host/ChangesWindow.cpp), and where an element of the app is
;;; written (its Source page). It is read from the host's assets and fed only by the host, from the version control,
;;; through its native calls changes.list, changes.load, changes.source, changes.preview, changes.accept,
;;; changes.reject, changes.undoPlan, changes.undo and changes.askUndo: nothing in the change or the source it shows can
;;; alter it. The host runs its actions by name (changes.refresh...), and scenarios play the host's part with
;;; changes.show.

(ns shared.changes
  (:require "locales/changes.en.lisp"
            "locales/changes.es.lisp"
            "locales/changes.fr.lisp"
            "../languages/json.lisp"
            "../languages/lisp.lisp"))

;;; What it shows

(defstate list {:proposals [] :history [] :selectionCount 0}
  "What the host lists: the proposals waiting, the commits applied, and which it wants shown (selection).")

(defstate selectionCount 0 "How many times the host has asked for a change: a new count shows its selection.")

(defstate selected nil "The change shown: {:proposal id} or {:to commit}.")

(defstate view nil "The change shown, as the host gives it: its files, author, message, warnings and impact.")

(defstate file "" "The file of the change shown.")

(defstate text nil "The file's texts, before and after, asked for when it is shown: a change's others may be large.")

(defstate mode split "How the diff is laid out: split or inline.")

(defstate notation nil "The notation the person chose for the files, or nil for the one the host shows first.")

(defstate reason "" "Why the person rejects a proposal, told to its author.")

(defstate message "" "What the host said went wrong.")

(defstate stale false "Whether the proposal shown was updated since it was loaded.")

(defstate closed false "Whether the proposal shown has been decided.")

(defstate source nil
  "The Source page, shown in the place of the change while it is not nil: where an element is written, as the host
  answered (Screen::source with blame and marked), or {:error ...}.")

(defstate sourceCount 0 "How many times the host has been asked for a source: a new count shows the Source page.")

(defstate plan nil
  "What undoing the applied change shown would do, as the host worked it out (changes.undoPlan): the later changes
  built on it and what stays. Nil until it answers: the Undo button as it always was.")

(defstate keepPlan nil
  "The same plan if the later changes built on it stay as they are: whether \"leave these\" can be done.")

(defstate asked false "Whether the app's assistant was asked to undo the change shown another way.")

(defcomputed added "The lines the change adds." (sum (map #(or-else (:added %) 0) (or-else state.view.files []))))

(defcomputed removed "The lines it removes."
  (sum (map #(or-else (:removed %) 0) (or-else state.view.files []))))

(defcomputed isProposal "Whether a proposal is shown." (and (some? state.view) (= state.view.kind :proposal)))

(defcomputed previewing "Whether the app shows the proposal."
  (and computed.isProposal (= state.list.previewing state.view.proposal)))

(defcomputed open "Whether the proposal shown waits for a decision." (and computed.isProposal (not state.closed)))

(defcomputed canAccept "Whether the person may accept it: not changed since it was read, and no conflicts."
  (and computed.open (not state.stale) (empty? (or-else state.view.conflicts []))))

(defcomputed leftOut "What a merge left out of the proposal shown, for the person to settle: [{:path :mine :theirs}]."
  (let [waiting (find #(= (:id %) state.view.proposal) (or-else state.list.proposals []))]
    (if (and computed.open (some? waiting)) (or-else (:leftOut waiting) []) [])))

(defcomputed decided "Whether nothing waits: a commit, or a proposal decided." (not computed.open))

(defcomputed planShown "Whether the host answered a plan for the applied change shown."
  (and (some? state.plan) (some? state.view) (= state.view.kind :commit)))

(defcomputed planOutcome "What undoing it takes: clean or ready (it can be done), choose or blocked; clean before."
  (if computed.planShown (or-else state.plan.outcome :clean) :clean))

(defcomputed canUndo "Whether the plain Undo button is offered."
  (and computed.decided (contains? #{:clean :ready} computed.planOutcome)))

(defcomputed choosing "Whether later changes were built on it: the person chooses what becomes of them."
  (and computed.decided (= computed.planOutcome :choose)))

(defcomputed undoBlocked "Whether it can't be undone, and why is shown."
  (and computed.decided (= computed.planOutcome :blocked)))

(defcomputed undoDoable "Whether it can be undone in some way: the plan's quiet lines are shown."
  (and computed.decided computed.planShown (not computed.undoBlocked)))

(defcomputed leaveBlocked "Whether leaving the later changes as they are can't be done."
  (and (some? state.keepPlan) (= state.keepPlan.outcome :blocked)))

(defcomputed canAsk "Whether the app's assistant can be asked to undo it and keep what was built on it."
  (and (= state.list.canAsk true) (or computed.choosing computed.undoBlocked)))

(defcomputed askText "The request to the assistant, in the person's language, built from the plan."
  (if (pos? (count (or-else state.plan.later [])))
    (t "changes.askKeep"
       {:message state.plan.of.message
        :n state.plan.of.number
        :later (join "; " (map #(t "changes.askLater" {:n (:number %) :message (:message %)}) state.plan.later))})
    (t "changes.askUndo" {:message state.plan.of.message :n state.plan.of.number})))

(defcomputed extras "What the change adds beside its files: its assets, then its steps that run once."
  (concat (map #(if (true? (:removed %)) (t "changes.removed" {:name (:name %)}) (:name %)) (or-else state.view.assets []))
          (map #(json %) (or-else state.view.run []))))

;;; What the host asks for, and what it answers

(defn ignored "Nothing: a call that failed changes nothing." [])

(defn failed "Says what went wrong." [error] (set "state.message" error))

(defn refreshChanges "Every half second: what the host wants shown, and whether what is shown has moved on." []
  (call "changes.list" :into "state.list" :then listed :onError ignored))

(defn listed "Shows what the host asks for; or notes that the proposal shown was decided or updated." []
  (cond (not= (or-else state.list.sourceCount 0) state.sourceCount)
        (do (set "state.sourceCount" state.list.sourceCount)
            (showSource state.list.source))
        (not= state.list.selectionCount state.selectionCount)
        (do (set "state.selectionCount" state.list.selectionCount)
            (selectChange state.list.selection))
        computed.open
        (if-some [waiting (find #(= (:id %) state.view.proposal) state.list.proposals)]
          (when (not= (:head waiting) state.view.head)
            (set "state.stale" true)
            (loadChange))
          (set "state.closed" true))))

(defn selectChange "Shows a change: {:proposal id} or {:to commit}." [what]
  (set "state.selected" what)
  (set "state.stale" false)
  (set "state.closed" false)
  (set "state.reason" "")
  (set "state.message" "")
  (set "state.file" "")
  (set "state.text" nil)
  (set "state.source" nil)
  (set "state.plan" nil)
  (set "state.keepPlan" nil)
  (set "state.asked" false)
  (loadChange))

(defn selectProposal "Shows a proposal." [id] (selectChange {:proposal id}))

(defn selectCommit "Shows a commit." [id] (selectChange {:to id}))

(defn loadChange "Asks the host for the change shown, in the notation chosen; then shows a file of it." []
  (when (some? state.selected)
    (call "changes.load"
          :with (if-some [notation state.notation] (assoc state.selected :notation notation) state.selected)
          :into "state.view"
          :then loaded
          :onError failed)))

(defn loaded "Shows the file shown before, if the change has it, or its first; for a commit, what undoing it takes." []
  (openFile (if (some #(= (:file %) state.file) state.view.files)
              state.file
              (or (:file (first state.view.files)) "")))
  (planUndo))

(defn planUndo "Asks the host what undoing the commit shown would do: nothing changes until the person chooses." []
  (when (and (some? state.view) (= state.view.kind :commit) (> state.view.revision 0))
    (call "changes.undoPlan" :with {:commit state.view.commit} :into "state.plan" :then planned :onError ignored)))

(defn planned "When later changes were built on it, asks too whether they can stay as they are." []
  (if (= state.plan.outcome :choose)
    (call "changes.undoPlan"
          :with {:commit state.view.commit :keep "all"}
          :into "state.keepPlan"
          :onError ignored)
    (set "state.keepPlan" nil)))

(defn openFile "Asks the host for one file's texts, before and after." [file]
  (set "state.file" file)
  (if (= file "")
    (set "state.text" nil)
    (call "changes.load"
          :with (assoc state.selected :file file :notation state.view.notation)
          :into "state.opened"
          :then [fileLoaded file]
          :onError failed)))

(defn fileLoaded "Shows the file's texts." [file]
  (set "state.text" (find #(= (:file %) file) state.opened.files)))

(defn setNotation "Shows the change's files in JSON or in Lisp." [notation]
  (set "state.notation" notation)
  (loadChange))

(defn setMode "Lays the diff out split or inline." [mode] (set "state.mode" mode))

(defn showSource "Shows the Source page for what the app asked: {:source at :line n}, the line optional." [asked]
  (set "state.message" "")
  (set "state.source" {:at asked.source})
  (call "changes.source"
        :with {:at asked.source :line asked.line}
        :into "state.source"
        :then sourceLoaded
        :onError failed))

(defn sourceLoaded "Says what the host answered went wrong, if anything: the page stays, with the error." []
  (set "state.message" (or (:error state.source) "")))

(defn showForScenario
  "For scenarios, in the host's place: event.list (with canAsk), event.selected, event.view, event.text, event.source,
  what changes.source answers, for the Source page, and event.plan and event.keepPlan, what changes.undoPlan answers."
  []
  (set "state.selectionCount" (or event.list.selectionCount 0))
  (set "state.sourceCount" (or event.list.sourceCount 0))
  (set "state.list" event.list)
  (set "state.selected" event.selected)
  (set "state.view" event.view)
  (set "state.text" event.text)
  (set "state.file" event.text.file)
  (set "state.source" event.source)
  (set "state.plan" event.plan)
  (set "state.keepPlan" event.keepPlan)
  (set "state.asked" false))

(defn answered "Says what the host answered went wrong, if anything; then looks again." [result]
  (set "state.message" (or (:error result) ""))
  (refreshChanges))

(defn previewChange "Shows the proposal in the app, or stops showing it." []
  (call "changes.preview" :with {:proposal state.view.proposal} :then answered :onError ignored))

(defn acceptChange "Accepts the proposal, as the person read it (its head)." []
  (call "changes.accept"
        :with {:proposal state.view.proposal :head state.view.head}
        :then accepted
        :onError ignored))

(defn accepted "The proposal is decided, unless the host refused." [result]
  (when (nil? (:error result)) (set "state.closed" true))
  (answered result))

(defn resolvePart "Settles a part a merge left out of the proposal: \"mine\" keeps main's value, \"theirs\" takes the
  proposal's (its head moves, so the change is shown again before it is accepted)." [path take]
  (call "changes.resolve"
        :with {:proposal state.view.proposal :path path :take take}
        :then resolved
        :onError ignored))

(defn resolved "Shows the proposal again as it is now, unless the host refused." [result]
  (if (some? (:error result)) (answered result) (selectChange state.selected)))

(defn rejectChange "Rejects the proposal, telling its author why." []
  (call "changes.reject" :with {:proposal state.view.proposal :reason state.reason} :then rejected :onError ignored))

(defn rejected "The proposal is decided." []
  (set "state.closed" true)
  (refreshChanges))

(defn undone "Says what the host answered went wrong, if anything; then looks again, and plans the undo again." [result]
  (set "state.plan" nil)
  (set "state.keepPlan" nil)
  (answered result)
  (planUndo))

(defn undoChange "Undoes the commit shown, keeping everything after it, as a new commit by the person." []
  (call "changes.undo" :with {:commit state.view.commit} :then undone :onError ignored))

(defn undoDropping "Undoes the commit shown and takes out the later changes built on it, as the plan showed them." []
  (call "changes.undo"
        :with {:commit state.view.commit :drop "all" :head state.plan.head}
        :then undone
        :onError ignored))

(defn undoLeaving "Undoes the rest of the commit shown, and leaves the later changes built on it as the plan showed." []
  (call "changes.undo"
        :with {:commit state.view.commit :keep "all" :head state.plan.head}
        :then undone
        :onError ignored))

(defn askAssistant "Asks the app's assistant to undo the commit shown and keep what was built on it: it proposes." []
  (call "changes.askUndo" :with {:commit state.view.commit :text computed.askText}
    :then assistantAsked :onError ignored))

(defn assistantAsked "Says the assistant was asked, or what the host answered went wrong." [result]
  (set "state.message" (or (:error result) ""))
  (set "state.asked" (nil? (:error result))))

(defpart host "The actions the host and scenarios run by name."
  :actions {:changes.refresh [{:lisp refreshChanges}]
            :changes.show [{:lisp showForScenario}]
            :changes.select [{:lisp selectChange}]
            :changes.load [{:lisp loadChange}]
            :changes.open [{:lisp openFile}]
            :changes.setNotation [{:lisp setNotation}]
            :changes.preview [{:lisp previewChange}]
            :changes.accept [{:lisp acceptChange}]
            :changes.reject [{:lisp rejectChange}]
            :changes.undo [{:lisp undoChange}]
            :changes.planUndo [{:lisp planUndo}]
            :changes.undoDropping [{:lisp undoDropping}]
            :changes.undoLeaving [{:lisp undoLeaving}]
            :changes.askUndo [{:lisp askAssistant}]})

;;; The look

(deftokens colours "The window's colours, light and dark."
  {:c.background {:light "#f6f5fb" :dark "#111118"}
   :c.surface {:light "#ffffff" :dark "#1c1c26"}
   :c.raised {:light "#f0eff7" :dark "#262633"}
   :c.selected {:light "#e6e3fb" :dark "#30304a"}
   :c.border {:light "#e3e1ee" :dark "#2b2b39"}
   :c.text {:light "#1b1a28" :dark "#ececf4"}
   :c.muted {:light "#5d5b72" :dark "#a4a2b9"}
   :c.accent {:light "#5b4fe0" :dark "#8b80ff"}
   :c.onAccent {:light "#ffffff" :dark "#111118"}
   :c.warning {:light "#9a5b00" :dark "#f1b65a"}
   :c.deleted {:light "#fbd9d9" :dark "#4a2428"}
   :c.inserted {:light "#d5f2dc" :dark "#1f3f2a"}
   :c.marked {:light "#fbefc4" :dark "#3d3519"}
   :syntax.keyword {:light "#6a3fc8" :dark "#b39cf0"}
   :syntax.type {:light "#1f6f9a" :dark "#7cc4ea"}
   :syntax.string {:light "#2f7a3e" :dark "#8fd19e"}
   :syntax.number {:light "#a2541c" :dark "#f0a46a"}
   :syntax.comment {:light "#7a788c" :dark "#7f7d94"}})

;;; The list on the left: the proposals waiting, then the commits applied

(defview proposal-row "A proposal waiting: its message, and who it is by." [p]
  `(node :id ~(str "changesProposal" p.id)
     :role button
     :focusable true
     :cursor hand
     :height 52
     :radius 8
     :anchors {:left 0 :right 0}
     :color ~(if (and computed.isProposal (= state.view.proposal p.id)) "@c.selected" "transparent")
     :selected ~(and computed.isProposal (= state.view.proposal p.id))
     :label ~p.message
     :onClick ~(handler selectProposal p.id)
     (label :text ~p.message :textColor @c.text :anchors {:left 10 :right 10 :top 8} :height 18)
     (label :text ~(t "changes.by" {:author p.author})
       :textColor @c.muted
       :font small
       :anchors {:left 10 :right 10 :top 28}
       :height 16)))

(defview commit-row "A commit applied: its message (the first, the file as loaded), its number and its author." [c]
  `(node :id ~(str "changesCommit" c.number)
     :role button
     :focusable true
     :cursor hand
     :height 52
     :radius 8
     :anchors {:left 0 :right 0}
     :color ~(if (and (some? state.view) (= state.view.commit c.id)) "@c.selected" "transparent")
     :selected ~(and (some? state.view) (= state.view.commit c.id))
     :label ~(if (zero? c.number) (t "changes.original") c.message)
     :onClick ~(handler selectCommit c.id)
     (label :text ~(if (zero? c.number) (t "changes.original") c.message)
       :textColor @c.text
       :anchors {:left 10 :right 10 :top 8}
       :height 18)
     (label :text ~(str (t "changes.revision" {:n c.number}) " · " c.author)
       :textColor @c.muted
       :font small
       :anchors {:left 10 :right 10 :top 28}
       :height 16)))

(defview side "The proposals waiting, then the commits applied." []
  `(column#changesSide :width 290 :gap 6 :alignItems stretch :anchors {:top 0 :bottom 0} :scroll vertical
     (label#changesProposalsTitle :when ~(pos? (count state.list.proposals))
       :text ~(t "changes.proposals")
       :font small
       :textColor @c.muted
       :height 20)
     ~(repeat [p state.list.proposals] (proposal-row p))
     (label#changesHistoryTitle :text ~(t "changes.history") :font small :textColor @c.muted :height 20)
     ~(repeat [c state.list.history] (commit-row c))))

;;; The change shown: its title, what it says, its files and a file's diff

(defview face "A button's text, centred." [text color]
  `(label :text ~text :textColor ~color :anchors {:left 10 :right 10 :top 7} :height 18 :align center))

(defview mode-button "Lays the diff out one way: split or inline." [id width mode key]
  `(node :id ~id
     :role button
     :focusable true
     :cursor hand
     :height 32
     :radius 8
     :color ~(if (= state.mode mode) "@c.selected" "@c.raised")
     :borderWidth 1
     :borderColor @c.border
     :label ~(t key)
     :focused {:borderColor @c.accent :borderWidth 1}
     :onClick ~(handler setMode mode)
     :width ~width
     :selected ~(= state.mode mode)
     (label :text ~(t key) :textColor @c.text :anchors {:left 10 :right 10 :top 7} :height 18 :align center)))

(defview notation-button "Shows the files in one notation: JSON or Lisp." [id width notation text]
  `(node :id ~id
     :role button
     :focusable true
     :cursor hand
     :height 32
     :radius 8
     :color ~(if (= state.view.notation notation) "@c.selected" "@c.raised")
     :borderWidth 1
     :borderColor @c.border
     :label ~text
     :focused {:borderColor @c.accent :borderWidth 1}
     :onClick ~(handler setNotation notation)
     :width ~width
     :selected ~(= state.view.notation notation)
     ~(face text "@c.text")))

(defview tools "The file's name, then how its diff is laid out and in which notation." []
  `(row#changesTools :gap 6 :height 32
     (label#changesFileName :text ~state.file :font code :textColor @c.text :grow 1 :height 32 :align "start center")
     ~(mode-button "changesSplit" 96 :split "changes.split")
     ~(mode-button "changesInline" 88 :inline "changes.inline")
     ~(notation-button "changesJson" 60 :json "JSON")
     ~(notation-button "changesLisp" 60 :lisp "Lisp")))

(defview file-row "A file of the change: its name, and the lines it adds and removes." [f]
  `(node :id ~(str "changesFile" f.file)
     :role button
     :focusable true
     :cursor hand
     :height 52
     :radius 8
     :anchors {:left 0 :right 0}
     :color ~(if (= state.file f.file) "@c.selected" "transparent")
     :selected ~(= state.file f.file)
     :label ~f.file
     :onClick ~(handler openFile f.file)
     (label :text ~f.file :textColor @c.text :anchors {:left 10 :right 10 :top 8} :height 18)
     (label :text ~(if (true? f.tooLarge) (t "changes.tooLarge") (str "+" f.added " −" f.removed))
       :textColor @c.muted
       :font small
       :anchors {:left 10 :right 10 :top 28}
       :height 16)))

(defview files "The change's files, then what it adds beside them." []
  `(column#changesFiles :width 240 :gap 2 :alignItems stretch :anchors {:top 0 :bottom 0} :scroll vertical
     (label#changesFilesTitle :text ~(t "changes.files") :font small :textColor @c.muted :height 20)
     (label#changesNoFiles :when ~(empty? state.view.files)
       :text ~(t "changes.none")
       :textColor @c.muted
       :height 20)
     ~(repeat [f state.view.files] (file-row f))
     (label#changesExtrasTitle :when ~(pos? (count computed.extras))
       :text ~(str (t "changes.assets") " · " (t "changes.run"))
       :font small
       :textColor @c.muted
       :height 20)
     ~(repeat [x computed.extras :index index]
       `(label :id ~(str "changesExtra" index) :text ~x :textColor @c.text :font small :height 18))))

(defview pane "The file shown: the tools, its diff, or why it isn't shown." []
  `(column#changesPane :grow 1 :gap 6 :alignItems stretch :anchors {:top 0 :bottom 0}
     ~(tools)
     (diff#changesDiff :when ~(and (some? state.text) (not= state.text.tooLarge true))
       :grow 1
       :font code
       :textColor @c.text
       :original ~(or-else state.text.before "")
       :modified ~(or-else state.text.after "")
       :mode ~state.mode
       :deletedColor @c.deleted
       :insertedColor @c.inserted)
     (label#changesTooLarge :when ~(and (some? state.text) (true? state.text.tooLarge))
       :text ~(t "changes.tooLarge")
       :textColor @c.muted
       :height 24)
     (label#changesLayoutNote :when ~(some? state.text)
       :text ~(t "changes.canonical")
       :font small
       :textColor @c.muted
       :height 16)))

(defview heading "The change's title, who it is by, how much it changes, and what to look out for." []
  `((label#changesPick :when ~(nil? state.view) :text ~(t "changes.pick") :textColor @c.muted :height 24)
    (label#changesTitle :when ~(some? state.view)
      :font heading
      :text ~state.view.message
      :textColor @c.text
      :height 26
      :role heading)
    ~(change-summary)
    (label#changesStale :when ~(and state.stale (not state.closed))
      :wrap true :text ~(t "changes.stale") :textColor @c.warning :anchors {:left 0 :right 0} :height 36)
    (label#changesClosed :when ~state.closed
      :wrap true :text ~(t "changes.closed") :textColor @c.muted :anchors {:left 0 :right 0} :height 36)
    (label#changesConflicts :when ~(pos? (count (or-else state.view.conflicts [])))
      :wrap true :text ~(t "changes.conflicts") :textColor @c.warning :anchors {:left 0 :right 0} :height 36)
    (label#changesPerson :when ~(pos? (count (or-else state.view.person [])))
      :wrap true
      :text ~(t "changes.person" {:lines (join "; " state.view.person)})
      :textColor @c.warning
      :anchors {:left 0 :right 0}
      :height 36)
    (label#changesBehaviour :when ~(pos? (count (or-else state.view.behaviour [])))
      :wrap true
      :text ~(t "changes.behaviour"
                {:lines (str (join "; " (slice state.view.behaviour 0 3))
                             (if (> (count state.view.behaviour) 3) "; …" ""))})
      :textColor @c.warning
      :anchors {:left 0 :right 0}
      :height 36)))

(defview change-summary "Who the change is by, how many files and lines it changes, and in which layers." []
  `(label#changesSummary :when ~(some? state.view)
     :text ~(str (t "changes.by" {:author state.view.author})
                 "   "
                 (t "changes.filesSummary"
                    {:n (count state.view.files) :added computed.added :removed computed.removed})
                 (if-some [impact state.view.impact]
                   (str "   " (t "changes.layers" {:view impact.view :logic impact.logic :data impact.data}))
                   ""))
     :textColor @c.muted
     :height 20))

;;; The Source page: where an element is written, shown in the place of the change

(defcomputed sourceShown "Whether the host answered with a source to show, not an error."
  (and (some? state.source) (some? state.source.text)))

(defcomputed sourceName
  "The source's place: its file, the definition's kind and name for a program or the node for a JSON document, and
  the line marked."
  (join " · "
        (filter #(some? %)
                [state.source.file
                 (if (some? state.source.kind)
                   (str state.source.kind " " (->> (str/split state.source.address "/") reverse first))
                   state.source.node)
                 (when-some [line state.source.marked] (t "changes.line" {:line line}))])))


(defview source-blame "Who last changed the source, and a button that shows that change." []
  `(row#changesSourceBlame :when ~(some? state.source.blame) :gap 8 :height 32
     (label#changesSourceLastChanged
       :text ~(str (t "changes.lastChanged" {:number state.source.blame.number :author state.source.blame.author})
                   ": "
                   state.source.blame.message)
       :textColor @c.muted
       :grow 1
       :height 32
       :align "start center")
     (node#changesSourceShowChange
       :role button
       :focusable true
       :cursor hand
       :height 32
       :radius 8
       :color @c.raised
       :borderWidth 1
       :borderColor @c.border
       :label ~(t "changes.showChange")
       :focused {:borderColor @c.accent :borderWidth 1}
       :onClick ~(handler selectCommit state.source.blame.id)
       :width 170
       (label :text ~(t "changes.showChange")
         :textColor @c.text
         :anchors {:left 10 :right 10 :top 7}
         :height 18
         :align center))))

(defview source-page "Where an element is written: its place, who last changed it, and its text, the line marked." []
  `(column#changesSource :when ~(some? state.source) :grow 1 :gap 8 :alignItems stretch :anchors {:top 0 :bottom 0}
     (label#changesSourceTitle :font heading :text ~(t "changes.source") :textColor @c.text :height 26 :role heading)
     (label#changesSourceName :when ~computed.sourceShown
       :text ~computed.sourceName
       :font code
       :textColor @c.text
       :anchors {:left 0 :right 0}
       :height 20)
     (label#changesSourcePreviewed :when ~(and computed.sourceShown (some? state.list.previewing))
       :wrap true
       :text ~(t "changes.sourcePreviewed" {:n state.list.previewing})
       :textColor @c.warning
       :anchors {:left 0 :right 0}
       :height 20)
     (label#changesSourceShared :when ~(= state.source.shared true)
       :wrap true
       :text ~(t "changes.sourceShared")
       :textColor @c.warning
       :anchors {:left 0 :right 0}
       :height 20)
     (label#changesSourceCut :when ~(= state.source.cut true)
       :wrap true
       :text ~(t "changes.sourceCut")
       :textColor @c.muted
       :anchors {:left 0 :right 0}
       :height 20)
     ~(source-blame)
     (editor#changesSourceText :when ~computed.sourceShown ; the marked line is the caret's, moved there by goTo
       :grow 1
       :font code
       :textColor @c.text
       :color @c.surface
       :text ~state.source.text
       :readOnly true
       :lineNumbers true
       :lineNumberColor @c.muted
       :activeLineNumberColor @c.text
       :lineColor ~(if (some? state.source.marked) "@c.marked" "transparent")
       :language ~state.source.notation
       :goTo ~(when-some [marked state.source.marked] {:line (dec marked) :column 0 :request state.sourceCount}))
     (label#changesSourceMessage :when ~(not= state.message "")
       :wrap true
       :text ~state.message
       :textColor @c.warning
       :height 36)))

;;; Undoing an applied change: what it takes, before it happens

(defview later-row "A later change built on the one to undo: its message, number, author and the parts it changed." [c]
  `(node :id ~(str "changesUndoLater" c.number)
     :height 40
     :radius 8
     :color @c.raised
     :anchors {:left 0 :right 0}
     (label :text ~c.message :textColor @c.text :anchors {:left 10 :right 10 :top 3} :height 18)
     (label :text ~(str (t "changes.revision" {:n c.number}) " · " c.author " · " (join ", " (or-else c.parts [])))
       :textColor @c.muted
       :font small
       :anchors {:left 10 :right 10 :top 21}
       :height 16)))

(defview undo-plan
  "What undoing the applied change shown takes: why it can't be, or the later changes built on it; what stays as it is,
  and what to look out for. Nothing while there is no plan."
  []
  `((label#changesUndoBlocked :when ~computed.undoBlocked
      :wrap true
      :text ~(t "changes.undoBlocked" {:reason state.plan.blocked})
      :textColor @c.warning
      :anchors {:left 0 :right 0}
      :height 36)
    (label#changesUndoLaterTitle :when ~computed.choosing
      :text ~(t "changes.builtOn")
      :font small
      :textColor @c.muted
      :height 20)
    ~(repeat [c (if computed.choosing state.plan.later [])] (later-row c))
    (label#changesUndoLaterMore :when ~(and computed.choosing (pos? (or-else state.plan.laterMore 0)))
      :text ~(t "changes.laterMore" {:n state.plan.laterMore})
      :font small
      :textColor @c.muted
      :height 18)
    (label#changesUndoUntouched :when ~(and computed.undoDoable (pos? (or-else state.plan.untouched 0)))
      :text ~(t "changes.untouched" {:n state.plan.untouched})
      :font small
      :textColor @c.muted
      :height 18)
    (label#changesUndoStateKept :when ~(and computed.undoDoable (pos? (count (or-else state.plan.state.kept []))))
      :text ~(t "changes.stateKept")
      :font small
      :textColor @c.muted
      :height 18)
    ~(repeat [w (if computed.undoDoable (or-else state.plan.warnings []) []) :index index]
      `(label :id ~(str "changesUndoWarning" index) :text ~w :textColor @c.warning :font small :height 18))
    (label#changesUndoAsked :when ~state.asked
      :wrap true
      :text ~(t "changes.asked")
      :font small
      :textColor @c.muted
      :anchors {:left 0 :right 0}
      :height 18)))

(defview leave-button "Undo the rest and leave the later changes: offered while choosing, shown off if it can't be." []
  `(node#changesUndoLeaving :when ~computed.choosing
     :role button
     :focusable true
     :cursor hand
     :height 32
     :radius 8
     :color @c.raised
     :borderWidth 1
     :borderColor @c.border
     :label ~(t "changes.undoLeaving")
     :disabled ~computed.leaveBlocked
     :focused {:borderColor @c.accent :borderWidth 1}
     :onClick undoLeaving
     :width 230
     (label :text ~(t "changes.undoLeaving")
       :textColor ~(if computed.leaveBlocked "@c.muted" "@c.text")
       :anchors {:left 10 :right 10 :top 7}
       :height 18
       :align center)))

;;; What a merge left out

(defview left-out-text "One side of a part left out: its text, read only." [id text]
  `(editor :id ~id
     :grow 1
     :height 96
     :font code
     :textColor @c.text
     :color @c.surface
     :borderWidth 1
     :borderColor @c.border
     :radius 8
     :text ~text
     :readOnly true))

(defview left-out-row "A part a merge left out: its path, main's and the proposal's texts side by side, and the choice."
  [part index]
  `(column :id ~(str "changesLeftOut" index) :height 182 :gap 6 :alignItems stretch :anchors {:left 0 :right 0}
     (label :text ~part.path :font code :textColor @c.text :height 18)
     (row :gap 8 :height 18
       (label :text ~(t "changes.mine") :font small :textColor @c.muted :grow 1 :height 18)
       (label :text ~(t "changes.theirs") :font small :textColor @c.muted :grow 1 :height 18))
     (row :gap 8 :height 96
       ~(left-out-text (str "changesLeftOutMine" index) 'part.mine)
       ~(left-out-text (str "changesLeftOutTheirs" index) 'part.theirs))
     (row :gap 8 :height 32
       (node :grow 1 :height 32 :color transparent)
       (node :id ~(str "changesKeepMine" index)
         :role button
         :focusable true
         :cursor hand
         :height 32
         :radius 8
         :color @c.raised
         :borderWidth 1
         :borderColor @c.border
         :label ~(t "changes.keepMine")
         :focused {:borderColor @c.accent :borderWidth 1}
         :onClick ~(handler resolvePart part.path "mine")
         :width 120
         (label :text ~(t "changes.keepMine") :textColor @c.text :anchors {:left 10 :right 10 :top 7} :height 18 :align center))
       (node :id ~(str "changesUseTheirs" index)
         :role button
         :focusable true
         :cursor hand
         :height 32
         :radius 8
         :color @c.raised
         :borderWidth 1
         :borderColor @c.border
         :label ~(t "changes.useTheirs")
         :focused {:borderColor @c.accent :borderWidth 1}
         :onClick ~(handler resolvePart part.path "theirs")
         :width 120
         (label :text ~(t "changes.useTheirs") :textColor @c.text :anchors {:left 10 :right 10 :top 7} :height 18 :align center)))))

(defview not-merged
  "What a merge left out of the proposal: each part, for the person to keep or take, one in view and more scrolling.
  Nothing if none."
  []
  `(column#changesNotMerged :when ~(pos? (count computed.leftOut))
     :height 262 ; its title, its note and one part; more scroll
     :scroll vertical
     :gap 8
     :alignItems stretch
     :anchors {:left 0 :right 0}
     (label#changesNotMergedTitle :text ~(t "changes.notMerged") :font small :textColor @c.muted :height 20)
     (label#changesNotMergedNote :wrap true :text ~(t "changes.notMergedNote") :textColor @c.warning
       :anchors {:left 0 :right 0} :height 36)
     ~(repeat [part computed.leftOut :index index] (left-out-row part index))))

;;; Deciding

(defview action-button "A button that decides: its text from a key, shown while `shown`."
  [id width shown accent key on-click]
  `(node :id ~id
     :when ~shown
     :role button
     :focusable true
     :cursor hand
     :height 32
     :radius 8
     :color ~(if accent "@c.accent" "@c.raised")
     :borderWidth ~(if accent 0 1)
     :borderColor @c.border
     :label ~(t key)
     :focused {:borderColor @c.accent :borderWidth 1}
     :onClick ~on-click
     :width ~width
     (label :text ~(t key)
       :textColor ~(if accent "@c.onAccent" "@c.text")
       :anchors {:left 10 :right 10 :top 7}
       :height 18
       :align center)))

(defview actions
  "Why, Preview, Reject and Accept for a proposal; Undo for a commit after the first, or, when later changes were built
  on it, the choices of what becomes of them."
  []
  `(row#changesActions
     :when ~(or computed.open (and (some? state.view) (= state.view.kind :commit) (pos? state.view.revision)))
     :gap 8
     :height 32
     (textarea#changesReason :when ~computed.open
       :bind state.reason
       :placeholder ~(t "changes.reason")
       :grow 1
       :height 32
       :color @c.surface
       :textColor @c.text
       :borderWidth 1
       :borderColor @c.border
       :radius 8
       :label ~(t "changes.reason"))
     (node#changesSpacer :when ~(not computed.open) :grow 1 :height 32 :color transparent)
     (node#changesPreview :when ~computed.open
       :role button
       :focusable true
       :cursor hand
       :height 32
       :radius 8
       :color @c.raised
       :borderWidth 1
       :borderColor @c.border
       :label ~(t (if computed.previewing "changes.endPreview" "changes.preview"))
       :focused {:borderColor @c.accent :borderWidth 1}
       :onClick previewChange
       :width 130
       (label :text ~(t (if computed.previewing "changes.endPreview" "changes.preview"))
         :textColor @c.text
         :anchors {:left 10 :right 10 :top 7}
         :height 18
         :align center))
     ~(action-button "changesReject" 96 'computed.open false "changes.reject" "rejectChange")
     ~(action-button "changesAccept" 96 'computed.canAccept true "changes.accept" "acceptChange")
     ~(action-button "changesUndo" 170 'computed.canUndo false "changes.undo" "undoChange")
     ~(action-button "changesUndoDropping" 240 'computed.choosing false "changes.undoDropping" "undoDropping")
     ~(leave-button)
     ~(action-button "changesAskUndo" 260 'computed.canAsk false "changes.askAssistant" "askAssistant")))

;;; The window

(defscreen main
  "The list on the left; on the right the change shown, its files and their diff, and the decision, or the Source page."
  :window {:title "Changes" :width 1120 :height 740 :minWidth 760 :minHeight 480}
  :theme ~window.colorScheme
  :onLoad refreshChanges
  :fonts {:default {:file "../fonts/Roboto-Regular.ttf" :size 14 :fallback [icons]}
          :small {:file "../fonts/Roboto-Regular.ttf" :size 12}
          :heading {:file "../fonts/Roboto-Regular.ttf" :size 18 :bold true}
          :code {:file "../fonts/JetBrainsMono-Regular.ttf" :size 13}
          :icons {:file "../fonts/Lucide.ttf" :size 15}}
  :strings {:default en}
  (node#changesRoot :color @c.background :every {:ms 500 :do refreshChanges}
    (row#changesLayout :fill 16 :gap 16
      ~(side)
      ~(source-page)
      (column#changesMain :when ~(nil? state.source) :grow 1 :gap 10 :alignItems stretch :anchors {:top 0 :bottom 0}
        ~@(heading)
        (row#changesBody :when ~(some? state.view) :grow 1 :gap 12
          ~(files)
          ~(pane))
        ~(not-merged)
        ~@(undo-plan)
        (label#changesMessage :when ~(not= state.message "")
          :wrap true
          :text ~state.message
          :textColor @c.warning
          :height 36)
        ~(actions)))))
