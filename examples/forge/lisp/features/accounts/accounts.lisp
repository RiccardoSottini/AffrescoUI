;;; accounts.lisp --- Forge's accounts: signing in to a document server (apps/server) with a device code, the settings kept in the person's own document ~settings and synced both ways, and the collaborators on a shared document with where each one is
;;;
;;; The provider's URLs and client id are placeholders to edit (no client secret: a public client with PKCE or a
;;; device code); the host allows their origins (Accounts.cpp). The token stays in the system's secret store, never in
;;; the state.

;;; State and definitions

(defstate accounts {:synced nil :revisions 0 :peers [] :where "" :menuOpen false})

(defcustom accounts.document
  "forge" "The document on the server whose collaborators the status bar shows." :description ~(t "settings.accounts.document.about") :type string :title ~(t "settings.accounts.document"))

(defpart accounts
         "What accounts adds."
         :connections {:accountsServer {:url "ws://127.0.0.1:7420/live"
                                        :auth {:flow device
                                               :deviceUrl "https://id.example.com/oauth/device"
                                               :tokenUrl "https://id.example.com/oauth/token"
                                               :clientId forge
                                               :issuer "https://id.example.com"
                                               :scopes [openid profile]
                                               :note "A placeholder provider: put your own here and in Accounts.cpp."}
                                        :messages {:open (open :document "~settings")
                                                   :revision (revision :id ~id :set {"/state/settings" ~settings})
                                                   :getDocument (getDocument)}
                                        :reconnect true
                                        :note "The person's settings: their own document on the server, ~settings."}
                       :accountsTeam {:url "ws://127.0.0.1:7420/live"
                                      :auth accountsServer
                                      :messages {:open (open :document ~document)
                                                 :presence (presence :where ~where :doing ~doing)}
                                      :reconnect true
                                      :note "Presence on the shared document the setting accounts.document names: who is here, and where."}}
         :watch {:state.settings accountsSend :state.activeGroup accountsPresence :state.groups accountsPresence})

;;; Functions and commands

(defcommand accountsSignIn "Sign in to the document server with a device code, to sync settings and see collaborators."
  :title ~(t "command.accountsSignIn") :category ~(t "category.accounts") :icon "user"
  :description ~(t "command.accountsSignIn.about") []
  (signIn "accountsServer" :onDone accountsSignInDone :onError accountsSignInFailed))

(defcommand accountsSignOut "Forget the sign-in; settings stay as they are on this machine."
  :title ~(t "command.accountsSignOut") :category ~(t "category.accounts") :icon "user"
  :description ~(t "command.accountsSignOut.about") []
  (signOut "accountsServer")
  (set "state.accounts.peers" [])
  (set "state.accounts.synced" nil))

(defcommand accountsMenu "Sign in, or see who else is on the shared document."
  :title ~(t "command.accountsMenu") :category ~(t "category.accounts") :icon "user"
  :description ~(t "command.accountsMenu.about") []
  (if (= auth.accountsServer.state "signedIn")
    (set "state.accounts.menuOpen" (not state.accounts.menuOpen))
    (accountsSignIn)))

(defn accountsServerOpen "TODO" []
  (request "accountsServer.open"))

(defn accountsServerHello "TODO" []
  (accountsReceive event.state.settings))

(defn accountsServerDocument "TODO" []
  (accountsReceive event.state.settings))

(defn accountsServerCommitted "TODO" []
  (request "accountsServer.getDocument"))

(defn accountsReceive "TODO" [settings]
  (when (some? settings) (set "state.accounts.synced" settings) (set "state.settings" settings)))

(defn accountsSend "TODO" []
  (when (and connections.accountsServer.online (not= state.settings state.accounts.synced))
    (set "state.accounts.revisions" (+ state.accounts.revisions 1))
    (set "state.accounts.synced" state.settings)
    (request "accountsServer.revision"
             :with {:id (str "settings-" (text state.accounts.revisions)) :settings state.settings})))

(defn accountsTeamOpen "TODO" []
  (request "accountsTeam.open" :with {:document settings.accounts.document})
  (set "state.accounts.where" "")
  (accountsPresence))

(defn accountsTeamPresence "TODO" []
  (set "state.accounts.peers" event.peers))

(defn accountsTeamClosed "TODO" []
  (set "state.accounts.peers" []))

(defn accountsPresence "TODO" []
  (when (and connections.accountsTeam.online (not= (or-else computed.activeDoc.label "") state.accounts.where))
    (set "state.accounts.where" (or-else computed.activeDoc.label ""))
    (request "accountsTeam.presence" :with {:where state.accounts.where :doing "editing"})))

(defn accountsSignInDone "What accountsSignIn does when it is done." []
  (notify "info" (str "Signed in as " (or-else auth.accountsServer.account "you"))))

(defn accountsSignInFailed "What accountsSignIn does when it fails." [error]
  (notify "error" (str "Not signed in: " error.message)))

(defpart accounts-events "What its channels' and services' events run: each a function."
  :actions {"accountsServer:open" ~(handler accountsServerOpen)
            "accountsServer:hello" ~(handler accountsServerHello)
            "accountsServer:document" ~(handler accountsServerDocument)
            "accountsServer:committed" ~(handler accountsServerCommitted)
            "accountsTeam:open" ~(handler accountsTeamOpen)
            "accountsTeam:presence" ~(handler accountsTeamPresence)
            "accountsTeam:closed" ~(handler accountsTeamClosed)})
