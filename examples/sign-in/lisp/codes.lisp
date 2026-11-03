;;; codes.lisp --- the screens that ask for a code: two-step (and an emailed code, a text or a backup code), and a
;;; new password after a reset
;;;
;;; Each screen counts down each second: the two-step one until a new code can be sent, the reset one until its code
;;; expires. What the buttons run is in sign-in.lisp.

;;; Two-step

(defview two-step-card "The code screen: its card, centred, the resend countdown ticking." []
  `(node#panel-two-step :style screen :every {:ms 1000 :do countDown}
     (node#card-two-step :style card :width ~(min 420 (- window.width 32)) :maxHeight ~(- window.height 116)
       (node#card-two-step-top :height 36 :color transparent)
       ~(two-step-form)
       (node#card-two-step-end :height 36 :color transparent))))

(defmacro by-code-mode "A choice by which code is asked for: pairs of a codeMode and its choice, then the default."
  [pairs default]
  `(cond ~@(apply concat (map (fn [[mode choice]] [(list '= 'state.codeMode mode) choice]) pairs)) :else ~default))

(defview two-step-form "The code form: what is asked and where, the code, Remember this device and the buttons." []
  `(node#stack-two-step :style form
     (node#label-two-step :style title
       :text ~(t (by-code-mode [["verify" "verifyEmailTitle"] ["email" "emailCodeTitle"] ["sms" "smsTitle"]
                                ["backup" "backupTitle"]]
                               "twoStepTitle"))
       :role heading)
     (node#label-two-step-caption :style caption
       :wrap true
       :height 40
       :text ~(t (by-code-mode [["verify" "verifyEmailCaption"] ["email" "emailCodeCaption"] ["sms" "smsCaption"]
                                ["backup" "backupCaption"]]
                               "twoStepCaption")
                 {:email state.pending.email :phone state.phone}))
     (node#label-code :style fieldLabel :text ~(t (if (= state.codeMode "backup") "backupCode" "code")))
     ~@(code-fields)
     (node#codeError :when ~(not= state.codeError "") :style inlineError :text ~(t "codeError" {:kind state.codeError}))
     (node#demoCode :style hint
       :icon info
       :text ~(if (= state.codeMode "backup")
                  (t "demoBackup" {:code (or-else (get state.backupCodes 0) "—")})
                  (t "demoCode" {:code state.demoCode})))
     ~(trust-row)
     ~@(two-step-buttons)
     ~(other-ways)
     (node#twoStepBack :style link
       :text ~(if (= state.codeMode "verify") (t "backToCreate") (t "backToSignIn"))
       :onClick codeBack)))

(defview code-fields "Six digits for a code, or a text field for a backup code." []
  [`(node#twoStepCode :when ~(not= state.codeMode "backup")
      :style code
      :dir ltr
      :bind state.code
      :label ~(t "code")
      :description ~(if (= state.codeError "") "" (t "codeError" {:kind state.codeError}))
      :cellBorderColor ~(if (= state.codeError "") "@color.border" "@color.error")
      :onChange codeChanged
      :onSubmit verify)
   `(node#backupField :when ~(= state.codeMode "backup")
      :style field
      :dir ltr
      :height 38
      :anchors {:left 0 :right 10}
      :borderColor ~(if (= state.codeError "") "@color.border" "@color.error")
      :placeholder "DEMO-1001"
      :label ~(t "backupCode")
      :description ~(if (= state.codeError "") "" (t "codeError" {:kind state.codeError}))
      :maxLength 9
      :bind state.backupCode
      :onChange {:set state.codeError :to ""}
      :onSubmit verify)])

(defview trust-row "Remember this device, for two-step only: a box and its words, both of which tick it." []
  `(node#row-trust :when ~(and (not= state.codeMode "verify") (not= state.codeMode "email")) :style checkRow
     (node#trust :style ~(if state.trust "checkOn" "checkOff")
       :label ~(t "trustDevice")
       :checked ~state.trust
       :onClick {:toggle state.trust}
       :keys {:space {:toggle state.trust}})
     (label#label-trust :font small
       :text ~(t "trustDevice")
       :fitText true
       :textColor @color.muted
       :cursor hand
       :onClick {:toggle state.trust})))

(defview two-step-buttons "Verify, and Resend, which counts down before it can send again." []
  [`(node#verify :style primary
      :icon ~(if (= state.busy "verify") "loader" "shield-check")
      :text ~(if (= state.busy "verify") computed.busyText (t "verify"))
      :disabled ~(not= state.busy "")
      :onClick verify)
   `(node#resend :when ~(not= state.codeMode "backup")
      :style link
      :text ~(cond (= state.busy "resend") computed.busyText
                   (> state.resendLeft 0) (t "resendIn" {:n state.resendLeft})
                   :else (t "resend"))
      :textColor ~(if (> state.resendLeft 0) "@color.muted" "@color.accent")
      :disabled ~(or (> state.resendLeft 0) (not= state.busy ""))
      :onClick resend)])

(defview other-ways "Two-step's other ways: the app, a text and a backup code, less the one in use." []
  `(row#row-other-ways :when ~(and (not= state.codeMode "verify") (not= state.codeMode "email"))
     :height 20
     :gap 16
     :justify center
     :alignItems center
     :anchors {:left 0 :right 10}
     ~(other-way "useApp" "twoStep")
     ~(other-way "textMe" "sms")
     ~(other-way "useBackup" "backup")))

(defview other-way "A link to another way of two-step, by its id (also its text's key) and its code mode." [id mode]
  `(node :id ~id :when ~(not= state.codeMode mode) :style smallLink :text ~(t id) :onClick ~(handler useMode mode)))

;;; A new password

(defview reset-card "The new-password screen: its card, centred, the code's expiry ticking." []
  `(node#panel-reset :style screen :every {:ms 1000 :do resetTick}
     (node#card-reset :style card :width ~(min 420 (- window.width 32)) :maxHeight ~(- window.height 116)
       (node#card-reset-top :height 36 :color transparent)
       ~(reset-form)
       (node#card-reset-end :height 36 :color transparent))))

(defview reset-form "The reset form: the emailed code and how long it lasts, then the new password twice." []
  `(node#stack-reset :style form
     (node#label-reset-title :style title :text ~(t "newPasswordTitle") :role heading)
     (node#label-reset-caption :style caption
       :wrap true
       :height 40
       :text ~(t "newPasswordCaption" {:email state.reset.email}))
     (node#label-reset-code :style fieldLabel :text ~(t "code"))
     ~(reset-code)
     ~@(reset-code-state)
     ~@(reset-password)
     (node#setPassword :style primary
       :icon ~(if (= state.busy "setPassword") "loader" "key-round")
       :text ~(if (= state.busy "setPassword") computed.busyText (t "setPassword"))
       :disabled ~(not= state.busy "")
       :onClick setPassword)
     (node#resetBack :style link :text ~(t "backToSignIn") :onClick goBack)))

(defview reset-code "The emailed code: six digits; Enter goes on to the new password." []
  `(node#resetCode :style code
     :dir ltr
     :bind state.reset.code
     :label ~(t "code")
     :description ~(if (= state.reset.error "") "" (t "codeError" {:kind state.reset.error}))
     :cellBorderColor ~(if (= state.reset.error "") "@color.border" "@color.error")
     :onChange resetCodeChanged
     :onSubmit {:focus "#textarea-placeholder"}))

(defview reset-code-state "The code's error, Resend once it expired, else how long it lasts, and the demo's code." []
  [`(node#resetCodeError :when ~(not= state.reset.error "")
      :style inlineError
      :text ~(t "codeError" {:kind state.reset.error}))
   `(node#resendReset :when ~(<= state.reset.left 0)
      :style smallLink
      :icon refresh-cw
      :text ~(t "resend")
      :onClick resendReset
      :height 18)
   `(node#resetExpiry :when ~(> state.reset.left 0)
      :style hint
      :icon timer
      :text ~(t "codeExpires" {:time computed.resetTime}))
   `(node#demoCode-reset :style hint :icon info :text ~(t "demoCode" {:code state.demoCode}))])

(defview reset-password "The new password, its strength, and its confirmation unless it is shown." []
  [`(node#label-new-password :style fieldLabel :text ~(t "newPassword"))
   `(node#reset-password :use checkedField
      :with {:submit setPassword
             :bind state.reset.password
             :content ~state.reset.password
             :key resetPassword
             :icon lock
             :fieldName ~(t "newPassword")
             :placeholder ~(t "passwordPlaceholder")
             :valid ~computed.resetValid
             :message ~(if (>= (count state.reset.password) 8) (t "stronger") (t "min8"))
             :submitted ~state.reset.submitted
             :secret true
             :eye true
             :maxLength 128})
   `(node#strength-reset :use strengthMeter :with {:password ~state.reset.password})
   `(node#label-reset-confirm :when ~(not state.showPassword) :style fieldLabel :text ~(t "confirm"))
   `(node#reset-confirm :when ~(not state.showPassword)
      :use checkedField
      :with {:submit setPassword
             :bind state.reset.confirm
             :content ~state.reset.confirm
             :key resetConfirm
             :icon lock-keyhole
             :fieldName ~(t "confirm")
             :placeholder ~(t "confirmPlaceholder")
             :valid ~computed.resetConfirmValid
             :message ~(t "mismatch")
             :submitted ~state.reset.submitted
             :secret true
             :maxLength 128})])
