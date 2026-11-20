;;; collections.lisp --- the library's own collections: making one, by hand or by a rule, and removing it
;;;
;;; A collection is a name and either the games picked for it (`:games`) or a rule (`:rule {:field :value}`) that
;;; keeps it filled by itself: every owned game whose field holds that value is in it, now and later. library.lisp shows
;;; them in the game list and as shelves on the home.

(ns arcadia.parts.collections)

;;; What the window keeps

(defstate newCollection {:open false :title "" :kind manual :games [] :rule nil :error ""})

(defstate collectionSeq 0)

(defcomputed collectionRules
  "The rules a dynamic collection can follow: installed, never played, favourites, or one of the owned games' genres."
  (concat [{:key "installed" :field "installed" :value true :title (t "coll.rule.installed")}
           {:key "unplayed" :field "hours" :value 0 :title (t "coll.rule.unplayed")}
           {:key "favourite" :field "favourite" :value true :title (t "coll.rule.favourite")}]
          (genre-rules computed.owned)))

(defn genre-rules "A rule per genre of the games, by the genre's name." [games]
  (map #(hash-map :key (str "genre-" (genre-key %)) :field "genre" :value % :title %)
       (sort (distinct (keep :genre games)))))

(defn genre-key "A genre's name as part of an id: lower case, a dash for each space." [genre]
  (str/replace (str/lower-case genre) " " "-"))

;;; The window

(defcomponent collectionDialog
  "A new collection: its name, then either the games ticked for it or the rule that fills it by itself."
  {}
  `(node :use dialog
     :with {:open state.newCollection.open
            :width 540
            :height 440
            :title ~(t "coll.title")
            :color @color.panel
            :textColor @color.text
            :dimColor @color.muted
            :highlight @color.accent
            :buttons [{:title ~(t "coll.create") :run createCollection :default true :keepOpen true}
                      {:title ~(t "dialog.cancel")}]}
     ~(collection-name)
     (label#coll-error :when ~(not= state.newCollection.error "")
       :font small
       :anchors {:left 20 :right 20 :top 92}
       :height 16
       :text ~state.newCollection.error
       :textColor @color.warmEnd
       :live polite)
     (row#coll-kind :anchors {:left 20 :right 20 :top 114} :height 40 :gap 6 :alignItems center :role radiogroup :arrows horizontal
       :label ~(t "coll.kind")
       ~(repeat [kind ["manual" "dynamic"]] (kind-pill kind)))
     (label :font small
       :wrap true
       :anchors {:left 20 :right 20 :top 160}
       :height 30
       :text ~(t (str "coll.kind." state.newCollection.kind ".about"))
       :textColor @color.muted)
     (column#coll-games :when ~(= state.newCollection.kind "manual")
       :anchors {:left 20 :right 20 :top 194 :bottom 64}
       :scroll vertical
       :role list
       :label ~(t "coll.pick")
       ~(repeat [g (sort-by #(str/lower-case (:title %)) computed.owned) :key g.id] (pick-row g)))
     (row#coll-rules :when ~(= state.newCollection.kind "dynamic")
       :anchors {:left 20 :right 20 :top 194}
       :height 120
       :wrap true
       :gap 6
       :role radiogroup
       :label ~(t "coll.rule")
       ~(repeat [rule computed.collectionRules :key rule.key] (rule-pill rule)))))

(defview collection-name "The collection's name; Enter makes it." []
  `(textarea#coll-name :height 32
     :padding 8
     :anchors {:left 20 :right 20 :top 56}
     :borderWidth 1
     :borderColor ~(if (not= state.newCollection.error "") "@color.warmEnd" "@color.border")
     :focused {:borderColor @color.accent}
     :bind state.newCollection.title
     :maxLength 40
     :radius 3
     :color @color.field
     :textColor @color.text
     :placeholderColor @color.dim
     :placeholder ~(t "coll.namePlaceholder")
     :label ~(t "coll.name")
     :autofocus true
     :onSubmit ~(handler createCollection)))

(defview kind-pill "Picked games or a rule, as a pill, lit when it is the kind chosen." [kind]
  `(node :id ~(str "coll-kind-" kind)
     :style pillButton
     :role radio
     :icon ~(if (= kind "manual") "list-checks" "sparkles")
     :text ~(t (str "coll.kind." kind))
     :selected ~(= state.newCollection.kind kind)
     :color ~(if (= state.newCollection.kind kind) "@color.select" "@color.raised")
     :textColor ~(if (= state.newCollection.kind kind) "@color.bright" "@color.text")
     :onClick ~(handler set-collection-kind kind)))

(defview pick-row "An owned game with a box to tick it into the new collection." [g]
  `(row :id ~(str "coll-pick-" g.id) :anchors {:left 0 :right 0} :height 30 :gap 10 :alignItems center
     (checkbox :id ~(str "coll-check-" g.id)
       :checked ~(contains state.newCollection.games g.id)
       :label ~g.title
       :width 18
       :height 18
       :activeColor @color.accent
       :trackColor @color.border
       :onChange ~(handler pick-collection-game g.id))
     (image :file ~(str "../assets/art/" (or-else g.art g.id) "/cover.svg") :width 18 :height 18 :fit cover :radius 2)
     (label :grow 1 :height 20 :text ~g.title :textColor @color.text)
     (label :font small :fitText true :text ~(or-else g.genre "") :textColor @color.muted)
     (panel :width 8 :height 1 :color transparent)))

(defview rule-pill "A rule a dynamic collection can follow, as a pill, lit when it is the one chosen." [rule]
  `(node :id ~(str "coll-rule-" rule.key)
     :style pillButton
     :role radio
     :text ~rule.title
     :selected ~(= state.newCollection.rule.key rule.key)
     :color ~(if (= state.newCollection.rule.key rule.key) "@color.select" "@color.raised")
     :textColor ~(if (= state.newCollection.rule.key rule.key) "@color.bright" "@color.text")
     :onClick ~(handler choose-collection-rule rule)))

;;; What it does

(defn set-collection-kind "Chooses picked games or a rule for the new collection." [kind]
  (set "state.newCollection.kind" kind)
  (set "state.newCollection.error" ""))

(defn pick-collection-game "Ticks a game into the new collection, or out of it." [id]
  (set "state.newCollection.games" (toggled state.newCollection.games id)))

(defn choose-collection-rule "Chooses the rule the new collection follows." [rule]
  (set "state.newCollection.rule" rule)
  (set "state.newCollection.error" ""))

(defcommand newCollection "Opens the window that makes a collection, of games picked by hand or filled by a rule."
  :title ~(t "coll.new") :description ~(t "command.newCollection.about") :category ~(t "category.library")
  :icon "folder-plus"
  []
  (dismiss)
  (set "state.newCollection" {:open true :title "" :kind "manual" :games [] :rule nil :error ""}))

(defcommand createCollection "Makes the collection the window describes; it needs a name, and games or a rule."
  :title ~(t "coll.create") :description ~(t "command.createCollection.about") :category ~(t "category.library")
  []
  (let [draft state.newCollection
        title (str/trim (:title draft))
        manual (= (:kind draft) "manual")]
    (cond
      (empty? title) (set "state.newCollection.error" (t "coll.error.name"))
      (and (not manual) (nil? (:rule draft))) (set "state.newCollection.error" (t "coll.error.rule"))
      :else (do (set "state.collectionSeq" (inc state.collectionSeq))
                (set "state.collections"
                     (concat state.collections
                             [(if manual
                                {:id (str "mine-" state.collectionSeq) :title title :games (:games draft)}
                                {:id (str "mine-" state.collectionSeq)
                                 :title title
                                 :games []
                                 :rule {:field (get-in draft [:rule :field]) :value (get-in draft [:rule :value])}})]))
                (set "state.newCollection.open" false)
                (notify (t "coll.created" {:name title}))))))

(defcommand deleteCollection "Removes a collection; its games stay in the library."
  :title ~(t "coll.delete") :description ~(t "command.deleteCollection.about") :category ~(t "category.library")
  :icon "trash-2"
  [id]
  (set "state.collections" (remove #(= (:id %) id) state.collections)))
