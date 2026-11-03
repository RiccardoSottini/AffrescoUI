{:screen index.screen.lisp
 :cases [{:name "the studio changes the screen"
          :revision {:author studio
                     :set {"/program/look.lisp/styles.caption" "(defstyle caption \"A caption.\" {:style small :fitText true :textColor \"#000000\"})"}}
          :expect allow}
         {:name "an agent's change waits for review"
          :revision {:author "agent/claude-code"
                     :set {"/program/look.lisp/styles.caption" "(defstyle caption \"A caption.\" {:style small :fitText true :textColor \"#000000\"})"}}
          :expect ask
          :reason "agent(/program)"}
         {:name "no one changes the app bar"
          :revision {:author studio
                     :set {"/program/list.lisp/app-bar" "(defview app-bar \"The app bar: the flame and the name, add, filter and the menu; the pick bar over it while picking.\" []\n  `(node#appBar :anchors {:left 0 :right 0 :top 0} :height 56 :color \"#000000\" :shadow {:y 2 :blur 4 :color \"#00000030\"}\n     (label#appMark :font icons\n       :icon flame\n       :width 24\n       :height 24\n       :align center\n       :textColor @c.primary\n       :anchors {:left 16 :top 16})\n     (label#label-t-app :text ~(t \"app\")\n       :font title\n       :textColor @c.text\n       :anchors {:left 48 :top 16}\n       :fitText true\n       :role heading)\n     (row#appActions :anchors {:right 4 :top 8} :width 128 :height 40 :gap 4 :arrows horizontal\n       ~(add-button)\n       ~(filter-button)\n       (node#moreButton :style iconButton\n         :icon ellipsis\n         :label ~(t \"more\")\n         :tooltip ~(t \"more\")\n         :textColor @c.muted\n         :onClick {:set state.menus.more :to true}\n         :children [~(menu-more-button)]\n         :isOpen ~state.menus.more))\n     ~(pick-bar)))"}}
          :expect deny
          :reason "#appBar"}
         {:name "an agent's new behaviour is always asked"
          :revision {:author "agent/claude-code"
                     :add {"/program/habits.lisp/policyExample" "(defn policyExample \"An agent's new function.\" [] (set \"state.theme\" \"dark\"))"}}
          :expect ask}
         {:name "twenty new panels are within the agent's budget"
          :revision {:author "agent/claude-code"
                     :set {"/program/index.screen.lisp/main" "(defscreen main \"Streak's one window: the list, the detail, and what opens over them.\"\n  :window {:title \"Streak\" :width 1280 :height 800 :minWidth 400 :minHeight 560}\n  :strings {:default en :locale ~state.locale :required true :keep [\"Streak\"]}\n  :fonts {:default {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 15 :fallback [icons]}\n          :small {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 13 :fallback [icons]}\n          :tiny {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 11 :fallback [icons]}\n          :heading {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 17 :fallback [icons]}\n          :title {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 20 :fallback [icons]}\n          :big {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 26 :fallback [icons]}\n          :icons {:file \"../../shared/fonts/Lucide.ttf\" :size 20}\n          :mark {:file \"../../shared/fonts/Lucide.ttf\" :size 22}\n          :iconsSmall {:file \"../../shared/fonts/Lucide.ttf\" :size 14}}\n  :persist [state.nextId\n            state.selected\n            state.sort\n            state.hideCompleted\n            state.showArchived\n            state.theme\n            state.pureBlack\n            state.firstWeekday\n            state.reverseDays\n            state.skipDays\n            state.reminders\n            state.locale\n            state.scorePeriod\n            state.historyPeriod]\n  :permissions {:allow [studio]\n                :ask [\"agent(/actions)\" \"agent(/program)\"]\n                :deny [\"*(#appBar)\"]\n                :reviewers [studio]\n                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}\n  :theme ~computed.theme\n  (node#root :color @c.bg\n    ~(body)\n    ~@(map (fn [n] `(node :id ~(str \"policyPanel\" n))) (range 20))))"}}
          :expect ask}
         {:name "a twenty-first is over it"
          :revision {:author "agent/claude-code"
                     :set {"/program/index.screen.lisp/main" "(defscreen main \"Streak's one window: the list, the detail, and what opens over them.\"\n  :window {:title \"Streak\" :width 1280 :height 800 :minWidth 400 :minHeight 560}\n  :strings {:default en :locale ~state.locale :required true :keep [\"Streak\"]}\n  :fonts {:default {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 15 :fallback [icons]}\n          :small {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 13 :fallback [icons]}\n          :tiny {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 11 :fallback [icons]}\n          :heading {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 17 :fallback [icons]}\n          :title {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 20 :fallback [icons]}\n          :big {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 26 :fallback [icons]}\n          :icons {:file \"../../shared/fonts/Lucide.ttf\" :size 20}\n          :mark {:file \"../../shared/fonts/Lucide.ttf\" :size 22}\n          :iconsSmall {:file \"../../shared/fonts/Lucide.ttf\" :size 14}}\n  :persist [state.nextId\n            state.selected\n            state.sort\n            state.hideCompleted\n            state.showArchived\n            state.theme\n            state.pureBlack\n            state.firstWeekday\n            state.reverseDays\n            state.skipDays\n            state.reminders\n            state.locale\n            state.scorePeriod\n            state.historyPeriod]\n  :permissions {:allow [studio]\n                :ask [\"agent(/actions)\" \"agent(/program)\"]\n                :deny [\"*(#appBar)\"]\n                :reviewers [studio]\n                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}\n  :theme ~computed.theme\n  (node#root :color @c.bg\n    ~(body)\n    ~@(map (fn [n] `(node :id ~(str \"policyPanel\" n))) (range 21))))"}}
          :expect deny
          :reason "over the budget"}
         {:name "the budget is the agent's only"
          :revision {:author studio
                     :set {"/program/index.screen.lisp/main" "(defscreen main \"Streak's one window: the list, the detail, and what opens over them.\"\n  :window {:title \"Streak\" :width 1280 :height 800 :minWidth 400 :minHeight 560}\n  :strings {:default en :locale ~state.locale :required true :keep [\"Streak\"]}\n  :fonts {:default {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 15 :fallback [icons]}\n          :small {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 13 :fallback [icons]}\n          :tiny {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 11 :fallback [icons]}\n          :heading {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 17 :fallback [icons]}\n          :title {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 20 :fallback [icons]}\n          :big {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 26 :fallback [icons]}\n          :icons {:file \"../../shared/fonts/Lucide.ttf\" :size 20}\n          :mark {:file \"../../shared/fonts/Lucide.ttf\" :size 22}\n          :iconsSmall {:file \"../../shared/fonts/Lucide.ttf\" :size 14}}\n  :persist [state.nextId\n            state.selected\n            state.sort\n            state.hideCompleted\n            state.showArchived\n            state.theme\n            state.pureBlack\n            state.firstWeekday\n            state.reverseDays\n            state.skipDays\n            state.reminders\n            state.locale\n            state.scorePeriod\n            state.historyPeriod]\n  :permissions {:allow [studio]\n                :ask [\"agent(/actions)\" \"agent(/program)\"]\n                :deny [\"*(#appBar)\"]\n                :reviewers [studio]\n                :budgets [{:author agent :added 20 :removed 5 :changed 50}]}\n  :theme ~computed.theme\n  (node#root :color @c.bg\n    ~(body)\n    ~@(map (fn [n] `(node :id ~(str \"policyPanel\" n))) (range 21))))"}}
          :expect allow}
         {:name "no one changes the permissions"
          :revision {:author studio
                     :set {"/program/index.screen.lisp/main/:permissions/:allow" "[\"*\"]"}}
          :expect deny
          :reason permissions}
         {:name "no one moves where the habits sync"
          :revision {:author studio :set {"/connections/streakServer/url" "ws://127.0.0.1:7421/live"}}
          :expect deny
          :reason "can't change \"connections\""}
         {:name "nor does an agent"
          :revision {:author "agent/claude-code" :set {"/connections/streakServer/messages/open/document" "~elsewhere"}}
          :expect deny
          :reason "can't change \"connections\""}]}
