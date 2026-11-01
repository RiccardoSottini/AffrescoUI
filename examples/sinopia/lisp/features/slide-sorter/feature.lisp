{:format affrescoui-feature
  :version 2
  :name slideSorter
  :release "1.0.0"
  :title "Slide sorter"
  :description "Sinopia's slide sorter as a part other apps install: thumbnails of an ordered list of pages, drag to reorder, click to select, and a button that duplicates the selected page."
  :license Apache-2.0
  :note "It names what it needs, not where it comes from: an app binds each need to its exports (directly, or through an adapter: Tempera's top-level layers are its pages, examples/tempera/json/sorter/sorter.adapter.json), and a composition proof checks the binding before the install revision is submitted."
  :requires {:format 2}
  :needs {:pages {:kind list
      :item (node #string :name string :image image)
      :description "The pages in order, each with an id, a name and an image the host renders."}
    :current {:kind state :type string :optional true :description "The id of the selected page."}
    :reorder {:kind action :arguments (node #string :onto string) :description "Page id takes the place of page onto."}
    :select {:kind action :arguments (node #string) :description "Selects page id."}
    :duplicate {:kind action :arguments (node #string) :description "Duplicates page id."}
    :dock {:kind slot :slot dock :description "Where the sorter sits."}}
  :provides {:state {:picked ""}
    :actions {:pick {:title "Select Page"
        :category "Slide Sorter"
        :do [{:set state.slideSorter.picked :to "{{id}}"} {:run app.select :with (node #"{{id}}")}]}
      :move {:title "Move Page"
        :category "Slide Sorter"
        :do [{:if "{{id != onto}}" :then [{:run app.reorder :with (node #"{{id}}" :onto "{{onto}}")}]}]}
      :duplicate {:title "Duplicate Page"
        :category "Slide Sorter"
        :do [{:run app.duplicate :with (node #"{{app.current ?? state.slideSorter.picked}}")}]}}
    :views {:dock {:slideSorter-view {:type column
          :width "100%"
          :height 132
          :gap 4
          :color transparent
          :borderWidth 1
          :borderColor @border
          :children [(row #slideSorter-header :width "100%" :height 24 :gap 6 :alignItems center
              (label #slideSorter-title
                :grow 1
                :height 24
                :padding 6
                :textColor @text
                :text "Pages ({{length(app.pages)}})")
              (label #slideSorter-duplicate
                :width 24
                :height 24
                :icon copy
                :cursor hand
                :tooltip "Duplicate page"
                :textColor @text
                :onClick slideSorter.duplicate))
            (row #slideSorter-pages :width "100%" :grow 1 :gap 8 :scroll horizontal :color transparent
              (node :repeat {:for page :in "{{app.pages}}" :key "{{page.id}}"}
                :type column
                #"slideSorter-page-{{page.id}}"
                :width 84
                :height 96
                :gap 2
                :color transparent
                :cursor hand
                :drag {:data (node #"{{page.id}}")}
                :onDrop {:run slideSorter.move :with (node #"{{event.data.id}}" :onto "{{page.id}}")}
                :onClick {:run slideSorter.pick :with (node #"{{page.id}}")}
                (image #"slideSorter-image-{{page.id}}"
                  :width 84
                  :height 70
                  :source "{{page.image}}"
                  :fit contain
                  :borderWidth "{{page.id == (app.current ?? state.slideSorter.picked) ? 2 : 1}}"
                  :borderColor "{{page.id == (app.current ?? state.slideSorter.picked) ? '@accent' : '@border'}}"
                  :dragOver {:borderColor @accent :borderWidth 3})
                (label #"slideSorter-name-{{page.id}}"
                  :width 84
                  :height 18
                  :align center
                  :fitText true
                  :textColor @muted
                  :text "{{page.name}}")))]}}}}
  :scenarios [{:name slideSorter-shows
      :note "The sorter is docked, with one thumbnail per page"
      :steps [{:expect {:node "#slideSorter-view" :exists true}} {:expect {:node "#slideSorter-pages" :exists true}}]}
    {:name slideSorter-starts-unpicked
      :note "Nothing is picked until a page is clicked, and the duplicate button is there"
      :steps [{:expect {:state slideSorter.picked :is ""}} {:expect {:node "#slideSorter-duplicate" :exists true}}]}]}
