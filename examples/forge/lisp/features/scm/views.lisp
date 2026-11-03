;;; views.lisp --- the Source Control view, and the diff tab
;;;
;;; The view: the commit message and its button, the staged changes and the changes (each row a file, its badge
;;; and its buttons), the branches and the history when shown. The diff tab compares a file's two texts, side by side
;;; or inline.

;;; The view

(defcomponent forgeScmView
  "The Source Control view: its head, the commit message, the changes, the branches and the history."
  {}
  `(column#scm-view :width "100%" :grow 1 :color @color.sideBar :gap 4
     ~(scm-head)
     ~(scm-empty)
     ~(scm-no-repo)
     ~(scm-error)
     (node :visible ~computed.scmHasRepo :use forgeInset :height 26
       (label#scm-branch :icon git-branch
         :text ~computed.scmBranchText
         :grow 1
         :height 26
         :textColor @color.text
         :cursor hand
         :tooltip ~(t "scm-branch.tip")
         :onClick scmBranches))
     (node :visible ~computed.scmHasRepo :use forgeInset :height 26
       (textarea#scm-message :grow 1
         :height 26
         :padding 6
         :color @color.input
         :textColor @color.text
         :placeholderColor @color.muted
         :bind state.scm.message
         :placeholder ~(t "scm-message.placeholder" {:branch (or state.scm.status.branch.head "HEAD")})
         :onSubmit scmCommit))
     (node :visible ~computed.scmHasRepo :use forgeInset :height 28
       (label#scm-commit :icon check
         :text ~(t "scm-commit")
         :grow 1
         :height 28
         :align center
         :radius 2
         :cursor hand
         :focusable true
         :textColor "#ffffff"
         :color ~(if (and (not (empty? (trim state.scm.message))) (> (count state.scm.status.staged) 0))
                     "@color.accent"
                     "@color.border")
         :onClick scmCommit))
     ~(scm-staged-head)
     ~(scm-staged)
     ~(scm-changes-head)
     ~(scm-changes)
     ~(scm-branch-list)
     ~(scm-history)))

(defview scm-head "The view's head: its title, then refresh, branches, fetch, pull, push and history." []
  `(row#scm-head :width "100%" :height 35 :alignItems center
     (label#scm-title :text ~(t "scm-title") :textColor @color.muted :grow 1 :height 35 :padding 12)
     (label#scm-refresh :icon refresh-cw
       :padding 6
       :fitText true
       :cursor hand
       :textColor ~(if state.scm.busy "@color.accent" "@color.muted")
       :tooltip ~(t "scm-refresh.tip")
       :hover {:textColor @color.text}
       :onClick scmRefresh)
     (label#scm-branches-toggle :icon git-branch
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-branches-toggle.tip")
       :hover {:textColor @color.text}
       :onClick scmBranches)
     (node#scm-fetch :when ~computed.scmHasRepo
       :use forgeScmTool
       :with {:icon refresh-ccw-dot :tip ~(t "scm-fetch.tip") :action scmFetch :on ~computed.scmHasRepo})
     ~(scm-pull)
     (node#scm-push :when ~computed.scmHasRepo
       :use forgeScmTool
       :with {:icon ~(if (empty? state.scm.status.branch.upstream) "cloud-upload" "arrow-up-from-line")
              :tip ~(if (empty? state.scm.status.branch.upstream)
                        (t "scm-push.publish")
                        (t "scm-push.tip" {:upstream state.scm.status.branch.upstream}))
              :action scmPush
              :on ~computed.scmCanPush})
     (node#scm-history-toggle :when ~computed.scmHasRepo
       :use forgeScmTool
       :with {:icon history :tip ~(t "scm-history-toggle.tip") :action scmHistory :on true})))

(defcomponent forgeScmTool "A button of an icon in the view's head, which runs `action`; dimmed while it can't."
  {:icon "" :tip "" :action "" :on true}
  `(label :icon ~icon
     :padding 6
     :fitText true
     :role button
     :label ~tip
     :tooltip ~tip
     :cursor ~(if on "hand" "arrow")
     :textColor ~(if on "@color.muted" "@color.border")
     :hover {:textColor ~(if on "@color.text" "@color.border")}
     :onClick {:run ~action}))

(defview scm-pull "Pull, from the branch's upstream." []
  `(node#scm-pull :when ~computed.scmHasRepo
     :use forgeScmTool
     :with {:icon arrow-down-to-line
            :action scmPull
            :tip ~(if (empty? state.scm.status.branch.upstream)
                      (t "scm-pull.none")
                      (t "scm-pull.tip" {:upstream state.scm.status.branch.upstream}))
            :on ~(and computed.scmHasRepo (not (empty? state.scm.status.branch.upstream)))}))

(defview scm-empty "No folder open: says to open a repository's." []
  `(label#scm-empty :when ~(empty? state.folder.root)
     :wrap true
     :width "100%"
     :padding 12
     :textColor @color.text
     :text ~(t "scm-empty")))

(defview scm-no-repo "A folder that isn't a repository: a button to create one." []
  `(column#scm-no-repo :when ~(and (not (empty? state.folder.root)) (not computed.scmHasRepo) (empty? state.scm.error))
     :width "100%"
     :height 150
     :gap 8
     (label#scm-no-repo-text :wrap true
       :width "100%"
       :height 70
       :padding 12
       :textColor @color.text
       :text ~(t "scm-no-repo-text"))
     (node :use forgeInset :height 28
       (label#scm-init :role button
         :label ~(t "scm-init.label")
         :text ~(t "scm-init")
         :grow 1
         :height 28
         :align center
         :radius 2
         :cursor hand
         :focusable true
         :color @color.accent
         :textColor "#ffffff"
         :onClick scmInit))
     (node :use forgeInset :height 28
       (label#scm-open-folder :role button
         :label ~(t "scm-open-folder.label")
         :text ~(t "scm-open-folder")
         :grow 1
         :height 28
         :align center
         :radius 2
         :cursor hand
         :focusable true
         :color @color.input
         :textColor @color.text
         :onClick openFolder))))

(defview scm-error "Why git's output couldn't be shown." []
  `(label#scm-error :when ~(not (empty? state.scm.error))
     :icon circle-x
     :wrap true
     :width "100%"
     :padding 12
     :textColor @color.text
     :text ~state.scm.error))

;;; The changes

(defview scm-staged-head "The staged changes' title and count, and Unstage All." []
  `(row#scm-staged-head :visible ~computed.scmHasRepo :width "100%" :height 22 :alignItems center
     (label#scm-staged-title :text ~(t "scm-staged-title" {:n (count state.scm.status.staged)})
       :textColor @color.muted
       :grow 1
       :padding 8
       :height 22)
     (label#scm-unstage :icon minus
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-unstage.tip")
       :hover {:textColor @color.text}
       :onClick scmUnstage)
     (label#scm-unstage-all :icon list-minus
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-unstage-all.tip")
       :hover {:textColor @color.text}
       :onClick scmUnstageAll)))

(defview scm-staged "The staged changes: a row a file." []
  `(list#scm-staged :visible ~computed.scmHasRepo
     :width "100%"
     :grow 1
     :items ~computed.scmStagedRows
     :bind state.scm.selectedStaged
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen scmOpenDiff))

(defview scm-changes-head "The changes' title and count, and Stash, Pop and Stage All." []
  `(row#scm-changes-head :visible ~computed.scmHasRepo :width "100%" :height 22 :alignItems center
     (label#scm-changes-title :text ~(t "scm-changes-title" {:n (count state.scm.status.changes)})
       :textColor @color.muted
       :grow 1
       :padding 8
       :height 22)
     (label#scm-stage :icon plus
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-stage.tip")
       :hover {:textColor @color.text}
       :onClick scmStage)
     (node#scm-stash :use forgeScmTool
       :with {:icon archive :tip ~(t "scm-stash.tip") :action scmStash :on ~(> state.scm.status.count 0)})
     (node#scm-stash-pop :use forgeScmTool
       :with {:icon archive-restore
              :action scmStashPop
              :tip ~(if (> state.scm.stashes 0)
                        (t "scm-stash-pop.tip" {:n state.scm.stashes})
                        (t "scm-stash-pop.none"))
              :on ~(> state.scm.stashes 0)})
     (label#scm-stage-all :icon list-plus
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-stage-all.tip")
       :hover {:textColor @color.text}
       :onClick scmStageAll)))

(defview scm-changes "The changes not staged: a row a file." []
  `(list#scm-changes :visible ~computed.scmHasRepo
     :width "100%"
     :grow 2
     :items ~computed.scmChangeRows
     :bind state.scm.selectedChange
     :rowHeight 22
     :color @color.sideBar
     :textColor @color.text
     :selectedTextColor @color.text
     :selectedBackground @color.selection
     :accentColor @color.accent
     :onOpen scmOpenDiff))

;;; Branches and history

(defview scm-branch-list-head "The branches' title, and the button that hides them." []
  `(row#scm-branch-list-head :width "100%" :height 22 :alignItems center
     (label#scm-branch-list-title :text
                                  ~(t "scm-branch-list-title")
                                  :textColor
                                  @color.muted
                                  :grow
                                  1
                                  :padding
                                  8
                                  :height
                                  22)
     (label#scm-branch-list-close :icon x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-branch-list-close.tip")
       :hover {:textColor @color.text}
       :onClick scmHideBranches)))

(defview scm-branch-list "The branches, the current one checked, and a field for a new one." []
  `(column#scm-branch-list :when ~state.scm.showBranches :width "100%" :grow 1
     ~(scm-branch-list-head)
     (list#scm-branch-items :width "100%"
       :grow 1
       :items ~computed.scmBranchRows
       :bind state.scm.selectedBranch
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :selectedTextColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :onOpen scmSwitch)
     (textarea#scm-new-branch :width "100%"
       :height 26
       :padding 6
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :bind state.scm.branchName
       :placeholder ~(t "scm-new-branch.placeholder")
       :onSubmit scmCreateBranch)))

(defview scm-history "The history: the commits as a graph, and the files of the one chosen." []
  `(column#scm-history :when ~state.scm.showHistory :width "100%" :grow 2
     ~(scm-history-head)
     (list#scm-commits :label ~(t "scm-commits.label")
       :font code
       :width "100%"
       :grow 1
       :items ~computed.scmCommitRows
       :bind state.scm.selectedCommit
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :selectedTextColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :onChange scmPickCommit)
     (list#scm-commit-files :label ~(t "scm-commit-files.label")
       :when ~(not (empty? state.scm.commit.files))
       :width "100%"
       :grow 1
       :items ~computed.scmCommitFileRows
       :bind state.scm.selectedCommitFile
       :rowHeight 22
       :color @color.sideBar
       :textColor @color.text
       :selectedTextColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :onOpen scmOpenCommitFile)))

(defview scm-history-head "The history's title and count, and the button that hides it." []
  `(row#scm-history-head :width "100%" :height 22 :alignItems center
     (label#scm-history-title :text ~(t "scm-history-title" {:n (count (filter (fn [it] (not (empty? it.hash))) state.scm.log))})
       :textColor @color.muted
       :grow 1
       :padding 8
       :height 22)
     (label#scm-history-close :icon x
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.muted
       :tooltip ~(t "scm-history-close.tip")
       :hover {:textColor @color.text}
       :onClick scmHideHistory)))

;;; The diff tab

(defcomponent forgeDiffEditor "The diff tab: the open document `d`'s two texts, side by side or inline."
  {:d 0}
  `(column#diff-editor :width "100%"
     :grow 1
     :color @color.editor
     :note "The working tree against HEAD: a diff node, side by side or inline (diffEditor.renderSideBySide); diffLines() counts the lines."
     ~(diff-head d)
     (diff#diff-view :font code
       :width "100%"
       :grow 1
       :original ~(or-else (get-in state.docs [d :diff :original]) "")
       :modified ~(or-else (get-in state.docs [d :diff :modified]) "")
       :mode ~(if settings.diffEditor.renderSideBySide "split" "inline")
       :language ~(get-in state.docs [d :diff :path])
       :color @color.editor
       :textColor @color.editorText
       :deletedColor "#f8514933"
       :insertedColor "#2ea04333")))

(defview diff-head "The diff tab's head: the file, the lines changed, and the button that switches the layout." [d]
  `(row#diff-head :width "100%" :height 28 :alignItems center :gap 8
     (label#diff-path :icon git-compare
       :text ~(get-in state.docs [d :diff :path])
       :textColor @color.text
       :padding 8
       :fitText true)
     (label#diff-added :text ~(str "+" (get-in state.docs [d :diff :added])) :textColor "#2ea043" :fitText true)
     (label#diff-removed :text ~(str "-" (get-in state.docs [d :diff :removed])) :textColor "#f85149" :fitText true)
     (label#diff-truncated :when ~(get-in state.docs [d :diff :truncated])
       :text ~(t "diff-truncated")
       :textColor @color.muted
       :fitText true)
     (node#diff-head-gap :grow 1)
     (label#diff-mode :role button
       :label ~(t "diff-mode.label")
       :text ~(if settings.diffEditor.renderSideBySide (t "diff-mode.inline") (t "diff-mode.sideBySide"))
       :icon columns
       :padding 6
       :fitText true
       :cursor hand
       :textColor @color.muted
       :hover {:color @color.hover}
       :onClick toggleDiffInline)))
