;;; profile.lisp --- what the account page does: the profile, emails, two-step, passkeys, devices, connected
;;; accounts, the activity log and deleting the account
;;;
;;; The account page's views are in account.lisp and account-sections.lisp. A sensitive change asks for the password
;;; again (stepUp), unless it was given in the last 2 minutes; it remembers what it is about in state.target and the
;;; function that makes it in state.reauth.then. Every change is logged (logEvent), and the account's lists are
;;; written back on it when the session ends (saveAccount).

;;; The account's record

(defn updateAccount "Changes some fields of the account of an email." [email fields]
  (set "state.accounts" (map (fn [it] (if (same-email? (:email it) email) (merge it fields) it)) state.accounts)))

(defn ofUser? "Whether a record (an account, a trusted device) is the signed-in account's, by its email." [record]
  (same-email? (:email record) state.user.email))

(defn seedAccount
  "The signed-in account's emails, passkeys, devices, connections and log: kept on the account between sessions."
  ;; the demo accounts start with a few made-up ones; endSession writes them back (saveAccount)
  []
  (let [user state.user
        demo (:demo user)]
    (set "state.section" "profile")
    (set "state.clock" 0)
    (set "state.photoOpen" false)
    (set "state.newEmail" {:open false :address "" :submitted false})
    (set "state.emails"
         (or-else (:emails user)
                  (concat [{:address (:email user) :primary true :verified true}] (if demo (demoEmails user) []))))
    (set "state.myPasskeys"
         (or-else (:passkeys user) (if (:passkey user) [{:id "pk1" :device "this" :added 12 :used 0}] [])))
    (set "state.sessions" (or-else (:sessions user) (concat [thisSession] (if demo demoSessions []))))
    (set "state.connected"
         (or-else (:connected user)
                  (if demo [{:provider "quillo" :account (str (:username user) "@quillo.example")}] [])))
    (set "state.log" (take 50 (concat [signedInHere] (or-else (:log user) (if demo (demoLog user) [])))))))

(def thisSession "This device's session, the first of the list."
  {:id "here" :device "this" :app "desktop" :place "Lakeside" :days 0})

(def signedInHere "The sign-in that opened this session: the log's newest event."
  {:kind "signIn" :at 0 :days 0 :device "this" :place "Lakeside"})

(def demoSessions "A demo account's other sessions: a phone and a laptop."
  [{:id "phone" :device "phone" :app "mobile" :place "Lakeside" :days 1}
   {:id "laptop" :device "laptop" :app "browser" :place "Northport" :days 6}])

(defn demoEmails "A demo account's other emails: one verified, one not." [user]
  [{:address (str (:username user) ".home@example.net") :primary false :verified true}
   {:address (str (:username user) "@work.example.org") :primary false :verified false}])

(defn demoLog "A demo account's past events: a sign-in, one blocked, and the last password change." [user]
  [{:kind "signIn" :at 0 :days 1 :device "phone" :place "Lakeside"}
   {:kind "signInBlocked" :at 0 :days 3 :device "laptop" :place "Eastvale"}
   {:kind "password" :at 0 :days (or-else (:passwordDays user) 30) :device "laptop" :place "Northport"}])

(defn saveAccount "Writes the session's lists back on the account, for the next sign-in (times kept relative)." []
  (updateAccount state.user.email
                 {:emails state.emails
                  :passkeys state.myPasskeys
                  :passkey (> (count state.myPasskeys) 0)
                  :sessions state.sessions
                  :connected state.connected
                  :log (map (fn [it] (merge it {:at (- (:at it) state.clock)})) state.log)}))

(defn logEvent "An event of the account's activity, the newest first, at most 50." [kind]
  (set "state.log" (take 50 (cons {:kind kind :at state.clock :days 0 :device "this" :place "Lakeside"} state.log))))

(defn setSection "Shows a section of the account page, closing what was open in the last." [section]
  (set "state.section" section)
  (set "state.newEmail.open" false)
  (set "state.photoOpen" false))

;;; Asking the password again

(defn stepUp "Runs a sensitive change, by its function's name, once the password was given again." [then]
  (if (or (> state.confirmedLeft 0) state.user.provider)
      (run then)
      (do (set "state.reauth" {:open true :password "" :error "" :then then})
          (focus "#reauthPassword"))))

(defn confirmReauth "Confirm: with the right password, the change goes on, and none is asked for 2 minutes." []
  (let [password (:password (accountOf state.user.email))]
    (set "state.reauth.open" true)
    (cond (= state.reauth.password "")
          (refuse "state.reauth.error" "empty" "#reauthPassword")
          (and (not= password "") (not= password state.reauth.password))
          (do (set "state.reauth.password" "")
              (refuse "state.reauth.error" "wrong" "#reauthPassword"))
          :else
          (do (set "state.confirmedLeft" 120)
              (set "state.reauth.open" false)
              (set "state.reauth.password" "")
              (run state.reauth.then)))))

(defn closeReauth "Cancels the password asked again." []
  (set "state.reauth.open" false)
  (set "state.reauth.password" ""))

;;; The profile

(defn saveProfile "Save changes: once the name is right, the fake server saves it." []
  (set "state.profile.submitted" true)
  (when (= computed.profileErrors 0) (serverCall "finishSave" "save")))

(defn finishSave "The name, saved on the account." []
  (updateAccount state.user.email {:name (trim state.profile.name)})
  (set "state.user.name" (trim state.profile.name))
  (set "state.profile.submitted" false)
  (logEvent "profile")
  (notify (t "saved")))

(defn setAvatar "The photo's colour, 0 for the initials on the accent." [colour]
  (set "state.user.avatar" colour)
  (updateAccount state.user.email {:avatar colour})
  (set "state.photoOpen" false)
  (focus "#changePhoto")
  (notify (t "photoSaved"))
  (logEvent "profile"))

;;; The password

(defn openChange "Change password, once the password was given again." [] (stepUp "showChange"))

(defn showChange "The change-password dialog, empty." []
  (set "state.change" {:open true :password "" :confirm "" :submitted false})
  (focus "#newPassword"))

(defn changePassword "Change: once the new password is right, the fake server saves it." []
  (set "state.change.submitted" true)
  (set "state.change.open" true)
  (if (not= computed.changeError "")
      (focus "#newPassword")
      (serverCall "finishChange" "password")))

(defn finishChange "The new password, saved on the account." []
  (updateAccount state.user.email {:password state.change.password :passwordDays 0})
  (set "state.user.passwordDays" 0)
  (set "state.change" {:open false :password "" :confirm "" :submitted false})
  (focus "#changePassword")
  (notify (t "changed"))
  (logEvent "password"))

(defn closeChange "Cancels the change of password." []
  (set "state.change.open" false)
  (focus "#changePassword"))

;;; Two-step

(defn setTwoStep "Two-step's switch: on first sets up the app and the backup codes, off asks the password." [on]
  (cond (and on (not state.user.twoStep)) (set "state.setup" {:open true :step "scan" :code "" :error "" :saved false})
        (and (not on) state.user.twoStep) (stepUp "twoStepOff")
        :else (applyTwoStep on)))

(defn twoStepOff "Turns two-step off, once the password was given again." [] (applyTwoStep false))

(defn applyTwoStep "Saves two-step's switch on the account." [on]
  (set "state.user.twoStep" on)
  (updateAccount state.user.email {:twoStep on})
  (notify (t "twoStepTurned" {:on on}))
  (logEvent (if on "twoStepOn" "twoStepOff")))

(defn setupCodeChanged "The setup's code typed: checked as soon as it has its six digits." []
  (set "state.setup.error" "")
  (when (= (count state.setup.code) 6) (setupNext)))

(defn setupFails "The setup stays open with an error: short, wrong or notSaved." [error]
  (set "state.setup.open" true)
  (set "state.setup.error" error))

(defn setupNext "Next, then Done: the code from the app, then the backup codes saved, and two-step is on." []
  (if (= state.setup.step "scan") (checkSetupCode) (finishSetup)))

(defn checkSetupCode "The setup's code: once it is the app's, the backup codes follow." []
  (cond (< (count state.setup.code) 6)
        (setupFails "short")
        (not= state.setup.code state.demoCode)
        (do (setupFails "wrong")
            (set "state.setup.code" "")
            (focus "#setupCode"))
        :else
        (do (set "state.setup.step" "codes")
            (set "state.setup.saved" false)
            (freshCodes)
            (after 0 focusOn "#savedCodes"))))

(defn finishSetup "Done: once the backup codes are saved, two-step is on (or its codes are new)." []
  (if (not state.setup.saved)
      (setupFails "notSaved")
      (do (set "state.setup.open" false)
          (if state.user.twoStep (notify (t "newCodesSaved")) (applyTwoStep true)))))

(defn freshCodes "New backup codes." []
  ;; one demo list for everyone: a real service makes random ones and keeps only their hashes
  (set "state.backupCodes" (map (fn [n] (str "DEMO-" (+ 1001 n))) (range 8))))

(defn setupCancel "Back to the scan from the codes of a first setup, else closes the setup." []
  (set "state.setup.error" "")
  (if (and (= state.setup.step "codes") (not state.user.twoStep))
      (do (set "state.setup.step" "scan")
          (set "state.setup.code" "")
          (set "state.setup.open" true)
          (after 0 focusOn "#setupCode"))
      (set "state.setup.open" false)))

(defn newCodes "New backup codes, the old ones gone." []
  (set "state.setup" {:open true :step "codes" :code "" :error "" :saved false})
  (freshCodes)
  (logEvent "newCodes"))

(defn toggleSaved "Ticks that the backup codes are saved, or not." []
  (set "state.setup.saved" (not state.setup.saved))
  (set "state.setup.error" ""))

(defn copySetupKey "Copies the setup's key, for an app that can't scan." []
  (copy "EXAMPLEEXAMPLE23")
  (notify (t "keyCopied")))

(defn copyBackupCodes "Copies the backup codes, one a line." []
  (copy (join "\n" state.backupCodes))
  (notify (t "codesCopied")))

(defn removeTrusted "Forgets a device two-step remembers for the account." [device]
  (set "state.trusted" (remove (fn [it] (and (ofUser? it) (= (:device it) device))) state.trusted))
  (notify (t "deviceRemoved")))

;;; Emails

(defn openAddEmail "Add email: its form, focused." []
  (set "state.newEmail" {:open true :address "" :submitted false})
  (focus "#newEmailField #textarea-placeholder"))

(defn closeAddEmail "Cancels the email being added." []
  (set "state.newEmail.open" false)
  (focus "#addEmail"))

(defn addEmail "Add: once the address is right, the fake server sends it a link." []
  (set "state.newEmail.submitted" true)
  (if (not= computed.newEmailError "")
      (focus "#newEmailField #textarea-placeholder")
      (serverCall "finishAddEmail" "verifyEmail")))

(defn finishAddEmail "The new email, unverified, its link sent." []
  (let [address (trim state.newEmail.address)]
    (push "state.emails" {:address address :primary false :verified false :sent true :sentAt state.clock})
    (notify (t "emailAdded" {:email address}))
    (set "state.newEmail" {:open false :address "" :submitted false})
    (focus "#addEmail")
    (logEvent "emailAdded")))

(defn verifyEmail "Verify: sends an email its link again." [address]
  (set "state.target" address)
  (serverCall "sendVerification" "verifyEmail"))

(defn sendVerification "The link sent: it opens itself 5 seconds later (tick)." []
  (set "state.emails" (map (fn [it] (if (= (:address it) state.target) (merge it {:sent true :sentAt state.clock}) it))
                           state.emails))
  (notify (t "emailAdded" {:email state.target})))

(defn removeEmail "Remove: an email that is not the primary one, once the password was given again." [address]
  (set "state.target" address)
  (stepUp "finishRemoveEmail"))

(defn finishRemoveEmail "The email, removed." []
  (set "state.emails" (filter (fn [it] (or (not= (:address it) state.target) (:primary it))) state.emails))
  (notify (t "emailRemoved" {:email state.target}))
  (focus "#addEmail")
  (logEvent "emailRemoved"))

(defn makePrimary "Make primary: a verified email, once the password was given again." [address]
  (set "state.target" address)
  (stepUp "finishPrimary"))

(defn finishPrimary "The account, its trusted devices and the sign-in form follow the new address." []
  (let [address state.target]
    (set "state.emails" (map (fn [it] (merge it {:primary (= (:address it) address)})) state.emails))
    (updateAccount state.user.email {:email address})
    (set "state.trusted" (map (fn [it] (if (ofUser? it) (merge it {:email address}) it)) state.trusted))
    (set "state.user.email" address)
    (set "state.email" address)
    (set "state.profile.email" address)
    (notify (t "primaryChanged" {:email address}))
    (logEvent "primaryChanged")))

;;; Passkeys

(defn addPasskey "Add passkey, once the password was given again." [] (stepUp "createPasskey"))

(defn createPasskey "The device's prompt, faked: a real one makes the key pair and sends only the public key." []
  (serverCall "finishPasskey" "passkey"))

(defn finishPasskey "The new passkey: this device's, or a security key's once this device has one." []
  (push "state.myPasskeys" {:id (str "pk" (+ (count state.myPasskeys) 1) "-" state.clock)
                            :device (if (some (fn [it] (= (:device it) "this")) state.myPasskeys) "key" "this")
                            :added 0
                            :used -1})
  (notify (t "passkeyAdded"))
  (focus "#addPasskey")
  (logEvent "passkeyAdded"))

(defn removePasskey "Remove: a passkey, once the password was given again." [id]
  (set "state.target" id)
  (stepUp "finishRemovePasskey"))

(defn finishRemovePasskey "The passkey, removed." []
  (set "state.myPasskeys" (remove (fn [it] (= (:id it) state.target)) state.myPasskeys))
  (notify (t "passkeyRemoved"))
  (logEvent "passkeyRemoved"))

;;; Devices

(defn signOutSession "Signs another device's session out." [id]
  (notify (t "signedOutDevice" {:device (t (str "device_" (:device (find (fn [it] (= (:id it) id)) state.sessions))))}))
  (set "state.sessions" (filter (fn [it] (or (not= (:id it) id) (= (:id it) "here"))) state.sessions))
  (logEvent "signedOutDevice"))

(defn signOutOthers "Sign out everywhere else, once the password was given again." [] (stepUp "endOtherSessions"))

(defn endOtherSessions "Asks the fake server to end the other sessions." []
  (serverCall "finishSignOutOthers" "devices"))

(defn finishSignOutOthers "Only this device's session is left." []
  (set "state.sessions" (filter (fn [it] (= (:id it) "here")) state.sessions))
  (notify (t "signedOutOthers"))
  (logEvent "signedOutOthers"))

;;; Connected accounts

(defn connectProvider "Connect: the provider's consent page, faked." [provider]
  (set "state.target" provider)
  (serverCall "finishConnect" "connect"))

(defn finishConnect "The provider connected, as the username, or the email's name, at the provider." []
  (let [username (or-else state.user.username "")
        local (if (not= username "") username (first (split state.user.email "@")))]
    (push "state.connected" {:provider state.target :account (str local "@" state.target ".example")})
    (notify (t "connectedNow" {:provider (t (str "provider_" state.target))}))
    (logEvent "connected")))

(defn disconnectProvider "Disconnect: a provider, once the password was given again." [provider]
  (set "state.target" provider)
  (stepUp "finishDisconnect"))

(defn finishDisconnect "The provider, disconnected." []
  (set "state.connected" (remove (fn [it] (= (:provider it) state.target)) state.connected))
  (notify (t "disconnectedNow" {:provider (t (str "provider_" state.target))}))
  (logEvent "disconnected"))

;;; Deleting the account

(defn openDelete "Delete account: its dialog, which asks for a word." []
  (set "state.deleting" {:open true :typed "" :error ""})
  (focus "#deleteConfirm"))

(defn closeDelete "Cancels deleting the account." []
  (set "state.deleting.open" false)
  (focus "#deleteAccount"))

(defn confirmDelete "Delete forever: the word typed, then the password asked again." []
  (set "state.deleting.open" true)
  (let [typed (trim state.deleting.typed)]
    (cond (= typed "")
          (refuse "state.deleting.error" "empty" "#deleteConfirm")
          (not= (upper-case typed) (upper-case computed.deleteWord))
          (refuse "state.deleting.error" "wrong" "#deleteConfirm")
          :else
          (do (set "state.deleting.open" false)
              (stepUp "deleteAccount")))))

(defn deleteAccount "Asks the fake server to delete the account." [] (serverCall "finishDelete" "delete"))

(defn finishDelete "The account, its trusted devices and its failed attempts go; nothing is written back." []
  (set "state.accounts" (remove ofUser? state.accounts))
  (set "state.trusted" (remove ofUser? state.trusted))
  (set "state.user.email" "")
  (set "state.email" "")
  (endSession)
  (notify (t "deleted")))
