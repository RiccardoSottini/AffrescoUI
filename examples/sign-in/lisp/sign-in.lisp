;;; sign-in.lisp --- what Sign in does to sign a person in, and out: its functions, commands and keys
;;;
;;; The screens and their views (sign-in-form.lisp, create-account.lisp, codes.lisp...) say what is shown; this file
;;; says what happens until a session opens, and when it ends. A function reads the running app through paths
;;; (`state.step`, `computed.account`) and changes it only through steps (`set`, `focus`, `navigate`...). There is no
;;; real server: `serverCall` answers at once, or after 800 ms with Slow network on, by running the function named
;;; to answer. What the account page does is in profile.lisp.

;;; The provider

(defpart provider "Example ID, the made-up OAuth 2 provider the sign-in form offers."
  ;; no real client id or secret: scenarios play its answers as the providerSignedIn and providerFailed functions
  :connections {:exampleId {:url "https://id.example.com/api"
                            :auth {:flow browser
                                   :signInUrl "https://id.example.com/authorize"
                                   :tokenUrl "https://id.example.com/token"
                                   :clientId affrescoui-sign-in-demo
                                   :scopes [openid email profile]}}})

;;; Small things every screen uses

(defn hideToast "Hides the notice in the corner." [] (set "state.toast.open" false))

(defn notify "A notice in the corner for a few seconds." [message]
  (set "state.toast" {:open true :message message :action ""})
  (debounce 4000 hideToast))

(defn refuse "Says why something was refused, at a path of the state, and moves the focus where to fix it."
  [path why target]
  (set path why)
  (focus target))

(defn focusOn "Moves the focus to a node, by its selector: run with after, once the node is drawn." [target]
  (focus target))

(defn runIfAny "Runs a function named by a component's parameter, if it names one." [action]
  (when (not= action "") (run action)))

(defn focusField "Marks a checked field focused, by its key." [key] (set "state.focus" key))

(defn blurField "Unmarks a checked field when it loses the focus, unless another took it already." [key]
  (when (= state.focus key) (set "state.focus" "")))

(defn accountOf "The account of an email, whatever its case." [email]
  (find (fn [it] (same-email? (:email it) email)) state.accounts))

(defn serverCall "Asks the fake server: what marks the busy button, answer the function that answers." [answer what]
  (when (= state.busy "")
    (set "state.busy" what)
    (if state.slow (after 800 answered answer) (answered answer))))

(defn answered "The fake server's answer: no longer busy, it runs the function named to answer." [answer]
  (set "state.busy" "")
  (run answer))

;;; Email, then password

(defn submitSignIn "The form's button and Enter: on the email step, looks the account up; on the next, signs in." []
  (set "state.submitted" true)
  (set "state.formError" "")
  (cond (= state.step "email") (when computed.emailValid (serverCall "lookup" "lookup"))
        computed.passwordValid (serverCall "checkPassword" "signIn")))

(defn lookup "The identifier step's answer: with an account for the email, the password step follows." []
  (if (nil? computed.account)
      (set "state.formError" "noAccount")
      (do (set "state.submitted" false)
          (set "state.step" "password")
          ;; the email field is gone on this step, so the first field is the password
          (focus "#textarea-placeholder"))))

(defn changeEmail "Not you?: back to the email step." []
  (set "state.step" "email")
  (set "state.password" "")
  (set "state.submitted" false)
  (set "state.formError" "")
  (focus "#textarea-placeholder"))

(defn wrongPassword? "The password typed is not the account's (a demo account takes any)." []
  (and (not= computed.account.password "") (not= computed.account.password state.password)))

(defn checkPassword
  "The password step's answer: a locked account stays shut, a wrong password counts toward the lock, else two-step."
  []
  (cond (nil? computed.account)
        (set "state.formError" "noAccount")
        computed.locked
        (do (hideToast)
            (refuse "state.formError" "locked" "#unlock"))
        (wrongPassword?)
        (do (hideToast)
            (push "state.failures" (lower-case (trim state.email)))
            (set "state.formError" (if computed.locked "locked" "wrongPassword")))
        :else
        (do (set "state.submitted" false)
            (set "state.pending" computed.account)
            (set "state.codeMode" "twoStep")
            (if (and computed.account.twoStep (not computed.trustedHere)) (startTwoStep) (enter)))))

(defn enter "Opens the session of the account signing in (state.pending), on the account page." []
  (set "state.user" state.pending)
  (set "state.email" state.pending.email)
  (set "state.password" "")
  (set "state.confirm" "")
  (set "state.code" "")
  (set "state.formError" "")
  (set "state.notice" "")
  (set "state.session" {:left 300 :warned false})
  (set "state.profile" {:name state.pending.name :email state.pending.email :submitted false})
  (set "state.confirmedLeft" 120)
  (seedAccount)
  (navigate "signedIn"))

(defn toCreate "No account yet?: the create-account screen, its password empty, not the one typed to sign in." []
  (set "state.submitted" false)
  (set "state.formError" "")
  (set "state.password" "")
  (set "state.confirm" "")
  (navigate "createAccount"))

(defn goBack "Back to the sign-in form's email step, from any inner screen (and Escape)." []
  (set "state.step" "email")
  (set "state.submitted" false)
  (set "state.codeError" "")
  (navigate "signInForm"))

;;; Codes

(defn startTwoStep "The code screen, a code sent: by the app for two-step, by email for the other modes." []
  (set "state.trust" false)
  (set "state.backupCode" "")
  (set "state.code" "")
  (set "state.codeError" "")
  (set "state.resendLeft" 30)
  (navigate "twoStep")
  (focus "#twoStepCode")
  (notify (t "codeSent" {:email state.pending.email})))

(defn codeChanged "A code typed: checked as soon as it has its six digits." []
  (set "state.codeError" "")
  (when (= (count state.code) 6) (verify)))

(defn verify "Verify: a backup code, or the six digits." []
  (cond (= state.codeMode "backup") (if (= (trim state.backupCode) "")
                                        (set "state.codeError" "shortBackup")
                                        (serverCall "checkBackup" "verify"))
        (< (count state.code) 6) (set "state.codeError" "short")
        :else (serverCall "checkCode" "verify")))

(defn checkCode
  "The code's answer: a new account is verified, an emailed code still asks for two-step, else the session opens."
  []
  (cond (not= state.code state.demoCode)
        (do (set "state.code" "")
            (refuse "state.codeError" "wrong" "#twoStepCode"))
        (= state.codeMode "verify")
        (finishVerify)
        (and (= state.codeMode "email") state.pending.twoStep (not computed.trustedHere))
        (do (set "state.codeMode" "twoStep")
            (startTwoStep))
        (= state.codeMode "email")
        (enter)
        :else
        (passTwoStep)))

(defn checkBackup "A backup code's answer: a code of the list works once." []
  (let [typed (upper-case (trim state.backupCode))]
    (if (not (contains state.backupCodes typed))
        (refuse "state.codeError" "wrongBackup" "#backupField")
        (do (set "state.backupCodes" (remove (fn [it] (= it typed)) state.backupCodes))
            (set "state.backupCode" "")
            (notify (t "backupUsed" {:n (count state.backupCodes)}))
            (passTwoStep)))))

(defn passTwoStep "The second step passed: remembers this device when asked, then opens the session." []
  (when (and state.trust (not computed.trustedHere))
    (set "state.trusted" (concat state.trusted [{:email state.pending.email :device "this" :days 30}])))
  (set "state.trust" false)
  (enter))

(defn useMode "Another way of two-step: the app, a text or a backup code." [mode]
  (hideToast)
  (set "state.codeMode" mode)
  (set "state.code" "")
  (set "state.codeError" "")
  (set "state.backupCode" "")
  (when (= mode "sms")
    (set "state.resendLeft" 30)
    (notify (t "smsSent" {:phone state.phone})))
  (after 0 focusOn (if (= mode "backup") "#backupField" "#twoStepCode")))

(defn resend "Resend, once its countdown is over." []
  (when (<= state.resendLeft 0) (serverCall "codeResent" "resend")))

(defn codeResent "A new code sent: the countdown starts again." []
  (set "state.resendLeft" 30)
  (set "state.code" "")
  (set "state.codeError" "")
  (focus "#twoStepCode")
  (notify (t "codeResent" {:email state.pending.email})))

(defn countDown "Each second on the code screen: the wait before a new code can be sent." []
  (when (> state.resendLeft 0) (increment "state.resendLeft" :by -1)))

(defn codeBack "Back: to the create-account screen while verifying a new account, else to the sign-in form." []
  (if (= state.codeMode "verify")
      (do (set "state.codeMode" "twoStep")
          (set "state.codeError" "")
          (navigate "createAccount"))
      (goBack)))

(defn emailCodeSignIn "A code sent to the account's email instead of the password." []
  (if (nil? computed.account)
      (set "state.formError" "noAccount")
      (do (set "state.formError" "")
          (set "state.submitted" false)
          (set "state.pending" computed.account)
          (set "state.codeMode" "email")
          (serverCall "startTwoStep" "emailCode"))))

;;; Creating an account

(defn submitNewAccount "Create account: once every field is right, the fake server makes it." []
  (set "state.submitted" true)
  (when (= computed.createErrors 0) (serverCall "finishCreate" "create")))

(defn finishCreate "The account waits for its email to be verified, by a code." []
  (set "state.submitted" false)
  (set "state.pending" {:email (trim state.email)
                        :name (trim state.name)
                        :username (lower-case (trim state.username))
                        :twoStep false
                        :provider false
                        :passkey false})
  (set "state.codeMode" "verify")
  (startTwoStep))

(defn finishVerify "The email is verified: the account exists, and its session opens." []
  (set "state.accounts" (concat (remove (fn [it] (same-email? (:email it) state.pending.email)) state.accounts)
                                [(merge state.pending {:password state.password})]))
  (set "state.codeMode" "twoStep")
  (enter)
  (notify (if computed.inviteValid
              (t "joinedTeam" {:name state.pending.name})
              (t "welcome" {:name state.pending.name})))
  (set "state.username" "")
  (set "state.usernameChecked" "")
  (set "state.terms" false)
  (set "state.invite" {:open false :code ""}))

(defn usernameChanged "The username typed: its spinner turns, and the demo server checks it once typing stops." []
  (set "state.spin" 0)
  (animate "state.spin" :to 360 :ms 600 :easing "linear")
  (debounce 600 checkUsername))

(defn checkUsername "The demo server's check of the username typed." []
  (set "state.usernameChecked" (lower-case (trim state.username))))

(defn toggleTerms "Ticks the terms, or not." [] (set "state.terms" (not state.terms)))

(defn openInvite "Have an invite?: its field, focused." []
  (set "state.invite.open" true)
  (after 0 focusOn "#invite #textarea-placeholder"))

;;; Other ways in

(defn passkeySignIn "The device's passkey picker, faked: the accounts on this device that saved one." []
  (when (= state.busy "")
    (set "state.formError" "")
    (set "state.passkeyOpen" true)))

(defn choosePasskey "An account chosen in the passkey picker, by its email." [email]
  (set "state.passkeyOpen" false)
  (set "state.pending" (merge (find (fn [it] (= (:email it) email)) state.accounts) {:twoStep false}))
  (serverCall "passkeyDone" "passkey"))

(defn passkeyDone "A passkey is two factors already, so it skips two-step." []
  (set "state.submitted" false)
  (enter))

(defn focusPasskey "The focus back on the passkey button." [] (focus "#passkey"))

(defn cancelPasskey "The passkey picker closed without a choice." []
  (set "state.passkeyOpen" false)
  (set "state.formError" "passkeyCancelled")
  (after 0 focusPasskey))

(defn providerSignIn "Example ID's button: its sign-in page, through the connection; scenarios play its answers." []
  (when (= state.busy "")
    (set "state.formError" "")
    (set "state.providerName" "exampleId")
    (set "state.busy" "provider")
    (signIn "exampleId" :onDone providerAnswered :onError providerRefused)))

(defn providerAnswered "Example ID signed in: its account goes on." []
  (finishProvider (or-else auth.exampleId.account "") ""))

(defn providerRefused "Example ID could not sign in." [error]
  (failProvider error.message))

(defn providerSignedIn "What Example ID answers after a sign-in, as an event: { account, name }. Scenarios play it."
  [event]
  (finishProvider (or-else event.account "") (or-else event.name "")))

(defn providerFailed "What Example ID answers when the sign-in fails, as an event: { message }. Scenarios play it."
  [event]
  (failProvider (or-else event.message "")))

(defn otherProvider "A made-up provider that always says yes, after a moment." [provider]
  (when (= state.busy "")
    (set "state.formError" "")
    (set "state.providerName" provider)
    (set "state.busy" "provider")
    (after 1200 finishProvider (str "you@" provider ".example") "")))

(defn finishProvider
  "A provider's answer: its account, made if it is new, signs in. Ignored once cancelled."
  ;; failProvider then finds nothing waited for, so an answer with no email only stops the wait
  [email given-name]
  (when (= state.busy "provider")
    (set "state.busy" "")
    (if (not (isEmail email))
        (failProvider "")
        (do (when (nil? (accountOf email))
              (push "state.accounts" {:email email
                                      :name (if (not= given-name "") given-name (first (split email "@")))
                                      :password ""
                                      :twoStep false
                                      :provider true}))
            (set "state.submitted" false)
            (set "state.pending" (merge (accountOf email) {:provider true :twoStep false}))
            (enter)))))

(defn failProvider "A provider could not sign in: why, over the form. Ignored once cancelled." [message]
  (when (= state.busy "provider")
    (set "state.busy" "")
    (set "state.providerMessage" message)
    (set "state.formError" "provider")
    (focus "#provider")))

(defn cancelProvider "Stops waiting for a provider, back to the email." []
  (set "state.busy" "")
  (focus "#textarea-placeholder"))

;;; A forgotten password

(defn openForgot "Forgot password?: its dialog, with the email typed so far." []
  (set "state.forgot" {:open true :email state.email :error ""})
  (focus "#forgotEmail"))

(defn closeForgot "Closes the forgot-password dialog." [] (set "state.forgot.open" false))

(defn sendReset "Sends a reset code to the email, and opens the new-password screen." []
  (let [email (trim state.forgot.email)]
    (if (not (isEmail email))
        (set "state.forgot.error" (t "emailOfAccount"))
        (do (set "state.forgot.open" false)
            (set "state.reset" {:email email :code "" :password "" :confirm "" :submitted false :error "" :left 600})
            (navigate "reset")
            (focus "#resetCode")
            (notify (t "linkSent" {:email email}))))))

(defn resetCodeChanged "The emailed code typed: no error, unless it expired." []
  (set "state.reset.error" (if (<= state.reset.left 0) "expired" "")))

(defn resetTick "Each second on the reset screen: the emailed code lasts 10 minutes." []
  (when (> state.reset.left 0)
    (increment "state.reset.left" :by -1)
    (when (<= state.reset.left 0) (set "state.reset.error" "expired"))))

(defn resendReset "A new code, for another 10 minutes." []
  (set "state.reset.left" 600)
  (set "state.reset.code" "")
  (set "state.reset.error" "")
  (focus "#resetCode")
  (notify (t "linkSent" {:email state.reset.email})))

(defn setPassword "Set password: once the code and the new password are right, the fake server saves it." []
  (set "state.reset.submitted" true)
  (set "state.reset.error" (if (< (count state.reset.code) 6) "short" ""))
  (when (and (= state.reset.error "") (= computed.resetErrors 0)) (serverCall "finishReset" "setPassword")))

(defn finishReset "The reset's answer: an expired or wrong code is refused, else the new password is saved." []
  (cond (<= state.reset.left 0)
        (refuse "state.reset.error" "expired" "#resendReset")
        (not= state.reset.code state.demoCode)
        (do (set "state.reset.code" "")
            (refuse "state.reset.error" "wrong" "#resetCode"))
        :else
        (passwordReset)))

(defn passwordReset "The new password, saved: the lock lifts, and the password step waits with the email." []
  (updateAccount state.reset.email {:password state.reset.password :passwordDays 0})
  (set "state.failures" (remove (fn [it] (= it (lower-case state.reset.email))) state.failures))
  (set "state.email" state.reset.email)
  (set "state.password" "")
  (set "state.submitted" false)
  (set "state.reset" {:email "" :code "" :password "" :confirm "" :submitted false :error "" :left 0})
  (set "state.step" "password")
  (navigate "signInForm")
  (focus "#textarea-placeholder")
  (notify (t "passwordChanged")))

;;; The session

(defn signOutHere "Sign out: ends the session on this device, and the provider's too." []
  (when state.user.provider (signOut "exampleId"))
  (endSession)
  (notify (t "signedOut")))

(defpart sign-out "Sign out, a command: its function is signOutHere, since signOut is the connection's own step."
  :actions {:signOut {:title ~(t "command" {:name "signOut"})
                      :category ~(t "category" {:name "account"})
                      :icon log-out
                      :enablement ~(= screen "signedIn")
                      :description ~(t "commandAbout" {:name "signOut"})
                      :do [{:lisp signOutHere}]}})

(defn endSession "Ends the session: the account's lists are written back, and the sign-in form opens." []
  (when (not= state.user.email "") (saveAccount))
  (set "state.confirmedLeft" 0)
  (set "state.reauth.open" false)
  (set "state.step" "email")
  (set "state.password" "")
  (set "state.confirm" "")
  (set "state.showPassword" false)
  (set "state.submitted" false)
  (set "state.change.open" false)
  (set "state.user" {:email "" :name "" :twoStep false :provider false})
  (navigate "signInForm"))

(defn arrived? "A verification link sent 5 seconds ago or more, not opened yet: the demo has no mail." [address]
  (and (:sent address) (not (:verified address)) (>= (- state.clock (:sentAt address)) 5)))

(defn tick "Each second on the account page: links open, the step-up and the session count down." []
  (increment "state.clock" :by 1)
  (when (some arrived? state.emails)
    (notify (t "emailVerified" {:email (:address (find arrived? state.emails))}))
    (set "state.emails" (map (fn [it] (if (arrived? it) (merge it {:verified true :sent false}) it)) state.emails))
    (logEvent "emailVerified"))
  (when (> state.confirmedLeft 0) (increment "state.confirmedLeft" :by -1))
  (increment "state.session.left" :by -1)
  (when (and (<= state.session.left 60) (not state.session.warned))
    (set "state.session.warned" true)
    (warnSession))
  (when (<= state.session.left 0) (expireSession)))

(defn warnSession "A notice that offers to stay signed in." []
  (set "state.toast" {:open true :message (t "sessionSoon") :action "stay"})
  (debounce 4000 hideToast))

(defn staySignedIn "Stay signed in: five more minutes." []
  (set "state.session" {:left 300 :warned false})
  (notify (t "sessionExtended")))

(defn expireSession "The session is over: signed out, with a notice on the sign-in form." []
  (when state.user.provider (signOut "exampleId"))
  (endSession)
  (hideToast)
  (set "state.notice" "expired")
  (focus "#signInAgain"))

(defn signInAgain "Sign in again, after the session expired: the email field." []
  (set "state.step" "email")
  (set "state.notice" "")
  (focus "#textarea-placeholder"))

;;; The demo's switches, as commands

(defn setLocale "Shows a language, by its code." [language] (set "state.locale" language))

(defcommand toggleTheme "Switches light and dark."
  :title ~(t "command" {:name "toggleTheme"}) :category ~(t "category" {:name "view"}) :icon moon
  :description ~(t "commandAbout" {:name "toggleTheme"})
  []
  (set "state.theme" (if (= state.theme "light") "dark" "light")))

(defcommand nextLanguage "English, then Italian, then Arabic."
  :title ~(t "command" {:name "nextLanguage"}) :category ~(t "category" {:name "view"}) :icon languages
  :description ~(t "commandAbout" {:name "nextLanguage"})
  []
  (set "state.locale" (get {"en" "it" "it" "ar"} state.locale "en")))

(defcommand togglePassword "Shows the passwords as text, or as dots."
  :title ~(t "command" {:name "togglePassword"}) :category ~(t "category" {:name "account"}) :icon eye
  :description ~(t "commandAbout" {:name "togglePassword"})
  []
  (set "state.showPassword" (not state.showPassword)))

(defcommand openShortcuts "Lists the commands and their keys."
  :title ~(t "command" {:name "openShortcuts"}) :category ~(t "category" {:name "help"}) :icon keyboard
  :description ~(t "commandAbout" {:name "openShortcuts"})
  []
  (set "state.shortcutsOpen" true))

(defn closeShortcuts "Closes the shortcuts' list." [] (set "state.shortcutsOpen" false))

(defcommand toggleSlow "The fake server answers after 800 ms, to see the busy states."
  :title ~(t "command" {:name "toggleSlow"}) :category ~(t "category" {:name "help"}) :icon wifi
  :description ~(t "commandAbout" {:name "toggleSlow"})
  []
  (set "state.slow" (not state.slow)))

;;; Keys

(defkey "escape" {:when ~(or (= screen "createAccount") (= screen "twoStep") (= screen "reset")) :do goBack})

(defkey "mod+j" toggleTheme)

(defkey "mod+l" nextLanguage)

(defkey "mod+shift+h" togglePassword)

(defkey "f1" openShortcuts)

(defkey "mod+shift+n" toggleSlow)

(defkey "mod+shift+q" signOut)
