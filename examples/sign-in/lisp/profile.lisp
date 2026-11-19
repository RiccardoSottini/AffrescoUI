;;; profile.lisp --- what the account page does: the profile, emails, two-step, passkeys, devices, connected
;;; accounts, the activity log and deleting the account
;;;
;;; The account page's views are in account.lisp and account-sections.lisp. A sensitive change asks for the password
;;; again (stepUp), unless it was given in the last 2 minutes; it remembers what it is about in state.target and the
;;; function that makes it in state.reauth.then. Every change is logged (logEvent), and the account's lists are
;;; written back on it when the session ends (saveAccount).

(ns sign-in.profile)

;;; The account's record

(defn update-account "Changes some fields of the account of an email." [email fields]
  (set "state.accounts" (map #(if (same-email? (:email %) email) (merge % fields) %) state.accounts)))

(defn of-user? "Whether a record (an account, a trusted device) is the signed-in account's, by its email." [record]
  (same-email? (:email record) state.user.email))

(defn seed-account
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
                  (concat [{:address (:email user) :primary true :verified true}] (if demo (demo-emails user) []))))
    (set "state.myPasskeys"
         (or-else (:passkeys user) (if (:passkey user) [{:id "pk1" :device "this" :added 12 :used 0}] [])))
    (set "state.sessions" (or-else (:sessions user) (concat [this-session] (if demo demo-sessions []))))
    (set "state.connected"
         (or-else (:connected user)
                  (if demo [{:provider "quillo" :account (str (:username user) "@quillo.example")}] [])))
    (set "state.log" (take 50 (concat [signed-in-here] (or-else (:log user) (if demo (demo-log user) [])))))))

(def this-session "This device's session, the first of the list."
  {:id "here" :device "this" :app "desktop" :place "Lakeside" :days 0})

(def signed-in-here "The sign-in that opened this session: the log's newest event."
  {:kind "signIn" :at 0 :days 0 :device "this" :place "Lakeside"})

(def demo-sessions "A demo account's other sessions: a phone and a laptop."
  [{:id "phone" :device "phone" :app "mobile" :place "Lakeside" :days 1}
   {:id "laptop" :device "laptop" :app "browser" :place "Northport" :days 6}])

(defn demo-emails "A demo account's other emails: one verified, one not." [user]
  [{:address (str (:username user) ".home@example.net") :primary false :verified true}
   {:address (str (:username user) "@work.example.org") :primary false :verified false}])

(defn demo-log "A demo account's past events: a sign-in, one blocked, and the last password change." [user]
  [{:kind "signIn" :at 0 :days 1 :device "phone" :place "Lakeside"}
   {:kind "signInBlocked" :at 0 :days 3 :device "laptop" :place "Eastvale"}
   {:kind "password" :at 0 :days (or-else (:passwordDays user) 30) :device "laptop" :place "Northport"}])

(defn save-account "Writes the session's lists back on the account, for the next sign-in (times kept relative)." []
  (update-account state.user.email
                 {:emails state.emails
                  :passkeys state.myPasskeys
                  :passkey (boolean (seq state.myPasskeys))
                  :sessions state.sessions
                  :connected state.connected
                  :log (map #(merge % {:at (- (:at %) state.clock)}) state.log)}))

(defn log-event "An event of the account's activity, the newest first, at most 50." [kind]
  (set "state.log" (take 50 (cons {:kind kind :at state.clock :days 0 :device "this" :place "Lakeside"} state.log))))

(defn set-section "Shows a section of the account page, closing what was open in the last." [section]
  (set "state.section" section)
  (set "state.newEmail.open" false)
  (set "state.photoOpen" false))

;;; Asking the password again

(defn step-up "Runs a sensitive change, by its function's name, once the password was given again." [then]
  (if (or (pos? state.confirmedLeft) state.user.provider)
      (run then)
      (do (set "state.reauth" {:open true :password "" :error "" :then then})
          (focus "#reauthPassword"))))

(defn confirm-reauth "Confirm: with the right password, the change goes on, and none is asked for 2 minutes." []
  (let [password (:password (account-of state.user.email))]
    (set "state.reauth.open" true)
    (cond (empty? state.reauth.password)
          (refuse "state.reauth.error" "empty" "#reauthPassword")
          (and (not= password "") (not= password state.reauth.password))
          (do (set "state.reauth.password" "")
              (refuse "state.reauth.error" "wrong" "#reauthPassword"))
          :else
          (do (set "state.confirmedLeft" 120)
              (set "state.reauth.open" false)
              (set "state.reauth.password" "")
              (run state.reauth.then)))))

(defn close-reauth "Cancels the password asked again." []
  (set "state.reauth.open" false)
  (set "state.reauth.password" ""))

;;; The profile

(defn save-profile "Save changes: once the name is right, the fake server saves it." []
  (set "state.profile.submitted" true)
  (when (zero? computed.profileErrors) (server-call "finishSave" "save")))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishSave "The name, saved on the account." []
  (update-account state.user.email {:name (str/trim state.profile.name)})
  (set "state.user.name" (str/trim state.profile.name))
  (set "state.profile.submitted" false)
  (log-event "profile")
  (notify (t "saved")))

(defn set-avatar "The photo's colour, 0 for the initials on the accent." [colour]
  (set "state.user.avatar" colour)
  (update-account state.user.email {:avatar colour})
  (set "state.photoOpen" false)
  (focus "#changePhoto")
  (notify (t "photoSaved"))
  (log-event "profile"))

;;; The password

(defn open-change "Change password, once the password was given again." [] (step-up "showChange"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn showChange "The change-password dialog, empty." []
  (set "state.change" {:open true :password "" :confirm "" :submitted false})
  (focus "#newPassword"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn changePassword "Change: once the new password is right, the fake server saves it." []
  (set "state.change.submitted" true)
  (set "state.change.open" true)
  (if (not= computed.changeError "")
      (focus "#newPassword")
      (server-call "finishChange" "password")))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishChange "The new password, saved on the account." []
  (update-account state.user.email {:password state.change.password :passwordDays 0})
  (set "state.user.passwordDays" 0)
  (set "state.change" {:open false :password "" :confirm "" :submitted false})
  (focus "#changePassword")
  (notify (t "changed"))
  (log-event "password"))

(defn close-change "Cancels the change of password." []
  (set "state.change.open" false)
  (focus "#changePassword"))

;;; Two-step

(defn set-two-step "Two-step's switch: on first sets up the app and the backup codes, off asks the password." [on]
  (cond (and on (not state.user.twoStep)) (set "state.setup" {:open true :step "scan" :code "" :error "" :saved false})
        (and (not on) state.user.twoStep) (step-up "twoStepOff")
        :else (apply-two-step on)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn twoStepOff "Turns two-step off, once the password was given again." [] (apply-two-step false))

(defn apply-two-step "Saves two-step's switch on the account." [on]
  (set "state.user.twoStep" on)
  (update-account state.user.email {:twoStep on})
  (notify (t "twoStepTurned" {:on on}))
  (log-event (if on "twoStepOn" "twoStepOff")))

(defn setup-code-changed "The setup's code typed: checked as soon as it has its six digits." []
  (set "state.setup.error" "")
  (when (= (count state.setup.code) 6) (setup-next)))

(defn setup-fails "The setup stays open with an error: short, wrong or notSaved." [error]
  (set "state.setup.open" true)
  (set "state.setup.error" error))

(defn setup-next "Next, then Done: the code from the app, then the backup codes saved, and two-step is on." []
  (if (= state.setup.step "scan") (check-setup-code) (finish-setup)))

(defn check-setup-code "The setup's code: once it is the app's, the backup codes follow." []
  (cond (< (count state.setup.code) 6)
        (setup-fails "short")
        (not= state.setup.code state.demoCode)
        (do (setup-fails "wrong")
            (set "state.setup.code" "")
            (focus "#setupCode"))
        :else
        (do (set "state.setup.step" "codes")
            (set "state.setup.saved" false)
            (fresh-codes)
            (after 0 focus-on "#savedCodes"))))

(defn finish-setup "Done: once the backup codes are saved, two-step is on (or its codes are new)." []
  (if-not state.setup.saved
      (setup-fails "notSaved")
      (do (set "state.setup.open" false)
          (if state.user.twoStep (notify (t "newCodesSaved")) (apply-two-step true)))))

(defn fresh-codes "New backup codes." []
  ;; one demo list for everyone: a real service makes random ones and keeps only their hashes
  (set "state.backupCodes" (map #(str "DEMO-" (+ 1001 %)) (range 8))))

(defn setup-cancel "Back to the scan from the codes of a first setup, else closes the setup." []
  (set "state.setup.error" "")
  (if (and (= state.setup.step "codes") (not state.user.twoStep))
      (do (set "state.setup.step" "scan")
          (set "state.setup.code" "")
          (set "state.setup.open" true)
          (after 0 focus-on "#setupCode"))
      (set "state.setup.open" false)))

; style: keep, a string of the code names it, and a rename never changes a string
(defn newCodes "New backup codes, the old ones gone." []
  (set "state.setup" {:open true :step "codes" :code "" :error "" :saved false})
  (fresh-codes)
  (log-event "newCodes"))

(defn toggle-saved "Ticks that the backup codes are saved, or not." []
  (set "state.setup.saved" (not state.setup.saved))
  (set "state.setup.error" ""))

(defn copy-setup-key "Copies the setup's key, for an app that can't scan." []
  (copy "EXAMPLEEXAMPLE23")
  (notify (t "keyCopied")))

(defn copy-backup-codes "Copies the backup codes, one a line." []
  (copy (str/join "\n" state.backupCodes))
  (notify (t "codesCopied")))

(defn remove-trusted "Forgets a device two-step remembers for the account." [device]
  (set "state.trusted" (remove #(and (of-user? %) (= (:device %) device)) state.trusted))
  (notify (t "deviceRemoved")))

;;; Emails

(defn open-add-email "Add email: its form, focused." []
  (set "state.newEmail" {:open true :address "" :submitted false})
  (focus "#newEmailField #textarea-placeholder"))

(defn close-add-email "Cancels the email being added." []
  (set "state.newEmail.open" false)
  (focus "#addEmail"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addEmail "Add: once the address is right, the fake server sends it a link." []
  (set "state.newEmail.submitted" true)
  (if (not= computed.newEmailError "")
      (focus "#newEmailField #textarea-placeholder")
      (server-call "finishAddEmail" "verifyEmail")))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishAddEmail "The new email, unverified, its link sent." []
  (let [address (str/trim state.newEmail.address)]
    (push "state.emails" {:address address :primary false :verified false :sent true :sentAt state.clock})
    (notify (t "emailAdded" {:email address}))
    (set "state.newEmail" {:open false :address "" :submitted false})
    (focus "#addEmail")
    (log-event "emailAdded")))

; style: keep, a string of the code names it, and a rename never changes a string
(defn verifyEmail "Verify: sends an email its link again." [address]
  (set "state.target" address)
  (server-call "sendVerification" "verifyEmail"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn sendVerification "The link sent: it opens itself 5 seconds later (tick)." []
  (set "state.emails" (map #(if (= (:address %) state.target) (merge % {:sent true :sentAt state.clock}) %)
                           state.emails))
  (notify (t "emailAdded" {:email state.target})))

(defn remove-email "Remove: an email that is not the primary one, once the password was given again." [address]
  (set "state.target" address)
  (step-up "finishRemoveEmail"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishRemoveEmail "The email, removed." []
  (set "state.emails" (filter #(or (not= (:address %) state.target) (:primary %)) state.emails))
  (notify (t "emailRemoved" {:email state.target}))
  (focus "#addEmail")
  (log-event "emailRemoved"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn makePrimary "Make primary: a verified email, once the password was given again." [address]
  (set "state.target" address)
  (step-up "finishPrimary"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishPrimary "The account, its trusted devices and the sign-in form follow the new address." []
  (let [address state.target]
    (set "state.emails" (map #(merge % {:primary (= (:address %) address)}) state.emails))
    (update-account state.user.email {:email address})
    (set "state.trusted" (map #(if (of-user? %) (merge % {:email address}) %) state.trusted))
    (set "state.user.email" address)
    (set "state.email" address)
    (set "state.profile.email" address)
    (notify (t "primaryChanged" {:email address}))
    (log-event "primaryChanged")))

;;; Passkeys

; style: keep, a string of the code names it, and a rename never changes a string
(defn addPasskey "Add passkey, once the password was given again." [] (step-up "createPasskey"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn createPasskey "The device's prompt, faked: a real one makes the key pair and sends only the public key." []
  (server-call "finishPasskey" "passkey"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishPasskey "The new passkey: this device's, or a security key's once this device has one." []
  (push "state.myPasskeys" {:id (str "pk" (inc (count state.myPasskeys)) "-" state.clock)
                            :device (if (some #(= (:device %) "this") state.myPasskeys) "key" "this")
                            :added 0
                            :used -1})
  (notify (t "passkeyAdded"))
  (focus "#addPasskey")
  (log-event "passkeyAdded"))

(defn remove-passkey "Remove: a passkey, once the password was given again." [id]
  (set "state.target" id)
  (step-up "finishRemovePasskey"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishRemovePasskey "The passkey, removed." []
  (set "state.myPasskeys" (remove #(= (:id %) state.target) state.myPasskeys))
  (notify (t "passkeyRemoved"))
  (log-event "passkeyRemoved"))

;;; Devices

(defn sign-out-session "Signs another device's session out." [id]
  (notify (t "signedOutDevice" {:device (t (str "device_" (:device (find #(= (:id %) id) state.sessions))))}))
  (set "state.sessions" (filter #(or (not= (:id %) id) (= (:id %) "here")) state.sessions))
  (log-event "signedOutDevice"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn signOutOthers "Sign out everywhere else, once the password was given again." [] (step-up "endOtherSessions"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn endOtherSessions "Asks the fake server to end the other sessions." []
  (server-call "finishSignOutOthers" "devices"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishSignOutOthers "Only this device's session is left." []
  (set "state.sessions" (filter #(= (:id %) "here") state.sessions))
  (notify (t "signedOutOthers"))
  (log-event "signedOutOthers"))

;;; Connected accounts

(defn connect-provider "Connect: the provider's consent page, faked." [provider]
  (set "state.target" provider)
  (server-call "finishConnect" "connect"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishConnect "The provider connected, as the username, or the email's name, at the provider." []
  (let [local (or (not-empty state.user.username) (first (str/split state.user.email "@")))]
    (push "state.connected" {:provider state.target :account (str local "@" state.target ".example")})
    (notify (t "connectedNow" {:provider (t (str "provider_" state.target))}))
    (log-event "connected")))

(defn disconnect-provider "Disconnect: a provider, once the password was given again." [provider]
  (set "state.target" provider)
  (step-up "finishDisconnect"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishDisconnect "The provider, disconnected." []
  (set "state.connected" (remove #(= (:provider %) state.target) state.connected))
  (notify (t "disconnectedNow" {:provider (t (str "provider_" state.target))}))
  (log-event "disconnected"))

;;; Deleting the account

(defn open-delete "Delete account: its dialog, which asks for a word." []
  (set "state.deleting" {:open true :typed "" :error ""})
  (focus "#deleteConfirm"))

(defn close-delete "Cancels deleting the account." []
  (set "state.deleting.open" false)
  (focus "#deleteAccount"))

(defn confirm-delete "Delete forever: the word typed, then the password asked again." []
  (set "state.deleting.open" true)
  (let [typed (str/trim state.deleting.typed)]
    (cond (empty? typed)
          (refuse "state.deleting.error" "empty" "#deleteConfirm")
          (not= (str/upper-case typed) (str/upper-case computed.deleteWord))
          (refuse "state.deleting.error" "wrong" "#deleteConfirm")
          :else
          (do (set "state.deleting.open" false)
              (step-up "deleteAccount")))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn deleteAccount "Asks the fake server to delete the account." [] (server-call "finishDelete" "delete"))

; style: keep, a string of the code names it, and a rename never changes a string
(defn finishDelete "The account, its trusted devices and its failed attempts go; nothing is written back." []
  (set "state.accounts" (remove of-user? state.accounts))
  (set "state.trusted" (remove of-user? state.trusted))
  (set "state.user.email" "")
  (set "state.email" "")
  (end-session)
  (notify (t "deleted")))
