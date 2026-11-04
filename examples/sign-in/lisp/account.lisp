;;; account.lisp --- the account page, once signed in: who is in, how long the session lasts, and the account's
;;; sections
;;;
;;; The sections (profile, emails, security, devices, connected accounts, activity, data) are tabs down the side on a
;;; wide window, and chips across the top on a narrow one. What the buttons run is in profile.lisp.

;;; The screen

(defview account-screen "The account page: its card, the session ticking each second, and the photo's colours." []
  `(node#panel-signed-in :style screen :every {:ms 1000 :do tick}
     ~(account-card)
     ~(photo-popover)))

(defview account-card "The card: who is signed in, the session, the section tabs and the section shown." []
  `(node#accountCard :style accountCard :width ~(min 880 (- window.width 32)) :height ~(min 604 (- window.height 116))
     (node#accountCard-top :height 14 :color transparent)
     ~(who-is-in)
     ~(session-line)
     (node#divider :style divider :anchors {:left 28 :right 28})
     ~(section-chips)
     (row#row-body :grow 1 :gap 24 :alignItems stretch :anchors {:left 28 :right 28}
       ~(section-tabs)
       ~(section-panel))
     (node#accountCard-end :height 14 :color transparent)))

(defview who-is-in "The avatar, You're in, the email signed in, and Sign out with its keys." []
  `(row#row-account :height 56 :gap 14 :alignItems center :anchors {:left 28 :right 28}
     (label#avatar :font title
       :text ~computed.initials
       :width 56
       :height 56
       :radius 999
       :align center
       :color ~computed.avatarColor
       :textColor ~computed.avatarText
       :role img
       :label ~state.user.name)
     (column#column-account :grow 1 :height 56 :gap 2
       (node#label-you-re-in :style title :text ~(t "youreIn") :role heading)
       (node#label-signed-in-as :style caption :text ~(t "signedInAs" {:email state.user.email})))
     (node#label-sign-out :style smallButton
       :icon log-out
       :text ~(t "signOutButton")
       :tooltip ~(str (t "signOutButton") " (" computed.signOutKey ")")
       :onClick signOut
       :height 38)))

(defview session-line "Whether this device stays signed in, the provider, and the time left, with Stay at the end." []
  `(row#row-meta :height 20 :gap 18 :alignItems center :anchors {:left 28 :right 28}
     (label#label-remembered :font small
       :height 20
       :icon ~(if state.remember "shield-check" "clock-3")
       :textColor @color.muted
       :text ~(if state.remember (t "staysSignedIn") (t "signedOutOnClose"))
       :fitText true)
     (label#label-provider :when ~state.user.provider
       :font small
       :height 20
       :icon key-round
       :textColor @color.muted
       :text ~(t "signedInWith")
       :fitText true)
     (label#session :font small
       :fitText true
       :height 20
       :icon timer
       :role timer
       :textColor ~(if (<= state.session.left 60) "@color.error" "@color.muted")
       :text ~(t "sessionEnds" {:time computed.sessionTime}))
     (node#staySignedIn :when ~(<= state.session.left 60)
       :style smallLink
       :height 20
       :text ~(t "stay")
       :onClick staySignedIn)))

;;; The sections' tabs

(defview section-chips "The sections as chips across the top, on a narrow window." []
  `(row#navRow :when ~(not computed.wide)
     :height 46
     :gap 4
     :role tablist
     :label ~(t "accountSections")
     :arrows horizontal
     :scroll horizontal
     :anchors {:left 28 :right 28}
     :alignItems start
     ~(repeat [tab computed.sections]
        `(node#navChip :use navItem :with {:section ~tab.name :icon ~tab.icon :wide false}))))

(defview section-tabs "The sections as tabs down the side, on a wide window." []
  `(column#navColumn :when ~computed.wide :width 196 :gap 2 :role tablist :label ~(t "accountSections") :arrows vertical
     ~(repeat [tab computed.sections]
        `(node#nav :use navItem :with {:section ~tab.name :icon ~tab.icon :wide true}))))

(defview section-panel "The section shown, scrolling; each section is there, shown when it is the one chosen." []
  `(column#sectionPanel :grow 1 :gap 10 :scroll vertical
     ~(profile-section)
     ~(emails-section)
     ~(security-section)
     ~(devices-section)
     ~(connected-section)
     ~(activity-section)
     ~(data-section)))

(defview account-section "A section of the account page: its column, shown while chosen, under its heading."
  [name & body]
  `(column :id ~(str "section-" name) :when ~(= state.section name)
     :fitContent height
     :gap 10
     :anchors {:left 0 :right 0}
     :role tabpanel
     :label ~(t (str "section_" name))
     (node :id ~(str "header-" name) :use sectionHeader :with {:section ~name})
     ~@body))

;;; Profile

(defview profile-section "The profile: the photo, the name, the username and Save." []
  (account-section "profile"
    (photo-row)
    `(node#label-account-name :style fieldLabel :text ~(t "name"))
    `(node#account-name :use checkedField
       :with {:submit saveProfile
              :bind state.profile.name
              :content ~state.profile.name
              :key accountName
              :icon user
              :fieldName ~(t "name")
              :placeholder ~(t "namePlaceholder")
              :valid ~(> (count (trim state.profile.name)) 0)
              :message ~(t "enterName")
              :submitted ~state.profile.submitted
              :maxLength 60})
    `(node#usernameLine :when ~(not= (or-else state.user.username "") "")
       :style hint
       :icon at-sign
       :text ~(t "usernameIs" {:name state.user.username}))
    `(row#row-account-buttons :height 42 :anchors {:left 0 :right 10}
       (node#saveProfile :style primary
         :anchors {}
         :icon ~(if (= state.busy "save") "loader" "save")
         :text ~(if (= state.busy "save") computed.busyText (t "saveChanges"))
         :disabled ~(not= state.busy "")
         :onClick saveProfile
         :width 200
         :height 42))))

(defview photo-row "The photo, large, and Change photo, which opens its colours." []
  `(row#row-photo :height 64 :gap 16 :alignItems center :anchors {:left 0 :right 10}
     (label#avatarLarge :font title
       :text ~computed.initials
       :width 64
       :height 64
       :radius 999
       :align center
       :color ~computed.avatarColor
       :textColor ~computed.avatarText
       :role img
       :label ~(t "photo"))
     (column#column-photo :grow 1 :height 42 :gap 2
       (node#label-photo :style itemTitle :text ~(t "photo"))
       (node#changePhoto :style smallLink
         :height 20
         :text ~(t "changePhoto")
         :onClick {:toggle state.photoOpen}
         :isOpen ~state.photoOpen))))

(defview photo-popover "The photo's colours, under Change photo: the initials on the accent, or one of six colours." []
  `(node#photoPopover :use popover
     :with {:open state.photoOpen
            :anchor "#changePhoto"
            :title ~(t "photoTitle")
            :text ~(t "photoText")
            :width 290
            :height 112
            :color @color.surface
            :textColor @color.text
            :dimColor @color.muted}
     (row#swatches :gap 8
       :height 30
       :role radiogroup
       :label ~(t "photoTitle")
       :arrows horizontal
       :anchors {:left 12 :right 12 :top 64}
       ~(repeat [colour (range 7)]
          (swatch colour)))))

(defview swatch "A colour of the photo, ticked while it is the one chosen; 0 is the initials on the accent." [colour]
  `(label#swatch :width 30
     :height 30
     :radius 999
     :cursor hand
     :focusable true
     :role radio
     :text ""
     :font iconsSmall
     :align center
     :icon ~(if (= (or-else state.user.avatar 0) colour) "check" "")
     :label ~(if (= colour 0) (t "photoColor" {:n 1}) (t "photoColor" {:n (+ colour 1)}))
     :selected ~(= (or-else state.user.avatar 0) colour)
     :color ~(if (= colour 0) "@color.accentSoft" (str "@color.avatar" colour))
     :textColor ~(if (= colour 0) "@color.accent" "@color.avatarText")
     :borderWidth ~(if (= (or-else state.user.avatar 0) colour) 2 0)
     :borderColor @color.text
     :onClick ~(handler setAvatar colour)
     :keys {:enter ~(handler setAvatar colour) :space ~(handler setAvatar colour)}))
