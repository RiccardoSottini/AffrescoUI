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
 :styles {:sectionCaps (label :font caps :textColor @color.muted :anchors {:left 0 :right 0} :height 18)
          :pillButton (label :fitText true
                        :padding 12
                        :height 30
                        :radius 15
                        :focusable true
                        :role button
                        :cursor hand
                        :color @color.raised
                        :textColor @color.text
                        :hover {:color @color.hover :textColor @color.bright})}
 :components {:choice {:note "One choice of a setting, as a pill: lit when it is the current value."
                       :params {:title "" :key "" :value nil :current nil}
                       :node {:style pillButton
                              :role radio
                              :text "{{title}}"
                              :selected "{{current == value}}"
                              :color "{{current == value ? '@color.select' : '@color.raised'}}"
                              :textColor "{{current == value ? '@color.bright' : '@color.text'}}"
                              :hover {:color "{{current == value ? '@color.select' : '@color.hover'}}"
                                      :textColor @color.bright}
                              :onClick {:run setSetting :with {:key "{{key}}" :value "{{value}}"}}}}
              :choiceRow {:note "A setting's caps title over its choices, each { value, title }."
                          :params {:title "" :key "" :current nil :options [] :top 0}
                          :node (node :anchors {:left 0 :right 0 :top "{{top}}"} :height 56 :color transparent
                                  (node :style sectionCaps :text "{{upper(title)}}")
                                  (row :anchors {:left 0 :right 0 :top 22}
                                    :height 30
                                    :gap 6
                                    :role radiogroup
                                    :label "{{title}}"
                                    :arrows horizontal
                                    (node :id "{{'setting-' + slug(key) + '-' + o.value}}"
                                      :repeat {:for o :in "{{options}}"}
                                      :use choice
                                      :with {:title "{{o.title}}"
                                             :key "{{key}}"
                                             :value "{{o.value}}"
                                             :current "{{current}}"})))}
              :settingsDialog {:note "Settings: a page per category on the left, like a game's properties."
                               :params {}
                               :node (node :use dialog
                                       :with {:open state.settingsDialog.open
                                              :width 700
                                              :height 470
                                              :title "{{t('menu.settings')}}"
                                              :color @color.panel
                                              :textColor @color.text
                                              :dimColor @color.muted
                                              :highlight @color.accent
                                              :buttons [{:title "{{t('dialog.close')}}"}]}
                                       (row#settings-body :anchors {:left 20 :right 20 :top 52 :bottom 60} :gap 20
                                         (column#settings-pages :width 150
                                           :alignSelf stretch
                                           :gap 2
                                           :role tablist
                                           :arrows vertical
                                           (label :id "{{'settings-page-' + page}}"
                                             :repeat {:for page
                                                      :in "{{['interface', 'downloads', 'storage', 'account']}}"}
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
                                         (panel :width 1 :alignSelf stretch :color @color.border)
                                         (node#settings-page :grow 1 :alignSelf stretch :color transparent
                                           (node :when "{{state.settingsDialog.page == 'interface'}}"
                                             :use settingsInterface)
                                           (node :when "{{state.settingsDialog.page == 'downloads'}}"
                                             :use settingsDownloads)
                                           (node :when "{{state.settingsDialog.page == 'storage'}}" :use settingsStorage)
                                           (node :when "{{state.settingsDialog.page == 'account'}}" :use settingsAccount))))}
              :settingsInterface {:params {}
                                  :node (node :fill true :color transparent
                                          (node :use choiceRow
                                            :with {:top 0
                                                   :title "{{t('settings.theme')}}"
                                                   :key appearance.theme
                                                   :current "{{settings.appearance.theme}}"
                                                   :options "{{[{ value: 'dark', title: t('settings.dark') }, { value: 'light', title: t('settings.light') }]}}"})
                                          (node :use choiceRow
                                            :with {:top 64
                                                   :title "{{t('settings.language')}}"
                                                   :key appearance.language
                                                   :current "{{settings.appearance.language}}"
                                                   :options [{:value en :title "English"}
                                                             {:value es :title "Español"}
                                                             {:value fr :title "Français"}]})
                                          (node :use choiceRow
                                            :with {:top 128
                                                   :title "{{t('settings.startPage')}}"
                                                   :key appearance.startPage
                                                   :current "{{settings.appearance.startPage}}"
                                                   :options "{{map(['library', 'store', 'community'], { value: it, title: t('nav.' + it) })}}"})
                                          (node :use choiceRow
                                            :with {:top 192
                                                   :title "{{t('settings.sortBy')}}"
                                                   :key library.sortBy
                                                   :current "{{settings.library.sortBy}}"
                                                   :options "{{map(['name', 'playtime', 'recent'], { value: it, title: t('sort.' + it) })}}"}))}
              :settingsDownloads {:params {}
                                  :node (node :fill true :color transparent
                                          (node :use choiceRow
                                            :with {:top 0
                                                   :title "{{t('settings.limit')}}"
                                                   :key downloads.limit
                                                   :current "{{settings.downloads.limit}}"
                                                   :options "{{map([0, 10, 25], { value: it, title: it == 0 ? t('settings.noLimit') : it + ' MB/s' })}}"})
                                          (row#settings-while-playing :anchors {:left 0 :right 0 :top 72}
                                            :height 30
                                            :alignItems center
                                            (label :grow 1
                                              :height 22
                                              :text "{{t('settings.whilePlaying')}}"
                                              :textColor @color.text)
                                            (toggle#settings-while-playing-toggle :width 40
                                              :height 22
                                              :label "{{t('settings.whilePlaying')}}"
                                              :checked "{{settings.downloads.whilePlaying}}"
                                              :activeColor @color.accent
                                              :onChange {:set settings.downloads.whilePlaying :to "{{event.value}}"}))
                                          (node :style sectionCaps
                                            :anchors {:left 0 :right 0 :top 124}
                                            :text "{{upper(t('settings.cache'))}}")
                                          (label :font small
                                            :wrap true
                                            :anchors {:left 0 :right 0 :top 146}
                                            :height 36
                                            :text "{{t('settings.cacheNote')}}"
                                            :textColor @color.muted)
                                          (node#settings-clear-cache :style pillButton
                                            :anchors {:left 0 :top 186}
                                            :text "{{t('settings.clearCache')}}"
                                            :onClick clearCache))}
              :settingsStorage {:params {}
                                :node (node :fill true :color transparent
                                        (node :style sectionCaps :text "{{upper(t('settings.drive'))}}")
                                        (label#storage-used :font small
                                          :anchors {:left 0 :right 0 :top 22}
                                          :height 18
                                          :text "{{t('settings.used', { used: formatNumber(computed.usedGb + 180, 0, 1), total: formatNumber(1000) })}}"
                                          :textColor @color.text)
                                        (row#storage-bar :anchors {:left 0 :right 0 :top 46}
                                          :height 12
                                          :radius 6
                                          :color @color.raised
                                          :label "{{t('settings.used', { used: formatNumber(computed.usedGb + 180, 0, 1), total: formatNumber(1000) })}}"
                                          (node :width "18%" :alignSelf stretch :color @color.dim)
                                          (node :repeat {:for g :in "{{computed.installedGames}}" :index i}
                                            :width "{{text(g.sizeGb / 10) + '%'}}"
                                            :alignSelf stretch
                                            :color "{{['@color.accent', '@color.play', '@color.online', '@color.warm', '@color.link'][i % 5]}}"))
                                        (column#storage-games :anchors {:left 0 :right 0 :top 72 :bottom 0}
                                          :scroll vertical
                                          (row :id "{{'storage-' + g.id}}"
                                            :repeat {:for g :in "{{computed.installedGames}}" :index i}
                                            :anchors {:left 0 :right 0}
                                            :height 32
                                            :gap 10
                                            :alignItems center
                                            (panel :width 10
                                              :height 10
                                              :radius 5
                                              :color "{{['@color.accent', '@color.play', '@color.online', '@color.warm', '@color.link'][i % 5]}}")
                                            (label :grow 1 :height 20 :text "{{g.title}}" :textColor @color.text)
                                            (label :font small
                                              :fitText true
                                              :text "{{t('properties.sizeValue', { size: formatNumber(g.sizeGb, 0, 1) })}}"
                                              :textColor @color.muted)
                                            (label :id "{{'storage-uninstall-' + g.id}}"
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
                                              :onClick {:run askUninstall :with {:id "{{g.id}}"}}))))}
              :settingsAccount {:params {}
                                :node (node :fill true :color transparent
                                        (image :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
                                          :anchors {:left 0 :top 0}
                                          :width 64
                                          :height 64
                                          :radius 32
                                          :fit cover)
                                        (label :font heading
                                          :anchors {:left 80 :top 8}
                                          :fitText true
                                          :text "{{state.user.name}}"
                                          :textColor @color.bright)
                                        (label :font small
                                          :anchors {:left 80 :top 36}
                                          :fitText true
                                          :text "{{t('settings.friendCode', { code: state.user.code })}}"
                                          :textColor @color.muted)
                                        (node :style sectionCaps
                                          :anchors {:left 0 :right 0 :top 88}
                                          :text "{{upper(t('settings.wallet'))}}")
                                        (label#settings-wallet :font heading
                                          :anchors {:left 0 :top 110}
                                          :fitText true
                                          :text "{{formatCurrency(state.user.wallet, 'EUR')}}"
                                          :textColor @color.text)
                                        (node#settings-redeem :style pillButton
                                          :anchors {:left 0 :top 144}
                                          :text "{{t('menu.redeem')}}"
                                          :onClick {:run openCode :with {:kind redeem}})
                                        (node#settings-sign-out :style pillButton
                                          :anchors {:left 0 :top 196}
                                          :text "{{t('account.signOut')}}"
                                          :onClick signOut))}
              :codeDialog {:note "A product key (adds its game) or a gift code (adds to the wallet); a wrong code says why."
                           :params {}
                           :node (node :use dialog
                                   :with {:open state.code.open
                                          :height 200
                                          :title "{{t(state.code.kind == 'activate' ? 'code.activateTitle' : 'code.redeemTitle')}}"
                                          :message "{{t(state.code.kind == 'activate' ? 'code.activateHelp' : 'code.redeemHelp')}}"
                                          :buttons [{:title "{{t('code.use')}}"
                                                     :run submitCode
                                                     :default true
                                                     :keepOpen true}
                                                    {:title "{{t('dialog.cancel')}}" :run closeCode :keepOpen true}]}
                                   (textarea#code-text :height 32
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
                                   (label#code-error :when "{{state.code.error != ''}}"
                                     :font small
                                     :anchors {:left 20 :right 20 :top 118}
                                     :height 18
                                     :text "{{state.code.error}}"
                                     :textColor @color.warmEnd
                                     :live polite))}
              :addGameDialog {:note "A game Arcadia doesn't sell, added to the library by its name."
                              :params {}
                              :node (node :use dialog
                                      :with {:open state.addGame.open
                                             :height 180
                                             :title "{{t('menu.addOther')}}"
                                             :message "{{t('addGame.help')}}"
                                             :buttons [{:title "{{t('addGame.add')}}" :run addLocalGame :default true}
                                                       {:title "{{t('dialog.cancel')}}"}]}
                                      (textarea#add-game-name :height 32
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
                                        :onSubmit {:run addLocalGame}))}
              :backupDialog {:note "Back up which games are installed (and how they are set up) to a file, or put a backup back."
                             :params {}
                             :node (node :use dialog
                                     :with {:open state.backup.open
                                            :width 460
                                            :height 170
                                            :title "{{t('menu.backup')}}"
                                            :message "{{t('backup.help')}}"
                                            :buttons [{:title "{{t('backup.save')}}" :run backUp :default true}
                                                      {:title "{{t('backup.restore')}}" :run restore}
                                                      {:title "{{t('dialog.cancel')}}"}]})}
              :infoDialog {:note "About Arcadia, support, privacy and system information: a title and a paragraph."
                           :params {}
                           :node (node :use dialog
                                   :with {:open state.info.open
                                          :width 480
                                          :height 210
                                          :title "{{t('menu.' + state.info.kind)}}"
                                          :message "{{t('info.' + state.info.kind, { width: window.width, height: window.height, games: length(computed.owned), installed: length(computed.installedGames), language: settings.appearance.language, theme: settings.appearance.theme })}}"
                                          :buttons [{:title "{{t('dialog.close')}}" :default true}]})}
              :clientToast {:params {}
                            :node (node#client-toast :use toast
                                    :with {:open state.toast.open
                                           :message "{{state.toast.text}}"
                                           :width 340
                                           :closeLabel "{{t('dialog.close')}}"
                                           :color @color.raised
                                           :textColor @color.text
                                           :accent @color.accent})}
              :offlinePage {:note "In place of the store and friends while offline."
                            :params {}
                            :node (column#offline-page :fill true :justify center :alignItems center :color @color.page
                                    (column :width 420 :height 140 :gap 12 :alignItems center
                                      (label :icon wifi-off :text "" :font nav :fitText true :textColor @color.muted)
                                      (label :font heading
                                        :fitText true
                                        :text "{{t('offline.title')}}"
                                        :textColor @color.text)
                                      (node#offline-online :style pillButton
                                        :text "{{t('offline.goOnline')}}"
                                        :onClick toggleOffline)))}
              :signedOutScreen {:note "After signing out: the account to sign back in to."
                                :params {}
                                :node (column#signed-out :overlay {:placement window
                                                                   :open state.session.signedOut
                                                                   :modal true}
                                        :fill true
                                        :justify center
                                        :alignItems center
                                        :gradient {:kind radial :stops [@color.raised @color.top]}
                                        (column :width 320
                                          :height 300
                                          :gap 14
                                          :alignItems center
                                          :role dialog
                                          :label "{{t('signIn.title')}}"
                                          (label :font logo :fitText true :text "Arcadia" :textColor @color.accent)
                                          (image :file "{{'../assets/art/avatars/' + state.user.avatar + '.svg'}}"
                                            :width 72
                                            :height 72
                                            :radius 36
                                            :fit cover)
                                          (label :font heading
                                            :fitText true
                                            :text "{{state.user.name}}"
                                            :textColor @color.bright)
                                          (node#sign-in :style pillButton
                                            :color @color.accent
                                            :textColor @color.top
                                            :font strong
                                            :text "{{t('signIn.button', { name: state.user.name })}}"
                                            :autofocus true
                                            :onClick signIn)))}
              :couchMode {:note "Couch mode: the whole screen, big tiles of the games played last, left and right to choose, Enter to play, Escape to leave."
                          :params {}
                          :node (node#couch :overlay {:placement window :open state.couch.open :modal true}
                                  :fill true
                                  :gradient {:kind linear :angle 160 :stops [@color.select @color.top]}
                                  (label :font logo
                                    :anchors {:left 48 :top 32}
                                    :fitText true
                                    :text "Arcadia"
                                    :textColor @color.accent)
                                  (node#couch-leave :style pillButton
                                    :anchors {:right 48 :top 44}
                                    :icon x
                                    :text "{{t('couch.leave')}}"
                                    :onClick leaveCouch)
                                  (row#couch-shelf :anchors {:left 48 :right 48 :top 140}
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
                                    (image :id "{{'couch-' + g.id}}"
                                      :repeat {:for g
                                               :in "{{slice(computed.couchGames, max(0, state.couch.at - 1), max(0, state.couch.at - 1) + 6)}}"
                                               :index i}
                                      :let {:on "{{g.id == computed.couchGame.id}}"}
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
                                      :onClick {:set state.couch.at
                                                :to "{{indexOf(map(computed.couchGames, it.id), g.id)}}"}))
                                  (label :font logo
                                    :anchors {:left 48 :right 48 :top 500}
                                    :height 60
                                    :text "{{computed.couchGame.title ?? ''}}"
                                    :textColor @color.bright)
                                  (node#couch-play :style pillButton
                                    :anchors {:left 48 :top 576}
                                    :height 44
                                    :radius 22
                                    :padding 22
                                    :font button
                                    :color "{{computed.couchGame.status == 'ready' ? '@color.play' : '@color.accent'}}"
                                    :textColor @color.top
                                    :text "{{t(computed.couchGame.status == 'ready' ? 'menu.game.play' : computed.couchGame.status == 'running' ? 'menu.game.stop' : computed.couchGame.status == 'update' ? 'menu.game.update' : 'menu.game.install')}}"
                                    :onClick {:run primary :with {:game "{{computed.couchGame}}"}})
                                  (label :font small
                                    :anchors {:left 48 :bottom 32}
                                    :fitText true
                                    :text "{{t('couch.hint')}}"
                                    :textColor @color.muted))}}}
