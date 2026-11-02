{:$schema "../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :strings {:default en :locale "{{state.locale == 'system' ? '' : state.locale}}"}
 :note "The AffrescoUI Launcher: the home of the example apps and of the person's own. Apps (your apps and the examples as cards, each tagged Example or Yours; a search; a click or Enter opens one), My apps (open, duplicate, show in the folder, delete after a confirm), Create (a name, a starting point, a description for the assistant, features from other apps) and Settings. It is a screen like any other, with no C++ of its own: affrescoui_launcher opens it when started without a file and grants it the folders it reads and writes (the services examples and apps) and the programs that open the apps (the service launch). Where nothing is granted (the rendering check), the services are refused and it shows the examples from apps.json and an empty My apps."
 :window {:title "AffrescoUI"
          :icon "../assets/affrescoui-icon.png"
          :width 1200
          :height 780
          :minWidth 800
          :minHeight 600
          :focusRing {:color @c.accent :width 2 :offset 2}}
 :include ["locales/en.lisp"
           "locales/es.lisp"
           "locales/fr.lisp"
           apps.lisp
           "../../shared/addons/intelligence/settings.lisp"]
 :theme "{{state.theme == 'system' ? window.colorScheme : state.theme}}"
 :persist [state.theme state.locale state.notation]
 :onLoad {:run launcher.refresh}
 :permissions {:allow [studio]
               :ask [agent]
               :reviewers [studio]
               :budgets [{:author agent :added 20 :removed 5 :changed 50}]}
 :services {:examples {:provider fs :root examples :readOnly true}
            :apps {:provider fs :root apps :copyFrom [examples]}
            :launch {:provider process :process affresco}}
 :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 15 :fallback [icons]}
         :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 13 :fallback [icons symbols]}
         :symbols {:file "../../shared/fonts/JetBrainsMono-Regular.ttf" :size 13}
         :strong {:file "../../shared/fonts/Roboto-Regular.ttf" :size 16 :bold true :fallback [icons]}
         :heading {:file "../../shared/fonts/Roboto-Regular.ttf" :size 19 :bold true :fallback [icons]}
         :title {:file "../../shared/fonts/Roboto-Regular.ttf" :size 30 :bold true}
         :brand {:file "../../shared/fonts/Roboto-Regular.ttf" :size 18 :bold true}
         :cover {:file "../../shared/fonts/Roboto-Regular.ttf" :size 64 :bold true}
         :initial {:file "../../shared/fonts/Roboto-Regular.ttf" :size 20 :bold true}
         :icons {:file "../../shared/fonts/Lucide.ttf" :size 15}
         :bigIcons {:file "../../shared/fonts/Lucide.ttf" :size 22}
         :heroIcons {:file "../../shared/fonts/Lucide.ttf" :size 44}}
 :tokens {:c.bg {:light "#f5f4fa" :dark "#101017"}
          :c.side {:light "#ffffff" :dark "#16161e"}
          :c.surface {:light "#ffffff" :dark "#1c1c26"}
          :c.raised {:light "#f0eff7" :dark "#252532"}
          :c.hover {:light "#e9e7f5" :dark "#2c2c3b"}
          :c.border {:light "#e3e1ee" :dark "#2b2b39"}
          :c.text {:light "#1b1a28" :dark "#ececf4"}
          :c.muted {:light "#5d5b72" :dark "#a4a2b9"}
          :c.dim {:light "#8d8ba2" :dark "#6e6c84"}
          :c.accent {:light "#5b4fe0" :dark "#8b80ff"}
          :c.accentHover {:light "#4a3fcc" :dark "#a097ff"}
          :c.accentSoft {:light "#ecebff" :dark "#28244d"}
          :c.onAccent "#ffffff"
          :c.danger {:light "#cf3446" :dark "#ff6b7a"}
          :c.dangerSoft {:light "#fdecee" :dark "#3a1d24"}
          :c.success {:light "#1f9d62" :dark "#4ad08f"}
          :c.shadow {:light "#2a1f7a1f" :dark "#00000066"}
          :kit.background @c.bg
          :kit.surface @c.surface
          :kit.bar @c.raised
          :kit.barHover @c.hover
          :kit.field @c.surface
          :kit.text @c.text
          :kit.dim @c.dim
          :kit.body @c.muted
          :kit.selection @c.accentSoft
          :kit.border @c.border
          :kit.primary @c.accent
          :kit.onPrimary "#ffffff"
          :kit.secondary @c.raised
          :kit.onSecondary @c.text
          :kit.accent @c.accent
          :kit.focus @c.accent
          :intelligence.accent @c.accent
          :intelligence.accentHover @c.accentHover
          :intelligence.onAccent @c.onAccent
          :intelligence.surface @c.surface
          :intelligence.raised @c.raised
          :intelligence.selected @c.accentSoft
          :intelligence.border @c.border
          :intelligence.text @c.text
          :intelligence.dim @c.muted
          :intelligence.success @c.success
          :intelligence.danger @c.danger
          :kit.radius 8
          :kit.cardRadius 14}
 :state {:page apps
         :query ""
         :theme system
         :locale system
         :notation json
         :mine []
         :mineScan []
         :mineNext []
         :mineLoaded false
         :mineError ""
         :mineRun 0
         :create {:name "" :from blank :describe "" :features [] :tried false :busy false :error ""}
         :confirmOpen false
         :confirm {:id "" :name ""}
         :toasts []
         :tileColors ["#5b4fe0" "#e0569a" "#1f9d62" "#e08a1e" "#2a86d6" "#9b4fd6"]
         :featureChoices [{:id board
                           :from "Flow"
                           :ask "a Kanban board with columns, cards and drag and drop, like Flow's (examples/flow)"}
                          {:id palette
                           :from "Flow"
                           :ask "a command palette (Cmd+K) listing the app's actions, like Flow's"}
                          {:id charts
                           :from "Haven"
                           :ask "charts from ../../shared/components/charts.json, like Haven's energy page"}
                          {:id heatmap :from "Streak" :ask "a year heatmap of daily entries, like Streak's"}
                          {:id settings :from "Arcadia" :ask "a settings page with declared settings, like Arcadia's"}
                          {:id signIn
                           :from "Sign in"
                           :ask "sign-in and create-account screens with checked fields, like the Sign in app's"}
                          {:id slideSorter
                           :from "Sinopia"
                           :ask "Sinopia's slide sorter (the installable part slideSorter of app:sinopia), bound with an adapter"}]}
 :stateTypes {:page {:enum [apps mine create settings]}
              :theme {:enum [system light dark]}
              :locale {:enum [system en es fr]}
              :notation {:enum [json lisp]}}
 :computed {:wide "{{window.width >= 960}}"
            :sideWidth "{{computed.wide ? 232 : 72}}"
            :columns "{{max(1, floor((window.width - computed.sideWidth - 52) / 290))}}"
            :everyApp "{{concat(map(state.mine, {id: it.id, name: it.name, modified: it.modified, mine: true}), state.examples)}}"
            :shown "{{filter(computed.everyApp, trim(state.query) == '' || contains(lower(it.name + ' ' + (it.mine ? it.id : t('tagline.' + it.id))), lower(trim(state.query))))}}"
            :starters "{{filter(state.examples, it.program == 'host' && it.starter != false)}}"
            :reserved "{{concat(map(state.examples, it.folder), ['shared', 'launcher'])}}"
            :myIds "{{map(state.mine, it.id)}}"
            :slug "{{slice(join(filter(split(join(map(split(lower(trim(state.create.name)), ''), contains('abcdefghijklmnopqrstuvwxyz0123456789', it) ? it : ' '), ''), ' '), it != ''), '-'), 0, 40)}}"
            :askText "{{trim(trim(state.create.describe) + (length(state.create.features) > 0 ? '\n\nAlso add these features from other apps: ' + join(map(filter(state.featureChoices, contains(state.create.features, it.id)), it.ask), '; ') + '.' : ''))}}"
            :createError "{{trim(state.create.name) == '' ? 'create.error.empty' : length(trim(state.create.name)) > 80 ? 'create.error.long' : contains(state.create.name, '{' + '{') || contains(state.create.name, '}' + '}') ? 'create.error.braces' : computed.slug == '' ? 'create.error.letters' : contains(computed.reserved, computed.slug) ? 'create.error.reserved' : contains(computed.myIds, computed.slug) ? 'create.error.exists' : length(computed.askText) > 4000 ? 'create.error.describe' : ''}}"
            :fromName "{{state.create.from == 'blank' ? t('create.blank') : (find(state.examples, it.id == state.create.from).name ?? '')}}"
            :excluded "{{['*.cpp', '*.hpp', '*.h', '*.c', '*.cc', '*.mm', '*.cmake', 'CMakeLists.txt', '*.py', '.*']}}"}
 :actions {:go [{:note "with: page"} {:set state.page :to "{{page}}"}]
           :toast [{:note "with: message, error"}
                   {:push state.toasts :value {:message "{{message}}" :color "{{error ? '@c.danger' : '@c.accent'}}"}}
                   {:debounce 4000 :do [{:set state.toasts :to []}]}]
           :launcher.refresh [{:note "Reads the apps folder again. Each read is numbered (mineRun): one started later makes an earlier one, still reading, stop, so two never fill the list together"}
                              {:increment state.mineRun}
                              {:run listMine :with {:run "{{state.mineRun}}"}}]
           :listMine [{:note "with: run"}
                      {:invoke apps.list
                       :with {:path "."}
                       :onDone [{:if "{{run != state.mineRun}}" :then [{:stop true}]}
                                {:set state.mineScan
                                 :to "{{filter(result.entries, it.kind == 'folder' && it.name != 'shared' && slice(it.name, 0, 1) != '.')}}"}
                                {:set state.mineNext :to []}
                                {:set state.mineError :to ""}
                                {:run loadMine :with {:i 0 :run "{{run}}"}}]
                       :onError [{:if "{{run != state.mineRun}}" :then [{:stop true}]}
                                 {:set state.mineError :to "{{error.message}}"}
                                 {:set state.mineLoaded :to true}]}]
           :launcher.show [{:note "Shows a list of the person's apps as the folder gave it (event.mine: [{id, name, modified}]): scenarios play the folder's answer with it"}
                           {:set state.mine :to "{{event.mine ?? []}}"}
                           {:set state.mineError :to ""}
                           {:set state.mineLoaded :to true}]
           :loadMine [{:note "with: i, run. Reads the apps one after another (an fs service runs 16 calls at once): each folder's screen's modified time and window title; the last changed first. A folder holds its screen, or a json folder (a copy of an example: loadMineCopy), or a lisp folder (a copy of an example written as a program: loadMineProgram). Each call's answer runs the next action by name, so a long list nests no deeper than it must where calls are answered at once"}
                      {:if "{{run != state.mineRun}}" :then [{:stop true}]}
                      {:if "{{i >= length(state.mineScan)}}"
                       :then [{:set state.mine :to "{{sort(state.mineNext, it.modified, 'desc')}}"}
                              {:set state.mineLoaded :to true}
                              {:stop true}]}
                      {:invoke apps.stat
                       :with {:path "{{state.mineScan[i].name + '/index.screen.json'}}"}
                       :onDone [{:run "{{result.exists == true && result.kind == 'file' ? 'readMine' : 'loadMineCopy'}}"
                                 :with {:i "{{i}}" :sub "" :modified "{{result.modified ?? 0}}" :run "{{run}}"}}]
                       :onError [{:run loadMineCopy :with {:i "{{i}}" :run "{{run}}"}}]}]
           :loadMineCopy [{:note "with: i, run"}
                          {:invoke apps.stat
                           :with {:path "{{state.mineScan[i].name + '/json/index.screen.json'}}"}
                           :onDone [{:run "{{result.exists == true && result.kind == 'file' ? 'readMine' : 'loadMineProgram'}}"
                                     :with {:i "{{i}}" :sub "/json" :modified "{{result.modified ?? 0}}" :run "{{run}}"}}]
                           :onError [{:run loadMineProgram :with {:i "{{i}}" :run "{{run}}"}}]}]
           :loadMineProgram [{:note "with: i, run. A copy of an example written as a program: a lisp folder, with its app.lisp; a folder with none of the three is left out"}
                             {:invoke apps.stat
                              :with {:path "{{state.mineScan[i].name + '/lisp/app.lisp'}}"}
                              :onDone [{:run "{{result.exists == true && result.kind == 'file' ? 'readMine' : 'loadMine'}}"
                                        :with {:i "{{result.exists == true && result.kind == 'file' ? i : i + 1}}"
                                               :sub "/lisp"
                                               :modified "{{result.modified ?? 0}}"
                                               :run "{{run}}"}}]
                              :onError [{:run loadMine :with {:i "{{i + 1}}" :run "{{run}}"}}]}]
           :readMine [{:note "with: i, sub, modified, run. The name is the window title of a JSON app's screen, or a program's name in its app.lisp"}
                      {:invoke apps.read
                       :with {:path "{{state.mineScan[i].name + sub + (sub == '/lisp' ? '/app.lisp' : '/index.screen.json')}}"}
                       :onDone [{:if "{{run != state.mineRun}}" :then [{:stop true}]}
                                {:push state.mineNext
                                 :value {:id "{{state.mineScan[i].name}}"
                                         :sub "{{sub}}"
                                         :name "{{(sub == '/lisp' ? formKeyword(result.text, 'name') : (parseJson(result.text) ?? {}).window.title) ?? state.mineScan[i].name}}"
                                         :modified "{{modified}}"}}
                                {:run loadMine :with {:i "{{i + 1}}" :run "{{run}}"}}]
                       :onError [{:run loadMine :with {:i "{{i + 1}}" :run "{{run}}"}}]}]
           :listNew [{:note "with: id, sub, title. Shows an app just made at the top of the list at once, before the folder is read again (launcher.refresh, a moment later: after the calls that made it have ended, so what reads the folder doesn't nest in them)"}
                     {:set state.mine
                      :to "{{concat([{id: id, sub: sub, name: title, modified: now()}], filter(state.mine, it.id != id))}}"}]
           :openExample [{:note "with: app"}
                         {:invoke launch.run
                          :with {:arguments "{{app.program == 'host' ? ['affrescoui_launcher', 'examples:' + app.folder + '/' + (app.notation ?? state.notation)] : [app.program]}}"}
                          :onDone [{:run toast :with {:message "{{t('toast.opening', {name: app.name})}}" :error false}}]
                          :onError [{:run toast
                                     :with {:message "{{t('toast.openFailed', {name: app.name, why: error.message})}}"
                                            :error true}}]}]
           :openMine [{:note "with: id, name, ask (empty for none)"}
                      {:invoke launch.run
                       :with {:arguments "{{concat(['affrescoui_launcher', 'apps:' + id + ((find(state.mine, it.id == id) ?? {}).sub == '/lisp' ? '/lisp' : ((find(state.mine, it.id == id) ?? {}).sub ?? '') + '/index.screen.json')], ask != '' ? ['--ask', ask] : [])}}"}
                       :onDone [{:run toast :with {:message "{{t('toast.opening', {name: name})}}" :error false}}]
                       :onError [{:run toast
                                  :with {:message "{{t('toast.openFailed', {name: name, why: error.message})}}"
                                         :error true}}]}]
           :revealMine [{:note "with: id ('' for the apps folder itself)"}
                        {:invoke launch.run
                         :with {:arguments [reveal "{{'apps:' + id}}"]}
                         :onError [{:run toast :with {:message "{{error.message}}" :error true}}]}]
           :askDelete [{:note "with: id, name"}
                       {:set state.confirm :to {:id "{{id}}" :name "{{name}}"}}
                       {:set state.confirmOpen :to true}]
           :deleteMine [{:invoke apps.delete
                         :with {:path "{{state.confirm.id}}"}
                         :onDone [{:run toast
                                   :with {:message "{{t('toast.deleted', {name: state.confirm.name})}}" :error false}}
                                  {:after 1 :do [{:run launcher.refresh}]}]
                         :onError [{:run toast :with {:message "{{error.message}}" :error true}}]}]
           :duplicateMine [{:note "with: id, name. The copy's folder is the first free <id>-copy, <id>-copy-2, ..."}
                           {:run copyMine
                            :with {:id "{{id}}"
                                   :name "{{name}}"
                                   :to "{{find(map(range(50), index == 0 ? id + '-copy' : id + '-copy-' + (index + 1)), !contains(computed.myIds, it))}}"}}]
           :copyMine [{:note "with: id, name, to"}
                      {:invoke apps.copy
                       :with {:path "{{id}}" :to "{{to}}"}
                       :onDone [{:run retitle
                                 :with {:id "{{to}}"
                                        :sub "{{(find(state.mine, it.id == id) ?? {}).sub ?? ''}}"
                                        :title "{{t('mine.copyName', {name: name})}}"
                                        :ask ""
                                        :then copied}}]
                       :onError [{:run toast :with {:message "{{error.message}}" :error true}}]}]
           :copied [{:run listNew :with {:id "{{id}}" :sub "{{sub}}" :title "{{title}}"}}
                    {:run toast :with {:message "{{t('toast.duplicated', {name: title})}}" :error false}}
                    {:after 1 :do [{:run launcher.refresh}]}]
           :retitle [{:note "with: id, sub (where the app's files are in its folder: '', '/json' or '/lisp'), title, then (an action, run with the same variables). Sets the window title of its index.screen.json, as text, so the file keeps its layout; a program's through its keywords (retitleProgram)"}
                     {:if "{{sub == '/lisp'}}"
                      :then [{:run retitleProgram
                              :with {:id "{{id}}" :title "{{title}}" :ask "{{ask}}" :then "{{then}}"}}
                             {:stop true}]}
                     {:invoke apps.read
                      :with {:path "{{id + sub + '/index.screen.json'}}"}
                      :onDone [{:invoke apps.write
                                :with {:path "{{id + sub + '/index.screen.json'}}"
                                       :text "{{(parseJson(result.text) ?? {}).window.title == null ? result.text : join(split(result.text, json((parseJson(result.text) ?? {}).window.title)), json(title))}}"}
                                :onDone [{:run reidentify
                                          :with {:id "{{id}}"
                                                 :sub "{{sub}}"
                                                 :title "{{title}}"
                                                 :ask "{{ask}}"
                                                 :then "{{then}}"}}]
                                :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                      :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
           :retitleProgram [{:note "with: id, title, ask, then. A program copied into a lisp folder: the :window :title of the entry its app.lisp names is set, by a keyword edit that keeps every other character, then its app.lisp's :id and :name"}
                            {:invoke apps.read
                             :with {:path "{{id + '/lisp/app.lisp'}}"}
                             :onDone [{:run retitleEntry
                                       :with {:id "{{id}}"
                                              :entry "{{formKeyword(result.text, 'entry') ?? 'index.screen.lisp'}}"
                                              :title "{{title}}"
                                              :ask "{{ask}}"
                                              :then "{{then}}"}}]
                             :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
           :retitleEntry [{:note "with: id, entry, title, ask, then. The entry an app.lisp names is a file of its folder: one going elsewhere (a path, a folder up) is taken as index.screen.lisp, so a copy never writes outside itself"}
                          {:if "{{type(entry) != 'string' || entry == '' || slice(entry, 0, 1) == '.' || contains(entry, '/') || contains(entry, '\\\\') || contains(entry, ':')}}"
                           :then [{:run retitleEntry
                                   :with {:id "{{id}}"
                                          :entry index.screen.lisp
                                          :title "{{title}}"
                                          :ask "{{ask}}"
                                          :then "{{then}}"}}
                                  {:stop true}]}
                          {:invoke apps.read
                           :with {:path "{{id + '/lisp/' + entry}}"}
                           :onDone [{:invoke apps.write
                                     :with {:path "{{id + '/lisp/' + entry}}"
                                            :text "{{setFormKeyword(result.text, ['window', 'title'], title) ?? result.text}}"}
                                     :onDone [{:run reidentifyProgram
                                               :with {:id "{{id}}" :title "{{title}}" :ask "{{ask}}" :then "{{then}}"}}]
                                     :onError [{:run createFailed
                                                :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                           :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
           :reidentifyProgram [{:note "with: id, title, ask, then. Gives the copy's app.lisp the new app's :id and :name, as reidentify does an app.json"}
                               {:invoke apps.read
                                :with {:path "{{id + '/lisp/app.lisp'}}"}
                                :onDone [{:invoke apps.write
                                          :with {:path "{{id + '/lisp/app.lisp'}}"
                                                 :text "{{setFormKeyword(setFormKeyword(result.text, 'id', id) ?? result.text, 'name', title) ?? result.text}}"}
                                          :onDone [{:run "{{then}}"
                                                    :with {:id "{{id}}" :sub "/lisp" :title "{{title}}" :ask "{{ask}}"}}]
                                          :onError [{:run createFailed
                                                     :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                                :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
           :reidentify [{:note "with: id, sub, title, ask, then. Gives a copy's app.json (a starter has none) the new app's id and name, as text, so it keeps its data and permissions apart from the app it was copied from"}
                        {:invoke apps.read
                         :with {:path "{{id + sub + '/app.json'}}"}
                         :onDone [{:invoke apps.write
                                   :with {:path "{{id + sub + '/app.json'}}"
                                          :text "{{join(split(join(split(result.text, '\"id\": ' + json((parseJson(result.text) ?? {}).id)), '\"id\": ' + json(id)), '\"name\": ' + json((parseJson(result.text) ?? {}).name)), '\"name\": ' + json(title))}}"}
                                   :onDone [{:run "{{then}}"
                                             :with {:id "{{id}}" :sub "{{sub}}" :title "{{title}}" :ask "{{ask}}"}}]
                                   :onError [{:run createFailed
                                              :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                         :onError [{:run "{{then}}"
                                    :with {:id "{{id}}" :sub "{{sub}}" :title "{{title}}" :ask "{{ask}}"}}]}]
           :startFrom [{:note "with: id"}
                       {:set state.create.from :to "{{id}}"}
                       {:set state.page :to create}
                       {:focus "#createName"}]
           :toggleFeature [{:note "with: id"}
                           {:set state.create.features
                            :to "{{contains(state.create.features, id) ? filter(state.create.features, it != id) : concat(state.create.features, [id])}}"}]
           :resetCreate [{:set state.create
                          :to {:name "" :from blank :describe "" :features [] :tried false :busy false :error ""}}]
           :createApp [{:set state.create.tried :to true}
                       {:set state.create.error :to ""}
                       {:if "{{computed.createError != '' || state.create.busy}}"
                        :then [{:focus "#createName"} {:stop true}]}
                       {:set state.create.busy :to true}
                       {:run "{{state.create.from == 'blank' ? 'createBlank' : 'createCopy'}}"
                        :with {:id "{{computed.slug}}"
                               :title "{{trim(state.create.name)}}"
                               :ask "{{computed.askText}}"
                               :folder "{{(find(state.examples, it.id == state.create.from) ?? {}).folder ?? ''}}"
                               :sub "{{(find(state.examples, it.id == state.create.from) ?? {}).notation == 'lisp' ? '/lisp' : '/json'}}"
                               :then createShared}}]
           :createBlank [{:note "with: id, title, ask, then. A folder, and the blank starter in it"}
                         {:invoke apps.mkdir
                          :with {:path "{{id}}"}
                          :onDone [{:invoke apps.copy
                                    :with {:from examples
                                           :path "launcher/json/starters/blank.screen.json"
                                           :to "{{id + '/index.screen.json'}}"}
                                    :onDone [{:run retitle
                                              :with {:id "{{id}}"
                                                     :sub ""
                                                     :title "{{title}}"
                                                     :ask "{{ask}}"
                                                     :then "{{then}}"}}]
                                    :onError [{:run createFailed
                                               :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                          :onError [{:run createFailed
                                     :with {:id "{{id}}"
                                            :why "{{error.code == 'exists' ? t('create.error.taken', {folder: id}) : error.message}}"
                                            :keep true}}]}]
           :createCopy [{:note "with: id, title, ask, folder, sub, then. A copy of an example's folder (sub: '/json', or '/lisp' for a program) and its assets, without its C++, into a folder of that name and an assets folder: its paths lead to the shared folder from there"}
                        {:invoke apps.mkdir
                         :with {:path "{{id}}"}
                         :onDone [{:invoke apps.copy
                                   :with {:from examples
                                          :path "{{folder + sub}}"
                                          :to "{{id + sub}}"
                                          :exclude "{{computed.excluded}}"}
                                   :onDone [{:invoke apps.copy
                                             :with {:from examples
                                                    :path "{{folder + '/assets'}}"
                                                    :to "{{id + '/assets'}}"
                                                    :exclude "{{computed.excluded}}"}
                                             :onDone [{:run retitle
                                                       :with {:id "{{id}}"
                                                              :sub "{{sub}}"
                                                              :title "{{title}}"
                                                              :ask "{{ask}}"
                                                              :then "{{then}}"}}]
                                             :onError [{:run retitle
                                                        :with {:id "{{id}}"
                                                               :sub "{{sub}}"
                                                               :title "{{title}}"
                                                               :ask "{{ask}}"
                                                               :then "{{then}}"}}]}]
                                   :onError [{:run createFailed
                                              :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
                         :onError [{:run createFailed
                                    :with {:id "{{id}}"
                                           :why "{{error.code == 'exists' ? t('create.error.taken', {folder: id}) : error.message}}"
                                           :keep true}}]}]
           :createShared [{:note "with: id, title, ask. The fonts, add-ons and components the apps name as ../shared, brought up to date beside them"}
                          {:invoke apps.copy
                           :with {:from examples :path shared :to shared :merge true :exclude "{{computed.excluded}}"}
                           :onDone [{:run listNew :with {:id "{{id}}" :sub "{{sub}}" :title "{{title}}"}}
                                    {:run resetCreate}
                                    {:set state.page :to mine}
                                    {:run openMine :with {:id "{{id}}" :name "{{title}}" :ask "{{ask}}"}}
                                    {:after 1 :do [{:run launcher.refresh}]}]
                           :onError [{:run createFailed :with {:id "{{id}}" :why "{{error.message}}" :keep false}}]}]
           :createFailed [{:note "with: id, why, keep (the folder isn't the new app's: it was there, or never made)"}
                          {:set state.create.busy :to false}
                          {:set state.create.error :to "{{t('create.failed', {why: why})}}"}
                          {:if "{{keep != true && id != ''}}"
                           :then [{:invoke apps.delete :with {:path "{{id}}"} :onError []}]}]
           :showApps {:title "{{t('nav.apps')}}" :category view :icon layout-grid :do [{:run go :with {:page apps}}]}
           :showMine {:title "{{t('nav.mine')}}" :category view :icon folder-heart :do [{:run go :with {:page mine}}]}
           :showCreate {:title "{{t('nav.create')}}"
                        :category view
                        :icon circle-plus
                        :do [{:run go :with {:page create}} {:focus "#createName"}]}
           :showSettings {:title "{{t('nav.settings')}}"
                          :category view
                          :icon settings
                          :do [{:run go :with {:page settings}}]}
           :findApp {:title "{{t('apps.search')}}"
                     :category view
                     :icon search
                     :do [{:run go :with {:page apps}} {:focus "#appSearch"}]}}
 :keys {"mod+1" showApps
        "mod+2" showMine
        "mod+3" showCreate
        "mod+n" showCreate
        "mod+comma" showSettings
        "mod+4" showSettings
        "mod+f" findApp
        "mod+r" launcher.refresh}
 :styles {:pageTitle (label :font title :textColor @c.text :anchors {:left 36 :top 30} :fitText true)
          :pageSub (label :textColor @c.muted :anchors {:left 36 :right 36 :top 72} :height 20)
          :sectionTitle (label :font heading :textColor @c.text :height 24)
          :hint (label :font small :textColor @c.muted :height 18)
          :primary (label :align center
                     :radius 10
                     :color @c.accent
                     :textColor @c.onAccent
                     :font strong
                     :cursor hand
                     :focusable true
                     :role button
                     :hover {:color @c.accentHover}
                     :transition {:color 120})
          :secondary (label :align center
                       :radius 10
                       :color @c.raised
                       :textColor @c.text
                       :cursor hand
                       :focusable true
                       :role button
                       :hover {:color @c.hover}
                       :transition {:color 120})
          :iconButton (label :align center
                        :radius 8
                        :color transparent
                        :textColor @c.muted
                        :font bigIcons
                        :width 34
                        :height 34
                        :cursor hand
                        :focusable true
                        :role button
                        :hover {:color @c.hover :textColor @c.text}
                        :focused {:color @c.hover :textColor @c.text})
          :card {:radius 16
                 :color @c.surface
                 :borderWidth 1
                 :borderColor @c.border
                 :shadow {:y 2 :blur 10 :color @c.shadow}}
          :field (textarea :radius 10
                   :color @c.surface
                   :borderWidth 1
                   :borderColor @c.border
                   :padding 10
                   :textColor @c.text
                   :placeholderColor @c.dim
                   :focused {:borderColor @c.accent})}
 :components {:navItem {:params {:id "" :icon "" :title "" :keyLabel ""}
                        :node (node#navItemPanel :height 42
                                :radius 10
                                :anchors {:left 0 :right 0}
                                :focusable true
                                :role tab
                                :cursor hand
                                :selected "{{state.page == id}}"
                                :label "{{title}}"
                                :tooltip "{{computed.wide ? '' : title}}"
                                :color "{{state.page == id ? '@c.accentSoft' : 'transparent'}}"
                                :hover {:color "{{state.page == id ? '@c.accentSoft' : '@c.hover'}}"}
                                :onClick {:run go :with {:page "{{id}}"}}
                                (label#navIcon :icon "{{icon}}"
                                  :font bigIcons
                                  :align center
                                  :textColor "{{state.page == id ? '@c.accent' : '@c.muted'}}"
                                  :anchors {:left "{{computed.wide ? 8 : 0}}" :top 0 :bottom 0}
                                  :width "{{computed.wide ? 32 : 48}}")
                                (label#navTitle :when "{{computed.wide}}"
                                  :text "{{title}}"
                                  :font "{{state.page == id ? 'strong' : 'default'}}"
                                  :textColor "{{state.page == id ? '@c.accent' : '@c.text'}}"
                                  :anchors {:left 48 :top 0 :bottom 0 :right 48}
                                  :align "start center")
                                (label#navKey :when "{{computed.wide}}"
                                  :text "{{keyLabel}}"
                                  :font small
                                  :textColor @c.dim
                                  :anchors {:right 12 :top 0 :bottom 0}
                                  :width 40
                                  :align "end center"))}
              :appCard {:params {:app {}}
                        :node (node#appCardPanel :style card
                                :height 244
                                :focusable true
                                :role button
                                :cursor hand
                                :label "{{app.mine ? t('mine.openLabel', {name: app.name}) : t('apps.openLabel', {name: app.name, tagline: t('tagline.' + app.id)})}}"
                                :hover {:borderColor @c.accent :shadow {:y 8 :blur 22 :color @c.shadow}}
                                :focused {:borderColor @c.accent :borderWidth 2}
                                :onClick {:if "{{app.mine}}"
                                          :then [{:run openMine :with {:id "{{app.id}}" :name "{{app.name}}" :ask ""}}]
                                          :else [{:run openExample :with {:app "{{app}}"}}]}
                                (node#cardCoverFrame :anchors {:left 10 :right 10 :top 10}
                                  :height 156
                                  :radius 10
                                  :color @c.raised
                                  (image#cardCover :when "{{!app.mine}}"
                                    :file "{{app.cover}}"
                                    :fit cover
                                    :fill true
                                    :label "{{app.name}}")
                                  (label#cardInitial :when "{{app.mine}}"
                                    :text "{{upper(slice(app.name, 0, 1))}}"
                                    :font cover
                                    :align center
                                    :textColor @c.accent
                                    :color @c.accentSoft
                                    :radius 10
                                    :fill true))
                                (label#cardName :text "{{app.name}}"
                                  :font strong
                                  :textColor @c.text
                                  :anchors {:left 16 :right 52 :top 178}
                                  :height 22)
                                (label#cardTagline :text "{{app.mine ? (app.modified > 0 ? t('mine.info', {folder: app.id, date: formatDate(app.modified, 'medium')}) : app.id) : t('tagline.' + app.id)}}"
                                  :font small
                                  :textColor @c.muted
                                  :wrap true
                                  :anchors {:left 16 :right 16 :top 202}
                                  :height 32)
                                (label#cardKind :font small
                                  :text "{{app.mine ? t('apps.tagMine') : t('apps.tagExample')}}"
                                  :textColor "#ffffff"
                                  :color "{{app.mine ? '@c.accent' : '#16142acc'}}"
                                  :radius 999
                                  :padding 5
                                  :anchors {:left 18 :top 18}
                                  :fitText true
                                  :height 22)
                                (node#cardStart :when "{{!app.mine && app.program == 'host' && app.starter != false}}"
                                  :style iconButton
                                  :icon copy-plus
                                  :anchors {:right 10 :top 172}
                                  :tooltip "{{t('apps.startFrom', {name: app.name})}}"
                                  :label "{{t('apps.startFrom', {name: app.name})}}"
                                  :onClick {:run startFrom :with {:id "{{app.id}}"}}))}
              :mineRow {:params {:app {} :index 0}
                        :node (node#mineRowPanel :style card
                                :height 76
                                :anchors {:left 0 :right 0}
                                :focusable true
                                :role button
                                :cursor hand
                                :label "{{t('mine.openLabel', {name: app.name})}}"
                                :hover {:borderColor @c.accent}
                                :focused {:borderColor @c.accent :borderWidth 2}
                                :onClick {:run openMine :with {:id "{{app.id}}" :name "{{app.name}}" :ask ""}}
                                :keys {:delete {:run askDelete :with {:id "{{app.id}}" :name "{{app.name}}"}}}
                                (label#mineInitial :text "{{upper(slice(app.name, 0, 1))}}"
                                  :font initial
                                  :align center
                                  :textColor "#ffffff"
                                  :color "{{state.tileColors[index % 6]}}"
                                  :radius 12
                                  :anchors {:left 16 :top 16}
                                  :width 44
                                  :height 44)
                                (label#mineName :text "{{app.name}}"
                                  :font strong
                                  :textColor @c.text
                                  :anchors {:left 76 :right 220 :top 17}
                                  :height 22)
                                (label#mineInfo :font small
                                  :textColor @c.muted
                                  :text "{{app.modified > 0 ? t('mine.info', {folder: app.id, date: formatDate(app.modified, 'medium')}) : app.id}}"
                                  :anchors {:left 76 :right 220 :top 41}
                                  :height 18)
                                (row#mineActions :gap 4 :anchors {:right 14 :top 21} :height 34 :width 210 :justify end
                                  (node#mineOpen :style primary
                                    :text "{{t('mine.open')}}"
                                    :width 74
                                    :height 34
                                    :onClick {:run openMine :with {:id "{{app.id}}" :name "{{app.name}}" :ask ""}})
                                  (node#mineDuplicate :style iconButton
                                    :icon copy
                                    :tooltip "{{t('mine.duplicate')}}"
                                    :label "{{t('mine.duplicateLabel', {name: app.name})}}"
                                    :onClick {:run duplicateMine :with {:id "{{app.id}}" :name "{{app.name}}"}})
                                  (node#mineReveal :style iconButton
                                    :icon folder-open
                                    :tooltip "{{t('mine.reveal')}}"
                                    :label "{{t('mine.revealLabel', {name: app.name})}}"
                                    :onClick {:run revealMine :with {:id "{{app.id}}"}})
                                  (node#mineDelete :style iconButton
                                    :icon trash-2
                                    :tooltip "{{t('mine.delete')}}"
                                    :label "{{t('mine.deleteLabel', {name: app.name})}}"
                                    :hover {:color @c.dangerSoft :textColor @c.danger}
                                    :focused {:color @c.dangerSoft :textColor @c.danger}
                                    :onClick {:run askDelete :with {:id "{{app.id}}" :name "{{app.name}}"}})))}
              :startTile {:params {:id blank :name "" :cover ""}
                          :node (node#startTilePanel :height 118
                                  :radius 12
                                  :role radio
                                  :focusable true
                                  :cursor hand
                                  :label "{{name}}"
                                  :selected "{{state.create.from == id}}"
                                  :color "{{state.create.from == id ? '@c.accentSoft' : '@c.surface'}}"
                                  :borderWidth "{{state.create.from == id ? 2 : 1}}"
                                  :borderColor "{{state.create.from == id ? '@c.accent' : '@c.border'}}"
                                  :hover {:borderColor @c.accent}
                                  :onClick {:set state.create.from :to "{{id}}"}
                                  (node#startPicture :anchors {:left 8 :right 8 :top 8}
                                    :height 72
                                    :radius 8
                                    :color @c.raised
                                    (image#startCover :when "{{cover != ''}}" :file "{{cover}}" :fit cover :fill true)
                                    (label#startBlank :when "{{cover == ''}}"
                                      :icon file-plus
                                      :font heroIcons
                                      :align center
                                      :textColor @c.accent
                                      :fill true))
                                  (label#startName :text "{{name}}"
                                    :font "{{state.create.from == id ? 'strong' : 'default'}}"
                                    :textColor @c.text
                                    :align center
                                    :anchors {:left 4 :right 4 :top 86}
                                    :height 22))}
              :settingsCard {:params {:title "" :icon "" :height 120}
                             :node (node#settingsCardPanel :style card :height "{{height}}" :anchors {:left 0 :right 0}
                                     (label#settingsCardIcon :icon "{{icon}}"
                                       :font bigIcons
                                       :textColor @c.accent
                                       :anchors {:left 20 :top 18}
                                       :width 24
                                       :height 24)
                                     (node#settingsCardTitle :style sectionTitle
                                       :text "{{title}}"
                                       :anchors {:left 54 :right 20 :top 18})
                                     (node :slot true))}}
 :root (node#launcher :color @c.bg
         (node#sidebar :anchors {:left 0 :top 0 :bottom 0}
           :width "{{computed.sideWidth}}"
           :color @c.side
           :dir auto
           :role navigation
           :label "{{t('nav.label')}}"
           (node#sidebarEdge :anchors {:right 0 :top 0 :bottom 0} :width 1 :color @c.border)
           (image#brandMark :file "../assets/affrescoui-mark.svg"
             :fit contain
             :anchors {:left "{{computed.wide ? 20 : 16}}" :top 22}
             :width 40
             :height 40
             :label "AffrescoUI")
           (label#brandName :when "{{computed.wide}}"
             :text "AffrescoUI"
             :font brand
             :textColor @c.text
             :anchors {:left 72 :top 22}
             :height 22
             :fitText true)
           (label#brandSub :when "{{computed.wide}}"
             :text "{{t('brand.sub')}}"
             :font small
             :textColor @c.muted
             :anchors {:left 72 :top 44}
             :height 18
             :fitText true)
           (column#nav :anchors {:left 12 :right 12 :top 90}
             :height 186
             :gap 6
             :arrows vertical
             :role tablist
             :label "{{t('nav.label')}}"
             (node#navApps :use navItem
               :with {:id apps
                      :icon layout-grid
                      :title "{{t('nav.apps')}}"
                      :keyLabel "{{context.platform == 'mac' ? '⌘1' : 'Ctrl+1'}}"})
             (node#navMine :use navItem
               :with {:id mine
                      :icon folder-heart
                      :title "{{t('nav.mine')}}"
                      :keyLabel "{{context.platform == 'mac' ? '⌘2' : 'Ctrl+2'}}"})
             (node#navCreate :use navItem
               :with {:id create
                      :icon circle-plus
                      :title "{{t('nav.create')}}"
                      :keyLabel "{{context.platform == 'mac' ? '⌘N' : 'Ctrl+N'}}"})
             (node#navSettings :use navItem
               :with {:id settings
                      :icon settings
                      :title "{{t('nav.settings')}}"
                      :keyLabel "{{context.platform == 'mac' ? '⌘,' : 'Ctrl+,'}}"}))
           (node#sideNew :style primary
             :icon plus
             :text "{{computed.wide ? t('nav.newApp') : ''}}"
             :font "{{computed.wide ? 'strong' : 'bigIcons'}}"
             :label "{{t('nav.newApp')}}"
             :tooltip "{{computed.wide ? '' : t('nav.newApp')}}"
             :anchors {:left 12 :right 12 :bottom 20}
             :height 42
             :onClick showCreate)
           (label#sideNote :when "{{computed.wide}}"
             :font small
             :textColor @c.dim
             :wrap true
             :text "{{t('brand.note')}}"
             :anchors {:left 20 :right 16 :bottom 74}
             :height 52))
         (node#main :anchors {:left "#sidebar.right" :right 0 :top 0 :bottom 0} :color transparent :dir auto
           (node#appsPage :when "{{state.page == 'apps'}}"
             :fill true
             :color transparent
             :role tabpanel
             :label "{{t('nav.apps')}}"
             (node#appsTitle :style pageTitle :text "{{t('apps.title')}}" :role heading)
             (node#appsSub :style pageSub :text "{{t('apps.sub')}}" :anchors {:left 36 :right 320 :top 72})
             (node#appSearch :use searchField
               :with {:name query
                      :placeholder "{{t('apps.search')}}"
                      :label "{{t('apps.search')}}"
                      :clearLabel "{{t('apps.clear')}}"
                      :width "{{min(280, window.width - computed.sideWidth - 420)}}"
                      :height 38
                      :color @c.surface}
               :anchors {:right 36 :top 36})
             (label#appsCount :when "{{window.width - computed.sideWidth > 860}}"
               :font small
               :textColor @c.dim
               :align "end center"
               :text "{{t('apps.count', {n: length(computed.shown)})}}"
               :anchors {:right 36 :top 82}
               :height 22
               :width 160)
             (grid#appsGrid :anchors {:left 24 :right 24 :top 116 :bottom 0}
               :columns "{{computed.columns}}"
               :gap 20
               :scroll vertical
               :role list
               :label "{{t('nav.apps')}}"
               (node#appCard :repeat {:for app :in computed.shown :key "{{app.id}}"} :use appCard :with {:app "{{app}}"}))
             (label#appsNone :when "{{length(computed.shown) == 0}}"
               :textColor @c.muted
               :align center
               :wrap true
               :text "{{t('apps.none', {query: trim(state.query)})}}"
               :anchors {:left 36 :right 36 :top 180}
               :height 48
               :live polite))
           (node#minePage :when "{{state.page == 'mine'}}"
             :fill true
             :color transparent
             :role tabpanel
             :label "{{t('nav.mine')}}"
             (node#mineTitle :style pageTitle :text "{{t('mine.title')}}" :role heading)
             (node#mineSub :style pageSub :text "{{t('mine.sub')}}" :anchors {:left 36 :right 360 :top 72})
             (row#mineButtons :gap 8 :anchors {:right 36 :top 38} :height 38 :width 330 :justify end
               (node#mineFolder :style secondary
                 :icon folder-open
                 :text "{{t('mine.showFolder')}}"
                 :width 150
                 :height 38
                 :onClick {:run revealMine :with {:id ""}})
               (node#mineNew :style primary
                 :icon plus
                 :text "{{t('nav.newApp')}}"
                 :width 130
                 :height 38
                 :onClick showCreate))
             (label#mineUnavailable :when "{{state.mineError != ''}}"
               :font small
               :wrap true
               :text "{{t('mine.unavailable')}}"
               :textColor @c.muted
               :color @c.raised
               :radius 10
               :padding 10
               :anchors {:left 36 :right 36 :top 108}
               :height 40
               :role status)
             (column#mineList :when "{{length(state.mine) > 0}}"
               :anchors {:left 36 :right 36 :top "{{state.mineError != '' ? 160 : 116}}" :bottom 0}
               :gap 10
               :scroll vertical
               :role list
               :label "{{t('nav.mine')}}"
               :arrows vertical
               (node#mineRow :repeat {:for app :in state.mine :index i :key "{{app.id}}"}
                 :use mineRow
                 :with {:app "{{app}}" :index "{{i}}"})
               (node#mineListEnd :height 12 :color transparent :anchors {:left 0 :right 0}))
             (node#mineEmpty :when "{{length(state.mine) == 0}}"
               :anchors {:top "{{state.mineError != '' ? 168 : 124}}"}
               :x "{{max(36, (window.width - computed.sideWidth - 460) / 2)}}"
               :width 460
               :height 300
               :color transparent
               (label#mineEmptyArt :icon sparkles
                 :font heroIcons
                 :align center
                 :textColor @c.accent
                 :color @c.accentSoft
                 :radius 999
                 :anchors {:top 20}
                 :x 190
                 :width 80
                 :height 80)
               (label#mineEmptyTitle :font heading
                 :text "{{t('mine.emptyTitle')}}"
                 :textColor @c.text
                 :align center
                 :anchors {:left 0 :right 0 :top 120}
                 :height 26)
               (label#mineEmptyText :text "{{t('mine.emptyText')}}"
                 :textColor @c.muted
                 :align center
                 :wrap true
                 :anchors {:left 0 :right 0 :top 152}
                 :height 44)
               (node#mineEmptyCreate :style primary
                 :icon plus
                 :text "{{t('mine.emptyButton')}}"
                 :anchors {:top 212}
                 :x 120
                 :width 220
                 :height 42
                 :onClick showCreate)))
           (node#createPage :when "{{state.page == 'create'}}"
             :fill true
             :color transparent
             :role tabpanel
             :label "{{t('nav.create')}}"
             (column#createScroll :anchors {:left 0 :right 0 :top 0 :bottom 0} :scroll vertical :gap 0
               (node#createForm :anchors {:left 36}
                 :width "{{min(820, window.width - computed.sideWidth - 72)}}"
                 :height 980
                 :color transparent
                 :keys {"mod+enter" createApp}
                 (label#createTitle :font title
                   :text "{{t('create.title')}}"
                   :textColor @c.text
                   :anchors {:left 0 :top 30}
                   :fitText true
                   :role heading)
                 (label#createSub :text "{{t('create.sub')}}"
                   :textColor @c.muted
                   :anchors {:left 0 :right 0 :top 72}
                   :height 20)
                 (node#createNameLabel :style sectionTitle
                   :text "{{t('create.name')}}"
                   :anchors {:left 0 :right 0 :top 118})
                 (node#createName :style field
                   :bind state.create.name
                   :maxLength 80
                   :placeholder "{{t('create.namePlaceholder')}}"
                   :label "{{t('create.name')}}"
                   :description "{{state.create.tried && computed.createError != '' ? t(computed.createError) : ''}}"
                   :borderColor "{{state.create.tried && computed.createError != '' ? '@c.danger' : '@c.border'}}"
                   :anchors {:left 0 :right 0 :top 148}
                   :height 42
                   :onSubmit createApp)
                 (label#createNameHint :font small
                   :anchors {:left 2 :right 0 :top 196}
                   :height 18
                   :live polite
                   :textColor "{{state.create.tried && computed.createError != '' ? '@c.danger' : '@c.muted'}}"
                   :text "{{state.create.tried && computed.createError != '' ? t(computed.createError) : computed.slug != '' ? t('create.folder', {folder: computed.slug}) : t('create.nameHint')}}")
                 (node#createFromLabel :style sectionTitle
                   :text "{{t('create.from')}}"
                   :anchors {:left 0 :right 0 :top 236})
                 (node#createFromHint :style hint :text "{{t('create.fromHint')}}" :anchors {:left 0 :right 0 :top 262})
                 (grid#createFrom :anchors {:left 0 :right 0 :top 290}
                   :height 252
                   :columns "{{max(2, min(5, floor((min(820, window.width - computed.sideWidth - 72) + 12) / 150)))}}"
                   :gap 12
                   :scroll vertical
                   :role radiogroup
                   :label "{{t('create.from')}}"
                   :arrows horizontal
                   (node#startBlankTile :use startTile :with {:id blank :name "{{t('create.blank')}}" :cover ""})
                   (node#startTile :repeat {:for app :in computed.starters :key "{{app.id}}"}
                     :use startTile
                     :with {:id "{{app.id}}" :name "{{app.name}}" :cover "{{app.cover}}"}))
                 (node#createDescribeLabel :style sectionTitle
                   :text "{{t('create.describe')}}"
                   :anchors {:left 0 :right 0 :top 566})
                 (node#createDescribeHint :style hint
                   :text "{{t('create.describeHint')}}"
                   :anchors {:left 0 :right 0 :top 592})
                 (editor#createDescribe :bind state.create.describe
                   :wrap true
                   :label "{{t('create.describe')}}"
                   :color @c.surface
                   :radius 10
                   :borderWidth 1
                   :borderColor @c.border
                   :focused {:borderColor @c.accent}
                   :textColor @c.text
                   :caretColor @c.accent
                   :lineColor transparent
                   :selectionColor @c.accentSoft
                   :anchors {:left 0 :right 0 :top 618}
                   :height 96)
                 (label#createDescribePlaceholder :when "{{state.create.describe == ''}}"
                   :text "{{t('create.describePlaceholder')}}"
                   :textColor @c.dim
                   :anchors {:left 12 :right 12 :top 626}
                   :height 20
                   :disabled true)
                 (node#createFeaturesLabel :style sectionTitle
                   :text "{{t('create.features')}}"
                   :anchors {:left 0 :right 0 :top 738})
                 (node#createFeaturesHint :style hint
                   :text "{{t('create.featuresHint')}}"
                   :anchors {:left 0 :right 0 :top 764})
                 (row#createFeatures :wrap true
                   :gap 8
                   :anchors {:left 0 :right 0 :top 792}
                   :height 80
                   :role group
                   :label "{{t('create.features')}}"
                   (label#featureChip :repeat {:for f :in state.featureChoices :key "{{f.id}}"}
                     :icon "{{contains(state.create.features, f.id) ? 'check' : 'plus'}}"
                     :text "{{t('feature.' + f.id) + ' · ' + f.from}}"
                     :font small
                     :fitText true
                     :height 32
                     :padding 10
                     :radius 999
                     :focusable true
                     :role checkbox
                     :checked "{{contains(state.create.features, f.id)}}"
                     :cursor hand
                     :label "{{t('feature.label', {feature: t('feature.' + f.id), app: f.from})}}"
                     :color "{{contains(state.create.features, f.id) ? '@c.accentSoft' : '@c.surface'}}"
                     :textColor "{{contains(state.create.features, f.id) ? '@c.accent' : '@c.text'}}"
                     :borderWidth 1
                     :borderColor "{{contains(state.create.features, f.id) ? '@c.accent' : '@c.border'}}"
                     :hover {:borderColor @c.accent}
                     :onClick {:run toggleFeature :with {:id "{{f.id}}"}}))
                 (label#createSummary :font small
                   :textColor @c.muted
                   :wrap true
                   :text "{{t('create.summary', {name: trim(state.create.name) == '' ? '…' : trim(state.create.name), from: computed.fromName})}}"
                   :anchors {:left 0 :right 240 :top 900}
                   :height 36)
                 (node#createSubmit :style primary
                   :icon "{{state.create.busy ? 'refresh-cw' : 'rocket'}}"
                   :text "{{state.create.busy ? t('create.busy') : t('create.submit')}}"
                   :disabled "{{state.create.busy}}"
                   :anchors {:right 0 :top 894}
                   :width 200
                   :height 44
                   :onClick createApp)
                 (label#createProblem :when "{{state.create.error != ''}}"
                   :font small
                   :wrap true
                   :role alert
                   :live assertive
                   :text "{{state.create.error}}"
                   :textColor @c.danger
                   :color @c.dangerSoft
                   :radius 8
                   :padding 8
                   :anchors {:left 0 :right 0 :top 946}
                   :height 32))))
           (node#settingsPage :when "{{state.page == 'settings'}}"
             :fill true
             :color transparent
             :role tabpanel
             :label "{{t('nav.settings')}}"
             (node#settingsTitle :style pageTitle :text "{{t('settings.title')}}" :role heading)
             (node#settingsSub :style pageSub :text "{{t('settings.sub')}}")
             (column#settingsList :anchors {:left 36 :top 112 :bottom 0}
               :width "{{min(760, window.width - computed.sideWidth - 72)}}"
               :gap 16
               :scroll vertical
               (node#settingsAppearance :use settingsCard
                 :with {:title "{{t('settings.appearance')}}" :icon sun :height 150}
                 (label#themeLabel :text "{{t('settings.theme')}}"
                   :textColor @c.muted
                   :anchors {:left 54 :top 62}
                   :width 110
                   :height 32
                   :align "start center")
                 (node#themeChoice :use segmentedControl
                   :anchors {:left 170 :top 62}
                   :with {:name theme
                          :label "{{t('settings.theme')}}"
                          :height 32
                          :color @c.raised
                          :selectedColor @c.surface
                          :highlight @c.hover
                          :items [{:id system :title "{{t('settings.system')}}" :icon monitor :width 116}
                                  {:id light :title "{{t('settings.light')}}" :icon sun :width 100}
                                  {:id dark :title "{{t('settings.dark')}}" :icon moon :width 100}]})
                 (label#languageLabel :text "{{t('settings.language')}}"
                   :textColor @c.muted
                   :anchors {:left 54 :top 102}
                   :width 110
                   :height 32
                   :align "start center")
                 (node#languageChoice :use segmentedControl
                   :anchors {:left 170 :top 102}
                   :with {:name locale
                          :label "{{t('settings.language')}}"
                          :height 32
                          :color @c.raised
                          :selectedColor @c.surface
                          :highlight @c.hover
                          :items [{:id system :title "{{t('settings.system')}}" :width 96}
                                  {:id en :title "English" :width 80}
                                  {:id es :title "Español" :width 80}
                                  {:id fr :title "Français" :width 84}]}))
               (node#settingsNotation :use settingsCard
                 :with {:title "{{t('settings.notation')}}" :icon braces :height 158}
                 (label#notationLabel :text "{{t('settings.notationLabel')}}"
                   :textColor @c.muted
                   :anchors {:left 54 :top 58}
                   :width 110
                   :height 32
                   :align "start center")
                 (node#notationChoice :use segmentedControl
                   :anchors {:left 170 :top 58}
                   :with {:name notation
                          :label "{{t('settings.notationLabel')}}"
                          :height 32
                          :color @c.raised
                          :selectedColor @c.surface
                          :highlight @c.hover
                          :items [{:id json :title "JSON" :width 80} {:id lisp :title "Lisp" :width 80}]})
                 (label#notationText :wrap true
                   :textColor @c.muted
                   :text "{{t('settings.notationText')}}"
                   :anchors {:left 54 :right 20 :top 102}
                   :height 44))
               (node#settingsFolder :use settingsCard
                 :with {:title "{{t('settings.folder')}}" :icon folder-heart :height 128}
                 (label#folderText :wrap true
                   :textColor @c.muted
                   :text "{{t('settings.folderText', {folder: services.apps.folder ?? 'AffrescoUI'})}}"
                   :anchors {:left 54 :right 200 :top 56}
                   :height 52)
                 (node#folderReveal :style secondary
                   :icon folder-open
                   :text "{{t('mine.showFolder')}}"
                   :anchors {:right 20 :top 62}
                   :width 160
                   :height 38
                   :onClick {:run revealMine :with {:id ""}}))
               (node#settingsIntelligence :anchors {:left 0 :right 0} :height 960 :color transparent
                 (node#intelligenceSettingsPage :use intelligenceSettings))
               (node#settingsKeys :use settingsCard :with {:title "{{t('settings.keys')}}" :icon keyboard :height 208}
                 (column#keysList :anchors {:left 54 :right 20 :top 56} :height 134 :gap 4
                   (row#keyRow :repeat {:for k
                                        :in "{{[['⌘1 · ⌘2 · ⌘4', 'settings.keyPages'], ['⌘N', 'settings.keyNew'], ['⌘F', 'settings.keySearch'], ['⌘R', 'settings.keyRefresh'], ['Delete', 'settings.keyDelete']]}}"}
                     :height 22
                     :anchors {:left 0 :right 0}
                     :gap 12
                     (label#keyCombo :font small
                       :text "{{context.platform == 'mac' ? k[0] : join(split(k[0], '⌘'), 'Ctrl+')}}"
                       :textColor @c.text
                       :color @c.raised
                       :radius 6
                       :width 150
                       :height 22
                       :align center)
                     (label#keyWhat :font small
                       :text "{{t(k[1])}}"
                       :textColor @c.muted
                       :grow 1
                       :height 22
                       :align "start center"))))
               (node#settingsAbout :use settingsCard :with {:title "{{t('settings.about')}}" :icon info :height 112}
                 (label#aboutText :wrap true
                   :textColor @c.muted
                   :text "{{t('settings.aboutText')}}"
                   :anchors {:left 54 :right 20 :top 54}
                   :height 44))
               (node#settingsEnd :height 12 :color transparent :anchors {:left 0 :right 0})))))
 :overlays {:confirmDelete (node#confirmDeleteDialog :use dialog
                             :with {:open state.confirmOpen
                                    :title "{{t('delete.title', {name: state.confirm.name})}}"
                                    :message "{{t('delete.message', {folder: state.confirm.id})}}"
                                    :width 460
                                    :height 180
                                    :highlight @c.accent
                                    :buttons [{:title "{{t('delete.confirm')}}" :run deleteMine}
                                              {:title "{{t('delete.cancel')}}" :default true}]})
            :launcherToasts (node#launcherToastList :use toasts :with {:name toasts})}}
