;;; computed.lisp --- what Sign in works out from its state
;;;
;;; Each defcomputed is a value the screen reads as computed.name: worked out when the state it reads changes, at an
;;; expression's speed (SCREEN_FORMAT.md, "Values, expressions and state"). The forms are Lisp; `it` is the item of a
;;; function on each item, as in (filter (fn [it] ...) list).

(ns sign-in.computed)

;;; Helpers

(defmacro strength-of "A password's strength, its score 0 and nothing missing while it is empty." [password]
  (list 'merge {:score 0 :label "" :missing []} (list 'passwordStrength password)))

(defmacro clock-text "Seconds as minutes:seconds, never below 0:00." [seconds]
  `(str (floor (/ (max 0 ~seconds) 60)) ":" (pad (mod (max 0 ~seconds) 60) 2 "0")))

(defmacro spelled-keys
  "A key label's symbols as words (⌘ as Cmd+, ⇧ as Shift+...), after a | that slice takes off."
  [label]
  (reduce (fn [form [sign word]] (list 'join word (list 'split form sign)))
          (list 'str "|" label)
          [["⌃" "Ctrl+"] ["⌥" "Option+"] ["⇧" "Shift+"] ["⌘" "Cmd+"]]))

(defmacro key-of "The keys of a command, spelled out." [command]
  `(:keyText (find #(= (:name %) ~command) computed.shortcuts)))

(defmacro same-email? "Two emails, the same whatever their case." [a b]
  `(= (str/lower-case ~a) (str/lower-case ~b)))

;;; The sign-in form

(defcomputed emailValid (isEmail (str/trim state.email)))

(defcomputed passwordValid (>= (count state.password) 8))

(defcomputed account "The account of the email typed, if there is one."
  (find #(same-email? (:email %) (str/trim state.email)) state.accounts))

(defcomputed locked "Five wrong passwords in a row lock the account until its password is reset."
  (>= (count (filter #(= % (str/lower-case (str/trim state.email))) state.failures)) 5))

(defcomputed errors "How many of the form's fields are wrong, once it was sent."
  (if-not state.submitted 0 (+ (if computed.emailValid 0 1) (if computed.passwordValid 0 1))))

(defcomputed passkeys "The accounts on this device that saved a passkey." (filter :passkey state.accounts))

(defcomputed providerTitle "The provider's name, for the error it gave."
  (if (= state.providerName "exampleId") "Example ID" (t (str "provider_" state.providerName))))

(defcomputed busyText "What the status line says while the fake server works."
  (if (= state.busy "") "" (t "busy" {:what state.busy})))

;;; Creating an account

(defcomputed strength (strength-of state.password))

(defcomputed newPasswordValid (and computed.passwordValid (>= computed.strength.score 2)))

(defcomputed newPasswordMessage (if computed.passwordValid (t "stronger") (t "min8")))

(defcomputed confirmValid (or state.showPassword (= state.confirm state.password)))

(defcomputed usernameStatus "Empty, invalid, checking (until the demo server answered), taken or available."
  (let [typed (str/trim state.username)]
    (cond (empty? typed) ""
          (or (< (count typed) 3)
              (> (count typed) 20)
              (not (every? #(str/includes? "abcdefghijklmnopqrstuvwxyz0123456789_." %)
                           (str/split (str/lower-case typed) "")))) "invalid"
          (not= state.usernameChecked (str/lower-case typed)) "checking"
          (#{"ada" "sam" "admin"} state.usernameChecked) "taken"
          :else "available")))

(defcomputed inviteValid (= (str/upper-case (str/trim state.invite.code)) "EXAMPLE-INVITE"))

(defcomputed createErrors "How many of the new account's fields are wrong, once it was sent."
  (if-not state.submitted
      0
      (+ (if (empty? (str/trim state.name)) 1 0)
         (if computed.emailValid 0 1)
         (if (= computed.usernameStatus "available") 0 1)
         (if computed.newPasswordValid 0 1)
         (if computed.confirmValid 0 1)
         (if state.terms 0 1)
         (if (and state.invite.open (not-empty (str/trim state.invite.code)) (not computed.inviteValid)) 1 0))))

;;; Codes and the new password

(defcomputed trustedHere "Two-step remembers this device for the account signing in."
  (some #(and (same-email? (:email %) state.pending.email) (= (:device %) "this")) state.trusted))

(defcomputed resetStrength (strength-of state.reset.password))

(defcomputed resetValid (and (>= (count state.reset.password) 8) (>= computed.resetStrength.score 2)))

(defcomputed resetConfirmValid (or state.showPassword (= state.reset.confirm state.reset.password)))

(defcomputed resetErrors
  (if-not state.reset.submitted 0 (+ (if computed.resetValid 0 1) (if computed.resetConfirmValid 0 1))))

(defcomputed resetTime "How long the emailed code still lasts." (clock-text state.reset.left))

;;; The account page

(defcomputed sessionTime "How long until the session ends." (clock-text state.session.left))

(defcomputed initials "The avatar's letter: the name's first, or the email's."
  (let [name (str/trim state.user.name)]
    (str/upper-case (slice (if (empty? name) state.user.email name) 0 1))))

(defcomputed avatarColor
  (if (pos? (or-else state.user.avatar 0)) (str "@color.avatar" state.user.avatar) "@color.accentSoft"))

(defcomputed avatarText (if (pos? (or-else state.user.avatar 0)) "@color.avatarText" "@color.accent"))

(defcomputed wide "The window is wide enough for the sections down the side." (>= window.width 760))

(defcomputed sections "The account's sections, in their order, each with its icon."
  [{:name "profile" :icon "user"}
   {:name "emails" :icon "mail"}
   {:name "security" :icon "shield-check"}
   {:name "devices" :icon "monitor-smartphone"}
   {:name "connected" :icon "link"}
   {:name "activity" :icon "history"}
   {:name "data" :icon "trash-2"}])

(defcomputed profileErrors (if (and state.profile.submitted (empty? (str/trim state.profile.name))) 1 0))

(defcomputed changeError "Why the new password in the change dialog is refused, or nothing."
  (cond (< (count state.change.password) 8) (t "min8")
        (< (:score (strength-of state.change.password)) 2) (t "stronger")
        (and (not state.showPassword) (not= state.change.confirm state.change.password)) (t "mismatch")
        :else ""))

(defcomputed newEmailError "Why the email being added is refused (invalid, taken, full), or nothing."
  (cond (not (isEmail (str/trim state.newEmail.address))) "invalid"
        (some #(same-email? (:address %) (str/trim state.newEmail.address)) state.emails) "taken"
        (>= (count state.emails) 5) "full"
        :else ""))

(defcomputed passwordDays (or-else state.user.passwordDays 0))

(defcomputed myTrusted "The devices two-step remembers for the account signed in."
  (filter #(same-email? (:email %) state.user.email) state.trusted))

(defcomputed otpUri "The two-step setup's QR code, as the authenticator apps read it."
  (str "otpauth://totp/Example%20ID:" state.user.email "?secret=EXAMPLEEXAMPLE23&issuer=Example%20ID"))

(defcomputed otpQr (qrCode computed.otpUri "M"))

(defcomputed providers "The made-up providers an account can connect."
  [{:name "wrenfield" :icon "bird"} {:name "mossbank" :icon "leaf"} {:name "quillo" :icon "feather"}])

(defcomputed deleteWord "The word to type to delete the account, in the language shown." (t "deleteWord"))

(defcomputed eventIcons "The activity log's icon for each kind of event."
  {:signIn "log-in"
   :signInBlocked "shield-alert"
   :password "key-round"
   :twoStepOn "shield-check"
   :twoStepOff "shield-off"
   :newCodes "key"
   :passkeyAdded "fingerprint"
   :passkeyRemoved "fingerprint"
   :emailAdded "mail-plus"
   :emailRemoved "mail-x"
   :emailVerified "mail-check"
   :primaryChanged "mail"
   :signedOutOthers "log-out"
   :signedOutDevice "log-out"
   :connected "link"
   :disconnected "link-2-off"
   :profile "user"})

(defcomputed deviceIcons {:this "monitor" :phone "smartphone" :laptop "laptop" :tablet "tablet" :key "key-round"})

;;; Keyboard shortcuts

(defcomputed shortcuts "The commands, each with its keys spelled out (Cmd+J for ⌘J)."
  (map #(merge % {:keyText (slice (spelled-keys (or-else (:keyLabel %) "")) 1)}) commands))

(defcomputed themeKey (key-of "toggleTheme"))

(defcomputed shortcutsKey (key-of "openShortcuts"))

(defcomputed signOutKey (key-of "signOut"))
