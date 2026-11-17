;;; catalogue.lisp --- the Catalogue page: the versions and features the AffrescoUI catalogue lists, as cards
;;;
;;; The host grants a catalogue when it trusts the key that signed its index; the page shows only then. A click on a
;;; card asks the packages service what installing the entry would do and shows the same review as for a file, with
;;; where it was listed. Listing an entry says it was reviewed and unchanged since, not that it is harmless: what it
;;; changes still waits as a proposal only the person accepts.

;;; What it keeps

(defstate catalogue {:available false :name "" :updated "" :signed false :entries []}
  "The catalogue the host grants: its name, when it was updated, and its entries; available once the service answers.")

(defstate catalogueQuery "" "The Catalogue page's search.")

(defcomputed catalogueShown "The entries whose title, description or id holds the search."
  (filter (fn [it] (or (= (trim state.catalogueQuery) "")
                       (contains (lower-case (str it.title " " it.description " " it.id))
                                 (lower-case (trim state.catalogueQuery)))))
          state.catalogue.entries))

(defcomputed listedLine "Where the entry reviewed was listed, or empty for a file."
  (if (= (or-else state.install.listed.catalogue "") "")
    ""
    (t "review.listed" {:date (if (= (or-else state.install.listed.updated "") "")
                                (t "review.undated")
                                state.install.listed.updated)})))

;;; The page

(defview catalogue-page "The Catalogue page: its title, the search, when it was updated, and the cards." []
  `(node#cataloguePage :when ~(= state.page "catalogue") :fill true :color transparent :role tabpanel
     :label ~(t "nav.catalogue")
     (node#catalogueTitle :style pageTitle :text ~(t "catalogue.title") :role heading)
     (node#catalogueSub :style pageSub :text ~(t "catalogue.sub") :anchors {:left 36 :right 320 :top 72})
     (node#catalogueSearch :use searchField
       :with {:name catalogueQuery
              :placeholder ~(t "catalogue.search")
              :label ~(t "catalogue.search")
              :clearLabel ~(t "apps.clear")
              :width ~(min 280 (- (- window.width computed.sideWidth) 420))
              :height 38
              :color @c.surface}
       :anchors {:right 36 :top 36})
     (label#catalogueUpdated :when ~(> (- window.width computed.sideWidth) 860)
       :font small
       :textColor @c.dim
       :align "end center"
       :text ~(if (= state.catalogue.updated "") "" (t "catalogue.updated" {:date state.catalogue.updated}))
       :anchors {:right 36 :top 82}
       :height 22
       :width 240)
     (grid#catalogueGrid :anchors {:left 24 :right 24 :top 116 :bottom 0}
       :columns ~computed.columns
       :gap 20
       :scroll vertical
       :role list
       :label ~(t "nav.catalogue")
       ~(repeat [entry computed.catalogueShown :key entry.id]
         `(node#catalogueCard :use catalogueCard :with {:entry ~entry})))
     (label#catalogueNone :when ~(= (count computed.catalogueShown) 0)
       :textColor @c.muted
       :align center
       :wrap true
       :text ~(if (= (trim state.catalogueQuery) "")
                (t "catalogue.empty")
                (t "apps.none" {:query (trim state.catalogueQuery)}))
       :anchors {:left 36 :right 36 :top 180}
       :height 48
       :live polite)))

;;; A card

(defcomponent catalogueCard "An entry as a card: a version of an example or a feature, its title, description and
  release; a click reviews it."
  {:entry {}}
  `(node#catalogueCardPanel :style card
     :height 168
     :focusable true
     :role button
     :cursor hand
     :label ~(t "catalogue.reviewLabel" {:name entry.title})
     :hover {:borderColor @c.accent :shadow {:y 8 :blur 22 :color @c.shadow}}
     :focused {:borderColor @c.accent :borderWidth 2}
     :onClick ~(handler reviewListed entry.id entry.release entry.description)
     (label#catalogueCardIcon :icon ~(if (= entry.kind "feature") "puzzle" "package")
       :font bigIcons
       :align center
       :textColor @c.accent
       :color @c.accentSoft
       :radius 10
       :anchors {:left 16 :top 16}
       :width 40
       :height 40)
     (label#catalogueCardKind :font small
       :textColor @c.muted
       :align "end center"
       :text ~(cond entry.installed (t "catalogue.installed")
                    (= entry.kind "feature") (t "catalogue.kindFeature")
                    :else (t "catalogue.kindVersion" {:base entry.base}))
       :anchors {:right 16 :top 16}
       :height 22
       :width 160)
     (label#catalogueCardName :text ~entry.title :font strong :textColor @c.text
       :anchors {:left 16 :right 16 :top 68} :height 22)
     (label#catalogueCardAbout :text ~entry.description :font small :textColor @c.muted :wrap true
       :anchors {:left 16 :right 16 :top 92} :height 48)
     (label#catalogueCardRelease :text ~(t "review.release" {:release entry.release}) :font small :textColor @c.dim
       :anchors {:left 16 :right 16 :bottom 12} :height 18)))

;;; What it does

(defn listCatalogue "Asks the packages service for the catalogue the host grants: the page shows only when there is
  one." []
  (invoke "packages.catalogue" :with {} :onDone catalogueRead :onError catalogueMissing))

(defn catalogueRead "The catalogue as the service gives it." [result]
  (set "state.catalogue" {:available true :name (:name result) :updated (:updated result) :signed (:signed result)
                          :entries (:entries result)}))

(defn catalogueMissing "No catalogue, or its index refused: the page stays hidden, as it starts." [error] nil)

(defn showListing "Shows a catalogue as event gives it ({name, updated, signed, entries}): scenarios play the
  service's answer with it." []
  (set "state.catalogue" {:available true :name (or event.name "") :updated (or event.updated "")
                          :signed (or event.signed false) :entries (or event.entries [])}))

(defn reviewListed "Asks the packages service what installing an entry would do: the same review as for a file."
  [id release description]
  (invoke "packages.inspect" :with {:catalogue id :release release}
          :onDone [listedInspected id release description] :onError [listedNotInspected id release description]))

(defn listedInspected "The review shows what the service said, and where the entry was listed."
  [id release description result]
  (reviewEntry (:summary result) "" (or (:listed result) {}) id release description))

(defn listedNotInspected "The review says why the entry can't be installed." [id release description error]
  (reviewEntry {} (:message error) {} id release description))

(defn reviewEntry "The review of a file, with the entry it came from and where it was listed."
  [summary why listed id release description]
  (review summary why)
  (set "state.install.package" {:description description})
  (set "state.install.entry" {:catalogue id :release release})
  (set "state.install.listed" listed))

(defn showListedReview "Shows the review of an entry as event gives it ({summary, listed, id, release, description,
  error?}): scenarios play the service's answer with it." []
  (reviewEntry (or event.summary {}) (or event.error "") (or event.listed {}) (or event.id "") (or event.release "")
               (or event.description "")))

(defcommand showCatalogue "Shows the Catalogue page."
  :title ~(t "nav.catalogue") :description ~(t "command.showCatalogue.about") :category ~(t "category.view")
  :icon "library" []
  (when state.catalogue.available (go "catalogue")))
