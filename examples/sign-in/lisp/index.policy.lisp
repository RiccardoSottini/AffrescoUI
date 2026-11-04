{:screen index.screen.lisp
 :cases [{:name "without permissions, a change applies"
          :revision {:author "agent/claude-code"
                     :set {"/program/look.lisp/styles.caption" "(defstyle caption \"A line of muted text under a title.\"\n  (label :height 20 :textColor \"#000000\" :anchors {:left 0 :right 10}))"}}
          :expect allow}
         {:name "so do new panels"
          :revision {:author "agent/claude-code"
                     :set {"/program/index.screen.lisp/main" "(defscreen main \"Sign in's one window: the glows, the screen shown, the bars, the dialogs and the toast.\"\n  :window {:title ~(t \"windowTitle\")\n           :width 960\n           :height 720\n           :minWidth 480\n           :minHeight 640\n           :focusRing {:color @color.accent :width 2 :offset 2}}\n  :strings {:default en :locale ~state.locale :keep [\"EXAM PLEE XAMP LE23\" \"DEMO-1001\" \"Example ID\"]}\n  :fonts {:default {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 15 :fallback [icons arabic]}\n          :small {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 13 :fallback [iconsSmall arabicSmall]}\n          :title {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 26 :fallback [arabicTitle]}\n          :heading {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 19 :fallback [icons arabicHeading]}\n          :icons {:file \"../../shared/fonts/Lucide.ttf\" :size 18}\n          :iconsSmall {:file \"../../shared/fonts/Lucide.ttf\" :size 13}\n          :logo {:file \"../../shared/fonts/Lucide.ttf\" :size 22}\n          :mono {:file \"../../shared/fonts/JetBrainsMono-Regular.ttf\" :size 15}\n          :arabic {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 15}\n          :arabicSmall {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 13}\n          :arabicTitle {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 26}\n          :arabicHeading {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 19}}\n  :persist [state.theme state.remember state.locale]\n  :theme ~state.theme\n  ;; the screens the outlet shows, one at a time (navigate): the first is shown first, so they are an option of main,\n  ;; in their order, not definitions of their own, whose names would sort them\n  :screens {:signInForm ~(sign-in-card)\n            :createAccount ~(create-card)\n            :twoStep ~(two-step-card)\n            :reset ~(reset-card)\n            :signedIn ~(account-screen)}\n  (node#panel-root :color @color.background\n    ~@(glows)\n    (outlet#screens :dir auto :anchors {:left 0 :right 0 :top 56 :bottom 44})\n    ~(announcer)\n    ~(top-bar)\n    ~(footer)\n    ~(forgot-dialog)\n    ~(passkey-dialog)\n    ~(setup-dialog)\n    ~(change-dialog)\n    ~(delete-dialog)\n    ~(reauth-dialog)\n    ~(shortcuts-dialog)\n    ~(toast)\n    ~@(map (fn [n] `(node :id ~(str \"policyPanel\" n))) (range 21))))"}}
          :expect allow}
         {:name "a revision that can't apply is denied"
          :revision {:author "agent/claude-code"
                     :remove "#noSuchNode"}
          :expect deny
          :reason "no node with id"}
         {:name "no one adds permissions"
          :revision {:author "agent/claude-code"
                     :set {"/program/index.screen.lisp/main" "(defscreen main \"Sign in's one window: the glows, the screen shown, the bars, the dialogs and the toast.\"\n  :window {:title ~(t \"windowTitle\")\n           :width 960\n           :height 720\n           :minWidth 480\n           :minHeight 640\n           :focusRing {:color @color.accent :width 2 :offset 2}}\n  :strings {:default en :locale ~state.locale :keep [\"EXAM PLEE XAMP LE23\" \"DEMO-1001\" \"Example ID\"]}\n  :fonts {:default {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 15 :fallback [icons arabic]}\n          :small {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 13 :fallback [iconsSmall arabicSmall]}\n          :title {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 26 :fallback [arabicTitle]}\n          :heading {:file \"../../shared/fonts/Roboto-Regular.ttf\" :size 19 :fallback [icons arabicHeading]}\n          :icons {:file \"../../shared/fonts/Lucide.ttf\" :size 18}\n          :iconsSmall {:file \"../../shared/fonts/Lucide.ttf\" :size 13}\n          :logo {:file \"../../shared/fonts/Lucide.ttf\" :size 22}\n          :mono {:file \"../../shared/fonts/JetBrainsMono-Regular.ttf\" :size 15}\n          :arabic {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 15}\n          :arabicSmall {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 13}\n          :arabicTitle {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 26}\n          :arabicHeading {:file \"../../shared/fonts/NotoSansArabic-Subset.ttf\" :size 19}}\n  :permissions {:allow [\"*\"]}\n  :persist [state.theme state.remember state.locale]\n  :theme ~state.theme\n  ;; the screens the outlet shows, one at a time (navigate): the first is shown first, so they are an option of main,\n  ;; in their order, not definitions of their own, whose names would sort them\n  :screens {:signInForm ~(sign-in-card)\n            :createAccount ~(create-card)\n            :twoStep ~(two-step-card)\n            :reset ~(reset-card)\n            :signedIn ~(account-screen)}\n  (node#panel-root :color @color.background\n    ~@(glows)\n    (outlet#screens :dir auto :anchors {:left 0 :right 0 :top 56 :bottom 44})\n    ~(announcer)\n    ~(top-bar)\n    ~(footer)\n    ~(forgot-dialog)\n    ~(passkey-dialog)\n    ~(setup-dialog)\n    ~(change-dialog)\n    ~(delete-dialog)\n    ~(reauth-dialog)\n    ~(shortcuts-dialog)\n    ~(toast)))"}}
          :expect deny
          :reason "can't change \"permissions\""}
         {:name "a node inside a screen changes too"
          :revision {:author "agent/claude-code"
                     :set {"/program/sign-in-form.lisp/sign-in-card" "(defview sign-in-card \"The sign-in screen: its card, centred, scrolling when the window is short.\" []\n  `(node#panel-sign-in :style screen\n     (node#card :style card :radius 20 :width ~(min 420 (- window.width 32)) :maxHeight ~(- window.height 116)\n       (node#card-top :height 36 :color transparent)\n       ~(sign-in-form)\n       (node#card-end :height 36 :color transparent))))"}}
          :expect allow}
         {:name "so does a translation"
          :revision {:author "agent/claude-code"
                     :set {"/strings/it/signInButton" "Entra"}}
          :expect allow}
         {:name "the fake provider is a capability: no one changes its address"
          :revision {:author "agent/claude-code"
                     :set {"/program/sign-in.lisp/part.provider" "(defpart provider \"Example ID, the made-up OAuth 2 provider the sign-in form offers.\"\n  ;; no real client id or secret: scenarios play its answers as the providerSignedIn and providerFailed functions\n  :connections {:exampleId {:url \"https://id.example.com/api\"\n                            :auth {:flow browser\n                                   :signInUrl \"https://id.example.com/authorize\"\n                                   :tokenUrl \"https://id.example.org/token\"\n                                   :clientId affrescoui-sign-in-demo\n                                   :scopes [openid email profile]}}})"}}
          :expect deny
          :reason "can't change \"connections\""}
         {:name "nor adds a connection"
          :revision {:author "agent/claude-code"
                     :set {"/program/sign-in.lisp/part.provider" "(defpart provider \"Example ID, the made-up OAuth 2 provider the sign-in form offers.\"\n  ;; no real client id or secret: scenarios play its answers as the providerSignedIn and providerFailed functions\n  :connections {:other {:url \"https://api.example.org\"}\n                :exampleId {:url \"https://id.example.com/api\"\n                            :auth {:flow browser\n                                   :signInUrl \"https://id.example.com/authorize\"\n                                   :tokenUrl \"https://id.example.com/token\"\n                                   :clientId affrescoui-sign-in-demo\n                                   :scopes [openid email profile]}}})"}}
          :expect deny
          :reason "can't change \"connections\""}
         {:name "nor makes another button sign in"
          :revision {:author "agent/claude-code"
                     :set {"/program/sign-in.lisp/closeShortcuts" "(defn closeShortcuts \"Closes the shortcuts' list.\" [] (signIn \"exampleId\"))"}}
          :expect deny
          :reason "can't add, change or move"}
         {:name "nor adds a sign-out to a button"
          :revision {:author "agent/claude-code"
                     :set {"/program/chrome.lisp/top-bar" "(defview top-bar \"The top bar, top right: the languages, Slow network, the shortcuts and the theme.\" []\n  `(row#topBar :dir auto :gap 6 :width 226 :height 32 :alignItems center :anchors {:right 16 :top 14}\n     (row#languages :role radiogroup :label ~(t \"language\") :arrows horizontal :gap 2 :height 32 :width 112\n       ~(repeat [language [\"en\" \"it\" \"ar\"]]\n          (language-chip language)))\n     ~(slow-toggle)\n     (node#shortcutsButton :style iconButton\n       :font iconsSmall\n       :icon keyboard\n       :label ~(t \"shortcuts\")\n       :tooltip ~(t \"shortcutsTooltip\" {:key computed.shortcutsKey})\n       :isOpen ~state.shortcutsOpen\n       :onClick openShortcuts)\n     (node#themeToggle :style iconButton\n       :icon ~(if (= state.theme \"light\") \"moon\" \"sun\")\n       :label ~(t \"themeTooltip\" {:dark (= state.theme \"dark\") :key computed.themeKey})\n       :tooltip ~(t \"themeTooltip\" {:dark (= state.theme \"dark\") :key computed.themeKey})\n       :onClick {:signOut exampleId})))"}}
          :expect deny
          :reason "can't add, change or move"}
         {:name "nor stores more than the screen persists"
          :revision {:author "agent/claude-code"
                     :set {"/program/index.screen.lisp/main/:persist" {:$form "[state.password]"}}}
          :expect deny
          :reason "can't change \"persist\""}]}
