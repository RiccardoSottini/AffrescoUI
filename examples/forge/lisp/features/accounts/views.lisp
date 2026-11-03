;;; views.lisp --- what accounts.lisp shows

;;; Views

(defcomponent forgeAccountsStatus "The forgeAccountsStatus component."
  {}
  `(column#status-accounts-slot :width 44 :height 42
     ~(status-accounts)
     ~(status-accounts-peers)))

(defview status-accounts "#status-accounts." []
  `(label#status-accounts :role button
     :icon circle-user
     :font iconsRail
     :width 44
     :height 42
     :align center
     :cursor hand
     :label ~(cond (= auth.accountsServer.state "signedIn") (+ (or-else auth.accountsServer.account "Signed in")
                      (if (> (count state.accounts.peers) 1)
                          (str " · " (text (count state.accounts.peers)) " here")
                          ""))
                   (and (= auth.accountsServer.state "signingIn") (some? auth.accountsServer.code)) (str "Code " auth.accountsServer.code " at " auth.accountsServer.verificationUrl)
                   :else "Sign in")
     :textColor ~(if (= auth.accountsServer.state "signedIn") "@color.text" "@color.muted")
     :tooltip ~(cond (= auth.accountsServer.state "signedIn") (+ (or-else auth.accountsServer.account "Signed in")
                        (if (> (count state.accounts.peers) 1)
                            (str " · " (text (count state.accounts.peers)) " here")
                            ""))
                     (and (= auth.accountsServer.state "signingIn") (some? auth.accountsServer.code)) (str "Code " auth.accountsServer.code " at " auth.accountsServer.verificationUrl)
                     :else "Sign in")
     :hover {:textColor @color.text}
     :onClick {:run accountsMenu}))

(defview status-accounts-peers "#status-accounts-peers." []
  `(column#status-accounts-peers :color @color.sideBar
     :borderWidth 1
     :borderColor @color.border
     :overlay {:anchor "#status-accounts" :placement right :open state.accounts.menuOpen :dismiss outside}
     (label#status-accounts-title :text ~(str "On " settings.accounts.document)
       :padding 4
       :fitText true
       :textColor @color.statusText)
     ~(repeat [p state.accounts.peers :index pi]
       `(label#status-accounts-peer :padding 4
          :fitText true
          :textColor @color.statusText
          :text ~(+ p.author (if (not= p.where "") (str " — " p.where) ""))))
     (label#status-accounts-signout :text ~(t "status-accounts-signout")
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.statusText
       :hover {:color @color.hover}
       :onClick {:run accountsSignOut})))
