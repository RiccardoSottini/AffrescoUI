;;; feature.lisp --- Sinopia's slide sorter, the feature other apps install: its manifest
;;;
;;; Data: who it is, what it needs from the app it goes into, the program files that hold what it gives (sorter.lisp),
;;; and the scenarios a composition proof plays on the app with it.

{:format affrescoui-feature
 :version 2
 :name slideSorter
 :release "2.0.0"
 :title "Slide sorter"
 :description "Sinopia's slide sorter as a part other apps install, written as a program: thumbnails of an ordered list of pages, drag to reorder, click to select, and a button that duplicates the selected page."
 :license "Apache-2.0"
 :note "Its definitions are in sorter.lisp, named in the feature's namespace; an app written as a program installs it, and a static app installs the JSON version, examples/sinopia/json/features/slide-sorter/feature.json. It names what it needs, not where it comes from: an app binds each need to its exports (directly, or through an adapter: Tempera's top-level layers are its pages, examples/tempera/json/sorter/sorter.adapter.json), and a composition proof checks the binding before the install revision is submitted."
 :requires {:format 2}
 :needs {:pages {:kind list
                 :item {:id string :name string :image image}
                 :description "The pages in order, each with an id, a name and an image the host renders."}
         :current (string :kind state :optional true :description "The id of the selected page.")
         :reorder {:kind action
                   :arguments {:id string :onto string}
                   :description "Page id takes the place of page onto."}
         :select {:kind action :arguments {:id string} :description "Selects page id."}
         :duplicate {:kind action :arguments {:id string} :description "Duplicates page id."}
         :dock {:kind slot :slot dock :description "Where the sorter sits."}}
 :program ["sorter.lisp"]
 :scenarios [{:name slideSorter-shows
              :note "The sorter is docked, with one thumbnail per page"
              :steps [{:expect {:node "#slideSorter-view" :exists true}}
                      {:expect {:node "#slideSorter-pages" :exists true}}]}
             {:name slideSorter-starts-unpicked
              :note "Nothing is picked until a page is clicked, and the duplicate button is there"
              :steps [{:expect {:state slideSorter.picked :is ""}}
                      {:expect {:node "#slideSorter-duplicate" :exists true}}]}]}
