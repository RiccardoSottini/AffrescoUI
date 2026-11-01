{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "The client's own windows, reached from the menus: Settings (interface, downloads, storage, account), product keys and gift codes, a game from this computer, back up and restore, About and Help, signing out, offline and couch mode (big tiles for a television). Product keys and gift codes are invented, for trying it out."
  :state {:settingsDialog {:open false :page interface}
    :code {:open false :kind activate :text "" :error ""}
    :addGame {:open false :name ""}
    :backup {:open false :restored nil}
    :info {:open false :kind about}
    :session {:signedOut false :offline false}
    :couch {:open false :at 0}
    :toast {:open false :text ""}
    :productKeys {:AR7Q2-KX4MZ-P9T3L glasswing :HX5RE-2PWNA-7LDKC volt}
    :giftCodes {:GIFT-2026-ARCD 20 :GIFT-5555-PLUM 5}
    :redeemed []}
  :computed {:couchGames "{{sort(computed.owned, it.lastPlayed ?? 0, 'desc')}}"
    :couchGame "{{computed.couchGames[state.couch.at]}}"
    :installedGames "{{sort(filter(data.games, it.installed), it.sizeGb ?? 0, 'desc')}}"
    :usedGb "{{sum(filter(data.games, it.installed), it.sizeGb ?? 0)}}"}
  :actions {:setSetting {:title "Change a setting"
      :category Settings
      :do [{:set "{{'settings.' + key}}" :to "{{value}}"}]}
    :openSettings {:title Settings
      :category Arcadia
      :icon settings
      :description "Opens Settings at a page (interface, downloads, storage or account)."
      :do [{:dismiss true} {:set state.settingsDialog :to {:open true :page "{{page ?? 'interface'}}"}}]}
    :notify {:title "Show a notice"
      :category Arcadia
      :description "Shows a short notice at the bottom right; it goes away by itself."
      :do [{:set state.toast :to {:open true :text "{{text}}"}}
        {:debounce 4000 :do [{:set state.toast.open :to false}]}]}
    :checkUpdates {:title "Check for client updates"
      :category Arcadia
      :do [{:run notify :with {:text "{{t('notice.checking')}}"}}
        {:after 1200 :do [{:run notify :with {:text "{{t('notice.upToDate')}}"}}]}]}
    :clearCache {:title "Clear the download cache"
      :category Settings
      :do [{:run notify :with {:text "{{t('settings.cacheCleared')}}"}}]}
    :toggleOffline {:title "Go offline or online"
      :category Arcadia
      :description "Offline, the store and friends wait for the network; installed games still play."
      :do [{:toggle state.session.offline}
        {:run notify :with {:text "{{t(state.session.offline ? 'notice.offline' : 'notice.online')}}"}}]}
    :signOut {:title "Sign out"
      :category Arcadia
      :do [{:dismiss true} {:set state.session.signedOut :to true} {:set state.couch.open :to false}]}
    :signIn {:title "Sign in" :category Arcadia :do [{:set state.session.signedOut :to false}]}
    :openCode {:title "Activate a product or redeem a gift code"
      :category Games
      :description "kind is activate (a product key adds a game) or redeem (a gift code adds to the wallet)."
      :do [{:set state.code :to {:open true :kind "{{kind}}" :text "" :error ""}}]}
    :closeCode {:title "Close the code window"
      :category Games
      :description "Closes only the code window, so Settings stays open under it."
      :do [{:set state.code.open :to false}]}
    :submitCode {:title "Use the code"
      :category Games
      :description "Checks the code typed; a good one adds its game or its amount, a wrong one says why."
      :do [{:set state.code.text :to "{{upper(trim(state.code.text))}}"}
        {:if "{{state.code.kind == 'activate'}}"
          :then [{:run activateKey :with (node #"{{state.productKeys[state.code.text]}}")}]
          :else [{:run redeemGift :with {:amount "{{state.giftCodes[state.code.text]}}"}}]}]}
    :activateKey {:title "Activate a product key"
      :category Games
      :do [{:if "{{id == null}}"
          :then [{:set state.code.error :to "{{t('code.badKey')}}"}]
          :else [{:if "{{find(data.games, it.id == id).owned}}"
              :then [{:set state.code.error :to "{{t('code.owned')}}"}]
              :else [(node :update games #"{{id}}" :fields {:owned true})
                {:set state.code.open :to false}
                {:run notify :with {:text "{{t('code.added', { game: find(data.games, it.id == id).title })}}"}}
                {:run openGame :with (node #"{{id}}")}]}]}]}
    :redeemGift {:title "Redeem a gift code"
      :category Games
      :do [{:if "{{amount == null}}"
          :then [{:set state.code.error :to "{{t('code.badGift')}}"}]
          :else [{:if "{{contains(state.redeemed, state.code.text)}}"
              :then [{:set state.code.error :to "{{t('code.used')}}"}]
              :else [{:push state.redeemed :value "{{state.code.text}}"}
                {:set state.user.wallet :to "{{round((state.user.wallet + amount) * 100) / 100}}"}
                {:set state.code.open :to false}
                {:run notify :with {:text "{{t('code.funded', { amount: formatCurrency(amount, 'EUR') })}}"}}]}]}]}
    :openAddGame {:title "Add a game from this computer"
      :category Games
      :icon package-plus
      :do [{:dismiss true} {:set state.addGame :to {:open true :name ""}}]}
    :addLocalGame {:title "Add a game from this computer"
      :category Games
      :description "Adds a game Arcadia doesn't sell to the library, ready to play from it."
      :do [{:if "{{trim(state.addGame.name) != ''}}"
          :then [{:insert games
              :value {:title "{{trim(state.addGame.name)}}"
                :genre Other
                :owned true
                :installed true
                :status ready
                :sizeGb 0
                :local true
                :art local
                :logo "#f5a524"
                :achievements [0 0]}
              :then [{:run openGame :with (node #"{{result.id}}")}]}
            {:set state.addGame.open :to false}]}]}
    :openBackup {:title "Back up and restore games"
      :category Arcadia
      :do [{:dismiss true} {:set state.backup :to {:open true :restored nil}}]}
    :backUp {:title "Back up the installed games"
      :category Arcadia
      :description "Saves which games are installed, with their launch options and update choices, as a JSON file."
      :do [{:saveFile "{{{ format: 'arcadia-backup', version: 1, games: map(filter(data.games, it.installed), { id: it.id, launchOptions: it.launchOptions ?? '', overlay: it.overlay, cloud: it.cloud, updates: it.updates }) }}}"
          :name arcadia-backup.json
          :onDone [{:run notify :with {:text "{{t('backup.saved')}}"}}]}]}
    :restore {:title "Restore a backup"
      :category Arcadia
      :description "Reads a backup and puts back the settings of the games it names that are in the library."
      :do [{:openFile state.backup.restored
          :extensions [json]
          :onDone [{:forEach "{{state.backup.restored.games ?? []}}"
              :as b
              :do [{:if "{{find(computed.owned, it.id == b.id) != null}}"
                  :then [(node :update games
                      #"{{b.id}}"
                      :fields {:launchOptions "{{text(b.launchOptions ?? '')}}" :updates "{{b.updates ?? 'auto'}}"})]}]}
            {:run notify :with {:text "{{t('backup.restored')}}"}}]}]}
    :showInfo {:title "About and help"
      :category Help
      :description "kind is about, support, privacy or system."
      :do [{:dismiss true} {:set state.info :to {:open true :kind "{{kind}}"}}]}
    :openNotice {:title "Open a notification"
      :category Arcadia
      :do [{:set "{{'state.notifications[' + i + '].unread'}}" :to false}
        {:run "{{state.notifications[i].icon == 'download' ? 'showDownloads' : state.notifications[i].icon == 'user-plus' ? 'showCommunity' : 'showStore'}}"}]}
    :enterCouch {:title "Couch mode"
      :category View
      :icon sofa
      :description "Big tiles for a television across the room, the whole screen, arrows and Enter."
      :do [{:dismiss true} {:set state.couch :to {:open true :at 0}} {:fullscreen true} {:focus "#couch-shelf"}]}
    :leaveCouch {:title "Leave couch mode" :category View :do [{:set state.couch.open :to false} {:fullscreen false}]}
    :couchMove {:title "Move along the couch shelf"
      :category View
      :do [{:set state.couch.at :to "{{clamp(state.couch.at + by, 0, length(computed.couchGames) - 1)}}"}]}}
  :styles {:sectionCaps {:type label :font caps :textColor @color.muted :anchors {:left 0 :right 0} :height 18}
    :pillButton {:type label
      :fitText true
      :padding 12
      :height 30
      :radius 15
      :focusable true
      :role button
      :cursor hand
      :color @color.raised
      :textColor @color.text
      :hover {:color @color.hover :textColor @color.bright}}}
  :components {:choice {:note "One choice of a setting, as a pill: lit when it is the current value."
      :params {:title "" :key "" :value nil :current nil}
      :node {:style pillButton
        :role radio
        :text "{{title}}"
        :selected "{{current == value}}"
        :color "{{current == value ? '@color.select' : '@color.raised'}}"
        :textColor "{{current == value ? '@color.bright' : '@color.text'}}"
        :hover {:color "{{current == value ? '@color.select' : '@color.hover'}}" :textColor @color.bright}
        :onClick {:run setSetting :with {:key "{{key}}" :value "{{value}}"}}}}
    :choiceRow {:note "A setting's caps title over its choices, each { value, title }."
      :params {:title "" :key "" :current nil :options [] :top 0}
      :node {:anchors {:left 0 :right 0 :top "{{top}}"}
        :height 56
        :color transparent
        :children [{:style sectionCaps :text "{{upper(title)}}"}
          {:type row
            :anchors {:left 0 :right 0 :top 22}
            :height 30
            :gap 6
            :role radiogroup
            :label "{{title}}"
            :arrows horizontal
            :children [(node :repeat {:for o :in "{{options}}"}
                #"{{'setting-' + slug(key) + '-' + o.value}}"
                :use choice
                :with {:title "{{o.title}}" :key "{{key}}" :value "{{o.value}}" :current "{{current}}"})]}]}}
    :settingsDialog {:note "Settings: a page per category on the left, like a game's properties."
      :params {}
      :node {:use dialog
        :with {:open state.settingsDialog.open
          :width 700
          :height 470
          :title "{{t('menu.settings')}}"
          :color @color.panel
          :textColor @color.text
          :dimColor @color.muted
          :highlight @color.accent
          :buttons [{:title "{{t('dialog.close')}}"}]}
        :children [(node #settings-body :type row :anchors {:left 20 :right 20 :top 52 :bottom 60} :gap 20
            (node #settings-pages :type column :width 150 :alignSelf stretch :gap 2 :role tablist :arrows vertical
              (node :repeat {:for page :in "{{['interface', 'downloads', 'storage', 'account']}}"}
                #"{{'settings-page-' + page}}"
                :type label
                :anchors {:left 0 :right 0}
                :height 30
                :padding 10
                :radius 15
                :role tab
                :focusable true
                :cursor hand
                :selected "{{state.settingsDialog.page == page}}"
                :text "{{t('settings.page.' + page)}}"
                :color "{{state.settingsDialog.page == page ? '@color.select' : 'transparent'}}"
                :textColor "{{state.settingsDialog.page == page ? '@color.bright' : '@color.muted'}}"
                :hover {:textColor @color.bright}
                :onClick {:set state.settingsDialog.page :to "{{page}}"}))
            {:type panel :width 1 :alignSelf stretch :color @color.border}
            (node #settings-page :grow 1 :alignSelf stretch :color transparent
              {:when "{{state.settingsDialog.page == 'interface'}}" :use settingsInterface}
              {:when "{{state.settingsDialog.page == 'downloads'}}" :use settingsDownloads}
              {:when "{{state.settingsDialog.page == 'storage'}}" :use settingsStorage}
              {:when "{{state.settingsDialog.page == 'account'}}" :use settingsAccount}))]}}
    :settingsInterface {:params {}
      :node {:fill true
        :color transparent
        :children [{:use choiceRow
            :with {:top 0
              :title "{{t('settings.theme')}}"
              :key appearance.theme
              :current "{{settings.appearance.theme}}"
              :options "{{[{ value: 'dark', title: t('settings.dark') }, { value: 'light', title: t('settings.light') }]}}"}}
          {:use choiceRow
            :with {:top 64
              :title "{{t('settings.language')}}"
              :key appearance.language
              :current "{{settings.appearance.language}}"
              :options [{:value en :title English} {:value es :title "Español"} {:value fr :title "Français"}]}}
          {:use choiceRow
            :with {:top 128
              :title "{{t('settings.startPage')}}"
              :key appearance.startPage
              :current "{{settings.appearance.startPage}}"
              :options "{{map(['library', 'store', 'community'], { value: it, title: t('nav.' + it) })}}"}}
          {:use choiceRow
            :with {:top 192
              :title "{{t('settings.sortBy')}}"
              :key library.sortBy
              :current "{{settings.library.sortBy}}"
              :options "{{map(['name', 'playtime', 'recent'], { value: it, title: t('sort.' + it) })}}"}}]}}
    :settingsDownloads {:params {}
      :node {:fill true
        :color transparent
        :children [{:use choiceRow
            :with {:top 0
              :title "{{t('settings.limit')}}"
              :key downloads.limit
              :current "{{settings.downloads.limit}}"
              :options "{{map([0, 10, 25], { value: it, title: it == 0 ? t('settings.noLimit') : it + ' MB/s' })}}"}}
          (node #settings-while-playing :type row :anchors {:left 0 :right 0 :top 72} :height 30 :alignItems center
            {:type label :grow 1 :height 22 :text "{{t('settings.whilePlaying')}}" :textColor @color.text}
            (node #settings-while-playing-toggle
              :type toggle
              :width 40
              :height 22
              :label "{{t('settings.whilePlaying')}}"
              :checked "{{settings.downloads.whilePlaying}}"
              :activeColor @color.accent
              :onChange {:set settings.downloads.whilePlaying :to "{{event.value}}"}))
          {:style sectionCaps :anchors {:left 0 :right 0 :top 124} :text "{{upper(t('settings.cache'))}}"}
          {:type label
            :font small
            :wrap true
            :anchors {:left 0 :right 0 :top 146}
            :height 36
            :text "{{t('settings.cacheNote')}}"
            :textColor @color.muted}
          (node #settings-clear-cache
            :style pillButton
            :anchors {:left 0 :top 186}
            :text "{{t('settings.clearCache')}}"
            :onClick clearCache)]}}
    :settingsStorage {:params {}
      :node {:fill true
        :color transparent
        :children [{:style sectionCaps :text "{{upper(t('settings.drive'))}}"}
          (node #storage-used
            :type label
            :font small
            :anchors {:left 0 :right 0 :top 22}
            :height 18
            :text "{{t('settings.used', { used: formatNumber(computed.usedGb + 180, 0, 1), total: formatNumber(1000) })}}"
            :textColor @color.text)
          (node #storage-bar
            :type row
            :anchors {:left 0 :right 0 :top 46}
            :height 12
            :radius 6
            :color @color.raised
            :label "{{t('settings.used', { used: formatNumber(computed.usedGb + 180, 0, 1), total: formatNumber(1000) })}}"
            {:width "18%" :alignSelf stretch :color @color.dim}
            {:repeat {:for g :in "{{computed.installedGames}}" :index i}
              :width "{{text(g.sizeGb / 10) + '%'}}"
              :alignSelf stretch
              :color "{{['@color.accent', '@color.play', '@color.online', '@color.warm', '@color.link'][i % 5]}}"})
          (node #storage-games :type column :anchors {:left 0 :right 0 :top 72 :bottom 0} :scroll vertical
            (node :repeat {:for g :in "{{computed.installedGames}}" :index i}
              #"{{'storage-' + g.id}}"
              :type row
              :anchors {:left 0 :right 0}
              :height 32
              :gap 10
              :alignItems center
              {:type panel
                :width 10
                :height 10
                :radius 5
                :color "{{['@color.accent', '@color.play', '@color.online', '@color.warm', '@color.link'][i % 5]}}"}
              {:type label :grow 1 :height 20 :text "{{g.title}}" :textColor @color.text}
              {:type label
                :font small
                :fitText true
                :text "{{t('properties.sizeValue', { size: formatNumber(g.sizeGb, 0, 1) })}}"
                :textColor @color.muted}
              (node #"{{'storage-uninstall-' + g.id}}"
                :type label
                :font small
                :fitText true
                :padding 6
                :radius 3
                :text "{{t('menu.game.uninstall')}}"
                :textColor "{{g.status == 'running' ? '@color.dim' : '@color.link'}}"
                :disabled "{{g.status == 'running'}}"
                :focusable true
                :role button
                :cursor hand
                :hover {:color @color.hover}
                :onClick {:run askUninstall :with (node #"{{g.id}}")})))]}}
    :settingsAccount {:params {}
      :node {:fill true
        :color transparent
        :children [{:type image
            :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
            :anchors {:left 0 :top 0}
            :width 64
            :height 64
            :radius 32
            :fit cover}
          {:type label
            :font heading
            :anchors {:left 80 :top 8}
            :fitText true
            :text "{{state.user.name}}"
            :textColor @color.bright}
          {:type label
            :font small
            :anchors {:left 80 :top 36}
            :fitText true
            :text "{{t('settings.friendCode', { code: state.user.code })}}"
            :textColor @color.muted}
          {:style sectionCaps :anchors {:left 0 :right 0 :top 88} :text "{{upper(t('settings.wallet'))}}"}
          (node #settings-wallet
            :type label
            :font heading
            :anchors {:left 0 :top 110}
            :fitText true
            :text "{{formatCurrency(state.user.wallet, 'EUR')}}"
            :textColor @color.text)
          (node #settings-redeem
            :style pillButton
            :anchors {:left 0 :top 144}
            :text "{{t('menu.redeem')}}"
            :onClick {:run openCode :with {:kind redeem}})
          (node #settings-sign-out
            :style pillButton
            :anchors {:left 0 :top 196}
            :text "{{t('account.signOut')}}"
            :onClick signOut)]}}
    :codeDialog {:note "A product key (adds its game) or a gift code (adds to the wallet); a wrong code says why."
      :params {}
      :node {:use dialog
        :with {:open state.code.open
          :height 200
          :title "{{t(state.code.kind == 'activate' ? 'code.activateTitle' : 'code.redeemTitle')}}"
          :message "{{t(state.code.kind == 'activate' ? 'code.activateHelp' : 'code.redeemHelp')}}"
          :buttons [{:title "{{t('code.use')}}" :run submitCode :default true :keepOpen true}
            {:title "{{t('dialog.cancel')}}" :run closeCode :keepOpen true}]}
        :children [(node #code-text
            :type textarea
            :height 32
            :padding 8
            :anchors {:left 20 :right 20 :top 80}
            :borderWidth 1
            :borderColor "{{state.code.error != '' ? '@color.warmEnd' : '@color.border'}}"
            :focused {:borderColor @color.accent}
            :bind state.code.text
            :maxLength 40
            :radius 3
            :color @color.field
            :textColor @color.text
            :placeholderColor @color.dim
            :placeholder "{{state.code.kind == 'activate' ? 'XXXXX-XXXXX-XXXXX' : 'GIFT-XXXX-XXXX'}}"
            :label "{{t(state.code.kind == 'activate' ? 'code.activateTitle' : 'code.redeemTitle')}}"
            :autofocus true
            :onSubmit {:run submitCode})
          (node #code-error
            :when "{{state.code.error != ''}}"
            :type label
            :font small
            :anchors {:left 20 :right 20 :top 118}
            :height 18
            :text "{{state.code.error}}"
            :textColor @color.warmEnd
            :live polite)]}}
    :addGameDialog {:note "A game Arcadia doesn't sell, added to the library by its name."
      :params {}
      :node {:use dialog
        :with {:open state.addGame.open
          :height 180
          :title "{{t('menu.addOther')}}"
          :message "{{t('addGame.help')}}"
          :buttons [{:title "{{t('addGame.add')}}" :run addLocalGame :default true} {:title "{{t('dialog.cancel')}}"}]}
        :children [(node #add-game-name
            :type textarea
            :height 32
            :padding 8
            :anchors {:left 20 :right 20 :top 80}
            :borderWidth 1
            :borderColor @color.border
            :focused {:borderColor @color.accent}
            :bind state.addGame.name
            :maxLength 100
            :radius 3
            :color @color.field
            :textColor @color.text
            :placeholderColor @color.dim
            :placeholder "{{t('addGame.placeholder')}}"
            :label "{{t('addGame.placeholder')}}"
            :autofocus true
            :onSubmit {:run addLocalGame})]}}
    :backupDialog {:note "Back up which games are installed (and how they are set up) to a file, or put a backup back."
      :params {}
      :node {:use dialog
        :with {:open state.backup.open
          :width 460
          :height 170
          :title "{{t('menu.backup')}}"
          :message "{{t('backup.help')}}"
          :buttons [{:title "{{t('backup.save')}}" :run backUp :default true}
            {:title "{{t('backup.restore')}}" :run restore}
            {:title "{{t('dialog.cancel')}}"}]}}}
    :infoDialog {:note "About Arcadia, support, privacy and system information: a title and a paragraph."
      :params {}
      :node {:use dialog
        :with {:open state.info.open
          :width 480
          :height 210
          :title "{{t('menu.' + state.info.kind)}}"
          :message "{{t('info.' + state.info.kind, { width: window.width, height: window.height, games: length(computed.owned), installed: length(computed.installedGames), language: settings.appearance.language, theme: settings.appearance.theme })}}"
          :buttons [{:title "{{t('dialog.close')}}" :default true}]}}}
    :clientToast {:params {}
      :node (node #client-toast
        :use toast
        :with {:open state.toast.open
          :message "{{state.toast.text}}"
          :width 340
          :closeLabel "{{t('dialog.close')}}"
          :color @color.raised
          :textColor @color.text
          :accent @color.accent})}
    :offlinePage {:note "In place of the store and friends while offline."
      :params {}
      :node (node #offline-page :fill true :type column :justify center :alignItems center :color @color.page
        {:type column
          :width 420
          :height 140
          :gap 12
          :alignItems center
          :children [{:type label :icon wifi-off :text "" :font nav :fitText true :textColor @color.muted}
            {:type label :font heading :fitText true :text "{{t('offline.title')}}" :textColor @color.text}
            (node #offline-online :style pillButton :text "{{t('offline.goOnline')}}" :onClick toggleOffline)]})}
    :signedOutScreen {:note "After signing out: the account to sign back in to."
      :params {}
      :node (node #signed-out
        :overlay {:placement window :open state.session.signedOut :modal true}
        :fill true
        :type column
        :justify center
        :alignItems center
        :gradient {:kind radial :stops [@color.raised @color.top]}
        {:type column
          :width 320
          :height 300
          :gap 14
          :alignItems center
          :role dialog
          :label "{{t('signIn.title')}}"
          :children [{:type label :font logo :fitText true :text Arcadia :textColor @color.accent}
            {:type image
              :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
              :width 72
              :height 72
              :radius 36
              :fit cover}
            {:type label :font heading :fitText true :text "{{state.user.name}}" :textColor @color.bright}
            (node #sign-in
              :style pillButton
              :color @color.accent
              :textColor @color.top
              :font strong
              :text "{{t('signIn.button', { name: state.user.name })}}"
              :autofocus true
              :onClick signIn)]})}
    :couchMode {:note "Couch mode: the whole screen, big tiles of the games played last, left and right to choose, Enter to play, Escape to leave."
      :params {}
      :node (node #couch
        :overlay {:placement window :open state.couch.open :modal true}
        :fill true
        :gradient {:kind linear :angle 160 :stops [@color.select @color.top]}
        {:type label :font logo :anchors {:left 48 :top 32} :fitText true :text Arcadia :textColor @color.accent}
        (node #couch-leave
          :style pillButton
          :anchors {:right 48 :top 44}
          :icon x
          :text "{{t('couch.leave')}}"
          :onClick leaveCouch)
        (node #couch-shelf
          :type row
          :anchors {:left 48 :right 48 :top 140}
          :height 330
          :gap 24
          :alignItems center
          :focusable true
          :role listbox
          :label "{{t('couch.shelf')}}"
          :autofocus true
          :keys {:left {:run couchMove :with {:by -1}}
            :right {:run couchMove :with {:by 1}}
            :enter {:run primary :with {:game "{{computed.couchGame}}"}}
            :escape leaveCouch}
          (node :repeat {:for g
              :in "{{slice(computed.couchGames, max(0, state.couch.at - 1), max(0, state.couch.at - 1) + 6)}}"
              :index i}
            :let {:on "{{g.id == computed.couchGame.id}}"}
            #"{{'couch-' + g.id}}"
            :type image
            :role option
            :label "{{g.title}}"
            :selected "{{on}}"
            :file "{{'../assets/art/' + (g.art ?? g.id) + '/cover.svg'}}"
            :fit cover
            :width "{{on ? 210 : 150}}"
            :height "{{on ? 315 : 225}}"
            :radius 12
            :borderWidth "{{on ? 4 : 0}}"
            :borderColor @color.accent
            :cursor hand
            :onClick {:set state.couch.at :to "{{indexOf(map(computed.couchGames, it.id), g.id)}}"}))
        {:type label
          :font logo
          :anchors {:left 48 :right 48 :top 500}
          :height 60
          :text "{{computed.couchGame.title ?? ''}}"
          :textColor @color.bright}
        (node #couch-play
          :style pillButton
          :anchors {:left 48 :top 576}
          :height 44
          :radius 22
          :padding 22
          :font button
          :color "{{computed.couchGame.status == 'ready' ? '@color.play' : '@color.accent'}}"
          :textColor @color.top
          :text "{{t(computed.couchGame.status == 'ready' ? 'menu.game.play' : computed.couchGame.status == 'running' ? 'menu.game.stop' : computed.couchGame.status == 'update' ? 'menu.game.update' : 'menu.game.install')}}"
          :onClick {:run primary :with {:game "{{computed.couchGame}}"}})
        {:type label
          :font small
          :anchors {:left 48 :bottom 32}
          :fitText true
          :text "{{t('couch.hint')}}"
          :textColor @color.muted})}}}
