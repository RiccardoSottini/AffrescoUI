[(node #FLO-1
    :ref FLO-1
    :priority 3
    :title "Onboarding checklist for new teams"
    :tag feature
    :who AL
    :points 5
    :notes "A short list of *first steps* on an empty board, under five items. Spec:"
    :linkText onboarding
    :link "https://example.com/specs/onboarding"
    :start "2026-03-12"
    :due "2026-03-20"
    :labels [frontend]
    :assignees [AL MK]
    :checklist [{:text "List the first steps" :done true}
      {:text "Draw the empty board" :done false}
      {:text "Write the copy" :done false}]
    :comments [{:who MK :text "The empty board drawing is on its way."}]
    :attachments [onboarding-flow.png]
    :board launch
    :status 0
    :order 0)
  (node #FLO-2
    :ref FLO-2
    :priority 4
    :title "Empty states for every page"
    :tag design
    :who MK
    :points 3
    :board launch
    :status 0
    :order 1)
  (node #FLO-3
    :ref FLO-3
    :priority 1
    :title "Crash when a board has no columns"
    :tag bug
    :who RS
    :points 2
    :notes "Opening a board whose columns were all deleted *crashes*. The steps:"
    :linkText "issue 214"
    :link "https://example.com/issues/214"
    :due "2026-03-14"
    :labels [urgent backend]
    :board launch
    :status 0
    :order 2)
  (node #FLO-4
    :ref FLO-4
    :priority 0
    :title "Upgrade the build toolchain"
    :tag chore
    :who JD
    :points 1
    :board launch
    :status 0
    :order 3)
  (node #FLO-5
    :ref FLO-5
    :priority 2
    :title "Keyboard shortcuts for moving cards"
    :tag feature
    :who RS
    :points 3
    :start "2026-03-10"
    :due "2026-03-17"
    :labels [frontend]
    :board launch
    :status 1
    :order 0)
  (node #FLO-6
    :ref FLO-6
    :priority 3
    :title "New colour palette for labels"
    :tag design
    :who MK
    :points 2
    :start "2026-03-11"
    :due "2026-03-18"
    :board launch
    :status 1
    :order 1)
  (node #FLO-7
    :ref FLO-7
    :priority 2
    :title "Card counts drift after a filter"
    :tag bug
    :who AL
    :points 1
    :board launch
    :status 1
    :order 2)
  (node #FLO-8
    :ref FLO-8
    :priority 2
    :title "Invite teammates by email"
    :tag feature
    :who JD
    :points 5
    :start "2026-03-09"
    :due "2026-03-19"
    :labels [backend]
    :comments [{:who JD :text "Ready for review, the email copy is still a draft."}]
    :board launch
    :status 2
    :order 0)
  (node #FLO-9
    :ref FLO-9
    :priority 4
    :title "Document the board API"
    :tag chore
    :who AL
    :points 2
    :start "2026-03-13"
    :due "2026-03-23"
    :labels [docs]
    :board launch
    :status 2
    :order 1)
  (node #FLO-10
    :ref FLO-10
    :priority 3
    :title "Drag-free card moves"
    :tag feature
    :who RS
    :points 3
    :board launch
    :status 3
    :order 0)
  (node #FLO-11
    :ref FLO-11
    :priority 1
    :title "Fix blurry text on HiDPI screens"
    :tag bug
    :who MK
    :points 2
    :board launch
    :status 3
    :order 1)
  (node #FLO-12
    :ref FLO-12
    :priority 4
    :title "Launch page illustrations"
    :tag design
    :who JD
    :points 3
    :board launch
    :status 3
    :order 2)
  (node #FLO-13
    :ref FLO-13
    :priority 3
    :title "Pricing page comparison table"
    :tag design
    :who MK
    :points 3
    :board website
    :status 0
    :order 0)
  (node #FLO-14
    :ref FLO-14
    :priority 2
    :title "Broken anchor links in the docs"
    :tag bug
    :who AL
    :points 1
    :board website
    :status 0
    :order 1)
  (node #FLO-15
    :ref FLO-15
    :priority 3
    :title "Blog with tags and search"
    :tag feature
    :who JD
    :points 5
    :board website
    :status 1
    :order 0)
  (node #FLO-16
    :ref FLO-16
    :priority 4
    :title "Move hosting to the new provider"
    :tag chore
    :who RS
    :points 2
    :board website
    :status 3
    :order 0)
  (node #FLO-17
    :ref FLO-17
    :priority 2
    :title "Offline mode for boards"
    :tag feature
    :who RS
    :points 8
    :board mobile
    :status 0
    :order 0)
  (node #FLO-18
    :ref FLO-18
    :priority 3
    :title "Push notifications for mentions"
    :tag feature
    :who AL
    :points 3
    :board mobile
    :status 1
    :order 0)
  (node #FLO-19
    :ref FLO-19
    :priority 4
    :title "Dark launch screen"
    :tag design
    :who MK
    :points 1
    :board mobile
    :status 1
    :order 1)
  (node #FLO-20
    :ref FLO-20
    :priority 1
    :title "Crash on rotate in card view"
    :tag bug
    :who JD
    :points 2
    :board mobile
    :status 2
    :order 0)]
