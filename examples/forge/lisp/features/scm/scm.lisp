;;; scm.lisp --- Forge's source control: git's status, staging, diffs, commits, branches and the history
;;;
;;; git runs through the process service (git.run), its output read by the expressions' gitStatus and gitBranches,
;;; and by this file for the log and a commit's files. A change's diff against HEAD opens in a diff tab, side by side
;;; or inline; the gutters of an open file mark its changed lines. The commit message goes to git's input, never its
;;; arguments.

;;; The view, and git's status

(defstate scm
  {:status {:branch {:head "" :upstream "" :ahead 0 :behind 0} :files [] :staged [] :changes [] :count 0}
   :branches {:current "" :branches []}
   :message ""
   :branchName ""
   :stashes 0
   :error ""
   :busy false
   :showBranches false
   :showHistory false
   :log []
   :selectedCommit nil
   :commit {:hash "" :files [] :before ""}
   :selectedCommitFile nil
   :selectedStaged nil
   :selectedChange nil
   :selectedBranch nil
   :compare {:path "" :original "" :modified ""}
   :gutters []})

(defpart scm-service "git, run by the process service in the open folder."
  :services {:git {:provider process :process git :root workspace}})

(defcommand showSourceControl "Show Source Control"
  :title ~(t "command.showSourceControl")
  :description ~(t "command.showSourceControl") :category ~(t "category.view") :icon "git-branch" []
  (forgeShowView "scm")
  (when (and (= state.sideView "scm") (seq state.folder.root)) (scmRefresh)))

(defcommand scmRefresh "Read the open folder's git status again."
  :title ~(t "command.scmRefresh") :category ~(t "category.sourceControl") :icon "refresh-cw"
  :description ~(t "command.scmRefresh.about") :enablement ~(not (empty? state.folder.root)) []
  (invoke "git.run"
          :id "scm-status"
          :loading "state.scm.busy"
          :with {:arguments ["status" "--porcelain=v2" "-z" "--branch" "--untracked-files=all"] :collect true}
          :onError scmGitDidNotRun
          :onDone scmStatusDone))

(defn scmStatusDone "git's status read; outside a repository, nothing to show." [result]
  (if (not= result.code 0)
    (do
      (set "state.scm.error" "")
      (set "state.scm.status"
           {:branch {:head "" :upstream "" :ahead 0 :behind 0} :files [] :staged [] :changes [] :count 0}))
    (do (scmStatusRead (gitStatus result.output)) (scmStashCount) (when (true? state.scm.showHistory) (scmLog)))))

(defn scmGitDidNotRun "Says, in the view, that git didn't run." [error]
  (set "state.scm.error" (t "scm.didNotRun" {:error error.message})))

(defn scmApplyStatus "Plays git's status given in the event (`event.output`), as the scenarios do." []
  (scmStatusRead (gitStatus event.output)))

(defn scmStatusRead "git's status, read: each file's row with its badge, staged or not." [read]
  (if (some? read)
    (do
      (set "state.scm.error" "")
      (set "state.scm.status"
           (merge read
                  {:items (map (fn [it]
                                 {:id it.path
                                  :label (baseName it.path)
                                  ; style: keep, the host's split keeps the empty name after a last slash
                                  :detail (str/join "/" (slice (split it.path "/") 0 (- 1)))
                                  :badge (case it.kind
                                           :added "A"
                                           :deleted "D"
                                           :renamed "R"
                                           :copied "C"
                                           :unmerged "!"
                                           :untracked "U"
                                           "M")
                                  :kind it.kind
                                  :staged it.staged
                                  :unstaged it.unstaged})
                               (remove #(= (:kind %) "ignored") read.files))}))
      (set "state.scm.status.staged" (filter :staged state.scm.status.items))
      (set "state.scm.status.changes" (filter :unstaged state.scm.status.items))
      (set "state.scm.status.count" (+ (count state.scm.status.staged) (count state.scm.status.changes))))
    (set "state.scm.error" (t "scm.statusUnread"))))

(defcomputed scmStagedRows
  (map (fn [it]
           {:id it.id
            :label (+ it.label (if (empty? it.detail) "" (str "  " it.detail)) "  " it.badge)
            :icon (case it.kind
                    (:added :untracked) "file-plus"
                    :deleted "file-minus"
                    :unmerged "alert-triangle"
                    (:renamed :copied) "file-symlink"
                    "file-diff")
            :iconColor (case it.kind (:added :untracked) "#2ea043" (:deleted :unmerged) "#f85149" "#d4a63a")})
         state.scm.status.staged))

(defcomputed scmChangeRows
  (map (fn [it]
           {:id it.id
            :label (+ it.label (if (empty? it.detail) "" (str "  " it.detail)) "  " it.badge)
            :icon (case it.kind
                    (:added :untracked) "file-plus"
                    :deleted "file-minus"
                    :unmerged "alert-triangle"
                    (:renamed :copied) "file-symlink"
                    "file-diff")
            :iconColor (case it.kind (:added :untracked) "#2ea043" (:deleted :unmerged) "#f85149" "#d4a63a")})
         state.scm.status.changes))

(defcomputed scmHasRepo (not (empty? state.scm.status.branch.head)))

(defcomputed scmCanPush
  (and (not (empty? state.scm.status.branch.head)) (not= state.scm.status.branch.head "(detached)")))

(defn scmStashCount "Counts the stashes, for the Stash Pop button." []
  (invoke "git.run"
          :id "scm-stashes"
          :with {:arguments ["stash" "list" "--format=%gd"] :collect true}
          :onDone scmStashesCounted))

(defn scmStashesCounted "The stashes, counted." [result]
  (set "state.scm.stashes"
       (if (= result.code 0) (count (remove str/blank? (str/split result.output "\n"))) 0)))

(defkey "mod+shift+g" showSourceControl)

;;; Running git

(defn scmGit "Runs git with arguments, and its input; then reads the status again." [arguments input]
  (invoke "git.run"
          :loading "state.scm.busy"
          :with {:arguments arguments :input input :collect true}
          :onError scmGitFailed
          :onDone [scmGitDone arguments]))

(defn scmGitDone "git ran: a failure is a notice, a commit or a fetch a line in Output." [arguments result]
  (let [output (str/trim result.output)]
    (if (not= result.code 0)
      (notify "error"
              (t (if (empty? output) "scm.failed" "scm.failedWith") {:command arguments.0 :output (slice output 0 300)}))
      (do
        (when (= arguments.0 "commit")
          (set "state.scm.message" "")
          (log-line (t "log.committed" {:output (slice output 0 200)})))
        (when (#{:fetch :pull :push :stash} arguments.0)
          (log-line (str "git " (str/join " " arguments) (if (empty? output) "" (str ": " (slice output 0 200)))))))))
  (scmRefresh)
  (when (= arguments.0 "switch") (scmBranches)))

(defn scmGitFailed "Says that git didn't run." [error]
  (notify "error" (t "scm.didNotRun" {:error error.message})))

(defcommand scmFetch "Fetch the remotes' branches (git fetch --prune); what is behind shows on the branch row."
  :title ~(t "command.scmFetch") :category ~(t "category.sourceControl") :icon "refresh-ccw-dot"
  :description ~(t "command.scmFetch.about") :enablement ~computed.scmHasRepo []
  (scmGit ["fetch" "--prune"] ""))

(defcommand scmPull "Bring the branch up to its upstream, only where that needs no merge (git pull --ff-only)."
  :title ~(t "command.scmPull") :category ~(t "category.sourceControl") :icon "arrow-down-to-line"
  :description ~(t "command.scmPull.about")
  :enablement ~(and computed.scmHasRepo (not (empty? state.scm.status.branch.upstream))) []
  (scmGit ["pull" "--ff-only"] ""))

(defcommand scmPush "Send the branch's commits to its upstream, or publish it to origin when it has none."
  :title ~(t "command.scmPush") :category ~(t "category.sourceControl") :icon "arrow-up-from-line"
  :description ~(t "command.scmPush.about") :enablement ~computed.scmCanPush []
  (scmGit (if (empty? state.scm.status.branch.upstream) ["push" "-u" "origin" "HEAD"] ["push"]) ""))

(defcommand scmStash "Put every change aside, new files too (git stash push --include-untracked)."
  :title ~(t "command.scmStash") :category ~(t "category.sourceControl") :icon "archive"
  :description ~(t "command.scmStash.about") :enablement ~(> state.scm.status.count 0) []
  (scmGit ["stash" "push" "--include-untracked"] ""))

(defcommand scmStashPop "Bring back the changes put aside last, and drop them from the stashes (git stash pop)."
  :title ~(t "command.scmStashPop") :category ~(t "category.sourceControl") :icon "archive-restore"
  :description ~(t "command.scmStashPop.about") :enablement ~(> state.scm.stashes 0) []
  (scmGit ["stash" "pop"] ""))

(defcommand scmInit "Make the open folder a git repository (git init), to track its changes."
  :title ~(t "command.scmInit") :category ~(t "category.sourceControl") :icon "git-branch-plus"
  :description ~(t "command.scmInit.about")
  :enablement ~(and (not (empty? state.folder.root)) (not computed.scmHasRepo)) []
  (scmGit ["init"] ""))

;;; Staging and committing

(defcommand scmStage "Stage Changes"
  :title ~(t "command.scmStage")
  :description ~(t "command.scmStage") :category ~(t "category.sourceControl") :icon "plus"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedChange))) []
  (let [path (or-else event.id state.scm.selectedChange)] (when (seq path) (scmGit ["add" "--" path] ""))))

(defcommand scmUnstage "Unstage Changes"
  :title ~(t "command.scmUnstage")
  :description ~(t "command.scmUnstage") :category ~(t "category.sourceControl") :icon "minus"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedStaged))) []
  (let [path (or-else event.id state.scm.selectedStaged)]
    (when (seq path) (scmGit ["restore" "--staged" "--" path] ""))))

(defcommand scmStageAll "Stage All Changes"
  :title ~(t "command.scmStageAll")
  :description ~(t "command.scmStageAll") :category ~(t "category.sourceControl") :icon "list-plus"
  :enablement ~(> (count state.scm.status.changes) 0) []
  (scmGit ["add" "--" "."] ""))

(defcommand scmUnstageAll "Unstage All Changes"
  :title ~(t "command.scmUnstageAll")
  :description ~(t "command.scmUnstageAll") :category ~(t "category.sourceControl") :icon "list-minus"
  :enablement ~(> (count state.scm.status.staged) 0) []
  (scmGit ["restore" "--staged" "--" "."] ""))

(defcommand scmCommit "Commit the staged changes with the message (it goes to git's input, never its arguments)."
  :title ~(t "command.scmCommit") :category ~(t "category.sourceControl") :icon "check"
  :description ~(t "command.scmCommit.about")
  :enablement ~(and (not (empty? (trim state.scm.message))) (> (count state.scm.status.staged) 0)) []
  (let [message (str/trim state.scm.message)]
    (if (or (empty? message) (empty? state.scm.status.staged))
      (notify "info" (t (if (empty? state.scm.status.staged) "scm.nothingStaged" "scm.noMessage")))
      (scmGit ["commit" "-F" "-"] (str message "\n")))))

;;; Branches

(defn scmHideBranches "Hides the branches." []
  (set "state.scm.showBranches" false))

(defcomputed scmBranchRows
  ; style: keep, #(...) can't give a map, and a binding's function names its item it
  (map (fn [it] {:id it.id :label it.label :icon (if it.current "check" "git-branch")}) state.scm.branches.branches))

(defcomputed scmBranchText
  (str (if (empty? state.scm.status.branch.head) (t "scm.noRepository") state.scm.status.branch.head)
       (if (> state.scm.status.branch.ahead 0) (str "  ↑" state.scm.status.branch.ahead) "")
       (if (> state.scm.status.branch.behind 0) (str "  ↓" state.scm.status.branch.behind) "")))

(defcommand scmBranches "Show Branches"
  :title ~(t "command.scmBranches")
  :description ~(t "command.scmBranches") :category ~(t "category.sourceControl") :icon "git-branch"
  :enablement ~(not (empty? state.folder.root)) []
  (set "state.scm.showBranches" true)
  (invoke "git.run"
          :id "scm-branches"
          :with {:arguments ["for-each-ref" "--format=%(HEAD)%00%(refname:short)" "refs/heads/"] :collect true}
          :onError scmGitDidNotRun
          :onDone scmBranchesListed))

(defn scmBranchesListed "The branches, listed." [result]
  (when (= result.code 0) (scmBranchesRead (gitBranches result.output))))

(defn scmApplyBranches "Plays git's branch list given in the event, as the scenarios do." []
  (set "state.scm.showBranches" true)
  (scmBranchesRead (gitBranches event.output)))

(defn scmBranchesRead "git's branch list, read." [read]
  (if (some? read)
    (set "state.scm.branches" read)
    (set "state.scm.error" (t "scm.branchesUnread"))))

(defcommand scmSwitch "Switch Branch"
  :title ~(t "command.scmSwitch")
  :description ~(t "command.scmSwitch") :category ~(t "category.sourceControl") :icon "git-branch"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedBranch))) []
  (let [name (or-else event.id state.scm.selectedBranch)]
    (when (and (seq name) (not= name state.scm.branches.current)) (scmGit ["switch" name] ""))))

(defcommand scmCreateBranch "Create a branch with the name typed under Branches and switch to it."
  :title ~(t "command.scmCreateBranch") :category ~(t "category.sourceControl") :icon "git-branch"
  :description ~(t "command.scmCreateBranch.about")
  :enablement ~(not (empty? (trim (or-else event.text state.scm.branchName)))) []
  (let [name (str/trim (or-else event.text state.scm.branchName))]
    (when (seq name) (set "state.scm.branchName" "") (scmGit ["switch" "-c" name] ""))))

;;; The history

(defn scmHideHistory "Hides the history." []
  (set "state.scm.showHistory" false))

(defcomputed scmCommitRows
  (map-indexed (fn [index it]
                   (if (empty? it.hash)
                       {:id (str "-" index) :label it.graph}
                       {:id it.hash
                        :label (+ it.graph
                                  (if (empty? it.graph) "" " ")
                                  (if (empty? it.refs) "" (str "(" it.refs ") "))
                                  it.subject
                                  "  "
                                  it.author
                                  ", "
                                  it.when)
                        :icon (if (empty? it.graph) "git-commit" "")}))
                 state.scm.log))

(defcomputed scmCommitFileRows
  (map (fn [it]
           {:id it.path
            :label (str (get (reverse (split it.path "/")) 0)
                        "  "
                        (join "/" (slice (split it.path "/") 0 (- 1)))
                        "  "
                        it.status)
            :icon (case it.status :A "file-plus" :D "file-minus" "file-diff")})
         state.scm.commit.files))

(defcommand scmHistory
  "Lists the branch's last hundred commits; a commit shows its files, a file compares it with the commit before."
  :title ~(t "command.scmHistory") :category ~(t "category.sourceControl") :icon "history"
  :description ~(t "command.scmHistory.about") :enablement ~(not (empty? state.folder.root)) []
  (set "state.scm.showHistory" true)
  (scmLog))

(defn scmLog "Reads the branch's last hundred commits." []
  (invoke "git.run"
          :id "scm-log"
          :with {:arguments ["log" "--graph" "-n" "100" "--format=%x1e%D%x1e%h%x1f%H%x1f%an%x1f%ar%x1f%s"]
                 :collect true}
          :onError scmGitDidNotRun
          :onDone scmLogListed))

(defn scmLogListed "The commits, read." [result]
  (set "state.scm.log" [])
  (when (= result.code 0) (scmLogRead result.output)))

(defn scmApplyLog "Plays git's log given in the event, as the scenarios do." []
  (set "state.scm.showHistory" true)
  (scmLogRead event.output))

(defn scmLogRead
  "git log --graph's lines (`output`): the graph, the refs, the hashes, author, when and subject of each commit."
  [output]
  (set "state.scm.log"
       (slice (map (fn [it]
                     (if (empty? it.f)
                       {:short "" :hash "" :author "" :when "" :subject "" :refs "" :graph it.graph}
                       {:short (slice it.f.0 0 12)
                        :hash it.f.1
                        :author (slice it.f.2 0 80)
                        :when (slice it.f.3 0 40)
                        :subject (slice (str/join " " (slice it.f 4)) 0 200)
                        :refs it.refs
                        :graph it.graph}))
                   (filter (fn [it]
                             (or (and (>= (count it.f) 5) (= (count it.f.1) 40))
                                 (and (empty? it.f)
                                      (seq (str/trim it.graph))
                                      (every? #(str/includes? "|/\\_*. -" %) (seq it.graph)))))
                           (map (fn [line]
                                  ; style: keep, the host's split keeps the empty fields at the end, which are counted
                                  (let [parts (split line "\u001e")
                                        fields (last parts)]
                                    {:graph (cond
                                              (= (count parts) 3) (slice (first parts) 0 60)
                                              (str/includes? line "\u001f") ""
                                              :else (slice line 0 60))
                                     :refs (if (= (count parts) 3) (slice (get parts 1) 0 120) "")
                                     ; style: keep, the host's split keeps an empty subject, which is counted
                                     :f (if (str/includes? fields "\u001f") (split fields "\u001f") [])}))
                                (str/split output "\n"))))
              0
              300)))

(defn scmPickCommit "A commit chosen in the history: lists the files it changed." []
  (let [hash (or-else event.value state.scm.selectedCommit)]
    (when (and (= (count (or-else hash "")) 40) (not (str/starts-with? hash "-")))
      (set "state.scm.commit" {:hash hash :files [] :before ""})
      (invoke "git.run"
              :id "scm-commit-files"
              :with {:arguments ["-c" "core.quotePath=false" "show" "--name-status" "--format=" "-M" hash]
                     :collect true}
              :onError scmGitFailed
              :onDone [scmCommitFilesListed hash]))))

(defn scmCommitFilesListed "A commit's files, listed." [hash result]
  (when (= result.code 0) (scmCommitFilesRead hash result.output)))

(defn scmApplyCommitFiles "Plays a commit's files given in the event, as the scenarios do." []
  (scmCommitFilesRead event.hash event.output))

(defn scmCommitFilesRead
  "git show --name-status's lines (`output`): each file's status and path, kept while the commit is chosen."
  [hash output]
  (when (= hash state.scm.commit.hash)
    (set "state.scm.commit.files"
         ; style: keep, #(...) can't give a map
         (slice (map (fn [it] {:status (slice it.0 0 1) :from it.1 :path (last it)})
                     (filter #(and (>= (count %) 2) (seq (get % 1)))
                             ; style: keep, the host's split keeps an empty path at the end, which is the path read
                             (map #(split % "\t") (str/split output "\n"))))
                0
                500))))

(defn scmOpenCommitFile "A file of a commit, opened: its text before the commit, then after it, in a diff tab." []
  (doseq [file (filter #(= (:path %) (or-else event.id state.scm.selectedCommitFile)) state.scm.commit.files)]
    (invoke "git.run"
            :id "scm-commit-before"
            :with {:arguments ["show" "--no-textconv" (str state.scm.commit.hash "^:" file.from)] :collect true}
            :onError scmGitFailed
            :onDone [scmCommitFileBefore file])))

(defn scmCommitFileBefore "The file before the commit; then reads it after." [file result]
  (set "state.scm.commit.before" (if (= result.code 0) result.output ""))
  (invoke "git.run"
          :id "scm-commit-after"
          :with {:arguments ["show" "--no-textconv" (str state.scm.commit.hash ":" file.path)] :collect true}
          :onDone [scmCommitFileAfter file]))

(defn scmCommitFileAfter "The file after the commit: both in a diff tab." [file result]
  (scmCommitCompare
    {:hash state.scm.commit.hash
     :path file.path
     :original state.scm.commit.before
     :modified (if (= result.code 0) result.output "")}))

(defn scmCommitCompare "A file before and after a commit (`event`: hash, path, original, modified), in a diff tab."
  [event]
  (let [page
        {:id (str "commit:" (slice event.hash 0 12) ":" event.path)
         :label (str (baseName event.path) " (" (slice event.hash 0 7) ")")
         :diff (merge (diffLines (or-else event.original "") (or-else event.modified "") {:context (- 1)})
                      {:path event.path :original (or-else event.original "") :modified (or-else event.modified "")})}]
    (set "state.docs" (remove #(= (:id %) page.id) state.docs))
    (push "state.docs"
          {:id page.id :label page.label :kind "diff" :text "" :dirty false :language "" :diff page.diff})
    (showTab page.id page.label)))

;;; Diffs

(defn scmOpenDiff "A change chosen: its file at HEAD and in the folder, compared in a diff tab." []
  (let [path (or-else event.id state.scm.selectedChange state.scm.selectedStaged)]
    (when (seq path)
      (set "state.scm.compare" {:path path :original "" :modified ""})
      (invoke "git.run"
              :id "scm-show"
              :with {:arguments ["show" "--no-textconv" (str "HEAD:" path)] :collect true}
              :onError scmGitFailed
              :onDone [scmDiffOriginal path]))))

(defn scmDiffOriginal "The file at HEAD; then reads it in the folder." [path result]
  (set "state.scm.compare.original" (if (= result.code 0) result.output ""))
  (invoke "files.read" :with {:path path} :onError scmDiffNoFile :onDone scmDiffModified))

(defn scmDiffModified "The file in the folder: the diff is shown." [result]
  (set "state.scm.compare.modified" result.text)
  (scmShowDiff))

(defn scmDiffNoFile "A file deleted in the folder: compared with nothing." []
  (scmShowDiff))

(defn scmCompare "Plays a comparison given in the event (`event`: path, original, modified), as the scenarios do." []
  (set "state.scm.compare"
       {:path event.path :original (or-else event.original "") :modified (or-else event.modified "")})
  (scmShowDiff))

(defn scmShowDiff "Compares the file's two texts, line by line." []
  (scmDiffShow (diffLines state.scm.compare.original state.scm.compare.modified {:context (- 1)})
               (:hunks (diffLines state.scm.compare.original state.scm.compare.modified {:context 0}))))

(defn scmDiffShow "A comparison shown: the file's gutters mark its changed lines, and a diff tab shows both sides."
  [diff changes]
  (set "state.scm.gutters"
       (conj (remove #(= (:path %) state.scm.compare.path) state.scm.gutters)
             {:path state.scm.compare.path
              :marks (map (fn [it]
                            {:line (max it.newStart 1)
                             :endLine (if (zero? it.newLines) (max it.newStart 1) (dec (+ it.newStart it.newLines)))
                             :kind "gutter"
                             :color (cond (zero? it.oldLines) "#2ea043" (zero? it.newLines) "#f85149" :else "#0078d4")
                             :text (cond (zero? it.oldLines) "added" (zero? it.newLines) "deleted" :else "modified")})
                          changes)}))
  (let [page
        {:id (str "diff:" state.scm.compare.path)
         :label (str (baseName state.scm.compare.path) " (Working Tree)")
         :diff (merge diff
                      {:path state.scm.compare.path
                       :original state.scm.compare.original
                       :modified state.scm.compare.modified
                       :items (map-indexed (fn [index it]
                                             {:id (text index)
                                              :label (str (case it.kind :insert "+ " :delete "- " "  ") it.text)
                                              :icon (case it.kind :insert "plus" :delete "minus" "")
                                              :iconColor (if (= it.kind "insert") "#2ea043" "#f85149")})
                                           (or-else diff.hunks.0.lines []))})}]
    (set "state.docs" (remove #(= (:id %) page.id) state.docs))
    (push "state.docs"
          {:id page.id :label page.label :kind "diff" :text "" :dirty false :language "" :diff page.diff})
    (showTab page.id page.label)))
