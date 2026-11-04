;;; dialogs.lisp --- what opens over the screens: forgot password, the passkey picker, two-step setup, change and
;;; confirm password, delete the account, and the keyboard shortcuts
;;;
;;; Each is the built-in dialog component (`:use dialog`), its buttons running functions of sign-in.lisp and
;;; profile.lisp; its fields sit under its title and message, from y 76.

;;; Signing in

(defview forgot-dialog "Forgot password: the account's email, to send it a reset code." []
  `(node#forgotDialog :dir auto
     :use dialog
     :with {:open state.forgot.open
            :title ~(t "resetTitle")
            :message ~(t "resetMessage")
            :width ~(min 420 (- window.width 32))
            :height ~(if (= state.forgot.error "") 196 218)
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "sendLink") :run sendReset} {:title ~(t "cancel") :run closeForgot}]}
     (node#forgotEmail :style field
       :anchors {:left 20 :right 20 :top 76}
       :height 38
       :borderColor ~(if (= state.forgot.error "") "@color.border" "@color.error")
       :placeholder ~(t "emailPlaceholder")
       :label ~(t "email")
       :description ~state.forgot.error
       :maxLength 254
       :bind state.forgot.email
       :onChange {:set state.forgot.error :to ""}
       :onSubmit sendReset)
     (node#forgotError :when ~(not= state.forgot.error "")
       :style inlineError
       :text ~state.forgot.error
       :anchors {:left 22 :right 20 :top 120})))

(defview passkey-dialog "The device's passkey picker, faked: one button for each account here that saved a passkey." []
  `(node#passkeyDialog :dir auto
     :use dialog
     :with {:open state.passkeyOpen
            :title ~(t "passkeyTitle")
            :message ~(t "passkeyMessage")
            :width ~(min 420 (- window.width 32))
            :height ~(+ 150 (* 46 (count computed.passkeys)))
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "cancel") :run cancelPasskey}]}
     ~(repeat [pk computed.passkeys :index i :key pk.email]
        (passkey-choice pk i))))

(defview passkey-choice "An account of the passkey picker, `i` its place." [pk i]
  `(node#passkey-choice :style secondary
     :height 40
     :anchors {:left 20 :right 20 :top ~(+ 76 (* 46 i))}
     :icon fingerprint-pattern
     :text ~(str pk.name " · " pk.email)
     :onClick ~(handler choosePasskey pk.email)))

;;; Two-step setup

(defview setup-dialog "Two-step setup: scan the QR code and type a code, then save the backup codes." []
  `(node#setupDialog :dir auto
     :use dialog
     :with {:open state.setup.open
            :title ~(t "setupTitle")
            :message ~(t (if (= state.setup.step "scan") "setupScanMessage" "setupCodesMessage"))
            :width ~(min 440 (- window.width 32))
            :height ~(+ (if (= state.setup.step "scan") 380 312) (if (= state.setup.error "") 0 24))
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t (if (= state.setup.step "scan") "next" "done")) :run setupNext}
                      {:title ~(t (if (and (= state.setup.step "codes") (not state.user.twoStep)) "back" "cancel"))
                       :run setupCancel}]}
     ~@(scan-step)
     ~@(backup-codes-step)
     (node#setupError :when ~(not= state.setup.error "")
       :style inlineError
       :text ~(t "setupError" {:kind state.setup.error})
       :anchors {:left 22 :right 20 :top ~(if (= state.setup.step "scan") 314 262)})))

(defview scan-step "The setup's first step: the QR code, the key to type instead, and the code field." []
  [(qr-code)
   `(node#label-key :when ~(= state.setup.step "scan")
      :style fieldLabel
      :text ~(t "orEnterKey")
      :anchors {:left 176 :right 20 :top 90})
   `(label#setupKey :when ~(= state.setup.step "scan")
      :font mono
      :dir ltr
      :height 22
      :text "EXAM PLEE XAMP LE23"
      :textColor @color.text
      :selectable true
      :anchors {:left 176 :right 20 :top 114})
   `(node#copyKey :when ~(= state.setup.step "scan")
      :style smallLink
      :icon copy
      :text ~(t "copyKey")
      :anchors {:left 176 :top 144}
      :onClick copySetupKey)
   `(node#demoSetup :when ~(= state.setup.step "scan")
      :style hint
      :icon info
      :wrap true
      :height 36
      :text ~(t "demoSetupCode" {:code state.demoCode})
      :anchors {:left 176 :right 20 :top 176})
   `(node#label-setup-code :when ~(= state.setup.step "scan")
      :style fieldLabel
      :text ~(t "code")
      :anchors {:left 20 :right 20 :top 232})
   (setup-code)])

(defview qr-code "The QR code an authenticator app scans, dark on white in either theme." []
  `(panel#qrBox :when ~(= state.setup.step "scan")
     :width 140
     :height 140
     :radius 8
     :color @color.qrBack
     :anchors {:left 20 :top 76}
     :role img
     :label ~(t "setupScanMessage")
     (path#qr :d ~computed.otpQr.d
       :viewBox ~computed.otpQr.viewBox
       :fillColor @color.qrInk
       :anchors {:left 4 :right 4 :top 4 :bottom 4})))

(defview setup-code "The setup's code field: six digits from the authenticator app." []
  `(node#setupCode :when ~(= state.setup.step "scan")
     :style code
     :tabIndex 1
     :dir ltr
     :bind state.setup.code
     :label ~(t "code")
     :description ~(if (= state.setup.error "") "" (t "setupError" {:kind state.setup.error}))
     :cellBorderColor ~(if (= state.setup.error "") "@color.border" "@color.error")
     :anchors {:left 20 :right 20 :top 254}
     :onChange setupCodeChanged
     :onSubmit setupNext))

(defview backup-codes-step "The setup's second step: the backup codes, a box to tick once saved, and Copy."
  []
  [(repeat [backup state.backupCodes :index i :key backup]
     (backup-code backup i))
   `(node#row-saved :when ~(= state.setup.step "codes") :style checkRow :anchors {:left 20 :right 20 :top 204}
      (node#savedCodes :style ~(if state.setup.saved "checkOn" "checkOff")
        :label ~(t "savedCodes")
        :checked ~state.setup.saved
        :onClick toggleSaved
        :keys {:space toggleSaved})
      (label#label-saved :font small
        :text ~(t "savedCodes")
        :fitText true
        :textColor @color.muted
        :cursor hand
        :onClick toggleSaved))
   `(node#copyCodes :when ~(= state.setup.step "codes")
      :style smallLink
      :icon copy
      :text ~(t "copyCodes")
      :anchors {:left 20 :top 234}
      :onClick copyBackupCodes)])

(defview backup-code "A backup code, in two columns of four, `i` its place." [backup i]
  `(label#backup-code :when ~(= state.setup.step "codes")
     :font mono
     :dir ltr
     :height 24
     :width 150
     :align center
     :radius 6
     :color @color.input
     :textColor @color.text
     :text ~backup
     :anchors {:left ~(if (= (mod i 2) 0) 40 210) :top ~(+ 76 (* 30 (floor (/ i 2))))}))

;;; The account's password, and deleting it

(defview change-dialog "Change password: the new one, its confirmation and its strength." []
  `(node#changeDialog :dir auto
     :use dialog
     :with {:open state.change.open
            :title ~(t "changePassword")
            :message ~(t "changePasswordMessage" {:email state.user.email})
            :width ~(min 420 (- window.width 32))
            :height ~(+ (if state.showPassword 222 268)
                        (if (and state.change.submitted (not= computed.changeError "")) 24 0))
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "change") :run changePassword} {:title ~(t "cancel") :run closeChange}]}
     ~@(new-password-fields)
     (node#change-meter :color transparent
       :use passwordMeter
       :anchors {:left 20 :top ~(if state.showPassword 124 170)}
       :with {:password ~state.change.password
              :width ~(- (min 420 (- window.width 32)) 40)
              :labels ~(meter-words)
              :track @color.border
              :textColor @color.muted
              :colors [@color.weak @color.weak @color.fair @color.good @color.strong]})
     (node#changeError :when ~(and state.change.submitted (not= computed.changeError ""))
       :style inlineError
       :text ~computed.changeError
       :anchors {:left 22 :right 20 :top ~(if state.showPassword 162 208)})))

(defview new-password-fields "The change dialog's two fields: the new password, and again unless it is shown." []
  [`(node#newPassword :style field
      :anchors {:left 20 :right 20 :top 76}
      :height 38
      :secret ~(not state.showPassword)
      :maxLength 128
      :placeholder ~(t "newPassword")
      :label ~(t "newPassword")
      :description ~(if state.change.submitted computed.changeError "")
      :bind state.change.password
      :keys {"mod+shift+h" togglePassword}
      :onSubmit changePassword)
   `(node#newPasswordConfirm :when ~(not state.showPassword)
      :style field
      :anchors {:left 20 :right 20 :top 122}
      :height 38
      :secret true
      :maxLength 128
      :placeholder ~(t "confirm")
      :label ~(t "confirm")
      :bind state.change.confirm
      :onSubmit changePassword)])

(defview reauth-dialog "The password asked again before a sensitive change." []
  `(node#reauthDialog :dir auto
     :use dialog
     :with {:open state.reauth.open
            :title ~(t "reauthTitle")
            :message ~(t "reauthMessage" {:email state.user.email})
            :width ~(min 420 (- window.width 32))
            :height ~(if (= state.reauth.error "") 196 218)
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "confirmIt") :run confirmReauth} {:title ~(t "cancel") :run closeReauth}]}
     (node#reauthPassword :style field
       :anchors {:left 20 :right 20 :top 76}
       :height 38
       :secret true
       :maxLength 128
       :borderColor ~(if (= state.reauth.error "") "@color.border" "@color.error")
       :placeholder ~(t "passwordPlaceholder")
       :label ~(t "password")
       :description ~(if (= state.reauth.error "") "" (t "reauthError" {:kind state.reauth.error}))
       :bind state.reauth.password
       :onChange {:set state.reauth.error :to ""}
       :onSubmit confirmReauth)
     (node#reauthError :when ~(not= state.reauth.error "")
       :style inlineError
       :text ~(t "reauthError" {:kind state.reauth.error})
       :anchors {:left 22 :right 20 :top 120})))

(defview delete-dialog "Delete the account: the word to type, in the language shown, before it goes for good." []
  `(node#deleteDialog :dir auto
     :use dialog
     :with {:open state.deleting.open
            :title ~(t "deleteDialogTitle" {:email state.user.email})
            :message ~(t "deleteDialogMessage" {:word computed.deleteWord})
            :width ~(min 420 (- window.width 32))
            :height ~(if (= state.deleting.error "") 196 218)
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.error
            :backdrop @color.backdrop
            :buttons [{:title ~(t "deleteForever") :run confirmDelete} {:title ~(t "cancel") :run closeDelete}]}
     (node#deleteConfirm :style field
       :anchors {:left 20 :right 20 :top 76}
       :height 38
       :maxLength 32
       :borderColor ~(if (= state.deleting.error "") "@color.border" "@color.error")
       :placeholder ~(t "deleteField" {:word computed.deleteWord})
       :label ~(t "deleteField" {:word computed.deleteWord})
       :description ~(if (= state.deleting.error "")
                         ""
                         (t "deleteError" {:kind state.deleting.error :word computed.deleteWord}))
       :bind state.deleting.typed
       :onChange {:set state.deleting.error :to ""}
       :onSubmit confirmDelete)
     (node#deleteError :when ~(not= state.deleting.error "")
       :style inlineError
       :text ~(t "deleteError" {:kind state.deleting.error :word computed.deleteWord})
       :anchors {:left 22 :right 20 :top 120})))

;;; Keyboard shortcuts

(defview shortcuts-dialog "Every command, with its keys spelled out." []
  `(node#shortcutsDialog :dir auto
     :use dialog
     :with {:open state.shortcutsOpen
            :title ~(t "shortcuts")
            :message ~(t "shortcutsMessage")
            :width ~(min 420 (- window.width 32))
            :height ~(+ 132 (* (count commands) 28))
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :backdrop @color.backdrop
            :buttons [{:title ~(t "close") :run closeShortcuts}]}
     (column#shortcutList :color transparent
       :role list
       :label ~(t "shortcuts")
       :anchors {:left 20 :right 20 :top 72}
       :height ~(* (count commands) 28)
       ~(repeat [command computed.shortcuts]
          (shortcut command)))))

(defview shortcut "A command of the list: its icon and name, greyed while it can't run, and its keys." [command]
  `(node#shortcut :role listitem
     :label ~(str (t "command" {:name command.name}) ", " command.keyText)
     :anchors {:left 0 :right 0}
     :height 28
     :color transparent
     (label#shortcut-title :font small
       :icon ~command.icon
       :text ~(t "command" {:name command.name})
       :textColor ~(if command.enabled "@color.text" "@color.muted")
       :align {:horizontal start :vertical center}
       :anchors {:left 0 :right 90 :top 4}
       :height 20)
     (label#shortcut-key :font small
       :dir ltr
       :text ~command.keyText
       :textColor @color.muted
       :align {:horizontal end :vertical center}
       :anchors {:right 0 :top 4}
       :width 86
       :height 20)))
