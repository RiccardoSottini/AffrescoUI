;;; views.lisp --- the account's button on the view rail, and who else is on the shared document

;;; The account

(defcomponent forgeAccountsStatus "The account's button, and the list of who is here that it opens once signed in."
  {}
  `(column#status-accounts-slot :width 44 :height 42
     ~(status-accounts)
     ~(status-accounts-peers)))

(defmacro account-label
  ;; A macro, not a function: the label and the tooltip share it, and it stays an expression.
  "Who is signed in and how many are here, the code to type while signing in, or Sign in."
  []
  `(cond (= auth.accountsServer.state "signedIn")
           (str (or-else auth.accountsServer.account (t "account.signedIn"))
                (if (> (count state.accounts.peers) 1) (t "account.here" {:n (count state.accounts.peers)}) ""))
         (and (= auth.accountsServer.state "signingIn") (some? auth.accountsServer.code))
           (t "account.code" {:code auth.accountsServer.code :url auth.accountsServer.verificationUrl})
         :else (t "account.signIn")))

(defview status-accounts "The account's button: signs in, or opens who is here." []
  `(label#status-accounts :role button
     :icon circle-user
     :font iconsRail
     :width 44
     :height 42
     :align center
     :cursor hand
     :label ~(account-label)
     :textColor ~(if (= auth.accountsServer.state "signedIn") "@color.text" "@color.muted")
     :tooltip ~(account-label)
     :hover {:textColor @color.text}
     :onClick accountsMenu))

(defview status-accounts-peers "Who is on the shared document, and in which file; and Sign Out." []
  `(column#status-accounts-peers :color @color.sideBar
     :borderWidth 1
     :borderColor @color.border
     :overlay {:anchor "#status-accounts" :placement right :open state.accounts.menuOpen :dismiss outside}
     (label#status-accounts-title :text ~(t "account.on" {:document settings.accounts.document})
       :padding 4
       :fitText true
       :textColor @color.statusText)
     ~(repeat [p state.accounts.peers :index pi]
       `(label#status-accounts-peer :padding 4
          :fitText true
          :textColor @color.statusText
          :text ~(str p.author (if (not= p.where "") (str " — " p.where) ""))))
     (label#status-accounts-signout :text ~(t "status-accounts-signout")
       :padding 4
       :fitText true
       :cursor hand
       :textColor @color.statusText
       :hover {:color @color.hover}
       :onClick accountsSignOut)))
