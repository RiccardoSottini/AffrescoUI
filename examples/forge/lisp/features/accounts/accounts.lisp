;;; accounts.lisp --- Forge's accounts: signing in to a document server, settings synced, and collaborators
;;;
;;; Signing in to a document server (apps/server) uses a device code; the person's settings are kept in their own
;;; document ~settings and synced both ways, and the status bar shows who else is on a shared document, and where.
;;; The provider's URLs and client id are placeholders to edit (no client secret: a public client with PKCE or a
;;; device code); the host allows their origins. The token stays in the system's secret store, never in the state.

;;; Signing in

(defstate accounts {:synced nil :revisions 0 :peers [] :where "" :menuOpen false})

(defcustom accounts.document "forge"
  "The document on the server whose collaborators the status bar shows."
  :description ~(t "settings.accounts.document.about")
  :type string
  :title ~(t "settings.accounts.document"))

(defpart accounts-connections
  "The two connections to the document server: the person's settings, and the shared document."
  :connections {:accountsServer {:url "ws://127.0.0.1:7420/live"
                                 :auth {:flow device
                                        :deviceUrl "https://id.example.com/oauth/device"
                                        :tokenUrl "https://id.example.com/oauth/token"
                                        :clientId forge
                                        :issuer "https://id.example.com"
                                        :scopes [openid profile]
                                        :note "A placeholder provider: put your own here and in Accounts.cpp."}
                                 :messages {:open {:type open :document "~settings"}
                                            :revision {:type revision :id ~id :set {"/state/settings" ~settings}}
                                            :getDocument {:type getDocument}}
                                 :reconnect true
                                 :note "The person's settings: their own document on the server, ~settings."}
                :accountsTeam {:url "ws://127.0.0.1:7420/live"
                               :auth accountsServer
                               :messages {:open {:type open :document ~document}
                                          :presence {:type presence :where ~where :doing ~doing}}
                               :reconnect true
                               :note "Presence on the shared document the setting accounts.document names: who is here, and where."}})

(defcommand accountsSignIn "Sign in to the document server with a device code, to sync settings and see collaborators."
  :title ~(t "command.accountsSignIn") :category ~(t "category.accounts") :icon "user"
  :description ~(t "command.accountsSignIn.about") []
  (signIn "accountsServer" :onDone accountsSignInDone :onError accountsSignInFailed))

(defn accountsSignInDone "Says who signed in." []
  (notify "info" (t "notify.signedIn" {:account (or-else auth.accountsServer.account (t "account.you"))})))

(defn accountsSignInFailed "Says why signing in failed." [error]
  (notify "error" (t "notify.notSignedIn" {:error error.message})))

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

;;; The person's settings

(defn accountsServerOpen "The server's connection is open: asks for the person's ~settings document." []
  (request "accountsServer.open"))

(defn accountsServerHello "The server's hello: the settings it holds replace this machine's." []
  (accountsReceive event.state.settings))

(defn accountsServerDocument "The ~settings document, read again: its settings replace this machine's." []
  (accountsReceive event.state.settings))

(defn accountsServerCommitted "The server committed a revision: reads the document again." []
  (request "accountsServer.getDocument"))

(defn accountsReceive
  "Settings from the server, kept as synced, so they aren't sent back; a document without settings changes nothing."
  [settings]
  (when (some? settings) (set "state.accounts.synced" settings) (set "state.settings" settings)))

(defn accountsSend
  "The settings changed here, sent to the server as a revision of ~settings, once they differ from what it has."
  []
  (when (and connections.accountsServer.online (not= state.settings state.accounts.synced))
    (set "state.accounts.revisions" (+ state.accounts.revisions 1))
    (set "state.accounts.synced" state.settings)
    (request "accountsServer.revision"
             :with {:id (str "settings-" (text state.accounts.revisions)) :settings state.settings})))

;;; Who else is here

(defn accountsTeamOpen "The shared document's connection is open: joins it, and says where the person is." []
  (request "accountsTeam.open" :with {:document settings.accounts.document})
  (set "state.accounts.where" "")
  (accountsPresence))

(defn accountsTeamPresence "Who else is on the shared document, and where." []
  (set "state.accounts.peers" event.peers))

(defn accountsTeamClosed "The shared document's connection closed: no one is shown." []
  (set "state.accounts.peers" []))

(defn accountsPresence "Tells the others which file the person is in, when it changes." []
  (when (and connections.accountsTeam.online (not= (or-else computed.activeDoc.label "") state.accounts.where))
    (set "state.accounts.where" (or-else computed.activeDoc.label ""))
    (request "accountsTeam.presence" :with {:where state.accounts.where :doing "editing"})))

;;; Events

(defpart accounts-watch "Settings changed here go to the server; a new active tab tells the others where the person is."
  :watch {:state.settings accountsSend :state.activeGroup accountsPresence :state.groups accountsPresence})

(defpart accounts-events "What the connections' events run: each a function."
  :actions {"accountsServer:open" ~(handler accountsServerOpen)
            "accountsServer:hello" ~(handler accountsServerHello)
            "accountsServer:document" ~(handler accountsServerDocument)
            "accountsServer:committed" ~(handler accountsServerCommitted)
            "accountsTeam:open" ~(handler accountsTeamOpen)
            "accountsTeam:presence" ~(handler accountsTeamPresence)
            "accountsTeam:closed" ~(handler accountsTeamClosed)})
