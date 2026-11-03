;;; features.lisp --- the features Orrery has installed, as the catalogue gives them
;;;
;;; Sinopia's slide sorter, signed and pinned by its digest: data, written as its manifest is, so that its text and its
;;; digest stay the same. It sits in the bodies dock, through offers.lisp's point and orrery-sorter.adapter.lisp.

{:features {:slideSorter {:release "1.0.0"
                          :title "Slide sorter"
                          :source "app:sinopia"
                          :digest "sha512:9kHJ4TytpjXMOg4IPjbNpIrBgsN4AodRwHOhIPKTExprjWpGlYyJRHVHxvqPpDfVjT7urDYkuSzYY7OEPqHUbQ"
                          :publisher nil
                          :enabled true
                          :order 10
                          :requires {:format 2}
                          :provides {:state {:picked ""}
                                     :actions {:pick {:title "Select Page"
                                                      :category "Slide Sorter"
                                                      :do [{:set state.slideSorter.picked :to "{{id}}"}
                                                           {:run app.select :with {:id "{{id}}"}}]}
                                               :move {:title "Move Page"
                                                      :category "Slide Sorter"
                                                      :do [{:if "{{id != onto}}"
                                                            :then [{:run app.reorder
                                                                    :with {:id "{{id}}" :onto "{{onto}}"}}]}]}
                                               :duplicate {:title "Duplicate Page"
                                                           :category "Slide Sorter"
                                                           :do [{:run app.duplicate
                                                                 :with {:id "{{app.current ?? state.slideSorter.picked}}"}}]}}
                                     :views {:dock {:slideSorter-view (column :width "100%"
                                                                        :height 132
                                                                        :gap 4
                                                                        :color transparent
                                                                        :borderWidth 1
                                                                        :borderColor @border
                                                                        (row#slideSorter-header :width "100%"
                                                                          :height 24
                                                                          :gap 6
                                                                          :alignItems center
                                                                          (label#slideSorter-title :grow 1
                                                                            :height 24
                                                                            :padding 6
                                                                            :textColor @text
                                                                            :text "Pages ({{length(app.pages)}})")
                                                                          (label#slideSorter-duplicate :width 24
                                                                            :height 24
                                                                            :icon copy
                                                                            :cursor hand
                                                                            :tooltip "Duplicate page"
                                                                            :textColor @text
                                                                            :onClick slideSorter.duplicate))
                                                                        (row#slideSorter-pages :width "100%"
                                                                          :grow 1
                                                                          :gap 8
                                                                          :scroll horizontal
                                                                          :color transparent
                                                                          (column :id "slideSorter-page-{{page.id}}"
                                                                            :repeat {:for page
                                                                                     :in "{{app.pages}}"
                                                                                     :key "{{page.id}}"}
                                                                            :width 84
                                                                            :height 96
                                                                            :gap 2
                                                                            :color transparent
                                                                            :cursor hand
                                                                            :drag {:data {:id "{{page.id}}"}}
                                                                            :onDrop {:run slideSorter.move
                                                                                     :with {:id "{{event.data.id}}"
                                                                                            :onto "{{page.id}}"}}
                                                                            :onClick {:run slideSorter.pick
                                                                                      :with {:id "{{page.id}}"}}
                                                                            (image :id "slideSorter-image-{{page.id}}"
                                                                              :width 84
                                                                              :height 70
                                                                              :source "{{page.image}}"
                                                                              :fit contain
                                                                              :borderWidth "{{page.id == (app.current ?? state.slideSorter.picked) ? 2 : 1}}"
                                                                              :borderColor "{{page.id == (app.current ?? state.slideSorter.picked) ? '@accent' : '@border'}}"
                                                                              :dragOver {:borderColor @accent
                                                                                         :borderWidth 3})
                                                                            (label :id "slideSorter-name-{{page.id}}"
                                                                              :width 84
                                                                              :height 18
                                                                              :align center
                                                                              :fitText true
                                                                              :textColor @muted
                                                                              :text "{{page.name}}"))))}}}
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
                                  :release "1.0.0"
                                  :source "app:sinopia"
                                  :host "app:orrery"
                                  :base ""
                                  :adapter {:digest "sha512:XRKpnO7AGnykUEjoBeWEhQ30LtfokmkdzNJ4Y5QavVXrt9OsZutr25eAovs7UCN-7_BuQEnKXVMlTD1IVr4vpw"
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
                                               :problem ""}]
                                  :passed true
                                  :unmet []}}}}
