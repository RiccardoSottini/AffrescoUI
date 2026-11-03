;;; sign-in-form.lisp --- the sign-in screen: the email, then the password, and the other ways in
;;;
;;; The form has two steps (state.step): the email first, then the password, with the account shown as a chip. A
;;; passkey, an emailed code, Example ID and three made-up providers sign in too. What the buttons run is in
;;; sign-in.lisp.

;;; The screen

(defview sign-in-card "The sign-in screen: its card, centred, scrolling when the window is short." []
  `(node#panel-sign-in :style screen
     (node#card :style card :width ~(min 420 (- window.width 32)) :maxHeight ~(- window.height 116)
       (node#card-top :height 36 :color transparent)
       ~(sign-in-form)
       (node#card-end :height 36 :color transparent))))

(defview sign-in-form "The form, top to bottom: the greeting, any error, the step's fields and the ways in." []
  `(node#stack-form :style form
     ~@(greeting)
     ~(session-notice)
     ~@(form-error)
     (node#label-email :when ~(= state.step "email") :style fieldLabel :text ~(t "email"))
     ~(email-field)
     ~(account-chip)
     ~@(password-step)
     (node#spacer-button :style spacer :height 2)
     ~(sign-in-button)
     ~(email-code-link)
     ~(or-line)
     ~@(other-ways-in)
     (node#toCreate :style link :text ~(t "toCreate") :onClick toCreate)))

(defview greeting "The logo, on windows tall enough for it, the welcome and what to sign in with." []
  [`(node#logo :style logo :when ~(>= window.height 700))
   `(node#spacer-logo :style spacer :height 4 :when ~(>= window.height 700))
   `(node#label-welcome-back :style title :text ~(t "welcomeBack") :role heading)
   `(node#label-sign-in-to :style caption :text ~(t "signInTo"))
   `(node#spacer-head :style spacer)])

(defview session-notice "The notice that the session expired, with a link to sign in again." []
  `(node#sessionNotice :when ~(= state.notice "expired")
     :anchors {:left 0 :right 10}
     :height 58
     :radius 8
     :color @color.notice
     :borderWidth 1
     :borderColor @color.noticeBorder
     :role alert
     :live assertive
     :label ~(t "sessionExpired")
     (label#label-session-expired :font small
       :icon clock-3
       :text ~(t "sessionExpired")
       :textColor @color.text
       :anchors {:left 12 :right 12 :top 8}
       :height 18)
     (node#signInAgain :style smallLink
       :text ~(t "signInAgain")
       :anchors {:left 12 :top 32}
       :height 18
       :onClick signInAgain)))

(defview form-error "Why the server refused, and, for a locked account, the way to unlock it." []
  [`(node#formError :when ~(not= state.formError "")
      :style alert
      :height ~(if (= state.formError "locked") 56 48)
      :text ~(t "serverError" {:kind state.formError :message state.providerMessage :provider computed.providerTitle}))
   `(node#unlock :when ~(= state.formError "locked")
      :style secondary
      :icon key-round
      :text ~(t "unlock")
      :onClick openForgot)])

;;; The two steps

(defview email-field "The first step's email field: Enter goes on to the password." []
  `(node#email :when ~(= state.step "email")
     :use checkedField
     :with {:submit submitSignIn
            :bind state.email
            :content ~state.email
            :key email
            :icon mail
            :fieldName ~(t "email")
            :placeholder ~(t "emailPlaceholder")
            :valid ~computed.emailValid
            :message ~(t "enterEmail")
            :submitted ~state.submitted
            :maxLength 254}))

(defview account-chip "The second step's account: its initial and email, and Not you? to go back." []
  `(row#accountChip :when ~(= state.step "password")
     :height 36
     :gap 10
     :alignItems center
     :anchors {:left 0 :right 10}
     :radius 18
     :color @color.input
     :borderWidth 1
     :borderColor @color.border
     :label ~(t "signingInAs" {:email (trim state.email)})
     (node#chip-avatar :width 32 :height 36 :color transparent
       (label#chip-initial :width 28
         :height 28
         :radius 14
         :align center
         :color @color.accent
         :textColor @color.accentText
         :font small
         :text ~(upper-case (slice (trim state.email) 0 1))
         :anchors {:left 4 :top 4}))
     (label#label-chip-email :font small :textColor @color.text :height 20 :fitText true :text ~(trim state.email))
     (row#spacer-chip :grow 1 :height 1)
     (node#changeEmail :style smallLink :height 20 :text ~(t "notYou") :onClick changeEmail)
     (row#spacer-chip-end :width 6 :height 1)))

(defview password-step "The second step: the password, Forgot password?, the Caps Lock warning and Remember me." []
  [`(row#row-password :when ~(= state.step "password") :height 18 :justify space-between :anchors {:left 0 :right 10}
      (node#label-password :style fieldLabel :anchors {} :fitText true :text ~(t "password"))
      (node#forgot :style smallLink :text ~(t "forgot") :onClick openForgot))
   (password-field)
   `(label#capsLockWarning :when ~(and (= state.step "password") window.capsLock)
      :font small
      :icon arrow-big-up
      :text ~(t "capsLock")
      :textColor @color.warning
      :height 18
      :anchors {:left 2 :right 10}
      :role status
      :live polite)
   (remember-me)])

(defview password-field "The password, as dots unless shown: Enter signs in." []
  `(node#password :when ~(= state.step "password")
     :use checkedField
     :with {:submit submitSignIn
            :bind state.password
            :content ~state.password
            :key password
            :icon lock
            :fieldName ~(t "password")
            :placeholder ~(t "yourPassword")
            :valid ~computed.passwordValid
            :message ~(t "min8")
            :submitted ~state.submitted
            :secret true
            :eye true
            :maxLength 128}))

(defview remember-me "Remember this device: a box and its words, both of which tick it." []
  `(node#row-remember :when ~(= state.step "password") :style checkRow
     (node#remember :style ~(if state.remember "checkOn" "checkOff")
       :label ~(t "remember")
       :onClick {:toggle state.remember})
     (label#label-remember :font small
       :text ~(t "rememberText")
       :fitText true
       :textColor @color.muted
       :cursor hand
       :onClick {:toggle state.remember})))

;;; The buttons

(defview sign-in-button "Continue on the first step, Sign in on the second; what is going on while busy." []
  `(node#signIn :style primary
     :icon ~(cond (or (= state.busy "signIn") (= state.busy "lookup")) "loader"
                  (= state.step "email") "arrow-right"
                  :else "log-in")
     :text ~(cond (or (= state.busy "signIn") (= state.busy "lookup")) computed.busyText
                  (= state.step "email") (t "continue")
                  :else (t "signInButton"))
     :disabled ~(not= state.busy "")
     :onClick submitSignIn))

(defview email-code-link "The second step's other way: a code sent to the email instead of the password." []
  `(node#emailCode :when ~(= state.step "password")
     :style link
     :font small
     :text ~(if (= state.busy "emailCode") (t "sending") (t "emailCode"))
     :disabled ~(not= state.busy "")
     :onClick emailCodeSignIn))

(defview or-line "The first step's line with or in it, over the other ways in." []
  `(row#row-or :when ~(= state.step "email") :height 18 :gap 10 :alignItems center :anchors {:left 0 :right 10}
     (node#line-or :grow 1 :height 1 :color @color.border)
     (label#label-or :font small :fitText true :height 18 :textColor @color.muted :text ~(t "or"))
     (node#line-or-2 :grow 1 :height 1 :color @color.border)))

(defview other-ways-in "A passkey, Example ID and the made-up providers, and a way to stop waiting for one." []
  [`(node#passkey :when ~(= state.step "email")
      :style secondary
      :icon ~(if (= state.busy "passkey") "loader" "fingerprint-pattern")
      :text ~(t "passkey")
      :disabled ~(not= state.busy "")
      :onClick passkeySignIn)
   `(node#provider :when ~(= state.step "email")
      :style secondary
      :icon ~(if (and (= state.busy "provider") (= state.providerName "exampleId")) "loader" "key-round")
      :text ~(if (and (= state.busy "provider") (= state.providerName "exampleId"))
                 (t "providerWaiting")
                 (t "provider"))
      :disabled ~(not= state.busy "")
      :onClick providerSignIn)
   `(row#row-providers :when ~(= state.step "email") :height 40 :gap 8 :anchors {:left 0 :right 10}
      ~(provider-button "wrenfield" "bird")
      ~(provider-button "mossbank" "leaf")
      ~(provider-button "quillo" "feather"))
   `(node#cancelProvider :when ~(= state.busy "provider")
      :style link
      :font small
      :text ~(t "cancelProvider")
      :onClick cancelProvider)])

(defview provider-button "A made-up provider's button, a spinner while it is waited for." [provider icon]
  `(node :id ~(str "provider-" provider)
     :style secondary
     :grow 1
     :height 40
     :anchors {}
     :font small
     :icon ~(if (and (= state.busy "provider") (= state.providerName provider)) "loader" icon)
     :text ~(t (str "provider_" provider))
     :description ~(t "otherProviders")
     :disabled ~(not= state.busy "")
     :onClick ~(handler otherProvider provider)))
