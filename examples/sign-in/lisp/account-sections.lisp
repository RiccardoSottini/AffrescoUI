;;; account-sections.lisp --- the account page's sections after the profile: emails, security, devices, connected
;;; accounts, activity and data
;;;
;;; Each is an `account-section` (account.lisp): a column shown while it is the one chosen, under its heading. A
;;; list's rows are itemRows (components.lisp), their links and buttons the row's own children.

;;; Emails

(defview emails-section "The account's emails, at most five, and a form to add one." []
  (account-section "emails"
    (repeat [address state.emails :key address.address]
      (email-row address))
    `(node#addEmail :when ~(and (not state.newEmail.open) (< (count state.emails) 5))
       :style smallButton
       :icon plus
       :text ~(t "addEmail")
       :onClick openAddEmail)
    (new-email-form)))

(defview email-row "An email: whether it is primary or verified, and what can be done with it." [address]
  `(node#emailRow :use itemRow
     :with {:icon ~(if address.verified "mail-check" "mail-warning")
            :title ~address.address
            :hint ~(cond address.primary (t "emailsHint")
                         address.verified (t "verified")
                         address.sent (t "linkSentHint")
                         :else (t "unverified"))}
     (node#badge-primary :when ~address.primary :style badge :text ~(t "primary"))
     (node#badge-unverified :when ~(not address.verified)
       :style badgeWarn
       :text ~(if address.sent (t "linkSentBadge") (t "unverified")))
     (node#verifyEmail :when ~(and (not address.verified) (not address.sent))
       :style smallLink
       :height 20
       :text ~(t "verifyEmail")
       :label ~(str (t "verifyEmail") " " address.address)
       :onClick ~(handler verifyEmail address.address))
     (node#makePrimary :when ~(and address.verified (not address.primary))
       :style smallLink
       :height 20
       :text ~(t "makePrimary")
       :label ~(str (t "makePrimary") ": " address.address)
       :onClick ~(handler makePrimary address.address))
     (node#removeEmail :when ~(not address.primary)
       :style smallLink
       :height 20
       :text ~(t "remove")
       :label ~(str (t "remove") " " address.address)
       :onClick ~(handler removeEmail address.address))))

(defview new-email-form "The email being added, with Add and Cancel." []
  `(column#newEmailForm :when ~state.newEmail.open :fitContent height :gap 8 :anchors {:left 0 :right 0}
     (node#label-new-email :style fieldLabel :text ~(t "newEmail"))
     (node#newEmailField :use checkedField
       :with {:submit addEmail
              :bind state.newEmail.address
              :content ~state.newEmail.address
              :key newEmail
              :icon mail-plus
              :fieldName ~(t "newEmail")
              :placeholder ~(t "emailPlaceholder")
              :valid ~(= computed.newEmailError "")
              :message ~(t "emailError" {:kind computed.newEmailError})
              :submitted ~state.newEmail.submitted
              :maxLength 254})
     (row#row-new-email :height 34 :gap 8 :anchors {:left 0 :right 10}
       (node#addEmailButton :style primary
         :anchors {}
         :height 34
         :font small
         :padding 12
         :fitText true
         :shadow {:color "#00000000"}
         :icon ~(if (= state.busy "verifyEmail") "loader" "plus")
         :text ~(if (= state.busy "verifyEmail") computed.busyText (t "add"))
         :disabled ~(not= state.busy "")
         :onClick addEmail)
       (node#cancelEmail :style smallButton :text ~(t "cancel") :onClick closeAddEmail))))

;;; Security

(defview security-section "The password, two-step and its trusted devices, and the passkeys." []
  (account-section "security"
    `(node#passwordRow :use itemRow
       :with {:icon key-round
              :title ~(t "password")
              :hint ~(if state.user.provider (t "providerPassword") (t "passwordAge" {:n computed.passwordDays}))}
       (node#changePassword :when ~(not state.user.provider)
         :style smallButton
         :anchors {}
         :icon key-round
         :text ~(t "changePassword")
         :isOpen ~state.change.open
         :onClick openChange))
    (two-step-box)
    `(node#label-passkeys :style fieldLabel :text ~(t "passkeys") :role heading)
    `(node#passkeysCaption :style hint :text ~(t "passkeysCaption"))
    (repeat [passkey state.myPasskeys :key passkey.id]
      (passkey-row passkey))
    `(node#noPasskeys :when ~(= (count state.myPasskeys) 0)
       :style hint
       :icon fingerprint-pattern
       :text ~(t "noPasskeys"))
    `(node#addPasskey :when ~(< (count state.myPasskeys) 5)
       :style smallButton
       :icon ~(if (= state.busy "passkey") "loader" "plus")
       :text ~(if (= state.busy "passkey") computed.busyText (t "addPasskey"))
       :disabled ~(not= state.busy "")
       :onClick addPasskey)))

(defview two-step-box "Two-step's switch and, while it is on, the backup codes left and the devices it remembers." []
  `(column#twoStepBox :fitContent height
     :gap 8
     :radius @radius.field
     :color @color.input
     :borderWidth 1
     :borderColor @color.border
     :anchors {:left 0 :right 10}
     (node#twoStepBox-top :height 4 :color transparent)
     ~(two-step-switch)
     (row#row-backup :when ~state.user.twoStep :height 18 :gap 12 :alignItems center :anchors {:left 12 :right 12}
       (node#backupLeft :style hint :icon key-round :grow 1 :anchors {}
         :text ~(t "backupLeft" {:n (count state.backupCodes)}))
       (node#newCodes :style smallLink :text ~(t "newCodes") :onClick newCodes))
     (node#label-trusted :when ~state.user.twoStep
       :style fieldLabel
       :text ~(t "trustedDevices")
       :anchors {:left 12 :right 12})
     ~(repeat [device computed.myTrusted :index i :key device.device]
        (trusted-device device))
     (node#noTrusted :when ~(and state.user.twoStep (= (count computed.myTrusted) 0))
       :style hint
       :text ~(t "noTrusted")
       :anchors {:left 12 :right 12})
     (node#twoStepBox-end :height 4 :color transparent)))

(defview two-step-switch "Two-step on or off: turning it on sets it up first, turning it off asks the password." []
  `(row#row-two-step :height 36 :gap 12 :alignItems center :anchors {:left 12 :right 12}
     (toggle#twoStep :width 44
       :height 24
       :role switch
       :focusable true
       :label ~(t "twoStep")
       :description ~(t "twoStepState" {:on state.user.twoStep})
       :checked ~state.user.twoStep
       :activeColor @color.accent
       :trackColor @color.border
       :onChange ~(handler setTwoStep event.value)
       :keys {:space ~(handler setTwoStep (not state.user.twoStep))
              :enter ~(handler setTwoStep (not state.user.twoStep))})
     (column#column-two-step :grow 1 :height 36
       (node#label-two-step-name :style fieldLabel :text ~(t "twoStep"))
       (node#label-two-step-state :style hint :text ~(t "twoStepState" {:on state.user.twoStep})))))

(defview trusted-device "A device two-step remembers, for how long, and a way to forget it." [device]
  `(row#row-trusted :when ~state.user.twoStep :height 18 :gap 12 :alignItems center :anchors {:left 12 :right 12}
     (node#trusted-name :style hint
       :grow 1
       :anchors {}
       :icon ~(if (= device.device "phone") "smartphone" "laptop")
       :text ~(str (t (str "device_" device.device)) "  ·  " (t "trustedDays" {:n device.days})))
     (node#trusted-remove :style smallLink
       :text ~(t "removeDevice")
       :label ~(str (t "removeDevice") " " (t (str "device_" device.device)))
       :onClick ~(handler removeTrusted device.device))))

(defview passkey-row "A passkey: its device, when it was added and last used, and Remove." [passkey]
  `(node#passkeyRow :use itemRow
     :with {:icon fingerprint-pattern
            :title ~(t (str "device_" passkey.device))
            :hint ~(if (< passkey.used 0)
                       (t "passkeyNew" {:added (t "daysAgo" {:n passkey.added})})
                       (t "passkeyLine"
                          {:added (t "daysAgo" {:n passkey.added}) :used (t "daysAgo" {:n passkey.used})}))}
     (node#removePasskey :style smallLink
       :height 20
       :text ~(t "remove")
       :label ~(str (t "remove") " " (t (str "device_" passkey.device)))
       :onClick ~(handler removePasskey passkey.id))))

;;; Devices

(defview devices-section "The sessions signed in, this one first, and Sign out of the others." []
  (account-section "devices"
    (repeat [session state.sessions :key session.id]
      (session-row session))
    `(node#signOutOthers :when ~(> (count state.sessions) 1)
       :style smallButton
       :icon ~(if (= state.busy "devices") "loader" "log-out")
       :text ~(if (= state.busy "devices") computed.busyText (t "signOutOthers"))
       :disabled ~(not= state.busy "")
       :onClick signOutOthers)
    `(node#onlyThisDevice :when ~(<= (count state.sessions) 1)
       :style hint
       :icon shield-check
       :text ~(t "onlyThisDevice"))))

(defview session-row "A session: its device and app, where and when it was last active, and Sign out." [session]
  `(node#sessionRow :use itemRow
     :with {:icon ~(get computed.deviceIcons session.device)
            :title ~(str (t (str "device_" session.device)) "  ·  " (t (str "app_" session.app)))
            :hint ~(str session.place
                        "  ·  "
                        (if (= session.id "here")
                            (t "activeNow")
                            (t "lastActive" {:when (t "daysAgo" {:n session.days})})))}
     (node#badge-here :when ~(= session.id "here") :style badge :text ~(t "thisDevice"))
     (node#signOutSession :when ~(not= session.id "here")
       :style smallLink
       :height 20
       :text ~(t "signOutDevice")
       :label ~(str (t "signOutDevice") ": " (t (str "device_" session.device)))
       :onClick ~(handler signOutSession session.id))))

;;; Connected accounts

(defview connected-section "The made-up providers, each connected or not." []
  (account-section "connected"
    (repeat [provider computed.providers :key provider.name]
      (provider-row provider))))

(defmacro connected-to? "Whether the account is connected to a provider, by its name." [provider]
  `(some (fn [it] (= it.provider ~provider)) state.connected))

(defview provider-row "A provider: the account it is connected as, and Connect or Disconnect." [provider]
  `(node#providerRow :use itemRow
     :with {:icon ~provider.icon
            :title ~(t (str "provider_" provider.name))
            :hint ~(if (connected-to? provider.name)
                       (t "connectedAs"
                          {:account (:account (find (fn [it] (= it.provider provider.name)) state.connected))})
                       (t "notConnected"))}
     (node#connectProvider :when ~(not (connected-to? provider.name))
       :style smallButton
       :icon ~(if (and (= state.busy "connect") (= state.target provider.name)) "loader" "link")
       :text ~(t "connect")
       :label ~(str (t "connect") " " (t (str "provider_" provider.name)))
       :disabled ~(not= state.busy "")
       :onClick ~(handler connectProvider provider.name))
     (node#disconnectProvider :when ~(connected-to? provider.name)
       :style smallButton
       :icon link-2-off
       :text ~(t "disconnect")
       :label ~(str (t "disconnect") " " (t (str "provider_" provider.name)))
       :onClick ~(handler disconnectProvider provider.name))))

;;; Activity

(defview activity-section "The account's activity, newest first." []
  (account-section "activity"
    (repeat [entry state.log :index i]
      (activity-row entry))))

(defview activity-row "An event of the log: what happened, when, on which device and where." [entry]
  `(node#eventRow :use itemRow
     :with {:icon ~(or-else (get computed.eventIcons entry.kind) "shield")
            :title ~(t "event" {:kind entry.kind})
            :hint ~(t "eventLine"
                      {:when (if (> entry.days 0)
                                 (t "daysAgo" {:n entry.days})
                                 (t "minutesAgo" {:n (floor (/ (max 0 (- state.clock entry.at)) 60))}))
                       :device (t (str "device_" entry.device))
                       :place entry.place})}
     (node#notYou :when ~(and (= entry.kind "signInBlocked") (not state.user.provider))
       :style smallLink
       :height 20
       :text ~(t "notYouLink")
       :onClick openChange)))

;;; Data

(defview data-section "What deleting the account takes away, and the button that starts it." []
  (account-section "data"
    `(label#deleteWarning :font small
       :icon alert-triangle
       :wrap true
       :padding 12
       :radius 8
       :height 62
       :color @color.errorSoft
       :textColor @color.error
       :anchors {:left 0 :right 10}
       :text ~(t "deleteText"))
    `(row#row-delete :height 42 :anchors {:left 0 :right 10}
       (node#deleteAccount :style danger
         :anchors {}
         :width 200
         :height 42
         :icon trash-2
         :text ~(t "deleteButton")
         :isOpen ~state.deleting.open
         :onClick openDelete))))
