;;; features.lisp --- the features Orrery has installed, as the catalogue gives them
;;;
;;; Sinopia's slide sorter, written as a program (examples/sinopia/lisp/features/slide-sorter/sorter.lisp) and pinned
;;; by its digest: data, the record `affrescoui check --compose ... --revision` writes, so that its text and its digest
;;; stay the same. It sits in the bodies dock, through offers.lisp's point and orrery-sorter.adapter.lisp.

{:features {:slideSorter {:release "2.0.0"
                          :title "Slide sorter"
                          :source "app:sinopia"
                          :digest "sha512:_pvcS7y1GIp_hp9wzaPyfmT21fXLtqpBkbW8JH77-L_lInXoxF6HVNbAG0sF3vAeJFGqWFgRk8JFfiiLJ4XVmQ"
                          :publisher nil
                          :enabled true
                          :order 10
                          :requires {:format 2}
                          :provides {:program {:sorter.lisp {:state.slideSorter ";;; sorter.lisp --- the slide sorter: thumbnails of an ordered list of pages, drag to reorder, click to select\n;;;\n;;; Written once, for Sinopia's slides, and installed into any app that offers its needs (feature.lisp): it reads\n;;; the app only as app.pages and app.current, and changes it only through the actions app.select, app.reorder and\n;;; app.duplicate. Its own state is the page picked last, the selection of an app that keeps none.\n\n;;; Its state\n\n(defstate slideSorter {:picked \"\"} \"The id of the page picked last: the selection when the app has no current page.\")"
                                                             :slideSorter-selected ";;; What it does\n\n(defn slideSorter-selected\n  \"The selected page: the app's current page, else the one picked last.\"\n  [current]\n  (if (nil? current) state.slideSorter.picked current))"
                                                             :slideSorter.pick "(defcommand slideSorter.pick\n  \"Selects a page: the sorter remembers it, and the app selects it.\"\n  :title \"Select Page\"\n  :category \"Slide Sorter\"\n  [id]\n  (set \"state.slideSorter.picked\" id)\n  (run \"app.select\" :with {:id id}))"
                                                             :slideSorter.move "(defcommand slideSorter.move\n  \"Moves page id to the place of page onto, unless it was dropped on itself.\"\n  :title \"Move Page\"\n  :category \"Slide Sorter\"\n  [id onto]\n  (when (not= id onto) (run \"app.reorder\" :with {:id id :onto onto})))"
                                                             :slideSorter.duplicate "(defcommand slideSorter.duplicate\n  \"Duplicates page id, the selected one, which the view passes.\"\n  :title \"Duplicate Page\"\n  :category \"Slide Sorter\"\n  [id]\n  (run \"app.duplicate\" :with {:id id}))"
                                                             :slideSorter-header ";;; What it shows\n\n(defview slideSorter-header\n  \"The strip's header: how many pages there are, and the button that duplicates the selected one.\"\n  []\n  `(row#slideSorter-header :width \"100%\" :height 24 :gap 6 :alignItems center\n     (label#slideSorter-title :grow 1 :height 24 :padding 6 :textColor @text\n       :text ~(str \"Pages (\" (count app.pages) \")\"))\n     (label#slideSorter-duplicate :width 24 :height 24 :icon copy :cursor hand :tooltip \"Duplicate page\"\n       :textColor @text :onClick ~(handler slideSorter.duplicate (slideSorter-selected app.current)))))"
                                                             :slideSorter-page "(defview slideSorter-page\n  \"One page: its thumbnail, framed in the accent when it is the selected one, and its name. Dragged onto another page,\n  it takes that page's place; clicked, it is selected.\"\n  [page]\n  `(column :id ~(str \"slideSorter-page-\" page.id) :width 84 :height 96 :gap 2 :color transparent :cursor hand\n     :drag {:data {:id ~page.id}}\n     :onDrop ~(handler slideSorter.move event.data.id page.id)\n     :onClick ~(handler slideSorter.pick page.id)\n     (image :id ~(str \"slideSorter-image-\" page.id) :width 84 :height 70 :source ~page.image :fit contain\n       :borderWidth ~(if (= page.id (slideSorter-selected app.current)) 2 1)\n       :borderColor ~(if (= page.id (slideSorter-selected app.current)) \"@accent\" \"@border\")\n       :dragOver {:borderColor @accent :borderWidth 3})\n     (label :id ~(str \"slideSorter-name-\" page.id) :width 84 :height 18 :align center :fitText true\n       :textColor @muted :text ~page.name)))"
                                                             :slideSorter-strip "(defview slideSorter-strip\n  \"The sorter: its header over the pages, in a row that scrolls across.\"\n  []\n  `(column :width \"100%\" :height 132 :gap 4 :color transparent :borderWidth 1 :borderColor @border\n     ~(slideSorter-header)\n     (row#slideSorter-pages :width \"100%\" :grow 1 :gap 8 :scroll horizontal :color transparent\n       ~(repeat [page app.pages :key page.id] (slideSorter-page page)))))"
                                                             :part.slideSorter-views ";;; Where it goes\n\n(defpart slideSorter-views\n  \"The strip, in the slot the app binds to the dock need.\"\n  :views {:dock {:slideSorter-view ~(slideSorter-strip)}})"}}}
                          :needs {:pages {:kind list
                                          :item {:id string :name string :image image}
                                          :description "The pages in order, each with an id, a name and an image the host renders."}
                                  :current (string :kind state
                                             :optional true
                                             :description "The id of the selected page.")
                                  :reorder {:kind action
                                            :arguments {:id string :onto string}
                                            :description "Page id takes the place of page onto."}
                                  :select {:kind action :arguments {:id string} :description "Selects page id."}
                                  :duplicate {:kind action :arguments {:id string} :description "Duplicates page id."}
                                  :dock {:kind slot :slot dock :description "Where the sorter sits."}}
                          :bindings {:pages {:value "{{map(filter(app.sky.bodies, it.parent != ''), {id: it.id, name: it.name, image: it.thumbnail})}}"}
                                     :current {:export sky.selected}
                                     :reorder {:action bodies.reorder :with {:args {:id "{{id}}" :onto "{{onto}}"}}}
                                     :select {:action bodies.select :with {:args {:id "{{id}}"}}}
                                     :duplicate {:action bodies.clone :with {:args {:id "{{id}}"}}}
                                     :dock {:point bodies.dock}}
                          :proof {:format affrescoui-proof
                                  :version 1
                                  :feature slideSorter
                                  :release "2.0.0"
                                  :source "app:sinopia"
                                  :host "app:orrery"
                                  :base ""
                                  :adapter {:digest "sha512:h3jaEv1bf5hYRZGh9GZnkxeiN_X9Iz4NBCRo4oWzd1hpCPat2K9zpv9T3Up70Sw-a2G_-gs7768qZr8juXqAMQ"
                                            :note "Sinopia's slide sorter in Orrery: every body but the Sun is a page, drawn from the thumbnail the screen draws for it, and the sorter's actions are the body actions Orrery's bodies dock receives."}
                                  :bindings [{:need pages
                                              :kind list
                                              :by adapter
                                              :to {:value "{{map(filter(app.sky.bodies, it.parent != ''), {id: it.id, name: it.name, image: it.thumbnail})}}"}}
                                             {:need current :kind state :by adapter :to {:export sky.selected}}
                                             {:need reorder
                                              :kind action
                                              :by adapter
                                              :to {:action bodies.reorder :with {:args {:id "{{id}}" :onto "{{onto}}"}}}}
                                             {:need select
                                              :kind action
                                              :by adapter
                                              :to {:action bodies.select :with {:args {:id "{{id}}"}}}}
                                             {:need duplicate
                                              :kind action
                                              :by adapter
                                              :to {:action bodies.clone :with {:args {:id "{{id}}"}}}}
                                             {:need dock :kind slot :by host :to {:point bodies.dock}}]
                                  :checks [{:name manifest :passed true :reasons []}
                                           {:name adapter :passed true :reasons []}
                                           {:name needs :passed true :reasons []}
                                           {:name composition :passed true :reasons []}
                                           {:name isolation :passed true :reasons []}
                                           {:name permissions :passed true :reasons []}
                                           {:name document :passed true :reasons []}
                                           {:name scenarios :passed true :reasons []}]
                                  :scenarios [{:file feature :name slideSorter-shows :passed true :problem ""}
                                              {:file feature :name slideSorter-starts-unpicked :passed true :problem ""}
                                              {:file index.scenarios.json
                                               :name starts-on-day-zero
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name next-event-moves-the-planets
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name play-advances-the-playhead
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name annotate-the-chart
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name reparent-in-the-graph
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name turn-and-zoom-the-view
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name overlays-show-the-selection-and-the-time
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name go-to-and-follow-the-selection
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name centre-the-selection
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name toggle-orbits-and-labels
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name time-stops-at-the-start
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name show-the-browser
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name narrow-and-widen-the-field
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name bookmark-and-return
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name bookmarks-keep-the-time-and-the-field
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name go-to-twice-lands-once
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name go-to-while-playing
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name reparent-onto-a-moon-but-not-into-a-loop
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name a-small-window-keeps-the-graph
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-lit-side-is-drawn
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name render-options-hide-the-stars
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-camera-follows-the-state
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name follow-switches-to-the-new-selection
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-wheel-wins-over-a-flight
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name a-place-stops-a-flight
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name m-marks-the-selected-body
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name keys-change-the-limiting-magnitude
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name muting-a-kind-of-event-skips-it
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name a-new-moon-orbits-close-and-goes-home
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-last-place-deleted
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-glow-toggles
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name an-eclipse-found-and-gone-to
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name an-unknown-eclipse-does-nothing
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-eclipse-finder-opens-from-the-menu
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-sky-chart-names-the-selected-body
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-sky-from-the-ground
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-sky-layers-toggle
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name an-eclipse-seen-from-the-ground
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name time-at-the-real-rate
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-light-theme
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name the-grid-names-its-azimuths
                                               :passed true
                                               :problem ""}
                                              {:file index.scenarios.json
                                               :name ask-the-assistant
                                               :passed true
                                               :problem ""}]
                                  :passed true
                                  :unmet []}
                          :installed "sha256:85a70ef2c2f374950c5ee4a1a3ba6b145b855ee8c14fb836c22d3e3255a20832"}}}
