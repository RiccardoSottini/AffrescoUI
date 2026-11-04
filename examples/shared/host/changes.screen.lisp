;;; changes.screen.lisp --- the host's own view of a change, in a window of its own
;;;
;;; What a proposal or a commit changes, file by file (apps/host/ChangesWindow.cpp). It is read from the host's assets
;;; and fed only by the host, from the version control, through its native calls changes.list, changes.load,
;;; changes.preview, changes.accept, changes.reject and changes.undo: nothing in the change it shows can alter it. The
;;; host runs its actions by name (changes.refresh...), and scenarios play the host's part with changes.show.

(include "locales/changes.en.lisp"
         "locales/changes.es.lisp"
         "locales/changes.fr.lisp")

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

(defcomputed added "The lines the change adds." (sum (map (fn [f] (or-else f.added 0)) (or-else state.view.files []))))

(defcomputed removed "The lines it removes."
  (sum (map (fn [f] (or-else f.removed 0)) (or-else state.view.files []))))

(defcomputed isProposal "Whether a proposal is shown." (and (some? state.view) (= state.view.kind "proposal")))

(defcomputed previewing "Whether the app shows the proposal."
  (and computed.isProposal (= state.list.previewing state.view.proposal)))

(defcomputed open "Whether the proposal shown waits for a decision." (and computed.isProposal (not state.closed)))

(defcomputed canAccept "Whether the person may accept it: not changed since it was read, and no conflicts."
  (and computed.open (not state.stale) (= (count (or-else state.view.conflicts [])) 0)))

(defcomputed decided "Whether nothing waits: a commit, or a proposal decided." (not computed.open))

(defcomputed extras "What the change adds beside its files: its assets, then its steps that run once."
  (concat (map (fn [a] (if (= a.removed true) (t "changes.removed" {:name a.name}) a.name))
               (or-else state.view.assets []))
          (map (fn [r] (json r)) (or-else state.view.run []))))

;;; What the host asks for, and what it answers

(defn ignored "Nothing: a call that failed changes nothing." [])

(defn failed "Says what went wrong." [error] (set "state.message" error))

(defn refreshChanges "Every half second: what the host wants shown, and whether what is shown has moved on." []
  (call "changes.list" :into "state.list" :then listed :onError ignored))

(defn listed "Shows what the host asks for; or notes that the proposal shown was decided or updated." []
  (cond (not= state.list.selectionCount state.selectionCount)
        (do (set "state.selectionCount" state.list.selectionCount)
            (selectChange state.list.selection))
        computed.open
        (let [waiting (find (fn [p] (= (:id p) state.view.proposal)) state.list.proposals)]
          (cond (nil? waiting) (set "state.closed" true)
                (not= (:head waiting) state.view.head) (do (set "state.stale" true)
                                                           (loadChange))))))

(defn selectChange "Shows a change: {:proposal id} or {:to commit}." [what]
  (set "state.selected" what)
  (set "state.stale" false)
  (set "state.closed" false)
  (set "state.reason" "")
  (set "state.message" "")
  (set "state.file" "")
  (set "state.text" nil)
  (loadChange))

(defn selectProposal "Shows a proposal." [id] (selectChange {:proposal id}))

(defn selectCommit "Shows a commit." [id] (selectChange {:to id}))

(defn loadChange "Asks the host for the change shown, in the notation chosen; then shows a file of it." []
  (when (some? state.selected)
    (call "changes.load"
          :with (if (nil? state.notation) state.selected (merge state.selected {:notation state.notation}))
          :into "state.view"
          :then loaded
          :onError failed)))

(defn loaded "Shows the file shown before, if the change has it, or its first." []
  (openFile (if (some (fn [f] (= (:file f) state.file)) state.view.files)
              state.file
              (or (:file (first state.view.files)) ""))))

(defn openFile "Asks the host for one file's texts, before and after." [file]
  (set "state.file" file)
  (if (= file "")
    (set "state.text" nil)
    (call "changes.load"
          :with (merge state.selected {:file file :notation state.view.notation})
          :into "state.opened"
          :then [fileLoaded file]
          :onError failed)))

(defn fileLoaded "Shows the file's texts." [file]
  (set "state.text" (find (fn [f] (= (:file f) file)) state.opened.files)))

(defn setNotation "Shows the change's files in JSON or in Lisp." [notation]
  (set "state.notation" notation)
  (loadChange))

(defn setMode "Lays the diff out split or inline." [mode] (set "state.mode" mode))

(defn showForScenario "For scenarios, in the host's place: event.list, event.selected, event.view, event.text." []
  (set "state.selectionCount" (or event.list.selectionCount 0))
  (set "state.list" event.list)
  (set "state.selected" event.selected)
  (set "state.view" event.view)
  (set "state.text" event.text)
  (set "state.file" event.text.file))

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

(defn rejectChange "Rejects the proposal, telling its author why." []
  (call "changes.reject" :with {:proposal state.view.proposal :reason state.reason} :then rejected :onError ignored))

(defn rejected "The proposal is decided." []
  (set "state.closed" true)
  (refreshChanges))

(defn undoChange "Undoes the commit shown, keeping everything after it, as a new commit by the person." []
  (call "changes.undo" :with {:commit state.view.commit} :then answered :onError ignored))

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
            :changes.undo [{:lisp undoChange}]})

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
   :c.inserted {:light "#d5f2dc" :dark "#1f3f2a"}})

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
     :label ~(if (= c.number 0) (t "changes.original") c.message)
     :onClick ~(handler selectCommit c.id)
     (label :text ~(if (= c.number 0) (t "changes.original") c.message)
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
     (label#changesProposalsTitle :when ~(> (count state.list.proposals) 0)
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
     ~(mode-button "changesSplit" 96 "split" "changes.split")
     ~(mode-button "changesInline" 88 "inline" "changes.inline")
     ~(notation-button "changesJson" 60 "json" "JSON")
     ~(notation-button "changesLisp" 60 "lisp" "Lisp")))

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
     (label :text ~(if (= f.tooLarge true) (t "changes.tooLarge") (str "+" f.added " −" f.removed))
       :textColor @c.muted
       :font small
       :anchors {:left 10 :right 10 :top 28}
       :height 16)))

(defview files "The change's files, then what it adds beside them." []
  `(column#changesFiles :width 240 :gap 2 :alignItems stretch :anchors {:top 0 :bottom 0} :scroll vertical
     (label#changesFilesTitle :text ~(t "changes.files") :font small :textColor @c.muted :height 20)
     (label#changesNoFiles :when ~(= (count state.view.files) 0)
       :text ~(t "changes.none")
       :textColor @c.muted
       :height 20)
     ~(repeat [f state.view.files] (file-row f))
     (label#changesExtrasTitle :when ~(> (count computed.extras) 0)
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
     (label#changesTooLarge :when ~(and (some? state.text) (= state.text.tooLarge true))
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
    (label#changesConflicts :when ~(> (count (or-else state.view.conflicts [])) 0)
      :wrap true :text ~(t "changes.conflicts") :textColor @c.warning :anchors {:left 0 :right 0} :height 36)
    (label#changesPerson :when ~(> (count (or-else state.view.person [])) 0)
      :wrap true
      :text ~(t "changes.person" {:lines (join "; " state.view.person)})
      :textColor @c.warning
      :anchors {:left 0 :right 0}
      :height 36)
    (label#changesBehaviour :when ~(> (count (or-else state.view.behaviour [])) 0)
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
                 (if (nil? state.view.impact)
                   ""
                   (str "   "
                        (t "changes.layers"
                           {:view state.view.impact.view
                            :logic state.view.impact.logic
                            :data state.view.impact.data}))))
     :textColor @c.muted
     :height 20))

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

(defview actions "Why, Preview, Reject and Accept for a proposal; Undo for a commit after the first." []
  `(row#changesActions
     :when ~(or computed.open (and (some? state.view) (= state.view.kind "commit") (> state.view.revision 0)))
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
     ~(action-button "changesUndo" 170 'computed.decided false "changes.undo" "undoChange")))

;;; The window

(defscreen main "The list on the left; the change shown, its files and their diff, and the decision on the right."
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
      (column#changesMain :grow 1 :gap 10 :alignItems stretch :anchors {:top 0 :bottom 0}
        ~@(heading)
        (row#changesBody :when ~(some? state.view) :grow 1 :gap 12
          ~(files)
          ~(pane))
        (label#changesMessage :when ~(not= state.message "")
          :wrap true
          :text ~state.message
          :textColor @c.warning
          :height 36)
        ~(actions)))))
