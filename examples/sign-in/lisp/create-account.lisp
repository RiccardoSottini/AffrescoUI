;;; create-account.lisp --- the create-account screen: name, email, username, password, terms and an invite
;;;
;;; Every field checks itself once the form is sent; the username asks the demo server whether it is free as it is
;;; typed. What the buttons run is in sign-in.lisp.

;;; The screen

(defview create-card "The create-account screen: its card, centred, scrolling when the window is short." []
  `(node#panel-create-account :style screen
     (node#panel-card :style card :width ~(min 420 (- window.width 32)) :maxHeight ~(- window.height 116)
       (node#panel-card-top :height 36 :color transparent)
       ~(create-form)
       (node#panel-card-end :height 36 :color transparent))))

(defview create-form "The form, top to bottom: the title, the fields, the terms, an invite and the buttons." []
  `(node#stack-form-2 :style form
     (node#label-create-an-account :style title :text ~(t "createAnAccount") :role heading)
     (node#label-create-caption :style caption :text ~(t "createCaption"))
     ~@(name-and-email)
     (node#label-username :style fieldLabel :text ~(t "username"))
     ~(username-field)
     ~(username-status)
     ~@(new-password)
     ~(terms-row)
     (node#termsError :when ~(and state.submitted (not state.terms)) :style inlineError :text ~(t "termsError"))
     ~@(invite)
     (node#label-create-account :style primary
       :icon ~(if (= state.busy "create") "loader" "user-plus")
       :text ~(if (= state.busy "create") computed.busyText (t "createAccount"))
       :disabled ~(not= state.busy "")
       :onClick submitNewAccount)
     (node#label-back-to-sign :style link :text ~(t "backToSignIn") :onClick goBack)))

;;; The fields

(defview name-and-email "The new account's name and email, each with its label." []
  [`(node#label-name :style fieldLabel :text ~(t "name"))
   `(node#checked-field-form :use checkedField
      :with {:submit submitNewAccount
             :bind state.name
             :content ~state.name
             :key name
             :icon user
             :fieldName ~(t "name")
             :placeholder ~(t "namePlaceholder")
             :valid ~(> (count (trim state.name)) 0)
             :message ~(t "enterName")
             :submitted ~state.submitted
             :maxLength 60})
   `(node#label-email-2 :style fieldLabel :text ~(t "email"))
   `(node#checked-field-form-2 :use checkedField
      :with {:submit submitNewAccount
             :bind state.email
             :content ~state.email
             :key email
             :icon mail
             :fieldName ~(t "email")
             :placeholder ~(t "emailPlaceholder")
             :valid ~computed.emailValid
             :message ~(t "enterEmail")
             :submitted ~state.submitted
             :maxLength 254})])

(defview username-field "The username: each change asks the demo server, once typing stops." []
  `(node#username :use checkedField
     :with {:submit submitNewAccount
            :change usernameChanged
            :bind state.username
            :content ~state.username
            :key username
            :icon at-sign
            :fieldName ~(t "username")
            :placeholder ~(t "usernamePlaceholder")
            :valid ~(= computed.usernameStatus "available")
            :message ~(t "usernameError" {:kind computed.usernameStatus})
            :submitted ~state.submitted
            :maxLength 20}))

(defmacro username-colour "The username status's colour: muted while checking, green when free, else the error's." []
  `(cond (= computed.usernameStatus "checking") "@color.muted"
         (= computed.usernameStatus "available") "@color.strong"
         :else "@color.error"))

(defview username-status "Under the username: a spinner while it is checked, then whether it is free." []
  `(row#usernameStatus :when ~(and (not= computed.usernameStatus "")
                                   (not (and state.submitted (not= computed.usernameStatus "available"))))
     :height 18
     :gap 6
     :alignItems center
     :anchors {:left 2 :right 10}
     :color transparent
     :role status
     :live polite
     :label ~(t "usernameStatus" {:kind computed.usernameStatus :name (lower-case (trim state.username))})
     (label#username-status-icon :font iconsSmall
       :text ""
       :width 14
       :height 14
       :align center
       :color transparent
       :icon ~(cond (= computed.usernameStatus "checking") "loader"
                    (= computed.usernameStatus "available") "check-circle"
                    :else "alert-circle")
       :rotate ~(if (= computed.usernameStatus "checking") state.spin 0)
       :textColor ~(username-colour))
     (label#username-status-text :font small
       :fitText true
       :height 18
       :color transparent
       :text ~(t "usernameStatus" {:kind computed.usernameStatus :name (lower-case (trim state.username))})
       :textColor ~(username-colour))))

(defview new-password "The password, its strength and rules, and its confirmation unless it is shown." []
  [`(node#label-password-2 :style fieldLabel :text ~(t "password"))
   `(node#checked-field-form-3 :use checkedField
      :with {:submit submitNewAccount
             :bind state.password
             :content ~state.password
             :key password
             :icon lock
             :fieldName ~(t "password")
             :placeholder ~(t "passwordPlaceholder")
             :valid ~computed.newPasswordValid
             :message ~computed.newPasswordMessage
             :submitted ~state.submitted
             :secret true
             :eye true
             :maxLength 128})
   `(node#strength-create :use strengthMeter :with {:password ~state.password})
   `(node#rules-create :use passwordRules :with {:password ~state.password})
   `(node#label-confirm :when ~(not state.showPassword) :style fieldLabel :text ~(t "confirm"))
   `(node#confirm-password :when ~(not state.showPassword)
      :use checkedField
      :with {:submit submitNewAccount
             :bind state.confirm
             :content ~state.confirm
             :key confirm
             :icon lock-keyhole
             :fieldName ~(t "confirm")
             :placeholder ~(t "confirmPlaceholder")
             :valid ~computed.confirmValid
             :message ~(t "mismatch")
             :submitted ~state.submitted
             :secret true
             :maxLength 128})])

(defview terms-row "The terms: a box and its words, both of which tick it." []
  `(node#row-terms :style checkRow
     (node#terms :style ~(if state.terms "checkOn" "checkOff")
       :label ~(t "terms")
       :description ~(if (and state.submitted (not state.terms)) (t "termsError") "")
       :onClick toggleTerms)
     (label#label-terms :font small :text ~(t "terms") :fitText true :textColor @color.text :cursor hand
       :onClick toggleTerms)))

(defview invite "Have an invite?, then its code, and a notice once the code is a team's." []
  [`(node#haveInvite :when ~(not state.invite.open) :style smallLink :text ~(t "haveInvite") :onClick openInvite)
   `(node#label-invite :when ~state.invite.open :style fieldLabel :text ~(t "inviteCode"))
   `(node#invite :when ~state.invite.open
      :use checkedField
      :with {:submit submitNewAccount
             :bind state.invite.code
             :content ~state.invite.code
             :key invite
             :icon ticket
             :fieldName ~(t "inviteCode")
             :placeholder ~(t "invitePlaceholder")
             :valid ~(or (= (count (trim state.invite.code)) 0) computed.inviteValid)
             :message ~(t "inviteUnknown")
             :submitted ~state.submitted
             :maxLength 40})
   `(label#inviteNotice :when ~(and state.invite.open computed.inviteValid)
      :font small
      :icon users
      :height 36
      :padding 10
      :anchors {:left 0 :right 10}
      :radius 8
      :color @color.notice
      :borderWidth 1
      :borderColor @color.noticeBorder
      :textColor @color.text
      :role status
      :live polite
      :text ~(t "inviteValid"))])
