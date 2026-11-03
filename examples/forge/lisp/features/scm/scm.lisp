;;; scm.lisp --- Forge's source control: the open folder's git status (staged changes and changes), staging, the diff of a change against HEAD, commits and branches, and the gutter marks of each compared file (state.scm.gutters)
;;;
;;; git runs through the process service the host granted: only the git grant's commands, as arrays, in the open
;;; folder's repository. Its output is read strictly by gitStatus() and gitBranches(), and diffLines() compares a file
;;; with HEAD (its hunks make the gutter marks). A commit message goes to git's input, never its arguments. The
;;; actions that take what git wrote as an event (scmApplyStatus, scmApplyBranches, scmCompare) let a scenario or an
;;; agent show a status or a diff without a repository.

;;; State and definitions

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

(defcomputed scmStagedRows
  (map (fn [it]
           {:id it.id
            :label (+ it.label (if (empty? it.detail) "" (str "  " it.detail)) "  " it.badge)
            :icon (cond (or (= it.kind "added") (= it.kind "untracked")) "file-plus"
                        (= it.kind "deleted") "file-minus"
                        (= it.kind "unmerged") "alert-triangle"
                        (or (= it.kind "renamed") (= it.kind "copied")) "file-symlink"
                        :else "file-diff")
            :iconColor (cond (or (= it.kind "added") (= it.kind "untracked")) "#2ea043"
                             (or (= it.kind "deleted") (= it.kind "unmerged")) "#f85149"
                             :else "#d4a63a")})
         state.scm.status.staged))

(defcomputed scmChangeRows
  (map (fn [it]
           {:id it.id
            :label (+ it.label (if (empty? it.detail) "" (str "  " it.detail)) "  " it.badge)
            :icon (cond (or (= it.kind "added") (= it.kind "untracked")) "file-plus"
                        (= it.kind "deleted") "file-minus"
                        (= it.kind "unmerged") "alert-triangle"
                        (or (= it.kind "renamed") (= it.kind "copied")) "file-symlink"
                        :else "file-diff")
            :iconColor (cond (or (= it.kind "added") (= it.kind "untracked")) "#2ea043"
                             (or (= it.kind "deleted") (= it.kind "unmerged")) "#f85149"
                             :else "#d4a63a")})
         state.scm.status.changes))

(defcomputed scmHasRepo (not (empty? state.scm.status.branch.head)))

(defcomputed scmCanPush
  (and (not (empty? state.scm.status.branch.head)) (not= state.scm.status.branch.head "(detached)")))

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
            :icon (cond (= it.status "A") "file-plus" (= it.status "D") "file-minus" :else "file-diff")})
         state.scm.commit.files))

(defcomputed scmBranchRows
  (map (fn [it] {:id it.id :label it.label :icon (if it.current "check" "git-branch")}) state.scm.branches.branches))

(defcomputed scmBranchText
  (+ (if (empty? state.scm.status.branch.head) "No repository" state.scm.status.branch.head)
       (if (> state.scm.status.branch.ahead 0) (str "  ↑" state.scm.status.branch.ahead) "")
       (if (> state.scm.status.branch.behind 0) (str "  ↓" state.scm.status.branch.behind) "")))

(defkey "mod+shift+g" showSourceControl)

(defpart scm "What scm adds." :services {:git {:provider process :process git :root workspace}})

;;; Functions and commands

(defcommand showSourceControl "Show Source Control"
  :title ~(t "command.showSourceControl") :category ~(t "category.view") :icon "git-branch" []
  (forgeShowView "scm")
  (when (and (= state.sideView "scm") (not (empty? state.folder.root))) (scmRefresh)))

(defcommand scmRefresh "Read the open folder's git status again."
  :title ~(t "command.scmRefresh") :category ~(t "category.sourceControl") :icon "refresh-cw"
  :description ~(t "command.scmRefresh.about") :enablement ~(not (empty? state.folder.root)) []
  (invoke "git.run"
          :id "scm-status"
          :loading "state.scm.busy"
          :with {:arguments ["status" "--porcelain=v2" "-z" "--branch" "--untracked-files=all"] :collect true}
          :onError scmRefreshFailed
          :onDone scmRefreshDone))

(defn scmStashCount "TODO" []
  (invoke "git.run"
          :id "scm-stashes"
          :with {:arguments ["stash" "list" "--format=%gd"] :collect true}
          :onDone scmStashCountDone))

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

(defn scmApplyStatus "TODO" []
  (scmStatusRead (gitStatus event.output)))

(defn scmStatusRead "TODO" [read]
  (if (nil? read)
    (set "state.scm.error" "git's status could not be read: it is not the output this view asks for.")
    (do
      (set "state.scm.error" "")
      (set "state.scm.status"
           (merge read
                  {:items (map (fn [it]
                                 {:id it.path
                                  :label (baseName it.path)
                                  :detail (join "/" (slice (split it.path "/") 0 (- (count (split it.path "/")) 1)))
                                  :badge (cond
                                           (= it.kind "added") "A"
                                           (= it.kind "deleted") "D"
                                           (= it.kind "renamed") "R"
                                           (= it.kind "copied") "C"
                                           (= it.kind "unmerged") "!"
                                           (= it.kind "untracked") "U"
                                           :else "M")
                                  :kind it.kind
                                  :staged it.staged
                                  :unstaged it.unstaged})
                               (filter (fn [it] (not= it.kind "ignored")) read.files))}))
      (set "state.scm.status.staged" (filter (fn [it] it.staged) state.scm.status.items))
      (set "state.scm.status.changes" (filter (fn [it] it.unstaged) state.scm.status.items))
      (set "state.scm.status.count" (+ (count state.scm.status.staged) (count state.scm.status.changes))))))

(defn scmGit "TODO" [arguments input]
  (invoke "git.run"
          :loading "state.scm.busy"
          :with {:arguments arguments :input input :collect true}
          :onError scmGitFailed
          :onDone [scmGitDone arguments]))

(defcommand scmStage "Stage Changes"
  :title ~(t "command.scmStage") :category ~(t "category.sourceControl") :icon "plus"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedChange))) []
  (doseq [path (filter (fn [it] (not (empty? it))) [(or-else event.id state.scm.selectedChange)])]
    (scmGit ["add" "--" path] "")))

(defcommand scmUnstage "Unstage Changes"
  :title ~(t "command.scmUnstage") :category ~(t "category.sourceControl") :icon "minus"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedStaged))) []
  (doseq [path (filter (fn [it] (not (empty? it))) [(or-else event.id state.scm.selectedStaged)])]
    (scmGit ["restore" "--staged" "--" path] "")))

(defcommand scmStageAll "Stage All Changes"
  :title ~(t "command.scmStageAll") :category ~(t "category.sourceControl") :icon "list-plus"
  :enablement ~(> (count state.scm.status.changes) 0) []
  (scmGit ["add" "--" "."] ""))

(defcommand scmUnstageAll "Unstage All Changes"
  :title ~(t "command.scmUnstageAll") :category ~(t "category.sourceControl") :icon "list-minus"
  :enablement ~(> (count state.scm.status.staged) 0) []
  (scmGit ["restore" "--staged" "--" "."] ""))

(defcommand scmCommit "Commit the staged changes with the message (it goes to git's input, never its arguments)."
  :title ~(t "command.scmCommit") :category ~(t "category.sourceControl") :icon "check"
  :description ~(t "command.scmCommit.about")
  :enablement ~(and (not (empty? (trim state.scm.message))) (> (count state.scm.status.staged) 0)) []
  (if (or (empty? (trim state.scm.message)) (= (count state.scm.status.staged) 0))
    (notify "info"
            (if (= (count state.scm.status.staged) 0)
              "There are no staged changes to commit."
              "Write a commit message first."))
    (scmGit ["commit" "-F" "-"] (str (trim state.scm.message) "\n"))))

(defcommand scmBranches "Show Branches"
  :title ~(t "command.scmBranches") :category ~(t "category.sourceControl") :icon "git-branch"
  :enablement ~(not (empty? state.folder.root)) []
  (set "state.scm.showBranches" true)
  (invoke "git.run"
          :id "scm-branches"
          :with {:arguments ["for-each-ref" "--format=%(HEAD)%00%(refname:short)" "refs/heads/"] :collect true}
          :onError scmBranchesFailed
          :onDone scmBranchesDone))

(defcommand scmHistory "List the branch's last hundred commits; pick one to see the files it changed, and open a file to compare it with the commit before."
  :title ~(t "command.scmHistory") :category ~(t "category.sourceControl") :icon "history"
  :description ~(t "command.scmHistory.about") :enablement ~(not (empty? state.folder.root)) []
  (set "state.scm.showHistory" true)
  (scmLog))

(defn scmLog "TODO" []
  (invoke "git.run"
          :id "scm-log"
          :with {:arguments ["log" "--graph" "-n" "100" "--format=%x1e%D%x1e%h%x1f%H%x1f%an%x1f%ar%x1f%s"]
                 :collect true}
          :onError scmLogFailed
          :onDone scmLogDone))

(defn scmApplyLog "TODO" []
  (set "state.scm.showHistory" true)
  (scmLogRead event.output))

(defn scmLogRead "output: git log --graph's lines, each the graph, the refs and then the short hash, hash, author, when and subject split by the unit separator (the graph and refs before record separators, and optional); a line of the graph alone joins two commits. Other lines are left out"
  [output]
  (set "state.scm.log"
       (slice (map (fn [it]
                     (if (= (count it.f) 0)
                       {:short "" :hash "" :author "" :when "" :subject "" :refs "" :graph it.graph}
                       {:short (slice it.f.0 0 12)
                        :hash it.f.1
                        :author (slice it.f.2 0 80)
                        :when (slice it.f.3 0 40)
                        :subject (slice (join " " (slice it.f 4)) 0 200)
                        :refs it.refs
                        :graph it.graph}))
                   (filter (fn [it]
                             (or (and (>= (count it.f) 5) (= (count it.f.1) 40))
                                 (and (= (count it.f) 0)
                                      (not (empty? (trim it.graph)))
                                      (every? (fn [it] (contains "|/\\_*. -" it)) (split it.graph "")))))
                           (map (fn [it]
                                  {:graph (cond
                                            (= (count (split it "\u001e")) 3) (slice (get (split it "\u001e") 0) 0 60)
                                            (contains it "\u001f") ""
                                            :else (slice it 0 60))
                                   :refs (if (= (count (split it "\u001e")) 3)
                                           (slice (get (split it "\u001e") 1) 0 120)
                                           "")
                                   :f (if (contains (get (reverse (split it "\u001e")) 0) "\u001f")
                                        (split (get (reverse (split it "\u001e")) 0) "\u001f")
                                        [])})
                                (split output "\n"))))
              0
              300)))

(defn scmPickCommit "TODO" []
  (doseq [hash
          (filter (fn [it] (and (= (count (or-else it "")) 40) (not= (slice it 0 1) "-")))
                  [(or-else event.value state.scm.selectedCommit)])]
    (set "state.scm.commit" {:hash hash :files [] :before ""})
    (invoke "git.run"
            :id "scm-commit-files"
            :with {:arguments ["-c" "core.quotePath=false" "show" "--name-status" "--format=" "-M" hash]
                   :collect true}
            :onError scmPickCommitFailed
            :onDone [scmPickCommitDone hash])))

(defn scmApplyCommitFiles "TODO" []
  (scmCommitFilesRead event.hash event.output))

(defn scmCommitFilesRead "output: git show --name-status's lines (a status letter, a tab, the path; a rename or copy has the old path first). Kept while the commit is still the one picked"
  [hash output]
  (when (= hash state.scm.commit.hash)
    (set "state.scm.commit.files"
         (slice (map (fn [it] {:status (slice it.0 0 1) :from it.1 :path (get (reverse it) 0)})
                     (filter (fn [it] (and (>= (count it) 2) (not (empty? it.1))))
                             (map (fn [it] (split it "\t")) (split output "\n"))))
                0
                500))))

(defn scmOpenCommitFile "TODO" []
  (doseq [file (filter (fn [it] (= it.path (or-else event.id state.scm.selectedCommitFile))) state.scm.commit.files)]
    (invoke "git.run"
            :id "scm-commit-before"
            :with {:arguments ["show" "--no-textconv" (str state.scm.commit.hash "^:" file.from)] :collect true}
            :onError scmOpenCommitFileFailed
            :onDone [scmOpenCommitFileDone file])))

(defn scmCommitCompare "event: { hash, path, original, modified }: a file before and after a commit, in a diff tab"
  [event]
  (doseq [page
          [{:id (str "commit:" (slice event.hash 0 12) ":" event.path)
            :label (str (baseName event.path) " (" (slice event.hash 0 7) ")")
            :diff (merge (diffLines (or-else event.original "") (or-else event.modified "") {:context (- 1)})
                         {:path event.path
                          :original (or-else event.original "")
                          :modified (or-else event.modified "")})}]]
    (set "state.docs" (filter (fn [it] (not= it.id page.id)) state.docs))
    (push "state.docs"
          {:id page.id :label page.label :kind "diff" :text "" :dirty false :language "" :diff page.diff})
    (showTab page.id page.label)))

(defn scmApplyBranches "TODO" []
  (set "state.scm.showBranches" true)
  (scmBranchesRead (gitBranches event.output)))

(defn scmBranchesRead "TODO" [read]
  (if (nil? read)
    (set "state.scm.error" "git's branch list could not be read: it is not the output this view asks for.")
    (set "state.scm.branches" read)))

(defcommand scmSwitch "Switch Branch"
  :title ~(t "command.scmSwitch") :category ~(t "category.sourceControl") :icon "git-branch"
  :enablement ~(not (empty? (or-else event.id state.scm.selectedBranch))) []
  (doseq [name
          (filter (fn [it] (and (not (empty? it)) (not= it state.scm.branches.current)))
                  [(or-else event.id state.scm.selectedBranch)])]
    (scmGit ["switch" name] "")))

(defcommand scmCreateBranch "Create a branch with the name typed under Branches and switch to it."
  :title ~(t "command.scmCreateBranch") :category ~(t "category.sourceControl") :icon "git-branch"
  :description ~(t "command.scmCreateBranch.about")
  :enablement ~(not (empty? (trim (or-else event.text state.scm.branchName)))) []
  (doseq [name (filter (fn [it] (not (empty? it))) [(trim (or-else event.text state.scm.branchName))])]
    (set "state.scm.branchName" "")
    (scmGit ["switch" "-c" name] "")))

(defn scmOpenDiff "TODO" []
  (doseq [path
          (filter (fn [it] (not (empty? it))) [(or-else event.id state.scm.selectedChange state.scm.selectedStaged)])]
    (set "state.scm.compare" {:path path :original "" :modified ""})
    (invoke "git.run"
            :id "scm-show"
            :with {:arguments ["show" "--no-textconv" (str "HEAD:" path)] :collect true}
            :onError scmOpenDiffFailed
            :onDone [scmOpenDiffDone path])))

(defn scmCompare "TODO" []
  (set "state.scm.compare"
       {:path event.path :original (or-else event.original "") :modified (or-else event.modified "")})
  (scmShowDiff))

(defn scmShowDiff "TODO" []
  (scmDiffShow (diffLines state.scm.compare.original state.scm.compare.modified {:context (- 1)})
               (:hunks (diffLines state.scm.compare.original state.scm.compare.modified {:context 0}))))

(defn scmDiffShow "TODO" [diff changes]
  (set "state.scm.gutters"
       (concat (filter (fn [it] (not= it.path state.scm.compare.path)) state.scm.gutters)
               [{:path state.scm.compare.path
                 :marks (map (fn [it]
                               {:line (max it.newStart 1)
                                :endLine (if (= it.newLines 0) (max it.newStart 1) (- (+ it.newStart it.newLines) 1))
                                :kind "gutter"
                                :color (cond (= it.oldLines 0) "#2ea043" (= it.newLines 0) "#f85149" :else "#0078d4")
                                :text (cond (= it.oldLines 0) "added" (= it.newLines 0) "deleted" :else "modified")})
                             changes)}]))
  (doseq [page
          [{:id (str "diff:" state.scm.compare.path)
            :label (str (baseName state.scm.compare.path) " (Working Tree)")
            :diff (merge diff
                         {:path state.scm.compare.path
                          :original state.scm.compare.original
                          :modified state.scm.compare.modified
                          :items (map-indexed (fn [index it]
                                                {:id (text index)
                                                 :label (str (cond
                                                               (= it.kind "insert") "+ "
                                                               (= it.kind "delete") "- "
                                                               :else "  ")
                                                             it.text)
                                                 :icon (cond
                                                         (= it.kind "insert") "plus"
                                                         (= it.kind "delete") "minus"
                                                         :else "")
                                                 :iconColor (if (= it.kind "insert") "#2ea043" "#f85149")})
                                              (or-else diff.hunks.0.lines []))})}]]
    (set "state.docs" (filter (fn [it] (not= it.id page.id)) state.docs))
    (push "state.docs"
          {:id page.id :label page.label :kind "diff" :text "" :dirty false :language "" :diff page.diff})
    (showTab page.id page.label)))

(defn scmRefreshFailed "What scmRefresh does when it fails." [error]
  (set "state.scm.error" (str "git did not run: " error.message)))

(defn scmRefreshDone "What scmRefresh does when it is done." [result]
  (if (not= result.code 0)
    (do
      (set "state.scm.error" "")
      (set "state.scm.status"
           {:branch {:head "" :upstream "" :ahead 0 :behind 0} :files [] :staged [] :changes [] :count 0}))
    (do (scmStatusRead (gitStatus result.output)) (scmStashCount) (when state.scm.showHistory (scmLog)))))

(defn scmStashCountDone "What scmStashCount does when it is done." [result]
  (set "state.scm.stashes"
       (if (= result.code 0) (count (filter (fn [it] (not (empty? (trim it)))) (split result.output "\n"))) 0)))

(defn scmGitFailed "What scmGit does when it fails." [error]
  (notify "error" (str "git did not run: " error.message)))

(defn scmGitDone "What scmGit does when it is done." [arguments result]
  (if (not= result.code 0)
    (notify "error"
            (str "git "
                 arguments.0
                 " failed"
                 (if (empty? (trim result.output)) "" (str ": " (slice (trim result.output) 0 300)))))
    (do
      (when (= arguments.0 "commit")
        (set "state.scm.message" "")
        (log (str "Committed: " (slice (trim result.output) 0 200))))
      (when (contains ["fetch" "pull" "push" "stash"] arguments.0)
        (log (str "git "
                  (join " " arguments)
                  (if (empty? (trim result.output)) "" (str ": " (slice (trim result.output) 0 200))))))))
  (scmRefresh)
  (when (= arguments.0 "switch") (scmBranches)))

(defn scmBranchesFailed "What scmBranches does when it fails." [error]
  (set "state.scm.error" (str "git did not run: " error.message)))

(defn scmBranchesDone "What scmBranches does when it is done." [result]
  (when (= result.code 0) (scmBranchesRead (gitBranches result.output))))

(defn scmLogFailed "What scmLog does when it fails." [error]
  (set "state.scm.error" (str "git did not run: " error.message)))

(defn scmLogDone "What scmLog does when it is done." [result]
  (set "state.scm.log" [])
  (when (= result.code 0) (scmLogRead result.output)))

(defn scmPickCommitFailed "What scmPickCommit does when it fails." [error]
  (notify "error" (str "git did not run: " error.message)))

(defn scmPickCommitDone "What scmPickCommit does when it is done." [hash result]
  (when (= result.code 0) (scmCommitFilesRead hash result.output)))

(defn scmOpenCommitFileFailed "What scmOpenCommitFile does when it fails." [error]
  (notify "error" (str "git did not run: " error.message)))

(defn scmOpenCommitFileDone "What scmOpenCommitFile does when it is done." [file result]
  (set "state.scm.commit.before" (if (= result.code 0) result.output ""))
  (invoke "git.run"
          :id "scm-commit-after"
          :with {:arguments ["show" "--no-textconv" (str state.scm.commit.hash ":" file.path)] :collect true}
          :onDone [scmOpenCommitFileDoneDone file]))

(defn scmOpenCommitFileDoneDone "What scmOpenCommitFileDone does when it is done." [file result]
  (scmCommitCompare
    {:hash state.scm.commit.hash
     :path file.path
     :original state.scm.commit.before
     :modified (if (= result.code 0) result.output "")}))

(defn scmOpenDiffFailed "What scmOpenDiff does when it fails." [error]
  (notify "error" (str "git did not run: " error.message)))

(defn scmOpenDiffDone "What scmOpenDiff does when it is done." [path result]
  (set "state.scm.compare.original" (if (= result.code 0) result.output ""))
  (invoke "files.read" :with {:path path} :onError scmOpenDiffDoneFailed :onDone scmOpenDiffDoneDone))

(defn scmOpenDiffDoneFailed "What scmOpenDiffDone does when it fails." []
  (scmShowDiff))

(defn scmOpenDiffDoneDone "What scmOpenDiffDone does when it is done." [result]
  (set "state.scm.compare.modified" result.text)
  (scmShowDiff))
