{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Sign in: sign in, create an account, two-step codes, a password reset, an account page with a session that expires, and a fake OAuth provider (Example ID). A fake server in actions answers, slowly when the Slow network setting is on. Light and dark, English, Italian and Arabic, no code."
  :include ["locales/ar.lisp" "locales/en.lisp" "locales/it.lisp" "../../shared/addons/assistant/assistant.lisp"]
  :window {:title "Sign in"
    :width 960
    :height 720
    :minWidth 480
    :minHeight 640
    :focusRing {:color @color.accent :width 2 :offset 2}}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons arabic]}
    :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [iconsSmall arabicSmall]}
    :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 26 :fallback [arabicTitle]}
    :icons {:file "../../shared/fonts/Lucide.ttf" :size 18}
    :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 13}
    :logo {:file "../../shared/fonts/Lucide.ttf" :size 22}
    :mono {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 15}
    :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 19 :fallback [icons arabicHeading]}
    :arabic {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 15}
    :arabicSmall {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 13}
    :arabicTitle {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 26}
    :arabicHeading {:file "../../shared/fonts/NotoSansArabic-Subset.ttf" :size 19}}
  :tokens {:radius.card 16
    :radius.field 10
    :color.background {:light "#eef2ff" :dark "#0b1020"}
    :color.glow {:light "#818cf855" :dark "#6366f130"}
    :color.glow2 {:light "#f0abfc44" :dark "#a855f724"}
    :color.surface {:light "#ffffff" :dark "#141b2e"}
    :color.text {:light "#0f172a" :dark "#e2e8f0"}
    :color.muted {:light "#64748b" :dark "#94a3b8"}
    :color.border {:light "#e2e8f0" :dark "#26304a"}
    :color.input {:light "#f8fafc" :dark "#0f1527"}
    :color.inputFocused {:light "#ffffff" :dark "#111a33"}
    :color.hover {:light "#f1f5f9" :dark "#1c2540"}
    :color.accent {:light "#6366f1" :dark "#818cf8"}
    :color.accentHover {:light "#7477f3" :dark "#939cfa"}
    :color.accentPressed {:light "#4f46e5" :dark "#6366f1"}
    :color.accentText {:light "#ffffff" :dark "#0b1020"}
    :color.accentSoft {:light "#e0e7ff" :dark "#1e2550"}
    :color.error {:light "#dc2626" :dark "#f87171"}
    :color.errorSoft {:light "#fef2f2" :dark "#2a1620"}
    :color.notice {:light "#fffbeb" :dark "#2a2414"}
    :color.noticeBorder {:light "#fcd34d" :dark "#a16207"}
    :color.warning {:light "#b45309" :dark "#fbbf24"}
    :color.weak {:light "#ef4444" :dark "#f87171"}
    :color.fair {:light "#f59e0b" :dark "#fbbf24"}
    :color.good {:light "#6366f1" :dark "#818cf8"}
    :color.strong {:light "#16a34a" :dark "#4ade80"}
    :color.shadow {:light "#0f172a1f" :dark "#00000066"}
    :color.backdrop {:light "#0f172a66" :dark "#000000a0"}
    :color.toast {:light "#0f172a" :dark "#e2e8f0"}
    :color.toastText {:light "#ffffff" :dark "#0f172a"}
    :color.qrBack {:light "#ffffff" :dark "#ffffff"}
    :color.qrInk {:light "#111111" :dark "#111111"}
    :color.avatar1 {:light "#0ea5e9" :dark "#38bdf8"}
    :color.avatar2 {:light "#10b981" :dark "#34d399"}
    :color.avatar3 {:light "#f59e0b" :dark "#fbbf24"}
    :color.avatar4 {:light "#ef4444" :dark "#f87171"}
    :color.avatar5 {:light "#8b5cf6" :dark "#a78bfa"}
    :color.avatar6 {:light "#ec4899" :dark "#f472b6"}
    :color.avatarText {:light "#ffffff" :dark "#0b1020"}
    :color.selected {:light "#eef2ff" :dark "#1a2244"}
    :color.toastAction {:light "#a5b4fc" :dark "#4f46e5"}
    :assistant.accent @color.accent
    :assistant.accentHover @color.accentHover
    :assistant.onAccent @color.accentText
    :assistant.surface @color.surface
    :assistant.raised {:light "#f8fafc" :dark "#1b2338"}
    :assistant.border @color.border
    :assistant.text @color.text
    :assistant.dim @color.muted
    :assistant.dot @color.muted
    :assistant.success {:light "#16a34a" :dark "#4ade80"}
    :assistant.danger @color.error
    :assistant.warning {:light "#fef3c7" :dark "#3b2e0a"}}
  :colors {:accent "#6366f1" :accentHover "#7477f3" :fieldFocused "#ffffff"}
  :theme "{{state.theme}}"
  :strings {:default en :locale "{{state.locale}}"}
  :connections {:exampleId {:note "A made-up OAuth 2 provider for the demo: no real client id or secret. Scenarios play its answers as the providerSignedIn and providerFailed actions."
      :url "https://id.example.com/api"
      :auth {:flow browser
        :signInUrl "https://id.example.com/authorize"
        :tokenUrl "https://id.example.com/token"
        :clientId affrescoui-sign-in-demo
        :scopes [openid email profile]}}}
  :state {:email ""
    :step email
    :password ""
    :confirm ""
    :name ""
    :submitted false
    :theme light
    :locale en
    :focus ""
    :showPassword false
    :remember true
    :slow false
    :busy ""
    :formError ""
    :providerMessage ""
    :notice ""
    :demoCode "246810"
    :accounts [{:email ada@example.com
        :name Ada
        :password ""
        :twoStep false
        :provider false
        :passkey true
        :username ada
        :demo true
        :passwordDays 30}
      {:email sam@example.com
        :name Sam
        :password ""
        :twoStep true
        :provider false
        :passkey false
        :username sam
        :demo true
        :passwordDays 90}]
    :user {:email "" :name "" :twoStep false :provider false :passkey false :username ""}
    :pending {:email "" :name "" :twoStep false :provider false :passkey false :username ""}
    :code ""
    :codeError ""
    :resendLeft 0
    :session {:left 300 :warned false}
    :profile {:name "" :email "" :submitted false}
    :change {:open false :password "" :confirm "" :submitted false}
    :reset {:email "" :code "" :password "" :confirm "" :submitted false :error "" :left 0}
    :shortcutsOpen false
    :forgot {:open false :email "" :error ""}
    :toast {:open false :message "" :action ""}
    :codeMode twoStep
    :providerName exampleId
    :passkeyOpen false
    :username ""
    :usernameChecked ""
    :spin 0
    :terms false
    :invite {:open false :code ""}
    :setup {:open false :step scan :code "" :error "" :saved false}
    :backupCodes [DEMO-1001 DEMO-1002 DEMO-1003 DEMO-1004 DEMO-1005 DEMO-1006 DEMO-1007 DEMO-1008]
    :backupCode ""
    :trust false
    :phone "555-0142"
    :trusted [{:email sam@example.com :device phone :days 12}]
    :failures []
    :confirmedLeft 0
    :reauth {:open false :password "" :error "" :then ""}
    :section profile
    :clock 0
    :target ""
    :photoOpen false
    :emails []
    :newEmail {:open false :address "" :submitted false}
    :myPasskeys []
    :sessions []
    :connected []
    :log []
    :deleting {:open false :typed "" :error ""}}
  :stateTypes {:email {:type string}
    :password {:type string}
    :confirm {:type string}
    :name {:type string}
    :submitted {:type boolean}
    :theme {:enum [light dark]}
    :locale {:enum [en it ar]}
    :focus {:type string}
    :showPassword {:type boolean}
    :remember {:type boolean}
    :slow {:type boolean}
    :busy {:type string}
    :code {:type string}
    :resendLeft {:type number}
    :step {:enum [email password]}
    :codeMode {:enum [twoStep email verify sms backup]}
    :username {:type string}
    :usernameChecked {:type string}
    :spin {:type number}
    :terms {:type boolean}
    :section {:enum [profile emails security devices connected activity data]}
    :clock {:type number}
    :target {:type string}}
  :persist [state.theme state.remember state.locale]
  :computed {:emailValid "{{isEmail(trim(state.email))}}"
    :passwordValid "{{length(state.password) >= 8}}"
    :strength "{{merge({ score: 0, label: '', missing: [] }, passwordStrength(state.password))}}"
    :newPasswordValid "{{computed.passwordValid && computed.strength.score >= 2}}"
    :newPasswordMessage "{{computed.passwordValid ? t('stronger') : t('min8')}}"
    :confirmValid "{{state.showPassword || state.confirm == state.password}}"
    :errors "{{!state.submitted ? 0 : (computed.emailValid ? 0 : 1) + (computed.passwordValid ? 0 : 1)}}"
    :createErrors "{{!state.submitted ? 0 : (length(trim(state.name)) > 0 ? 0 : 1) + (computed.emailValid ? 0 : 1) + (computed.usernameStatus == 'available' ? 0 : 1) + (computed.newPasswordValid ? 0 : 1) + (computed.confirmValid ? 0 : 1) + (state.terms ? 0 : 1) + (state.invite.open && length(trim(state.invite.code)) > 0 && !computed.inviteValid ? 1 : 0)}}"
    :account "{{find(state.accounts, lower(it.email) == lower(trim(state.email)))}}"
    :initials "{{upper(slice(length(trim(state.user.name)) > 0 ? trim(state.user.name) : state.user.email, 0, 1))}}"
    :resetStrength "{{merge({ score: 0, label: '', missing: [] }, passwordStrength(state.reset.password))}}"
    :resetValid "{{length(state.reset.password) >= 8 && computed.resetStrength.score >= 2}}"
    :resetConfirmValid "{{state.showPassword || state.reset.confirm == state.reset.password}}"
    :resetErrors "{{!state.reset.submitted ? 0 : (computed.resetValid ? 0 : 1) + (computed.resetConfirmValid ? 0 : 1)}}"
    :changeError "{{length(state.change.password) < 8 ? t('min8') : merge({ score: 0, label: '', missing: [] }, passwordStrength(state.change.password)).score < 2 ? t('stronger') : !state.showPassword && state.change.confirm != state.change.password ? t('mismatch') : ''}}"
    :profileErrors "{{!state.profile.submitted ? 0 : (length(trim(state.profile.name)) > 0 ? 0 : 1)}}"
    :busyText "{{state.busy == '' ? '' : t('busy', { what: state.busy })}}"
    :sessionTime "{{floor(max(0, state.session.left) / 60) + ':' + pad(max(0, state.session.left) % 60, 2, '0')}}"
    :shortcuts "{{map(commands, merge(it, { keyText: slice(join(split(join(split(join(split(join(split('|' + (it.keyLabel ?? ''), '⌃'), 'Ctrl+'), '⌥'), 'Option+'), '⇧'), 'Shift+'), '⌘'), 'Cmd+'), 1) }))}}"
    :themeKey "{{find(computed.shortcuts, it.name == 'toggleTheme').keyText}}"
    :shortcutsKey "{{find(computed.shortcuts, it.name == 'openShortcuts').keyText}}"
    :signOutKey "{{find(computed.shortcuts, it.name == 'signOut').keyText}}"
    :passkeys "{{filter(state.accounts, it.passkey)}}"
    :providerTitle "{{state.providerName == 'exampleId' ? 'Example ID' : t('provider_' + state.providerName)}}"
    :usernameStatus "{{length(trim(state.username)) == 0 ? '' : length(trim(state.username)) < 3 || length(trim(state.username)) > 20 || !every(split(lower(trim(state.username)), ''), contains('abcdefghijklmnopqrstuvwxyz0123456789_.', it)) ? 'invalid' : state.usernameChecked != lower(trim(state.username)) ? 'checking' : contains(['ada', 'sam', 'admin'], state.usernameChecked) ? 'taken' : 'available'}}"
    :inviteValid "{{upper(trim(state.invite.code)) == 'EXAMPLE-INVITE'}}"
    :otpUri "{{'otpauth://totp/Example%20ID:' + state.user.email + '?secret=EXAMPLEEXAMPLE23&issuer=Example%20ID'}}"
    :otpQr "{{qrCode(computed.otpUri, 'M')}}"
    :trustedHere "{{some(state.trusted, lower(it.email) == lower(state.pending.email) && it.device == 'this')}}"
    :myTrusted "{{filter(state.trusted, lower(it.email) == lower(state.user.email))}}"
    :locked "{{count(state.failures, it == lower(trim(state.email))) >= 5}}"
    :resetTime "{{floor(max(0, state.reset.left) / 60) + ':' + pad(max(0, state.reset.left) % 60, 2, '0')}}"
    :sections "{{[{ name: 'profile', icon: 'user' }, { name: 'emails', icon: 'mail' }, { name: 'security', icon: 'shield-check' }, { name: 'devices', icon: 'monitor-smartphone' }, { name: 'connected', icon: 'link' }, { name: 'activity', icon: 'history' }, { name: 'data', icon: 'trash-2' }]}}"
    :wide "{{window.width >= 760}}"
    :avatarColor "{{(state.user.avatar ?? 0) > 0 ? '@color.avatar' + state.user.avatar : '@color.accentSoft'}}"
    :avatarText "{{(state.user.avatar ?? 0) > 0 ? '@color.avatarText' : '@color.accent'}}"
    :passwordDays "{{state.user.passwordDays ?? 0}}"
    :newEmailError "{{!isEmail(trim(state.newEmail.address)) ? 'invalid' : some(state.emails, lower(it.address) == lower(trim(state.newEmail.address))) ? 'taken' : length(state.emails) >= 5 ? 'full' : ''}}"
    :providers "{{[{ name: 'wrenfield', icon: 'bird' }, { name: 'mossbank', icon: 'leaf' }, { name: 'quillo', icon: 'feather' }]}}"
    :deleteWord "{{t('deleteWord')}}"
    :eventIcons {:signIn log-in
      :signInBlocked shield-alert
      :password key-round
      :twoStepOn shield-check
      :twoStepOff shield-off
      :newCodes key
      :passkeyAdded fingerprint
      :passkeyRemoved fingerprint
      :emailAdded mail-plus
      :emailRemoved mail-x
      :emailVerified mail-check
      :primaryChanged mail
      :signedOutOthers log-out
      :signedOutDevice log-out
      :connected link
      :disconnected link-2-off
      :profile user}
    :deviceIcons {:this monitor :phone smartphone :laptop laptop :tablet tablet :key key-round}}
  :styles {:caption {:type label :height 20 :textColor @color.muted :anchors {:left 0 :right 10}}
    :fieldLabel {:type label
      :font small
      :height 18
      :color transparent
      :textColor @color.text
      :anchors {:left 0 :right 10}}
    :fieldBox {:anchors {:left 0 :right 0 :top 0}
      :height 44
      :radius @radius.field
      :color @color.input
      :borderWidth 1
      :borderColor @color.border
      :hover {:borderColor @color.muted}}
    :fieldBoxOn {:style fieldBox
      :color @color.inputFocused
      :borderWidth 2
      :borderColor @color.accent
      :hover {:borderColor @color.accent}
      :shadow {:y 0 :blur 8 :color @color.glow}}
    :fieldBoxError {:style fieldBox :borderColor @color.error :hover {:borderColor @color.error}}
    :input {:type textarea :padding 8 :color transparent :textColor @color.text :placeholderColor @color.muted}
    :field {:type textarea
      :padding 10
      :radius @radius.field
      :borderWidth 1
      :borderColor @color.border
      :color @color.input
      :focused {:borderColor @color.accent}
      :textColor @color.text
      :placeholderColor @color.muted}
    :code {:type codeInput
      :height 52
      :anchors {:left 0 :right 10}
      :cellColor @color.input
      :cellBorderColor @color.border
      :cellRadius 8
      :cellGap 8
      :textColor @color.text}
    :title {:type label :font title :height 34 :textColor @color.text :anchors {:left 0 :right 10}}
    :primary {:type label
      :height 46
      :anchors {:left 0 :right 10}
      :align center
      :radius @radius.field
      :color @color.accent
      :textColor @color.accentText
      :cursor hand
      :focusable true
      :role button
      :shadow {:y 4 :blur 12 :color @color.glow}
      :hover {:color @color.accentHover}
      :pressed {:color @color.accentPressed}
      :transition {:color 120}}
    :secondary {:style primary
      :color transparent
      :textColor @color.text
      :borderWidth 1
      :borderColor @color.border
      :shadow {:color "#00000000"}
      :hover {:color @color.hover}
      :pressed {:color @color.border}}
    :link {:type label
      :height 30
      :anchors {:left 0 :right 10}
      :align center
      :textColor @color.accent
      :cursor hand
      :focusable true
      :role link
      :hover {:textColor @color.accentPressed}}
    :smallLink {:type label
      :font small
      :textColor @color.accent
      :cursor hand
      :fitText true
      :focusable true
      :role link
      :hover {:textColor @color.accentPressed}}
    :iconButton {:type label
      :text ""
      :width 32
      :height 32
      :radius 8
      :color transparent
      :textColor @color.muted
      :align center
      :cursor hand
      :focusable true
      :role button
      :hover {:color @color.hover :textColor @color.text}}
    :chip {:type label
      :font small
      :width 36
      :height 32
      :radius 8
      :align center
      :color transparent
      :textColor @color.muted
      :cursor hand
      :focusable true
      :role radio
      :hover {:color @color.hover :textColor @color.text}}
    :chipOn {:style chip :color @color.accentSoft :textColor @color.accent}
    :card {:radius @radius.card
      :color @color.surface
      :borderWidth 1
      :borderColor @color.border
      :shadow {:y 12 :blur 32 :color @color.shadow}
      :type column
      :fitContent height
      :scroll vertical}
    :form {:type column :fitContent height :gap 8 :anchors {:left 36 :right 26}}
    :logo {:type label
      :font logo
      :icon feather
      :text ""
      :width 48
      :height 48
      :radius 14
      :align center
      :color @color.accent
      :textColor @color.accentText
      :shadow {:y 6 :blur 16 :color @color.glow}}
    :spacer {:height 6 :anchors {:left 0 :right 10} :color transparent}
    :checkRow {:type row :height 22 :gap 8 :alignItems center :anchors {:left 0 :right 10}}
    :checkOff {:type label
      :font iconsSmall
      :text ""
      :width 18
      :height 18
      :radius 5
      :align center
      :color @color.input
      :borderWidth 1
      :borderColor @color.border
      :cursor hand
      :focusable true
      :role checkbox
      :hover {:borderColor @color.accent}}
    :checkOn {:style checkOff :icon check :color @color.accent :borderColor @color.accent :textColor @color.accentText}
    :alert {:type label
      :font small
      :icon alert-circle
      :wrap true
      :padding 8
      :radius 8
      :role alert
      :live assertive
      :color @color.errorSoft
      :textColor @color.error
      :anchors {:left 0 :right 10}}
    :inlineError {:type label
      :font small
      :icon alert-circle
      :height 18
      :role alert
      :live assertive
      :textColor @color.error
      :anchors {:left 0 :right 10}}
    :hint {:type label :font small :height 18 :textColor @color.muted :anchors {:left 0 :right 10}}
    :screen {:type column :fill 0 :justify center :alignItems center :color transparent}
    :accountCard {:style card :gap 14 :scroll none :fitContent false}
    :heading {:type label :font heading :height 28 :textColor @color.text :role heading :anchors {:left 0 :right 10}}
    :navItem {:type label
      :font default
      :height 38
      :radius 8
      :padding 10
      :color transparent
      :textColor @color.muted
      :cursor hand
      :focusable true
      :role tab
      :anchors {:left 0 :right 0}
      :hover {:color @color.hover :textColor @color.text}}
    :navItemOn {:style navItem
      :color @color.selected
      :textColor @color.accent
      :hover {:color @color.selected :textColor @color.accent}}
    :itemRow {:type row
      :height 60
      :gap 12
      :alignItems center
      :radius @radius.field
      :color @color.input
      :borderWidth 1
      :borderColor @color.border
      :anchors {:left 0 :right 10}}
    :itemIcon {:type label
      :font icons
      :text ""
      :width 36
      :height 36
      :radius 8
      :align center
      :color @color.accentSoft
      :textColor @color.accent}
    :itemText {:type column :grow 1 :height 38 :gap 2}
    :itemTitle {:type label :height 20 :textColor @color.text :anchors {:left 0 :right 0}}
    :itemHint {:type label :font small :height 16 :textColor @color.muted :anchors {:left 0 :right 0}}
    :badge {:type label
      :font small
      :height 22
      :radius 999
      :padding 8
      :fitText true
      :color @color.accentSoft
      :textColor @color.accent}
    :badgeWarn {:style badge :color @color.notice :textColor @color.warning}
    :smallButton {:style secondary
      :height 34
      :anchors {}
      :font small
      :padding 12
      :fitText true
      :shadow {:color "#00000000"}}
    :danger {:style primary
      :color @color.error
      :textColor @color.accentText
      :shadow {:color "#00000000"}
      :hover {:color @color.weak}
      :pressed {:color @color.error}}
    :navItemChip {:style navItem :fitText true :anchors {}}
    :navItemOnChip {:style navItemOn :fitText true :anchors {}}
    :divider {:height 1 :color @color.border :anchors {:left 0 :right 0}}}
  :components {:checkedField {:note "A field with an icon, an accessible name and its error under it once its form was submitted (the error is also the field's description, read out as an alert); secret ones show dots and, with eye, a button to show them. params: bind, content, key, icon, name, placeholder, valid, message, submitted, secret, eye, maxLength, submit (the action Enter runs)"
      :params {:bind ""
        :content ""
        :key ""
        :icon ""
        :name ""
        :placeholder ""
        :valid true
        :message ""
        :submitted false
        :secret false
        :eye false
        :maxLength 0
        :submit ""
        :change ""}
      :node (node #panel-checked-field
        :check {:value "{{!submitted || valid}}" :rule "{{value}}" :message "{{message}}"}
        :let {:on "{{state.focus == key}}"}
        :anchors {:left 0 :right 10}
        :height "{{error ? 68 : 44}}"
        :color transparent
        (node #field-box :style "{{on ? 'fieldBoxOn' : error ? 'fieldBoxError' : 'fieldBox'}}"
          (node #field-icon
            :type label
            :font icons
            :icon "{{icon}}"
            :text ""
            :anchors {:left 12 :top 0 :bottom 0}
            :width 20
            :align center
            :textColor "{{on ? '@color.accent' : '@color.muted'}}")
          (node #textarea-placeholder
            :style input
            :anchors {:left 34 :right "{{eye ? 44 : 8}}" :top 1 :bottom 1}
            :placeholder "{{placeholder}}"
            :bind "{{bind}}"
            :secret "{{secret && !state.showPassword}}"
            :maxLength "{{maxLength}}"
            :label "{{name}}"
            :description "{{error || ''}}"
            :onSubmit {:if "{{submit != ''}}" :then [{:run "{{submit}}"}]}
            :onFocus {:set state.focus :to "{{key}}"}
            :onBlur {:if "{{state.focus == key}}" :then [{:set state.focus :to ""}]}
            :onChange {:if "{{change != ''}}" :then [{:run "{{change}}"}]})
          (node #field-eye
            :when "{{eye}}"
            :style iconButton
            :font iconsSmall
            :icon "{{state.showPassword ? 'eye-off' : 'eye'}}"
            :anchors {:right 6 :top 6}
            :label "{{state.showPassword ? t('hidePassword') : t('showPassword')}}"
            :tooltip "{{state.showPassword ? t('hidePassword') : t('showPassword')}}"
            :onClick togglePassword))
        (node #label-error :when "{{error}}" :style inlineError :text "{{error}}" :anchors {:left 2 :right 0 :top 50}))}
    :strengthMeter {:note "The passwordMeter with the screen's words and colours, and a hint of what would make the password stronger. params: password"
      :params {:password ""}
      :node (node #strength
        :let {:s "{{merge({ score: 0, label: '', missing: [] }, passwordStrength(password))}}"}
        :anchors {:left 0 :right 10}
        :height 52
        :color transparent
        (node #strength-meter
          :color transparent
          :use passwordMeter
          :with {:password "{{password}}"
            :width "{{min(420, window.width - 32) - 72}}"
            :labels {:empty "{{t('strengthEmpty')}}"
              :weak "{{t('weak')}}"
              :fair "{{t('fair')}}"
              :good "{{t('good')}}"
              :strong "{{t('strong')}}"}
            :track @color.border
            :textColor @color.muted
            :colors [@color.weak @color.weak @color.fair @color.good @color.strong]})
        (node #strength-name
          :style hint
          :anchors {:left 0 :right 0 :top 32}
          :text "{{length(password) == 0 ? t('strengthHint') : length(s.missing) == 0 ? t('strengthDone') : t('strengthAdd', { list: formatList(map(s.missing, t('missingItem', { what: it })), 'and') })}}"))}
    :passwordRules {:note "What a new password needs, each ticked off as it is met. params: password"
      :params {:password ""}
      :node (node #rules
        :type grid
        :columns 2
        :rowGap 4
        :columnGap 8
        :height 40
        :color transparent
        :anchors {:left 0 :right 10}
        :let {:m "{{merge({ score: 0, label: '', missing: [] }, passwordStrength(password)).missing}}"
          :n "{{length(password)}}"}
        (node #rule-length
          :type label
          :font small
          :height 18
          :color transparent
          :icon "{{n >= 8 ? 'check-circle' : 'circle'}}"
          :text "{{t('rule8')}}"
          :textColor "{{n >= 8 ? '@color.text' : '@color.muted'}}"
          :label "{{t('rule8') + ': ' + t('ruleState', { ok: n >= 8 })}}")
        (node #rule-case
          :type label
          :font small
          :height 18
          :color transparent
          :icon "{{n > 0 && !contains(m, 'upper') && !contains(m, 'lower') ? 'check-circle' : 'circle'}}"
          :text "{{t('ruleCase')}}"
          :textColor "{{n > 0 && !contains(m, 'upper') && !contains(m, 'lower') ? '@color.text' : '@color.muted'}}"
          :label "{{t('ruleCase') + ': ' + t('ruleState', { ok: n > 0 && !contains(m, 'upper') && !contains(m, 'lower') })}}")
        (node #rule-digit
          :type label
          :font small
          :height 18
          :color transparent
          :icon "{{n > 0 && !contains(m, 'digit') ? 'check-circle' : 'circle'}}"
          :text "{{t('ruleDigit')}}"
          :textColor "{{n > 0 && !contains(m, 'digit') ? '@color.text' : '@color.muted'}}"
          :label "{{t('ruleDigit') + ': ' + t('ruleState', { ok: n > 0 && !contains(m, 'digit') })}}")
        (node #rule-symbol
          :type label
          :font small
          :height 18
          :color transparent
          :icon "{{n > 0 && !contains(m, 'symbol') ? 'check-circle' : 'circle'}}"
          :text "{{t('ruleSymbol')}}"
          :textColor "{{n > 0 && !contains(m, 'symbol') ? '@color.text' : '@color.muted'}}"
          :label "{{t('ruleSymbol') + ': ' + t('ruleState', { ok: n > 0 && !contains(m, 'symbol') })}}"))}
    :sectionHeader {:note "A section's heading and caption. params: name (the section)"
      :params {:name ""}
      :node (node #column-section-header :type column :fitContent height :gap 2 :anchors {:left 0 :right 10}
        (node #section-title :style heading :text "{{t('section_' + name)}}")
        (node #section-caption :style caption :text "{{t('caption_' + name)}}"))}
    :itemRow {:note "A row of a list: an icon, a title and a hint, then the row's own children (badges, links, buttons). params: icon, title, hint, label (the row's accessible name)"
      :params {:icon "" :title "" :hint "" :label ""}
      :node (node #row-row :style itemRow :role group :label "{{label || title}}"
        (node #item-start :width 0 :color transparent)
        (node #item-icon :style itemIcon :icon "{{icon}}")
        (node #item-text :style itemText
          (node #item-title :style itemTitle :text "{{title}}")
          (node #item-hint :style itemHint :text "{{hint}}"))
        {:slot true}
        (node #item-end :width 0 :color transparent))}
    :navItem {:note "One tab of the account's sections. params: name, icon, wide (a full-width row, or a chip)"
      :params {:name "" :icon "" :wide true}
      :node (node #panel-t-section-name
        :style "{{(state.section == name ? 'navItemOn' : 'navItem') + (wide ? '' : 'Chip')}}"
        :icon "{{icon}}"
        :text "{{t('section_' + name)}}"
        :selected "{{state.section == name}}"
        :onClick {:run setSection :with {:name "{{name}}"}}
        :keys {:enter {:run setSection :with {:name "{{name}}"}} :space {:run setSection :with {:name "{{name}}"}}})}}
  :actions {:notify [{:note "with: message. A toast in the corner for a few seconds"}
      {:set state.toast :to {:open true :message "{{message}}" :action ""}}
      {:debounce 4000 :do [{:set state.toast.open :to false}]}]
    :warnSession [{:note "A toast that offers to stay signed in, for longer than the others"}
      {:set state.toast :to {:open true :message "{{t('sessionSoon')}}" :action stay}}
      {:debounce 4000 :do [{:set state.toast.open :to false}]}]
    :serverCall [{:note "with: op (the action that answers), what (what is busy). The fake server: answers at once, or after 800 ms when Slow network is on. state.busy marks the busy button and the status line."}
      {:if "{{state.busy != ''}}" :then [{:stop true}]}
      {:set state.busy :to "{{what}}"}
      {:if "{{state.slow}}"
        :then [{:after 800 :do [{:set state.busy :to ""} {:run "{{op}}"}]}]
        :else [{:set state.busy :to ""} {:run "{{op}}"}]}]
    :signIn [{:set state.submitted :to true}
      {:set state.formError :to ""}
      {:if "{{state.step == 'email'}}"
        :then [{:if "{{!computed.emailValid}}" :then [{:stop true}]}
          {:run serverCall :with {:op lookup :what lookup}}
          {:stop true}]}
      {:if "{{!computed.passwordValid}}" :then [{:stop true}]}
      {:run serverCall :with {:op checkPassword :what signIn}}]
    :lookup [{:note "The identifier step: is there an account for this email? The password step follows"}
      {:if "{{computed.account == null}}" :then [{:set state.formError :to noAccount} {:stop true}]}
      {:set state.submitted :to false}
      {:set state.step :to password}
      {:note "The email field is gone on this step, so the first field is the password"}
      {:focus "#textarea-placeholder"}]
    :changeEmail [{:set state.step :to email}
      {:set state.password :to ""}
      {:set state.submitted :to false}
      {:set state.formError :to ""}
      {:focus "#textarea-placeholder"}]
    :checkPassword [{:if "{{computed.account == null}}" :then [{:set state.formError :to noAccount} {:stop true}]}
      {:if "{{computed.locked}}"
        :then [{:set state.toast.open :to false} {:set state.formError :to locked} {:focus "#unlock"} {:stop true}]}
      {:if "{{computed.account.password != '' && computed.account.password != state.password}}"
        :then [{:note "Five wrong passwords in a row lock the account until its password is reset"}
          {:set state.toast.open :to false}
          {:push state.failures :value "{{lower(trim(state.email))}}"}
          {:set state.formError :to "{{computed.locked ? 'locked' : 'wrongPassword'}}"}
          {:stop true}]}
      {:set state.submitted :to false}
      {:set state.pending :to "{{computed.account}}"}
      {:set state.codeMode :to twoStep}
      {:if "{{computed.account.twoStep && computed.trustedHere}}" :then [{:run enter} {:stop true}]}
      {:if "{{computed.account.twoStep}}" :then [{:run startTwoStep} {:stop true}]}
      {:run enter}]
    :enter [{:note "Opens the session of state.pending"}
      {:set state.user :to "{{state.pending}}"}
      {:set state.email :to "{{state.pending.email}}"}
      {:set state.password :to ""}
      {:set state.confirm :to ""}
      {:set state.code :to ""}
      {:set state.formError :to ""}
      {:set state.notice :to ""}
      {:set state.session :to {:left 300 :warned false}}
      {:set state.profile :to {:name "{{state.pending.name}}" :email "{{state.pending.email}}" :submitted false}}
      {:set state.confirmedLeft :to 120}
      {:run seedAccount}
      {:navigate signedIn}]
    :startTwoStep [{:set state.trust :to false}
      {:set state.backupCode :to ""}
      {:set state.code :to ""}
      {:set state.codeError :to ""}
      {:set state.resendLeft :to 30}
      {:navigate twoStep}
      {:focus "#twoStepCode"}
      {:run notify :with {:message "{{t('codeSent', { email: state.pending.email })}}"}}]
    :codeChanged [{:set state.codeError :to ""} {:if "{{length(state.code) == 6}}" :then [{:run verify}]}]
    :verify [{:if "{{state.codeMode == 'backup'}}"
        :then [{:if "{{trim(state.backupCode) == ''}}" :then [{:set state.codeError :to shortBackup} {:stop true}]}
          {:run serverCall :with {:op checkBackup :what verify}}
          {:stop true}]}
      {:if "{{length(state.code) < 6}}" :then [{:set state.codeError :to short} {:stop true}]}
      {:run serverCall :with {:op checkCode :what verify}}]
    :checkCode [{:if "{{state.code != state.demoCode}}"
        :then [{:set state.codeError :to wrong} {:set state.code :to ""} {:focus "#twoStepCode"} {:stop true}]}
      {:if "{{state.codeMode == 'verify'}}" :then [{:run finishVerify} {:stop true}]}
      {:if "{{state.codeMode == 'email' && state.pending.twoStep && !computed.trustedHere}}"
        :then [{:set state.codeMode :to twoStep} {:run startTwoStep} {:stop true}]}
      {:run "{{state.codeMode == 'email' ? 'enter' : 'passTwoStep'}}"}]
    :resend [{:if "{{state.resendLeft > 0}}" :then [{:stop true}]}
      {:run serverCall :with {:op codeResent :what resend}}]
    :codeResent [{:set state.resendLeft :to 30}
      {:set state.code :to ""}
      {:set state.codeError :to ""}
      {:focus "#twoStepCode"}
      {:run notify :with {:message "{{t('codeResent', { email: state.pending.email })}}"}}]
    :countDown [{:if "{{state.resendLeft > 0}}" :then [{:increment state.resendLeft :by -1}]}]
    :createAccount [{:set state.submitted :to true}
      {:if "{{computed.createErrors > 0}}" :then [{:stop true}]}
      {:run serverCall :with {:op finishCreate :what create}}]
    :finishCreate [{:note "The account waits for its email to be verified"}
      {:set state.submitted :to false}
      {:set state.pending
        :to {:email "{{trim(state.email)}}"
          :name "{{trim(state.name)}}"
          :username "{{lower(trim(state.username))}}"
          :twoStep false
          :provider false
          :passkey false}}
      {:set state.codeMode :to verify}
      {:run startTwoStep}]
    :providerSignIn [{:if "{{state.busy != ''}}" :then [{:stop true}]}
      {:set state.formError :to ""}
      {:set state.providerName :to exampleId}
      {:set state.busy :to provider}
      {:signIn exampleId
        :onDone [{:run finishProvider :with {:email "{{auth.exampleId.account ?? ''}}" :name ""}}]
        :onError [{:run failProvider :with {:message "{{error.message}}"}}]}]
    :providerSignedIn [{:note "What Example ID answers after a sign-in, as an event: { account, name }. Scenarios play it."}
      {:run finishProvider :with {:email "{{event.account ?? ''}}" :name "{{event.name ?? ''}}"}}]
    :providerFailed [{:note "What Example ID answers when the sign-in fails, as an event: { message }. Scenarios play it."}
      {:run failProvider :with {:message "{{event.message ?? ''}}"}}]
    :finishProvider [{:note "with: email, name. Ignored once cancelled"}
      {:if "{{state.busy != 'provider'}}" :then [{:stop true}]}
      {:set state.busy :to ""}
      {:if "{{!isEmail(email)}}" :then [{:run failProvider :with {:message ""}} {:stop true}]}
      {:if "{{find(state.accounts, lower(it.email) == lower(email)) == null}}"
        :then [{:push state.accounts
            :value {:email "{{email}}"
              :name "{{name != '' ? name : split(email, '@')[0]}}"
              :password ""
              :twoStep false
              :provider true}}]}
      {:set state.submitted :to false}
      {:set state.pending
        :to "{{merge(find(state.accounts, lower(it.email) == lower(email)), { provider: true, twoStep: false })}}"}
      {:run enter}]
    :failProvider [{:note "with: message"}
      {:if "{{state.busy != 'provider'}}" :then [{:stop true}]}
      {:set state.busy :to ""}
      {:set state.providerMessage :to "{{message}}"}
      {:set state.formError :to provider}
      {:focus "#provider"}]
    :cancelProvider [{:set state.busy :to ""} {:focus "#textarea-placeholder"}]
    :signOut {:title "Sign out"
      :category Account
      :icon log-out
      :enablement "{{screen == 'signedIn'}}"
      :description "Ends the session on this device"
      :do [{:if "{{state.user.provider}}" :then [{:signOut exampleId}]}
        {:run endSession}
        {:run notify :with {:message "{{t('signedOut')}}"}}]}
    :endSession [{:if "{{state.user.email != ''}}" :then [{:run saveAccount}]}
      {:set state.confirmedLeft :to 0}
      {:set state.reauth.open :to false}
      {:set state.step :to email}
      {:set state.password :to ""}
      {:set state.confirm :to ""}
      {:set state.showPassword :to false}
      {:set state.submitted :to false}
      {:set state.change.open :to false}
      {:set state.user :to {:email "" :name "" :twoStep false :provider false}}
      {:navigate signInForm}]
    :tick [{:increment state.clock :by 1}
      {:if "{{some(state.emails, it.sent && !it.verified && state.clock - it.sentAt >= 5)}}"
        :then [{:note "The demo has no mail: a link sent is opened 5 seconds later"}
          {:run notify
            :with {:message "{{t('emailVerified', { email: find(state.emails, it.sent && !it.verified && state.clock - it.sentAt >= 5).address })}}"}}
          {:set state.emails
            :to "{{map(state.emails, it.sent && !it.verified && state.clock - it.sentAt >= 5 ? merge(it, { verified: true, sent: false }) : it)}}"}
          {:run logEvent :with {:kind emailVerified}}]}
      {:if "{{state.confirmedLeft > 0}}" :then [{:increment state.confirmedLeft :by -1}]}
      {:increment state.session.left :by -1}
      {:if "{{state.session.left <= 60 && !state.session.warned}}"
        :then [{:set state.session.warned :to true} {:run warnSession}]}
      {:if "{{state.session.left <= 0}}" :then [{:run expireSession}]}]
    :expireSession [{:if "{{state.user.provider}}" :then [{:signOut exampleId}]}
      {:run endSession}
      {:set state.toast.open :to false}
      {:set state.notice :to expired}
      {:focus "#signInAgain"}]
    :staySignedIn [{:set state.session :to {:left 300 :warned false}}
      {:run notify :with {:message "{{t('sessionExtended')}}"}}]
    :signInAgain [{:set state.step :to email}
      {:set state.notice :to ""}
      {:note "The first field of the screen: its email"}
      {:focus "#textarea-placeholder"}]
    :goBack [{:set state.step :to email}
      {:set state.submitted :to false}
      {:set state.codeError :to ""}
      {:navigate signInForm}]
    :toCreate [{:set state.submitted :to false}
      {:set state.formError :to ""}
      {:note "A new account starts with an empty password, not the one typed to sign in"}
      {:set state.password :to ""}
      {:set state.confirm :to ""}
      {:navigate createAccount}]
    :openForgot [{:set state.forgot :to {:open true :email "{{state.email}}" :error ""}} {:focus "#forgotEmail"}]
    :sendReset [{:if "{{!isEmail(trim(state.forgot.email))}}"
        :then [{:set state.forgot.error :to "{{t('emailOfAccount')}}"} {:stop true}]}
      {:set state.forgot.open :to false}
      {:set state.reset
        :to {:email "{{trim(state.forgot.email)}}"
          :code ""
          :password ""
          :confirm ""
          :submitted false
          :error ""
          :left 600}}
      {:navigate reset}
      {:focus "#resetCode"}
      {:run notify :with {:message "{{t('linkSent', { email: trim(state.forgot.email) })}}"}}]
    :closeForgot [{:set state.forgot.open :to false}]
    :setPassword [{:set state.reset.submitted :to true}
      {:set state.reset.error :to "{{length(state.reset.code) < 6 ? 'short' : ''}}"}
      {:if "{{state.reset.error != '' || computed.resetErrors > 0}}" :then [{:stop true}]}
      {:run serverCall :with {:op finishReset :what setPassword}}]
    :finishReset [{:if "{{state.reset.left <= 0}}"
        :then [{:set state.reset.error :to expired} {:focus "#resendReset"} {:stop true}]}
      {:if "{{state.reset.code != state.demoCode}}"
        :then [{:set state.reset.error :to wrong} {:set state.reset.code :to ""} {:focus "#resetCode"} {:stop true}]}
      {:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.reset.email) ? merge(it, { password: state.reset.password, passwordDays: 0 }) : it)}}"}
      {:set state.failures :to "{{filter(state.failures, it != lower(state.reset.email))}}"}
      {:set state.email :to "{{state.reset.email}}"}
      {:set state.password :to ""}
      {:set state.submitted :to false}
      {:set state.reset :to {:email "" :code "" :password "" :confirm "" :submitted false :error "" :left 0}}
      {:set state.step :to password}
      {:navigate signInForm}
      {:focus "#textarea-placeholder"}
      {:run notify :with {:message "{{t('passwordChanged')}}"}}]
    :saveProfile [{:set state.profile.submitted :to true}
      {:if "{{computed.profileErrors > 0}}" :then [{:stop true}]}
      {:run serverCall :with {:op finishSave :what save}}]
    :finishSave [{:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { name: trim(state.profile.name) }) : it)}}"}
      {:set state.user.name :to "{{trim(state.profile.name)}}"}
      {:set state.profile.submitted :to false}
      {:run logEvent :with {:kind profile}}
      {:run notify :with {:message "{{t('saved')}}"}}]
    :openChange [{:run stepUp :with {:then showChange}}]
    :changePassword [{:set state.change.submitted :to true}
      {:set state.change.open :to true}
      {:if "{{computed.changeError != ''}}" :then [{:focus "#newPassword"} {:stop true}]}
      {:run serverCall :with {:op finishChange :what password}}]
    :finishChange [{:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { password: state.change.password, passwordDays: 0 }) : it)}}"}
      {:set state.user.passwordDays :to 0}
      {:set state.change :to {:open false :password "" :confirm "" :submitted false}}
      {:focus "#changePassword"}
      {:run notify :with {:message "{{t('changed')}}"}}
      {:run logEvent :with {:kind password}}]
    :closeChange [{:set state.change.open :to false} {:focus "#changePassword"}]
    :setTwoStep [{:note "with: on. Turning it on first sets up the app and the backup codes"}
      {:if "{{on && !state.user.twoStep}}"
        :then [{:set state.setup :to {:open true :step scan :code "" :error "" :saved false}} {:stop true}]}
      {:if "{{!on && state.user.twoStep}}" :then [{:run stepUp :with {:then twoStepOff}} {:stop true}]}
      {:run applyTwoStep :with {:on "{{on}}"}}]
    :setLocale [{:note "with: code"} {:set state.locale :to "{{code}}"}]
    :closeShortcuts [{:set state.shortcutsOpen :to false}]
    :toggleTheme {:title "Switch light and dark"
      :category View
      :icon moon
      :do [{:set state.theme :to "{{state.theme == 'light' ? 'dark' : 'light'}}"}]}
    :nextLanguage {:title "Next language"
      :category View
      :icon languages
      :do [{:set state.locale :to "{{state.locale == 'en' ? 'it' : state.locale == 'it' ? 'ar' : 'en'}}"}]}
    :togglePassword {:title "Show or hide the password"
      :category Account
      :icon eye
      :do [{:set state.showPassword :to "{{!state.showPassword}}"}]}
    :openShortcuts {:title "Keyboard shortcuts" :category Help :icon keyboard :do [{:set state.shortcutsOpen :to true}]}
    :toggleSlow {:title "Slow network (demo)"
      :category Help
      :icon wifi
      :description "The fake server answers after 800 ms, to see the busy states"
      :do [{:set state.slow :to "{{!state.slow}}"}]}
    :passkeySignIn [{:note "The browser's passkey picker, faked: the accounts on this device that saved one"}
      {:if "{{state.busy != ''}}" :then [{:stop true}]}
      {:set state.formError :to ""}
      {:set state.passkeyOpen :to true}]
    :choosePasskey [{:note "with: email"}
      {:set state.passkeyOpen :to false}
      {:set state.pending :to "{{merge(find(state.accounts, it.email == email), { twoStep: false })}}"}
      {:run serverCall :with {:op passkeyDone :what passkey}}]
    :passkeyDone [{:note "A passkey is two factors already, so it skips two-step"}
      {:set state.submitted :to false}
      {:run enter}]
    :cancelPasskey [{:set state.passkeyOpen :to false}
      {:set state.formError :to passkeyCancelled}
      {:after 0 :do [{:focus "#passkey"}]}]
    :emailCodeSignIn [{:if "{{computed.account == null}}" :then [{:set state.formError :to noAccount} {:stop true}]}
      {:set state.formError :to ""}
      {:set state.submitted :to false}
      {:set state.pending :to "{{computed.account}}"}
      {:set state.codeMode :to email}
      {:run serverCall :with {:op startTwoStep :what emailCode}}]
    :otherProvider [{:note "with: name. A made-up provider that always says yes, after a moment"}
      {:if "{{state.busy != ''}}" :then [{:stop true}]}
      {:set state.formError :to ""}
      {:set state.providerName :to "{{name}}"}
      {:set state.busy :to provider}
      {:after 1200 :do [{:run finishProvider :with {:email "{{'you@' + name + '.example'}}" :name ""}}]}]
    :usernameChanged [{:note "Asks the demo server whether the username is free, once typing stops"}
      {:set state.spin :to 0}
      {:animate state.spin :to 360 :ms 600 :easing linear}
      {:debounce 600 :do [{:set state.usernameChecked :to "{{lower(trim(state.username))}}"}]}]
    :toggleTerms [{:set state.terms :to "{{!state.terms}}"}]
    :openInvite [{:set state.invite.open :to true} {:after 0 :do [{:focus "#invite #textarea-placeholder"}]}]
    :finishVerify [{:note "The email is verified: the account exists"}
      {:set state.accounts
        :to "{{concat(filter(state.accounts, lower(it.email) != lower(state.pending.email)), [merge(state.pending, { password: state.password })])}}"}
      {:set state.codeMode :to twoStep}
      {:run enter}
      {:run notify
        :with {:message "{{computed.inviteValid ? t('joinedTeam', { name: state.pending.name }) : t('welcome', { name: state.pending.name })}}"}}
      {:set state.username :to ""}
      {:set state.usernameChecked :to ""}
      {:set state.terms :to false}
      {:set state.invite :to {:open false :code ""}}]
    :codeBack [{:if "{{state.codeMode == 'verify'}}"
        :then [{:set state.codeMode :to twoStep} {:set state.codeError :to ""} {:navigate createAccount} {:stop true}]}
      {:run goBack}]
    :applyTwoStep [{:note "with: on. Saves the switch on the account"}
      {:set state.user.twoStep :to "{{on}}"}
      {:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { twoStep: on }) : it)}}"}
      {:run notify :with {:message "{{t('twoStepTurned', { on: on })}}"}}
      {:if "{{on}}" :then [{:run logEvent :with {:kind twoStepOn}}] :else [{:run logEvent :with {:kind twoStepOff}}]}]
    :setupCodeChanged [{:set state.setup.error :to ""}
      {:if "{{length(state.setup.code) == 6}}" :then [{:run setupNext}]}]
    :setupNext [{:if "{{state.setup.step == 'scan'}}"
        :then [{:if "{{length(state.setup.code) < 6}}"
            :then [{:set state.setup.open :to true} {:set state.setup.error :to short} {:stop true}]}
          {:if "{{state.setup.code != state.demoCode}}"
            :then [{:set state.setup.open :to true}
              {:set state.setup.error :to wrong}
              {:set state.setup.code :to ""}
              {:focus "#setupCode"}
              {:stop true}]}
          {:set state.setup.step :to codes}
          {:set state.setup.saved :to false}
          {:run freshCodes}
          {:after 0 :do [{:focus "#savedCodes"}]}
          {:stop true}]}
      {:if "{{!state.setup.saved}}"
        :then [{:set state.setup.open :to true} {:set state.setup.error :to notSaved} {:stop true}]}
      {:set state.setup.open :to false}
      {:if "{{state.user.twoStep}}" :then [{:run notify :with {:message "{{t('newCodesSaved')}}"}} {:stop true}]}
      {:run applyTwoStep :with {:on true}}]
    :freshCodes [{:note "One demo list for everyone: a real service makes random ones and keeps only their hashes"}
      {:set state.backupCodes :to "{{map(range(8), 'DEMO-' + (1001 + it))}}"}]
    :setupCancel [{:set state.setup.error :to ""}
      {:if "{{state.setup.step == 'codes' && !state.user.twoStep}}"
        :then [{:set state.setup.step :to scan}
          {:set state.setup.code :to ""}
          {:set state.setup.open :to true}
          {:after 0 :do [{:focus "#setupCode"}]}
          {:stop true}]}
      {:set state.setup.open :to false}]
    :newCodes [{:set state.setup :to {:open true :step codes :code "" :error "" :saved false}}
      {:run freshCodes}
      {:run logEvent :with {:kind newCodes}}]
    :toggleSaved [{:set state.setup.saved :to "{{!state.setup.saved}}"} {:set state.setup.error :to ""}]
    :useMode [{:note "with: mode (twoStep, sms or backup)"}
      {:set state.toast.open :to false}
      {:set state.codeMode :to "{{mode}}"}
      {:set state.code :to ""}
      {:set state.codeError :to ""}
      {:set state.backupCode :to ""}
      {:if "{{mode == 'sms'}}"
        :then [{:set state.resendLeft :to 30} {:run notify :with {:message "{{t('smsSent', { phone: state.phone })}}"}}]}
      {:after 0 :do [{:focus "{{mode == 'backup' ? '#backupField' : '#twoStepCode'}}"}]}]
    :checkBackup [{:if "{{!contains(state.backupCodes, upper(trim(state.backupCode)))}}"
        :then [{:set state.codeError :to wrongBackup} {:focus "#backupField"} {:stop true}]}
      {:set state.backupCodes :to "{{filter(state.backupCodes, it != upper(trim(state.backupCode)))}}"}
      {:set state.backupCode :to ""}
      {:run notify :with {:message "{{t('backupUsed', { n: length(state.backupCodes) })}}"}}
      {:run passTwoStep}]
    :passTwoStep [{:note "The second step passed: remembers this device when asked, then opens the session"}
      {:if "{{state.trust && !computed.trustedHere}}"
        :then [{:set state.trusted
            :to "{{concat(state.trusted, [{ email: state.pending.email, device: 'this', days: 30 }])}}"}]}
      {:set state.trust :to false}
      {:run enter}]
    :removeTrusted [{:note "with: device"}
      {:set state.trusted
        :to "{{filter(state.trusted, !(lower(it.email) == lower(state.user.email) && it.device == device))}}"}
      {:run notify :with {:message "{{t('deviceRemoved')}}"}}]
    :resetTick [{:note "Each second on the reset screen: the emailed code lasts 10 minutes"}
      {:if "{{state.reset.left > 0}}"
        :then [{:increment state.reset.left :by -1}
          {:if "{{state.reset.left <= 0}}" :then [{:set state.reset.error :to expired}]}]}]
    :resendReset [{:set state.reset.left :to 600}
      {:set state.reset.code :to ""}
      {:set state.reset.error :to ""}
      {:focus "#resetCode"}
      {:run notify :with {:message "{{t('linkSent', { email: state.reset.email })}}"}}]
    :showChange [{:set state.change :to {:open true :password "" :confirm "" :submitted false}} {:focus "#newPassword"}]
    :stepUp [{:note "with: then (the action to run). Sensitive changes ask for the password again, unless it was given in the last 2 minutes"}
      {:if "{{state.confirmedLeft > 0 || state.user.provider}}" :then [{:run "{{then}}"} {:stop true}]}
      {:set state.reauth :to {:open true :password "" :error "" :then "{{then}}"}}
      {:focus "#reauthPassword"}]
    :confirmReauth [{:set state.reauth.open :to true}
      {:if "{{state.reauth.password == ''}}"
        :then [{:set state.reauth.error :to empty} {:focus "#reauthPassword"} {:stop true}]}
      {:if "{{find(state.accounts, lower(it.email) == lower(state.user.email)).password != '' && find(state.accounts, lower(it.email) == lower(state.user.email)).password != state.reauth.password}}"
        :then [{:set state.reauth.error :to wrong}
          {:set state.reauth.password :to ""}
          {:focus "#reauthPassword"}
          {:stop true}]}
      {:set state.confirmedLeft :to 120}
      {:set state.reauth.open :to false}
      {:set state.reauth.password :to ""}
      {:run "{{state.reauth.then}}"}]
    :closeReauth [{:set state.reauth.open :to false} {:set state.reauth.password :to ""}]
    :twoStepOff [{:run applyTwoStep :with {:on false}}]
    :seedAccount [{:note "The signed-in account's emails, passkeys, devices, connections and log: kept on the account between sessions (endSession writes them back); the demo accounts start with a few made-up ones"}
      {:set state.section :to profile}
      {:set state.clock :to 0}
      {:set state.photoOpen :to false}
      {:set state.newEmail :to {:open false :address "" :submitted false}}
      {:set state.emails
        :to "{{state.user.emails ?? concat([{ address: state.user.email, primary: true, verified: true }], state.user.demo ? [{ address: state.user.username + '.home@example.net', primary: false, verified: true }, { address: state.user.username + '@work.example.org', primary: false, verified: false }] : [])}}"}
      {:set state.myPasskeys
        :to "{{state.user.passkeys ?? (state.user.passkey ? [{ id: 'pk1', device: 'this', added: 12, used: 0 }] : [])}}"}
      {:set state.sessions
        :to "{{state.user.sessions ?? concat([{ id: 'here', device: 'this', app: 'desktop', place: 'Lakeside', days: 0 }], state.user.demo ? [{ id: 'phone', device: 'phone', app: 'mobile', place: 'Lakeside', days: 1 }, { id: 'laptop', device: 'laptop', app: 'browser', place: 'Northport', days: 6 }] : [])}}"}
      {:set state.connected
        :to "{{state.user.connected ?? (state.user.demo ? [{ provider: 'quillo', account: state.user.username + '@quillo.example' }] : [])}}"}
      {:set state.log
        :to "{{slice(concat([{ kind: 'signIn', at: 0, days: 0, device: 'this', place: 'Lakeside' }], state.user.log ?? (state.user.demo ? [{ kind: 'signIn', at: 0, days: 1, device: 'phone', place: 'Lakeside' }, { kind: 'signInBlocked', at: 0, days: 3, device: 'laptop', place: 'Eastvale' }, { kind: 'password', at: 0, days: state.user.passwordDays ?? 30, device: 'laptop', place: 'Northport' }] : [])), 0, 50)}}"}]
    :saveAccount [{:note "Writes the session's lists back on the account, so the next sign-in finds them (times kept relative)"}
      {:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { emails: state.emails, passkeys: state.myPasskeys, passkey: length(state.myPasskeys) > 0, sessions: state.sessions, connected: state.connected, log: map(state.log, merge(it, { at: it.at - state.clock })) }) : it)}}"}]
    :logEvent [{:note "with: kind. The newest first, at most 50"}
      {:set state.log
        :to "{{slice(concat([{ kind: kind, at: state.clock, days: 0, device: 'this', place: 'Lakeside' }], state.log), 0, 50)}}"}]
    :setSection [{:note "with: name"}
      {:set state.section :to "{{name}}"}
      {:set state.newEmail.open :to false}
      {:set state.photoOpen :to false}]
    :setAvatar [{:note "with: color (0 for the initials on the accent)"}
      {:set state.user.avatar :to "{{color}}"}
      {:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { avatar: color }) : it)}}"}
      {:set state.photoOpen :to false}
      {:focus "#changePhoto"}
      {:run notify :with {:message "{{t('photoSaved')}}"}}
      {:run logEvent :with {:kind profile}}]
    :openAddEmail [{:set state.newEmail :to {:open true :address "" :submitted false}}
      {:focus "#newEmailField #textarea-placeholder"}]
    :closeAddEmail [{:set state.newEmail.open :to false} {:focus "#addEmail"}]
    :addEmail [{:set state.newEmail.submitted :to true}
      {:if "{{computed.newEmailError != ''}}" :then [{:focus "#newEmailField #textarea-placeholder"} {:stop true}]}
      {:run serverCall :with {:op finishAddEmail :what verifyEmail}}]
    :finishAddEmail [{:push state.emails
        :value {:address "{{trim(state.newEmail.address)}}"
          :primary false
          :verified false
          :sent true
          :sentAt "{{state.clock}}"}}
      {:run notify :with {:message "{{t('emailAdded', { email: trim(state.newEmail.address) })}}"}}
      {:set state.newEmail :to {:open false :address "" :submitted false}}
      {:focus "#addEmail"}
      {:run logEvent :with {:kind emailAdded}}]
    :verifyEmail [{:note "with: address"}
      {:set state.target :to "{{address}}"}
      {:run serverCall :with {:op sendVerification :what verifyEmail}}]
    :sendVerification [{:set state.emails
        :to "{{map(state.emails, it.address == state.target ? merge(it, { sent: true, sentAt: state.clock }) : it)}}"}
      {:run notify :with {:message "{{t('emailAdded', { email: state.target })}}"}}]
    :removeEmail [{:note "with: address"}
      {:set state.target :to "{{address}}"}
      {:run stepUp :with {:then finishRemoveEmail}}]
    :finishRemoveEmail [{:set state.emails :to "{{filter(state.emails, it.address != state.target || it.primary)}}"}
      {:run notify :with {:message "{{t('emailRemoved', { email: state.target })}}"}}
      {:focus "#addEmail"}
      {:run logEvent :with {:kind emailRemoved}}]
    :makePrimary [{:note "with: address (a verified one)"}
      {:set state.target :to "{{address}}"}
      {:run stepUp :with {:then finishPrimary}}]
    :finishPrimary [{:note "The account, its trusted devices and the sign-in form follow the new address"}
      {:set state.emails :to "{{map(state.emails, merge(it, { primary: it.address == state.target }))}}"}
      {:set state.accounts
        :to "{{map(state.accounts, lower(it.email) == lower(state.user.email) ? merge(it, { email: state.target }) : it)}}"}
      {:set state.trusted
        :to "{{map(state.trusted, lower(it.email) == lower(state.user.email) ? merge(it, { email: state.target }) : it)}}"}
      {:set state.user.email :to "{{state.target}}"}
      {:set state.email :to "{{state.target}}"}
      {:set state.profile.email :to "{{state.target}}"}
      {:run notify :with {:message "{{t('primaryChanged', { email: state.target })}}"}}
      {:run logEvent :with {:kind primaryChanged}}]
    :addPasskey [{:run stepUp :with {:then createPasskey}}]
    :createPasskey [{:note "The device's prompt is simulated: a real one creates the key pair and sends only the public key"}
      {:run serverCall :with {:op finishPasskey :what passkey}}]
    :finishPasskey [{:push state.myPasskeys
        :value (node #"{{'pk' + (length(state.myPasskeys) + 1) + '-' + state.clock}}"
          :device "{{some(state.myPasskeys, it.device == 'this') ? 'key' : 'this'}}"
          :added 0
          :used -1)}
      {:run notify :with {:message "{{t('passkeyAdded')}}"}}
      {:focus "#addPasskey"}
      {:run logEvent :with {:kind passkeyAdded}}]
    :removePasskey [{:note "with: id"} {:set state.target :to "{{id}}"} {:run stepUp :with {:then finishRemovePasskey}}]
    :finishRemovePasskey [{:set state.myPasskeys :to "{{filter(state.myPasskeys, it.id != state.target)}}"}
      {:run notify :with {:message "{{t('passkeyRemoved')}}"}}
      {:run logEvent :with {:kind passkeyRemoved}}]
    :signOutSession [{:note "with: id (another device's session)"}
      {:run notify
        :with {:message "{{t('signedOutDevice', { device: t('device_' + find(state.sessions, it.id == id).device) })}}"}}
      {:set state.sessions :to "{{filter(state.sessions, it.id != id || it.id == 'here')}}"}
      {:run logEvent :with {:kind signedOutDevice}}]
    :signOutOthers [{:run stepUp :with {:then endOtherSessions}}]
    :endOtherSessions [{:run serverCall :with {:op finishSignOutOthers :what devices}}]
    :finishSignOutOthers [{:set state.sessions :to "{{filter(state.sessions, it.id == 'here')}}"}
      {:run notify :with {:message "{{t('signedOutOthers')}}"}}
      {:run logEvent :with {:kind signedOutOthers}}]
    :connectProvider [{:note "with: provider. The provider's consent page is simulated"}
      {:set state.target :to "{{provider}}"}
      {:run serverCall :with {:op finishConnect :what connect}}]
    :finishConnect [{:push state.connected
        :value {:provider "{{state.target}}"
          :account "{{(state.user.username || split(state.user.email, '@')[0]) + '@' + state.target + '.example'}}"}}
      {:run notify :with {:message "{{t('connectedNow', { provider: t('provider_' + state.target) })}}"}}
      {:run logEvent :with {:kind connected}}]
    :disconnectProvider [{:note "with: provider"}
      {:set state.target :to "{{provider}}"}
      {:run stepUp :with {:then finishDisconnect}}]
    :finishDisconnect [{:set state.connected :to "{{filter(state.connected, it.provider != state.target)}}"}
      {:run notify :with {:message "{{t('disconnectedNow', { provider: t('provider_' + state.target) })}}"}}
      {:run logEvent :with {:kind disconnected}}]
    :openDelete [{:set state.deleting :to {:open true :typed "" :error ""}} {:focus "#deleteConfirm"}]
    :closeDelete [{:set state.deleting.open :to false} {:focus "#deleteAccount"}]
    :confirmDelete [{:set state.deleting.open :to true}
      {:if "{{trim(state.deleting.typed) == ''}}"
        :then [{:set state.deleting.error :to empty} {:focus "#deleteConfirm"} {:stop true}]}
      {:if "{{upper(trim(state.deleting.typed)) != upper(computed.deleteWord)}}"
        :then [{:set state.deleting.error :to wrong} {:focus "#deleteConfirm"} {:stop true}]}
      {:set state.deleting.open :to false}
      {:run stepUp :with {:then deleteAccount}}]
    :deleteAccount [{:run serverCall :with {:op finishDelete :what delete}}]
    :finishDelete [{:note "The account, its trusted devices and its failed attempts go; nothing is written back"}
      {:set state.accounts :to "{{filter(state.accounts, lower(it.email) != lower(state.user.email))}}"}
      {:set state.trusted :to "{{filter(state.trusted, lower(it.email) != lower(state.user.email))}}"}
      {:set state.user.email :to ""}
      {:set state.email :to ""}
      {:run endSession}
      {:run notify :with {:message "{{t('deleted')}}"}}]}
  :keys {:escape {:when "{{screen == 'createAccount' || screen == 'twoStep' || screen == 'reset'}}" :do goBack}
    :"mod+j" toggleTheme
    :"mod+l" nextLanguage
    :"mod+shift+h" togglePassword
    :f1 openShortcuts
    :"mod+shift+n" toggleSlow
    :"mod+shift+q" signOut}
  :screens {:signInForm (node #panel-sign-in :style screen
      (node #card
        :style card
        :width "{{min(420, window.width - 32)}}"
        :children [(node #card-top :height 36 :color transparent)
          (node #stack-form :style form
            (node #logo :style logo :when "{{window.height >= 700}}")
            (node #spacer-logo :style spacer :height 4 :when "{{window.height >= 700}}")
            (node #label-welcome-back :style title :text "{{t('welcomeBack')}}" :role heading)
            (node #label-sign-in-to :style caption :text "{{t('signInTo')}}")
            (node #spacer-head :style spacer)
            (node #sessionNotice
              :when "{{state.notice == 'expired'}}"
              :anchors {:left 0 :right 10}
              :height 58
              :radius 8
              :color @color.notice
              :borderWidth 1
              :borderColor @color.noticeBorder
              :role alert
              :live assertive
              :label "{{t('sessionExpired')}}"
              (node #label-session-expired
                :type label
                :font small
                :icon clock-3
                :text "{{t('sessionExpired')}}"
                :textColor @color.text
                :anchors {:left 12 :right 12 :top 8}
                :height 18)
              (node #signInAgain
                :style smallLink
                :text "{{t('signInAgain')}}"
                :anchors {:left 12 :top 32}
                :height 18
                :onClick signInAgain))
            (node #formError
              :when "{{state.formError != ''}}"
              :style alert
              :height "{{state.formError == 'locked' ? 56 : 48}}"
              :text "{{t('serverError', { kind: state.formError, message: state.providerMessage, provider: computed.providerTitle })}}")
            (node #unlock
              :when "{{state.formError == 'locked'}}"
              :style secondary
              :icon key-round
              :text "{{t('unlock')}}"
              :onClick openForgot)
            (node #label-email :when "{{state.step == 'email'}}" :style fieldLabel :text "{{t('email')}}")
            (node #email
              :when "{{state.step == 'email'}}"
              :use checkedField
              :with {:submit signIn
                :bind state.email
                :content "{{state.email}}"
                :key email
                :icon mail
                :name "{{t('email')}}"
                :placeholder "{{t('emailPlaceholder')}}"
                :valid "{{computed.emailValid}}"
                :message "{{t('enterEmail')}}"
                :submitted "{{state.submitted}}"
                :maxLength 254})
            (node #accountChip
              :when "{{state.step == 'password'}}"
              :type row
              :height 36
              :gap 10
              :alignItems center
              :anchors {:left 0 :right 10}
              :radius 18
              :color @color.input
              :borderWidth 1
              :borderColor @color.border
              :label "{{t('signingInAs', { email: trim(state.email) })}}"
              (node #chip-avatar :width 32 :height 36 :color transparent
                (node #chip-initial
                  :type label
                  :width 28
                  :height 28
                  :radius 14
                  :align center
                  :color @color.accent
                  :textColor @color.accentText
                  :font small
                  :text "{{upper(slice(trim(state.email), 0, 1))}}"
                  :anchors {:left 4 :top 4}))
              (node #label-chip-email
                :type label
                :font small
                :textColor @color.text
                :height 20
                :fitText true
                :text "{{trim(state.email)}}")
              (node #spacer-chip :type row :grow 1 :height 1)
              (node #changeEmail :style smallLink :height 20 :text "{{t('notYou')}}" :onClick changeEmail)
              (node #spacer-chip-end :type row :width 6 :height 1))
            (node #row-password
              :when "{{state.step == 'password'}}"
              :type row
              :height 18
              :justify space-between
              :anchors {:left 0 :right 10}
              (node #label-password :style fieldLabel :anchors {} :fitText true :text "{{t('password')}}")
              (node #forgot :style smallLink :text "{{t('forgot')}}" :onClick openForgot))
            (node #password
              :when "{{state.step == 'password'}}"
              :use checkedField
              :with {:submit signIn
                :bind state.password
                :content "{{state.password}}"
                :key password
                :icon lock
                :name "{{t('password')}}"
                :placeholder "{{t('yourPassword')}}"
                :valid "{{computed.passwordValid}}"
                :message "{{t('min8')}}"
                :submitted "{{state.submitted}}"
                :secret true
                :eye true
                :maxLength 128})
            (node #capsLockWarning
              :when "{{state.step == 'password' && window.capsLock}}"
              :type label
              :font small
              :icon arrow-big-up
              :text "{{t('capsLock')}}"
              :textColor @color.warning
              :height 18
              :anchors {:left 2 :right 10}
              :role status
              :live polite)
            (node #row-remember :when "{{state.step == 'password'}}" :style checkRow
              (node #remember
                :style "{{state.remember ? 'checkOn' : 'checkOff'}}"
                :label "{{t('remember')}}"
                :onClick {:set state.remember :to "{{!state.remember}}"})
              (node #label-remember
                :type label
                :font small
                :text "{{t('rememberText')}}"
                :fitText true
                :textColor @color.muted
                :cursor hand
                :onClick {:set state.remember :to "{{!state.remember}}"}))
            (node #spacer-button :style spacer :height 2)
            (node #signIn
              :style primary
              :icon "{{state.busy == 'signIn' || state.busy == 'lookup' ? 'loader' : state.step == 'email' ? 'arrow-right' : 'log-in'}}"
              :text "{{state.busy == 'signIn' || state.busy == 'lookup' ? computed.busyText : state.step == 'email' ? t('continue') : t('signInButton')}}"
              :disabled "{{state.busy != ''}}"
              :onClick signIn)
            (node #emailCode
              :when "{{state.step == 'password'}}"
              :style link
              :font small
              :text "{{state.busy == 'emailCode' ? t('sending') : t('emailCode')}}"
              :disabled "{{state.busy != ''}}"
              :onClick emailCodeSignIn)
            (node #row-or
              :type row
              :height 18
              :gap 10
              :alignItems center
              :anchors {:left 0 :right 10}
              :children [(node #line-or :grow 1 :height 1 :color @color.border)
                (node #label-or
                  :type label
                  :font small
                  :fitText true
                  :height 18
                  :textColor @color.muted
                  :text "{{t('or')}}")
                (node #line-or-2 :grow 1 :height 1 :color @color.border)]
              :when "{{state.step == 'email'}}")
            (node #passkey
              :when "{{state.step == 'email'}}"
              :style secondary
              :icon "{{state.busy == 'passkey' ? 'loader' : 'fingerprint-pattern'}}"
              :text "{{t('passkey')}}"
              :disabled "{{state.busy != ''}}"
              :onClick passkeySignIn)
            (node #provider
              :style secondary
              :icon "{{state.busy == 'provider' && state.providerName == 'exampleId' ? 'loader' : 'key-round'}}"
              :text "{{state.busy == 'provider' && state.providerName == 'exampleId' ? t('providerWaiting') : t('provider')}}"
              :disabled "{{state.busy != ''}}"
              :onClick providerSignIn
              :when "{{state.step == 'email'}}")
            (node #row-providers
              :when "{{state.step == 'email'}}"
              :type row
              :height 40
              :gap 8
              :anchors {:left 0 :right 10}
              (node #provider-wrenfield
                :style secondary
                :grow 1
                :height 40
                :anchors {}
                :font small
                :icon "{{state.busy == 'provider' && state.providerName == 'wrenfield' ? 'loader' : 'bird'}}"
                :text "{{t('provider_wrenfield')}}"
                :description "{{t('otherProviders')}}"
                :disabled "{{state.busy != ''}}"
                :onClick {:run otherProvider :with {:name wrenfield}})
              (node #provider-mossbank
                :style secondary
                :grow 1
                :height 40
                :anchors {}
                :font small
                :icon "{{state.busy == 'provider' && state.providerName == 'mossbank' ? 'loader' : 'leaf'}}"
                :text "{{t('provider_mossbank')}}"
                :description "{{t('otherProviders')}}"
                :disabled "{{state.busy != ''}}"
                :onClick {:run otherProvider :with {:name mossbank}})
              (node #provider-quillo
                :style secondary
                :grow 1
                :height 40
                :anchors {}
                :font small
                :icon "{{state.busy == 'provider' && state.providerName == 'quillo' ? 'loader' : 'feather'}}"
                :text "{{t('provider_quillo')}}"
                :description "{{t('otherProviders')}}"
                :disabled "{{state.busy != ''}}"
                :onClick {:run otherProvider :with {:name quillo}}))
            (node #cancelProvider
              :when "{{state.busy == 'provider'}}"
              :style link
              :font small
              :text "{{t('cancelProvider')}}"
              :onClick cancelProvider)
            (node #toCreate :style link :text "{{t('toCreate')}}" :onClick toCreate))
          (node #card-end :height 36 :color transparent)]
        :maxHeight "{{window.height - 116}}"))
    :createAccount (node #panel-create-account :style screen
      (node #panel-card
        :style card
        :width "{{min(420, window.width - 32)}}"
        :children [(node #panel-card-top :height 36 :color transparent)
          (node #stack-form-2 :style form
            (node #label-create-an-account :style title :text "{{t('createAnAccount')}}" :role heading)
            (node #label-create-caption :style caption :text "{{t('createCaption')}}")
            (node #label-name :style fieldLabel :text "{{t('name')}}")
            (node #checked-field-form
              :use checkedField
              :with {:submit createAccount
                :bind state.name
                :content "{{state.name}}"
                :key name
                :icon user
                :name "{{t('name')}}"
                :placeholder "{{t('namePlaceholder')}}"
                :valid "{{length(trim(state.name)) > 0}}"
                :message "{{t('enterName')}}"
                :submitted "{{state.submitted}}"
                :maxLength 60})
            (node #label-email-2 :style fieldLabel :text "{{t('email')}}")
            (node #checked-field-form-2
              :use checkedField
              :with {:submit createAccount
                :bind state.email
                :content "{{state.email}}"
                :key email
                :icon mail
                :name "{{t('email')}}"
                :placeholder "{{t('emailPlaceholder')}}"
                :valid "{{computed.emailValid}}"
                :message "{{t('enterEmail')}}"
                :submitted "{{state.submitted}}"
                :maxLength 254})
            (node #label-username :style fieldLabel :text "{{t('username')}}")
            (node #username
              :use checkedField
              :with {:submit createAccount
                :change usernameChanged
                :bind state.username
                :content "{{state.username}}"
                :key username
                :icon at-sign
                :name "{{t('username')}}"
                :placeholder "{{t('usernamePlaceholder')}}"
                :valid "{{computed.usernameStatus == 'available'}}"
                :message "{{t('usernameError', { kind: computed.usernameStatus })}}"
                :submitted "{{state.submitted}}"
                :maxLength 20})
            (node #usernameStatus
              :when "{{computed.usernameStatus != '' && !(state.submitted && computed.usernameStatus != 'available')}}"
              :type row
              :height 18
              :gap 6
              :alignItems center
              :anchors {:left 2 :right 10}
              :color transparent
              :role status
              :live polite
              :label "{{t('usernameStatus', { kind: computed.usernameStatus, name: lower(trim(state.username)) })}}"
              (node #username-status-icon
                :type label
                :font iconsSmall
                :text ""
                :width 14
                :height 14
                :align center
                :color transparent
                :icon "{{computed.usernameStatus == 'checking' ? 'loader' : computed.usernameStatus == 'available' ? 'check-circle' : 'alert-circle'}}"
                :rotate "{{computed.usernameStatus == 'checking' ? state.spin : 0}}"
                :textColor "{{computed.usernameStatus == 'checking' ? '@color.muted' : computed.usernameStatus == 'available' ? '@color.strong' : '@color.error'}}")
              (node #username-status-text
                :type label
                :font small
                :fitText true
                :height 18
                :color transparent
                :text "{{t('usernameStatus', { kind: computed.usernameStatus, name: lower(trim(state.username)) })}}"
                :textColor "{{computed.usernameStatus == 'checking' ? '@color.muted' : computed.usernameStatus == 'available' ? '@color.strong' : '@color.error'}}"))
            (node #label-password-2 :style fieldLabel :text "{{t('password')}}")
            (node #checked-field-form-3
              :use checkedField
              :with {:submit createAccount
                :bind state.password
                :content "{{state.password}}"
                :key password
                :icon lock
                :name "{{t('password')}}"
                :placeholder "{{t('passwordPlaceholder')}}"
                :valid "{{computed.newPasswordValid}}"
                :message "{{computed.newPasswordMessage}}"
                :submitted "{{state.submitted}}"
                :secret true
                :eye true
                :maxLength 128})
            (node #strength-create :use strengthMeter :with {:password "{{state.password}}"})
            (node #rules-create :use passwordRules :with {:password "{{state.password}}"})
            (node #label-confirm :when "{{!state.showPassword}}" :style fieldLabel :text "{{t('confirm')}}")
            (node #confirm-password
              :when "{{!state.showPassword}}"
              :use checkedField
              :with {:submit createAccount
                :bind state.confirm
                :content "{{state.confirm}}"
                :key confirm
                :icon lock-keyhole
                :name "{{t('confirm')}}"
                :placeholder "{{t('confirmPlaceholder')}}"
                :valid "{{computed.confirmValid}}"
                :message "{{t('mismatch')}}"
                :submitted "{{state.submitted}}"
                :secret true
                :maxLength 128})
            (node #row-terms :style checkRow
              (node #terms
                :style "{{state.terms ? 'checkOn' : 'checkOff'}}"
                :label "{{t('terms')}}"
                :description "{{state.submitted && !state.terms ? t('termsError') : ''}}"
                :onClick toggleTerms)
              (node #label-terms
                :type label
                :font small
                :text "{{t('terms')}}"
                :fitText true
                :textColor @color.text
                :cursor hand
                :onClick toggleTerms))
            (node #termsError :when "{{state.submitted && !state.terms}}" :style inlineError :text "{{t('termsError')}}")
            (node #haveInvite
              :when "{{!state.invite.open}}"
              :style smallLink
              :text "{{t('haveInvite')}}"
              :onClick openInvite)
            (node #label-invite :when "{{state.invite.open}}" :style fieldLabel :text "{{t('inviteCode')}}")
            (node #invite
              :when "{{state.invite.open}}"
              :use checkedField
              :with {:submit createAccount
                :bind state.invite.code
                :content "{{state.invite.code}}"
                :key invite
                :icon ticket
                :name "{{t('inviteCode')}}"
                :placeholder "{{t('invitePlaceholder')}}"
                :valid "{{length(trim(state.invite.code)) == 0 || computed.inviteValid}}"
                :message "{{t('inviteUnknown')}}"
                :submitted "{{state.submitted}}"
                :maxLength 40})
            (node #inviteNotice
              :when "{{state.invite.open && computed.inviteValid}}"
              :type label
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
              :text "{{t('inviteValid')}}")
            (node #label-create-account
              :style primary
              :icon "{{state.busy == 'create' ? 'loader' : 'user-plus'}}"
              :text "{{state.busy == 'create' ? computed.busyText : t('createAccount')}}"
              :disabled "{{state.busy != ''}}"
              :onClick createAccount)
            (node #label-back-to-sign :style link :text "{{t('backToSignIn')}}" :onClick goBack))
          (node #panel-card-end :height 36 :color transparent)]
        :maxHeight "{{window.height - 116}}"))
    :twoStep (node #panel-two-step :style screen :every {:ms 1000 :do countDown}
      (node #card-two-step
        :style card
        :width "{{min(420, window.width - 32)}}"
        :children [(node #card-two-step-top :height 36 :color transparent)
          (node #stack-two-step :style form
            (node #label-two-step
              :style title
              :text "{{t(state.codeMode == 'verify' ? 'verifyEmailTitle' : state.codeMode == 'email' ? 'emailCodeTitle' : state.codeMode == 'sms' ? 'smsTitle' : state.codeMode == 'backup' ? 'backupTitle' : 'twoStepTitle')}}"
              :role heading)
            (node #label-two-step-caption
              :style caption
              :wrap true
              :height 40
              :text "{{t(state.codeMode == 'verify' ? 'verifyEmailCaption' : state.codeMode == 'email' ? 'emailCodeCaption' : state.codeMode == 'sms' ? 'smsCaption' : state.codeMode == 'backup' ? 'backupCaption' : 'twoStepCaption', { email: state.pending.email, phone: state.phone })}}")
            (node #label-code :style fieldLabel :text "{{t(state.codeMode == 'backup' ? 'backupCode' : 'code')}}")
            (node #twoStepCode
              :style code
              :dir ltr
              :bind state.code
              :label "{{t('code')}}"
              :description "{{state.codeError == '' ? '' : t('codeError', { kind: state.codeError })}}"
              :cellBorderColor "{{state.codeError == '' ? '@color.border' : '@color.error'}}"
              :onChange codeChanged
              :onSubmit verify
              :when "{{state.codeMode != 'backup'}}")
            (node #backupField
              :when "{{state.codeMode == 'backup'}}"
              :style field
              :dir ltr
              :height 38
              :anchors {:left 0 :right 10}
              :borderColor "{{state.codeError == '' ? '@color.border' : '@color.error'}}"
              :placeholder DEMO-1001
              :label "{{t('backupCode')}}"
              :description "{{state.codeError == '' ? '' : t('codeError', { kind: state.codeError })}}"
              :maxLength 9
              :bind state.backupCode
              :onChange {:set state.codeError :to ""}
              :onSubmit verify)
            (node #codeError
              :when "{{state.codeError != ''}}"
              :style inlineError
              :text "{{t('codeError', { kind: state.codeError })}}")
            (node #demoCode
              :style hint
              :icon info
              :text "{{state.codeMode == 'backup' ? t('demoBackup', { code: state.backupCodes[0] ?? '—' }) : t('demoCode', { code: state.demoCode })}}")
            (node #row-trust :when "{{state.codeMode != 'verify' && state.codeMode != 'email'}}" :style checkRow
              (node #trust
                :style "{{state.trust ? 'checkOn' : 'checkOff'}}"
                :label "{{t('trustDevice')}}"
                :checked "{{state.trust}}"
                :onClick {:set state.trust :to "{{!state.trust}}"}
                :keys {:space {:set state.trust :to "{{!state.trust}}"}})
              (node #label-trust
                :type label
                :font small
                :text "{{t('trustDevice')}}"
                :fitText true
                :textColor @color.muted
                :cursor hand
                :onClick {:set state.trust :to "{{!state.trust}}"}))
            (node #verify
              :style primary
              :icon "{{state.busy == 'verify' ? 'loader' : 'shield-check'}}"
              :text "{{state.busy == 'verify' ? computed.busyText : t('verify')}}"
              :disabled "{{state.busy != ''}}"
              :onClick verify)
            (node #resend
              :style link
              :text "{{state.busy == 'resend' ? computed.busyText : state.resendLeft > 0 ? t('resendIn', { n: state.resendLeft }) : t('resend')}}"
              :textColor "{{state.resendLeft > 0 ? '@color.muted' : '@color.accent'}}"
              :disabled "{{state.resendLeft > 0 || state.busy != ''}}"
              :onClick resend
              :when "{{state.codeMode != 'backup'}}")
            (node #row-other-ways
              :when "{{state.codeMode != 'verify' && state.codeMode != 'email'}}"
              :type row
              :height 20
              :gap 16
              :justify center
              :alignItems center
              :anchors {:left 0 :right 10}
              (node #useApp
                :when "{{state.codeMode != 'twoStep'}}"
                :style smallLink
                :text "{{t('useApp')}}"
                :onClick {:run useMode :with {:mode twoStep}})
              (node #textMe
                :when "{{state.codeMode != 'sms'}}"
                :style smallLink
                :text "{{t('textMe')}}"
                :onClick {:run useMode :with {:mode sms}})
              (node #useBackup
                :when "{{state.codeMode != 'backup'}}"
                :style smallLink
                :text "{{t('useBackup')}}"
                :onClick {:run useMode :with {:mode backup}}))
            (node #twoStepBack
              :style link
              :text "{{state.codeMode == 'verify' ? t('backToCreate') : t('backToSignIn')}}"
              :onClick codeBack))
          (node #card-two-step-end :height 36 :color transparent)]
        :maxHeight "{{window.height - 116}}"))
    :reset (node #panel-reset
      :style screen
      :children [(node #card-reset
          :style card
          :width "{{min(420, window.width - 32)}}"
          :children [(node #card-reset-top :height 36 :color transparent)
            (node #stack-reset :style form
              (node #label-reset-title :style title :text "{{t('newPasswordTitle')}}" :role heading)
              (node #label-reset-caption
                :style caption
                :wrap true
                :height 40
                :text "{{t('newPasswordCaption', { email: state.reset.email })}}")
              (node #label-reset-code :style fieldLabel :text "{{t('code')}}")
              (node #resetCode
                :style code
                :dir ltr
                :bind state.reset.code
                :label "{{t('code')}}"
                :description "{{state.reset.error == '' ? '' : t('codeError', { kind: state.reset.error })}}"
                :cellBorderColor "{{state.reset.error == '' ? '@color.border' : '@color.error'}}"
                :onChange {:set state.reset.error :to "{{state.reset.left <= 0 ? 'expired' : ''}}"}
                :onSubmit {:focus "#textarea-placeholder"})
              (node #resetCodeError
                :when "{{state.reset.error != ''}}"
                :style inlineError
                :text "{{t('codeError', { kind: state.reset.error })}}")
              (node #resendReset
                :when "{{state.reset.left <= 0}}"
                :style smallLink
                :icon refresh-cw
                :text "{{t('resend')}}"
                :onClick resendReset
                :height 18)
              (node #resetExpiry
                :when "{{state.reset.left > 0}}"
                :style hint
                :icon timer
                :text "{{t('codeExpires', { time: computed.resetTime })}}")
              (node #demoCode-reset :style hint :icon info :text "{{t('demoCode', { code: state.demoCode })}}")
              (node #label-new-password :style fieldLabel :text "{{t('newPassword')}}")
              (node #reset-password
                :use checkedField
                :with {:submit setPassword
                  :bind state.reset.password
                  :content "{{state.reset.password}}"
                  :key resetPassword
                  :icon lock
                  :name "{{t('newPassword')}}"
                  :placeholder "{{t('passwordPlaceholder')}}"
                  :valid "{{computed.resetValid}}"
                  :message "{{length(state.reset.password) >= 8 ? t('stronger') : t('min8')}}"
                  :submitted "{{state.reset.submitted}}"
                  :secret true
                  :eye true
                  :maxLength 128})
              (node #strength-reset :use strengthMeter :with {:password "{{state.reset.password}}"})
              (node #label-reset-confirm :when "{{!state.showPassword}}" :style fieldLabel :text "{{t('confirm')}}")
              (node #reset-confirm
                :when "{{!state.showPassword}}"
                :use checkedField
                :with {:submit setPassword
                  :bind state.reset.confirm
                  :content "{{state.reset.confirm}}"
                  :key resetConfirm
                  :icon lock-keyhole
                  :name "{{t('confirm')}}"
                  :placeholder "{{t('confirmPlaceholder')}}"
                  :valid "{{computed.resetConfirmValid}}"
                  :message "{{t('mismatch')}}"
                  :submitted "{{state.reset.submitted}}"
                  :secret true
                  :maxLength 128})
              (node #setPassword
                :style primary
                :icon "{{state.busy == 'setPassword' ? 'loader' : 'key-round'}}"
                :text "{{state.busy == 'setPassword' ? computed.busyText : t('setPassword')}}"
                :disabled "{{state.busy != ''}}"
                :onClick setPassword)
              (node #resetBack :style link :text "{{t('backToSignIn')}}" :onClick goBack))
            (node #card-reset-end :height 36 :color transparent)]
          :maxHeight "{{window.height - 116}}")]
      :every {:ms 1000 :do resetTick})
    :signedIn (node #panel-signed-in :style screen :every {:ms 1000 :do tick}
      (node #accountCard
        :style accountCard
        :width "{{min(880, window.width - 32)}}"
        :height "{{min(604, window.height - 116)}}"
        (node #accountCard-top :height 14 :color transparent)
        (node #row-account :type row :height 56 :gap 14 :alignItems center :anchors {:left 28 :right 28}
          (node #avatar
            :type label
            :font title
            :text "{{computed.initials}}"
            :width 56
            :height 56
            :radius 999
            :align center
            :color "{{computed.avatarColor}}"
            :textColor "{{computed.avatarText}}"
            :role img
            :label "{{state.user.name}}")
          (node #column-account :type column :grow 1 :height 56 :gap 2
            (node #label-you-re-in :style title :text "{{t('youreIn')}}" :role heading)
            (node #label-signed-in-as :style caption :text "{{t('signedInAs', { email: state.user.email })}}"))
          (node #label-sign-out
            :style smallButton
            :icon log-out
            :text "{{t('signOutButton')}}"
            :tooltip "{{t('signOutButton')}} ({{computed.signOutKey}})"
            :onClick signOut
            :height 38))
        (node #row-meta :type row :height 20 :gap 18 :alignItems center :anchors {:left 28 :right 28}
          (node #label-remembered
            :type label
            :font small
            :height 20
            :icon "{{state.remember ? 'shield-check' : 'clock-3'}}"
            :textColor @color.muted
            :text "{{state.remember ? t('staysSignedIn') : t('signedOutOnClose')}}"
            :fitText true)
          (node #label-provider
            :when "{{state.user.provider}}"
            :type label
            :font small
            :height 20
            :icon key-round
            :textColor @color.muted
            :text "{{t('signedInWith')}}"
            :fitText true)
          (node #session
            :type label
            :font small
            :fitText true
            :height 20
            :icon timer
            :role timer
            :textColor "{{state.session.left <= 60 ? '@color.error' : '@color.muted'}}"
            :text "{{t('sessionEnds', { time: computed.sessionTime })}}")
          (node #staySignedIn
            :when "{{state.session.left <= 60}}"
            :style smallLink
            :height 20
            :text "{{t('stay')}}"
            :onClick staySignedIn))
        (node #divider :style divider :anchors {:left 28 :right 28})
        (node #navRow
          :when "{{!computed.wide}}"
          :type row
          :height 46
          :gap 4
          :role tablist
          :label "{{t('accountSections')}}"
          :arrows horizontal
          :scroll horizontal
          :anchors {:left 28 :right 28}
          :children [(node #navChip
              :repeat {:for s :in computed.sections}
              :use navItem
              :with {:name "{{s.name}}" :icon "{{s.icon}}" :wide false})]
          :alignItems start)
        (node #row-body :type row :grow 1 :gap 24 :alignItems stretch :anchors {:left 28 :right 28}
          (node #navColumn
            :when "{{computed.wide}}"
            :type column
            :width 196
            :gap 2
            :role tablist
            :label "{{t('accountSections')}}"
            :arrows vertical
            (node #nav
              :repeat {:for s :in computed.sections}
              :use navItem
              :with {:name "{{s.name}}" :icon "{{s.icon}}" :wide true}))
          (node #sectionPanel :type column :grow 1 :gap 10 :scroll vertical
            (node #section-profile
              :when "{{state.section == 'profile'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_profile')}}"
              (node #header-profile :use sectionHeader :with {:name profile})
              (node #row-photo :type row :height 64 :gap 16 :alignItems center :anchors {:left 0 :right 10}
                (node #avatarLarge
                  :type label
                  :font title
                  :text "{{computed.initials}}"
                  :width 64
                  :height 64
                  :radius 999
                  :align center
                  :color "{{computed.avatarColor}}"
                  :textColor "{{computed.avatarText}}"
                  :role img
                  :label "{{t('photo')}}")
                (node #column-photo :type column :grow 1 :height 42 :gap 2
                  (node #label-photo :style itemTitle :text "{{t('photo')}}")
                  (node #changePhoto
                    :style smallLink
                    :height 20
                    :text "{{t('changePhoto')}}"
                    :onClick {:toggle state.photoOpen}
                    :isOpen "{{state.photoOpen}}")))
              (node #label-account-name :style fieldLabel :text "{{t('name')}}")
              (node #account-name
                :use checkedField
                :with {:submit saveProfile
                  :bind state.profile.name
                  :content "{{state.profile.name}}"
                  :key accountName
                  :icon user
                  :name "{{t('name')}}"
                  :placeholder "{{t('namePlaceholder')}}"
                  :valid "{{length(trim(state.profile.name)) > 0}}"
                  :message "{{t('enterName')}}"
                  :submitted "{{state.profile.submitted}}"
                  :maxLength 60})
              (node #usernameLine
                :when "{{(state.user.username ?? '') != ''}}"
                :style hint
                :icon at-sign
                :text "{{t('usernameIs', { name: state.user.username })}}")
              (node #row-account-buttons :type row :height 42 :anchors {:left 0 :right 10}
                (node #saveProfile
                  :style primary
                  :anchors {}
                  :icon "{{state.busy == 'save' ? 'loader' : 'save'}}"
                  :text "{{state.busy == 'save' ? computed.busyText : t('saveChanges')}}"
                  :disabled "{{state.busy != ''}}"
                  :onClick saveProfile
                  :width 200
                  :height 42)))
            (node #section-emails
              :when "{{state.section == 'emails'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_emails')}}"
              (node #header-emails :use sectionHeader :with {:name emails})
              (node #emailRow
                :repeat {:for em :in state.emails :key "{{em.address}}"}
                :use itemRow
                :with {:icon "{{em.verified ? 'mail-check' : 'mail-warning'}}"
                  :title "{{em.address}}"
                  :hint "{{em.primary ? t('emailsHint') : em.verified ? t('verified') : em.sent ? t('linkSentHint') : t('unverified')}}"}
                (node #badge-primary :when "{{em.primary}}" :style badge :text "{{t('primary')}}")
                (node #badge-unverified
                  :when "{{!em.verified}}"
                  :style badgeWarn
                  :text "{{em.sent ? t('linkSentBadge') : t('unverified')}}")
                (node #verifyEmail
                  :style smallLink
                  :height 20
                  :text "{{t('verifyEmail')}}"
                  :onClick {:run verifyEmail :with {:address "{{em.address}}"}}
                  :when "{{!em.verified && !em.sent}}"
                  :label "{{t('verifyEmail') + ' ' + em.address}}")
                (node #makePrimary
                  :style smallLink
                  :height 20
                  :text "{{t('makePrimary')}}"
                  :onClick {:run makePrimary :with {:address "{{em.address}}"}}
                  :when "{{em.verified && !em.primary}}"
                  :label "{{t('makePrimary') + ': ' + em.address}}")
                (node #removeEmail
                  :style smallLink
                  :height 20
                  :text "{{t('remove')}}"
                  :onClick {:run removeEmail :with {:address "{{em.address}}"}}
                  :when "{{!em.primary}}"
                  :label "{{t('remove') + ' ' + em.address}}"))
              (node #addEmail
                :style smallButton
                :icon plus
                :text "{{t('addEmail')}}"
                :onClick openAddEmail
                :when "{{!state.newEmail.open && length(state.emails) < 5}}")
              (node #newEmailForm
                :when "{{state.newEmail.open}}"
                :type column
                :fitContent height
                :gap 8
                :anchors {:left 0 :right 0}
                (node #label-new-email :style fieldLabel :text "{{t('newEmail')}}")
                (node #newEmailField
                  :use checkedField
                  :with {:submit addEmail
                    :bind state.newEmail.address
                    :content "{{state.newEmail.address}}"
                    :key newEmail
                    :icon mail-plus
                    :name "{{t('newEmail')}}"
                    :placeholder "{{t('emailPlaceholder')}}"
                    :valid "{{computed.newEmailError == ''}}"
                    :message "{{t('emailError', { kind: computed.newEmailError })}}"
                    :submitted "{{state.newEmail.submitted}}"
                    :maxLength 254})
                (node #row-new-email :type row :height 34 :gap 8 :anchors {:left 0 :right 10}
                  (node #addEmailButton
                    :style primary
                    :anchors {}
                    :height 34
                    :font small
                    :padding 12
                    :fitText true
                    :shadow {:color "#00000000"}
                    :icon "{{state.busy == 'verifyEmail' ? 'loader' : 'plus'}}"
                    :text "{{state.busy == 'verifyEmail' ? computed.busyText : t('add')}}"
                    :disabled "{{state.busy != ''}}"
                    :onClick addEmail)
                  (node #cancelEmail :style smallButton :text "{{t('cancel')}}" :onClick closeAddEmail))))
            (node #section-security
              :when "{{state.section == 'security'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_security')}}"
              (node #header-security :use sectionHeader :with {:name security})
              (node #passwordRow
                :use itemRow
                :with {:icon key-round
                  :title "{{t('password')}}"
                  :hint "{{state.user.provider ? t('providerPassword') : t('passwordAge', { n: computed.passwordDays })}}"}
                (node #changePassword
                  :when "{{!state.user.provider}}"
                  :style smallButton
                  :anchors {}
                  :icon key-round
                  :text "{{t('changePassword')}}"
                  :isOpen "{{state.change.open}}"
                  :onClick openChange))
              (node #twoStepBox
                :type column
                :fitContent height
                :gap 8
                :radius @radius.field
                :color @color.input
                :borderWidth 1
                :borderColor @color.border
                :anchors {:left 0 :right 10}
                (node #twoStepBox-top :height 4 :color transparent)
                (node #row-two-step :type row :height 36 :gap 12 :alignItems center :anchors {:left 12 :right 12}
                  (node #twoStep
                    :type toggle
                    :width 44
                    :height 24
                    :role switch
                    :focusable true
                    :label "{{t('twoStep')}}"
                    :description "{{t('twoStepState', { on: state.user.twoStep })}}"
                    :checked "{{state.user.twoStep}}"
                    :activeColor @color.accent
                    :trackColor @color.border
                    :onChange {:run setTwoStep :with {:on "{{event.value}}"}}
                    :keys {:space {:run setTwoStep :with {:on "{{!state.user.twoStep}}"}}
                      :enter {:run setTwoStep :with {:on "{{!state.user.twoStep}}"}}})
                  (node #column-two-step :type column :grow 1 :height 36
                    (node #label-two-step-name :style fieldLabel :text "{{t('twoStep')}}")
                    (node #label-two-step-state :style hint :text "{{t('twoStepState', { on: state.user.twoStep })}}")))
                (node #row-backup
                  :when "{{state.user.twoStep}}"
                  :type row
                  :height 18
                  :gap 12
                  :alignItems center
                  :anchors {:left 12 :right 12}
                  (node #backupLeft
                    :style hint
                    :icon key-round
                    :grow 1
                    :anchors {}
                    :text "{{t('backupLeft', { n: length(state.backupCodes) })}}")
                  (node #newCodes :style smallLink :text "{{t('newCodes')}}" :onClick newCodes))
                (node #label-trusted
                  :when "{{state.user.twoStep}}"
                  :style fieldLabel
                  :text "{{t('trustedDevices')}}"
                  :anchors {:left 12 :right 12})
                (node #row-trusted
                  :when "{{state.user.twoStep}}"
                  :repeat {:for dv :in computed.myTrusted :index i :key "{{dv.device}}"}
                  :type row
                  :height 18
                  :gap 12
                  :alignItems center
                  :anchors {:left 12 :right 12}
                  (node #trusted-name
                    :style hint
                    :grow 1
                    :anchors {}
                    :icon "{{dv.device == 'phone' ? 'smartphone' : 'laptop'}}"
                    :text "{{t('device_' + dv.device) + '  ·  ' + t('trustedDays', { n: dv.days })}}")
                  (node #trusted-remove
                    :style smallLink
                    :text "{{t('removeDevice')}}"
                    :label "{{t('removeDevice') + ' ' + t('device_' + dv.device)}}"
                    :onClick {:run removeTrusted :with {:device "{{dv.device}}"}}))
                (node #noTrusted
                  :when "{{state.user.twoStep && length(computed.myTrusted) == 0}}"
                  :style hint
                  :text "{{t('noTrusted')}}"
                  :anchors {:left 12 :right 12})
                (node #twoStepBox-end :height 4 :color transparent))
              (node #label-passkeys :style fieldLabel :text "{{t('passkeys')}}" :role heading)
              (node #passkeysCaption :style hint :text "{{t('passkeysCaption')}}")
              (node #passkeyRow
                :repeat {:for pk :in state.myPasskeys :key "{{pk.id}}"}
                :use itemRow
                :with {:icon fingerprint-pattern
                  :title "{{t('device_' + pk.device)}}"
                  :hint "{{pk.used < 0 ? t('passkeyNew', { added: t('daysAgo', { n: pk.added }) }) : t('passkeyLine', { added: t('daysAgo', { n: pk.added }), used: t('daysAgo', { n: pk.used }) })}}"}
                (node #removePasskey
                  :style smallLink
                  :height 20
                  :text "{{t('remove')}}"
                  :onClick {:run removePasskey :with (node #"{{pk.id}}")}
                  :label "{{t('remove') + ' ' + t('device_' + pk.device)}}"))
              (node #noPasskeys
                :when "{{length(state.myPasskeys) == 0}}"
                :style hint
                :icon fingerprint-pattern
                :text "{{t('noPasskeys')}}")
              (node #addPasskey
                :style smallButton
                :icon "{{state.busy == 'passkey' ? 'loader' : 'plus'}}"
                :text "{{state.busy == 'passkey' ? computed.busyText : t('addPasskey')}}"
                :onClick addPasskey
                :when "{{length(state.myPasskeys) < 5}}"
                :disabled "{{state.busy != ''}}"))
            (node #section-devices
              :when "{{state.section == 'devices'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_devices')}}"
              (node #header-devices :use sectionHeader :with {:name devices})
              (node #sessionRow
                :repeat {:for ss :in state.sessions :key "{{ss.id}}"}
                :use itemRow
                :with {:icon "{{computed.deviceIcons[ss.device]}}"
                  :title "{{t('device_' + ss.device) + '  ·  ' + t('app_' + ss.app)}}"
                  :hint "{{ss.place + '  ·  ' + (ss.id == 'here' ? t('activeNow') : t('lastActive', { when: t('daysAgo', { n: ss.days }) }))}}"}
                (node #badge-here :when "{{ss.id == 'here'}}" :style badge :text "{{t('thisDevice')}}")
                (node #signOutSession
                  :style smallLink
                  :height 20
                  :text "{{t('signOutDevice')}}"
                  :onClick {:run signOutSession :with (node #"{{ss.id}}")}
                  :when "{{ss.id != 'here'}}"
                  :label "{{t('signOutDevice') + ': ' + t('device_' + ss.device)}}"))
              (node #signOutOthers
                :style smallButton
                :icon "{{state.busy == 'devices' ? 'loader' : 'log-out'}}"
                :text "{{state.busy == 'devices' ? computed.busyText : t('signOutOthers')}}"
                :onClick signOutOthers
                :when "{{length(state.sessions) > 1}}"
                :disabled "{{state.busy != ''}}")
              (node #onlyThisDevice
                :when "{{length(state.sessions) <= 1}}"
                :style hint
                :icon shield-check
                :text "{{t('onlyThisDevice')}}"))
            (node #section-connected
              :when "{{state.section == 'connected'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_connected')}}"
              (node #header-connected :use sectionHeader :with {:name connected})
              (node #providerRow
                :repeat {:for pv :in computed.providers :key "{{pv.name}}"}
                :use itemRow
                :with {:icon "{{pv.icon}}"
                  :title "{{t('provider_' + pv.name)}}"
                  :hint "{{some(state.connected, it.provider == pv.name) ? t('connectedAs', { account: find(state.connected, it.provider == pv.name).account }) : t('notConnected')}}"}
                (node #connectProvider
                  :style smallButton
                  :icon "{{state.busy == 'connect' && state.target == pv.name ? 'loader' : 'link'}}"
                  :text "{{t('connect')}}"
                  :onClick {:run connectProvider :with {:provider "{{pv.name}}"}}
                  :when "{{!some(state.connected, it.provider == pv.name)}}"
                  :disabled "{{state.busy != ''}}"
                  :label "{{t('connect') + ' ' + t('provider_' + pv.name)}}")
                (node #disconnectProvider
                  :style smallButton
                  :icon link-2-off
                  :text "{{t('disconnect')}}"
                  :onClick {:run disconnectProvider :with {:provider "{{pv.name}}"}}
                  :when "{{some(state.connected, it.provider == pv.name)}}"
                  :label "{{t('disconnect') + ' ' + t('provider_' + pv.name)}}")))
            (node #section-activity
              :when "{{state.section == 'activity'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_activity')}}"
              (node #header-activity :use sectionHeader :with {:name activity})
              (node #eventRow
                :repeat {:for ev :in state.log :index i}
                :use itemRow
                :with {:icon "{{computed.eventIcons[ev.kind] ?? 'shield'}}"
                  :title "{{t('event', { kind: ev.kind })}}"
                  :hint "{{t('eventLine', { when: ev.days > 0 ? t('daysAgo', { n: ev.days }) : t('minutesAgo', { n: floor(max(0, state.clock - ev.at) / 60) }), device: t('device_' + ev.device), place: ev.place })}}"}
                (node #notYou
                  :style smallLink
                  :height 20
                  :text "{{t('notYouLink')}}"
                  :onClick openChange
                  :when "{{ev.kind == 'signInBlocked' && !state.user.provider}}")))
            (node #section-data
              :when "{{state.section == 'data'}}"
              :type column
              :fitContent height
              :gap 10
              :anchors {:left 0 :right 0}
              :role tabpanel
              :label "{{t('section_data')}}"
              (node #header-data :use sectionHeader :with {:name data})
              (node #deleteWarning
                :type label
                :font small
                :icon alert-triangle
                :wrap true
                :padding 12
                :radius 8
                :height 62
                :color @color.errorSoft
                :textColor @color.error
                :anchors {:left 0 :right 10}
                :text "{{t('deleteText')}}")
              (node #row-delete :type row :height 42 :anchors {:left 0 :right 10}
                (node #deleteAccount
                  :style danger
                  :anchors {}
                  :width 200
                  :height 42
                  :icon trash-2
                  :text "{{t('deleteButton')}}"
                  :isOpen "{{state.deleting.open}}"
                  :onClick openDelete)))))
        (node #accountCard-end :height 14 :color transparent))
      (node #photoPopover
        :use popover
        :with {:open state.photoOpen
          :anchor "#changePhoto"
          :title "{{t('photoTitle')}}"
          :text "{{t('photoText')}}"
          :width 290
          :height 112
          :color @color.surface
          :textColor @color.text
          :dimColor @color.muted}
        (node #swatches
          :type row
          :gap 8
          :height 30
          :role radiogroup
          :label "{{t('photoTitle')}}"
          :arrows horizontal
          :anchors {:left 12 :right 12 :top 64}
          (node #swatch
            :repeat {:for c :in "{{range(7)}}"}
            :type label
            :width 30
            :height 30
            :radius 999
            :cursor hand
            :focusable true
            :role radio
            :text ""
            :font iconsSmall
            :align center
            :icon "{{(state.user.avatar ?? 0) == c ? 'check' : ''}}"
            :label "{{c == 0 ? t('photoColor', { n: 1 }) : t('photoColor', { n: c + 1 })}}"
            :selected "{{(state.user.avatar ?? 0) == c}}"
            :color "{{c == 0 ? '@color.accentSoft' : '@color.avatar' + c}}"
            :textColor "{{c == 0 ? '@color.accent' : '@color.avatarText'}}"
            :borderWidth "{{(state.user.avatar ?? 0) == c ? 2 : 0}}"
            :borderColor @color.text
            :onClick {:run setAvatar :with {:color "{{c}}"}}
            :keys {:enter {:run setAvatar :with {:color "{{c}}"}} :space {:run setAvatar :with {:color "{{c}}"}}}))))}
  :root (node #panel-root :color @color.background
    (node #glow
      :width 520
      :height 520
      :radius 999
      :x -160
      :y -200
      :color @color.glow
      :shadow {:blur 120 :color @color.glow})
    (node #glow-2
      :width 460
      :height 460
      :radius 999
      :anchors {:right -140 :bottom -180}
      :color @color.glow2
      :shadow {:blur 120 :color @color.glow2})
    (node #screens :dir auto :type outlet :anchors {:left 0 :right 0 :top 56 :bottom 44})
    (node #announcer
      :dir auto
      :type label
      :font small
      :role status
      :live polite
      :label "{{computed.busyText}}"
      :icon "{{state.busy == '' ? '' : 'loader'}}"
      :text "{{computed.busyText}}"
      :textColor @color.muted
      :anchors {:left 20 :top 22}
      :height 24
      :fitText true)
    (node #topBar :dir auto :type row :gap 6 :width 226 :height 32 :alignItems center :anchors {:right 16 :top 14}
      (node #languages
        :type row
        :role radiogroup
        :label "{{t('language')}}"
        :arrows horizontal
        :gap 2
        :height 32
        :width 112
        (node #lang
          :repeat {:for code :in "{{['en', 'it', 'ar']}}"}
          :style "{{state.locale == code ? 'chipOn' : 'chip'}}"
          :text "{{upper(code)}}"
          :label "{{t('langName', { code: code })}}"
          :tooltip "{{t('langName', { code: code })}}"
          :selected "{{state.locale == code}}"
          :onClick {:run setLocale :with {:code "{{code}}"}}))
      (node #slowToggle
        :style iconButton
        :font iconsSmall
        :icon "{{state.slow ? 'snail' : 'wifi'}}"
        :textColor "{{state.slow ? '@color.accent' : '@color.muted'}}"
        :role switch
        :label "{{t('slow')}}"
        :tooltip "{{t('slowTooltip', { on: state.slow })}}"
        :selected "{{state.slow}}"
        :onClick toggleSlow)
      (node #shortcutsButton
        :style iconButton
        :font iconsSmall
        :icon keyboard
        :label "{{t('shortcuts')}}"
        :tooltip "{{t('shortcutsTooltip', { key: computed.shortcutsKey })}}"
        :isOpen "{{state.shortcutsOpen}}"
        :onClick openShortcuts)
      (node #themeToggle
        :style iconButton
        :icon "{{state.theme == 'light' ? 'moon' : 'sun'}}"
        :label "{{t('themeTooltip', { dark: state.theme == 'dark', key: computed.themeKey })}}"
        :tooltip "{{t('themeTooltip', { dark: state.theme == 'dark', key: computed.themeKey })}}"
        :onClick toggleTheme))
    (node #footer
      :when "{{window.width >= 640}}"
      :type label
      :font small
      :textColor @color.muted
      :align center
      :anchors {:left 0 :right 0 :bottom 18}
      :height 18
      :text "{{t('footer')}}")
    (node #forgotDialog
      :dir auto
      :use dialog
      :with {:open state.forgot.open
        :title "{{t('resetTitle')}}"
        :message "{{t('resetMessage')}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{state.forgot.error == '' ? 196 : 218}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('sendLink')}}" :run sendReset} {:title "{{t('cancel')}}" :run closeForgot}]}
      (node #forgotEmail
        :style field
        :anchors {:left 20 :right 20 :top 76}
        :height 38
        :borderColor "{{state.forgot.error == '' ? '@color.border' : '@color.error'}}"
        :placeholder "{{t('emailPlaceholder')}}"
        :label "{{t('email')}}"
        :description "{{state.forgot.error}}"
        :maxLength 254
        :bind state.forgot.email
        :onChange {:set state.forgot.error :to ""}
        :onSubmit sendReset)
      (node #forgotError
        :when "{{state.forgot.error != ''}}"
        :style inlineError
        :text "{{state.forgot.error}}"
        :anchors {:left 22 :right 20 :top 120}))
    (node #passkeyDialog
      :dir auto
      :use dialog
      :with {:open state.passkeyOpen
        :title "{{t('passkeyTitle')}}"
        :message "{{t('passkeyMessage')}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{150 + 46 * length(computed.passkeys)}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('cancel')}}" :run cancelPasskey}]}
      (node #passkey-choice
        :repeat {:for pk :in computed.passkeys :index i :key "{{pk.email}}"}
        :style secondary
        :height 40
        :anchors {:left 20 :right 20 :top "{{76 + 46 * i}}"}
        :icon fingerprint-pattern
        :text "{{pk.name + ' · ' + pk.email}}"
        :onClick {:run choosePasskey :with {:email "{{pk.email}}"}}))
    (node #setupDialog
      :dir auto
      :use dialog
      :with {:open state.setup.open
        :title "{{t('setupTitle')}}"
        :message "{{t(state.setup.step == 'scan' ? 'setupScanMessage' : 'setupCodesMessage')}}"
        :width "{{min(440, window.width - 32)}}"
        :height "{{(state.setup.step == 'scan' ? 380 : 312) + (state.setup.error == '' ? 0 : 24)}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t(state.setup.step == 'scan' ? 'next' : 'done')}}" :run setupNext}
          {:title "{{t(state.setup.step == 'codes' && !state.user.twoStep ? 'back' : 'cancel')}}" :run setupCancel}]}
      (node #qrBox
        :when "{{state.setup.step == 'scan'}}"
        :type panel
        :width 140
        :height 140
        :radius 8
        :color @color.qrBack
        :anchors {:left 20 :top 76}
        :role img
        :label "{{t('setupScanMessage')}}"
        (node #qr
          :type path
          :d "{{computed.otpQr.d}}"
          :viewBox "{{computed.otpQr.viewBox}}"
          :fillColor @color.qrInk
          :anchors {:left 4 :right 4 :top 4 :bottom 4}))
      (node #label-key
        :when "{{state.setup.step == 'scan'}}"
        :style fieldLabel
        :text "{{t('orEnterKey')}}"
        :anchors {:left 176 :right 20 :top 90})
      (node #setupKey
        :when "{{state.setup.step == 'scan'}}"
        :type label
        :font mono
        :dir ltr
        :height 22
        :text "EXAM PLEE XAMP LE23"
        :textColor @color.text
        :selectable true
        :anchors {:left 176 :right 20 :top 114})
      (node #copyKey
        :when "{{state.setup.step == 'scan'}}"
        :style smallLink
        :icon copy
        :text "{{t('copyKey')}}"
        :anchors {:left 176 :top 144}
        :onClick [{:copy EXAMPLEEXAMPLE23} {:run notify :with {:message "{{t('keyCopied')}}"}}])
      (node #demoSetup
        :when "{{state.setup.step == 'scan'}}"
        :style hint
        :icon info
        :wrap true
        :height 36
        :text "{{t('demoSetupCode', { code: state.demoCode })}}"
        :anchors {:left 176 :right 20 :top 176})
      (node #label-setup-code
        :when "{{state.setup.step == 'scan'}}"
        :style fieldLabel
        :text "{{t('code')}}"
        :anchors {:left 20 :right 20 :top 232})
      (node #setupCode
        :when "{{state.setup.step == 'scan'}}"
        :style code
        :tabIndex 1
        :dir ltr
        :bind state.setup.code
        :label "{{t('code')}}"
        :description "{{state.setup.error == '' ? '' : t('setupError', { kind: state.setup.error })}}"
        :cellBorderColor "{{state.setup.error == '' ? '@color.border' : '@color.error'}}"
        :anchors {:left 20 :right 20 :top 254}
        :onChange setupCodeChanged
        :onSubmit setupNext)
      (node #backup-code
        :when "{{state.setup.step == 'codes'}}"
        :repeat {:for bc :in state.backupCodes :index i :key "{{bc}}"}
        :type label
        :font mono
        :dir ltr
        :height 24
        :width 150
        :align center
        :radius 6
        :color @color.input
        :textColor @color.text
        :text "{{bc}}"
        :anchors {:left "{{i % 2 == 0 ? 40 : 210}}" :top "{{76 + 30 * floor(i / 2)}}"})
      (node #row-saved :when "{{state.setup.step == 'codes'}}" :style checkRow :anchors {:left 20 :right 20 :top 204}
        (node #savedCodes
          :style "{{state.setup.saved ? 'checkOn' : 'checkOff'}}"
          :label "{{t('savedCodes')}}"
          :checked "{{state.setup.saved}}"
          :onClick toggleSaved
          :keys {:space toggleSaved})
        (node #label-saved
          :type label
          :font small
          :text "{{t('savedCodes')}}"
          :fitText true
          :textColor @color.muted
          :cursor hand
          :onClick toggleSaved))
      (node #copyCodes
        :when "{{state.setup.step == 'codes'}}"
        :style smallLink
        :icon copy
        :text "{{t('copyCodes')}}"
        :anchors {:left 20 :top 234}
        :onClick [{:copy "{{join(state.backupCodes, '\\n')}}"} {:run notify :with {:message "{{t('codesCopied')}}"}}])
      (node #setupError
        :when "{{state.setup.error != ''}}"
        :style inlineError
        :text "{{t('setupError', { kind: state.setup.error })}}"
        :anchors {:left 22 :right 20 :top "{{state.setup.step == 'scan' ? 314 : 262}}"}))
    (node #changeDialog
      :dir auto
      :use dialog
      :with {:open state.change.open
        :title "{{t('changePassword')}}"
        :message "{{t('changePasswordMessage', { email: state.user.email })}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{(state.showPassword ? 222 : 268) + (state.change.submitted && computed.changeError != '' ? 24 : 0)}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('change')}}" :run changePassword} {:title "{{t('cancel')}}" :run closeChange}]}
      (node #newPassword
        :style field
        :anchors {:left 20 :right 20 :top 76}
        :height 38
        :secret "{{!state.showPassword}}"
        :maxLength 128
        :placeholder "{{t('newPassword')}}"
        :label "{{t('newPassword')}}"
        :description "{{state.change.submitted ? computed.changeError : ''}}"
        :bind state.change.password
        :keys {:"mod+shift+h" togglePassword}
        :onSubmit changePassword)
      (node #newPasswordConfirm
        :when "{{!state.showPassword}}"
        :style field
        :anchors {:left 20 :right 20 :top 122}
        :height 38
        :secret true
        :maxLength 128
        :placeholder "{{t('confirm')}}"
        :label "{{t('confirm')}}"
        :bind state.change.confirm
        :onSubmit changePassword)
      (node #change-meter
        :color transparent
        :use passwordMeter
        :anchors {:left 20 :top "{{state.showPassword ? 124 : 170}}"}
        :with {:password "{{state.change.password}}"
          :width "{{min(420, window.width - 32) - 40}}"
          :labels {:empty "{{t('strengthEmpty')}}"
            :weak "{{t('weak')}}"
            :fair "{{t('fair')}}"
            :good "{{t('good')}}"
            :strong "{{t('strong')}}"}
          :track @color.border
          :textColor @color.muted
          :colors [@color.weak @color.weak @color.fair @color.good @color.strong]})
      (node #changeError
        :when "{{state.change.submitted && computed.changeError != ''}}"
        :style inlineError
        :text "{{computed.changeError}}"
        :anchors {:left 22 :right 20 :top "{{state.showPassword ? 162 : 208}}"}))
    (node #deleteDialog
      :dir auto
      :use dialog
      :with {:open state.deleting.open
        :title "{{t('deleteDialogTitle', { email: state.user.email })}}"
        :message "{{t('deleteDialogMessage', { word: computed.deleteWord })}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{state.deleting.error == '' ? 196 : 218}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.error
        :backdrop @color.backdrop
        :buttons [{:title "{{t('deleteForever')}}" :run confirmDelete} {:title "{{t('cancel')}}" :run closeDelete}]}
      (node #deleteConfirm
        :style field
        :anchors {:left 20 :right 20 :top 76}
        :height 38
        :maxLength 32
        :borderColor "{{state.deleting.error == '' ? '@color.border' : '@color.error'}}"
        :placeholder "{{t('deleteField', { word: computed.deleteWord })}}"
        :label "{{t('deleteField', { word: computed.deleteWord })}}"
        :description "{{state.deleting.error == '' ? '' : t('deleteError', { kind: state.deleting.error, word: computed.deleteWord })}}"
        :bind state.deleting.typed
        :onChange {:set state.deleting.error :to ""}
        :onSubmit confirmDelete)
      (node #deleteError
        :when "{{state.deleting.error != ''}}"
        :style inlineError
        :text "{{t('deleteError', { kind: state.deleting.error, word: computed.deleteWord })}}"
        :anchors {:left 22 :right 20 :top 120}))
    (node #reauthDialog
      :dir auto
      :use dialog
      :with {:open state.reauth.open
        :title "{{t('reauthTitle')}}"
        :message "{{t('reauthMessage', { email: state.user.email })}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{state.reauth.error == '' ? 196 : 218}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('confirmIt')}}" :run confirmReauth} {:title "{{t('cancel')}}" :run closeReauth}]}
      (node #reauthPassword
        :style field
        :anchors {:left 20 :right 20 :top 76}
        :height 38
        :secret true
        :maxLength 128
        :borderColor "{{state.reauth.error == '' ? '@color.border' : '@color.error'}}"
        :placeholder "{{t('passwordPlaceholder')}}"
        :label "{{t('password')}}"
        :description "{{state.reauth.error == '' ? '' : t('reauthError', { kind: state.reauth.error })}}"
        :bind state.reauth.password
        :onChange {:set state.reauth.error :to ""}
        :onSubmit confirmReauth)
      (node #reauthError
        :when "{{state.reauth.error != ''}}"
        :style inlineError
        :text "{{t('reauthError', { kind: state.reauth.error })}}"
        :anchors {:left 22 :right 20 :top 120}))
    (node #shortcutsDialog
      :dir auto
      :use dialog
      :with {:open state.shortcutsOpen
        :title "{{t('shortcuts')}}"
        :message "{{t('shortcutsMessage')}}"
        :width "{{min(420, window.width - 32)}}"
        :height "{{132 + length(commands) * 28}}"
        :color @color.surface
        :textColor @color.text
        :dimColor @color.muted
        :highlight @color.accent
        :backdrop @color.backdrop
        :buttons [{:title "{{t('close')}}" :run closeShortcuts}]}
      (node #shortcutList
        :type column
        :color transparent
        :role list
        :label "{{t('shortcuts')}}"
        :anchors {:left 20 :right 20 :top 72}
        :height "{{length(commands) * 28}}"
        (node #shortcut
          :repeat {:for c :in "{{computed.shortcuts}}"}
          :role listitem
          :label "{{t('command', { name: c.name })}}, {{c.keyText}}"
          :anchors {:left 0 :right 0}
          :height 28
          :color transparent
          (node #shortcut-title
            :type label
            :font small
            :icon "{{c.icon}}"
            :text "{{t('command', { name: c.name })}}"
            :textColor "{{c.enabled ? '@color.text' : '@color.muted'}}"
            :align {:horizontal start :vertical center}
            :anchors {:left 0 :right 90 :top 4}
            :height 20)
          (node #shortcut-key
            :type label
            :font small
            :dir ltr
            :text "{{c.keyText}}"
            :textColor @color.muted
            :align {:horizontal end :vertical center}
            :anchors {:right 0 :top 4}
            :width 86
            :height 20))))
    (node #toast
      :dir auto
      :type row
      :overlay {:placement window :open "{{state.toast.open}}"}
      :anchors {:left 0 :right 0 :bottom 58}
      :height 40
      :justify center
      :color transparent
      :role status
      :live polite
      (node #toastPill
        :type row
        :fitContent width
        :height 40
        :gap 10
        :alignItems center
        :radius 20
        :color @color.toast
        :shadow {:y 6 :blur 18 :color @color.shadow}
        (node #toast-start :width 6 :color transparent)
        (node #toastIcon :type label :font iconsSmall :icon check :textColor @color.toastText :fitText true)
        (node #toastMessage
          :type label
          :font small
          :text "{{state.toast.message}}"
          :textColor @color.toastText
          :fitText true)
        (node #toastStay
          :when "{{state.toast.action == 'stay'}}"
          :type label
          :font small
          :text "{{t('stay')}}"
          :textColor @color.toastAction
          :fitText true
          :cursor hand
          :focusable true
          :role button
          :onClick [{:set state.toast.open :to false} {:run staySignedIn}])
        (node #toastClose
          :type label
          :font iconsSmall
          :icon x
          :text ""
          :textColor @color.toastText
          :fitText true
          :cursor hand
          :focusable true
          :role button
          :label "{{t('close')}}"
          :onClick {:set state.toast.open :to false})
        (node #toast-end :width 6 :color transparent))))}
