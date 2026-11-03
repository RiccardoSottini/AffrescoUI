;;; hub.lisp --- A game's player hub: what players post about it, as discussions and guides, opened from the links under the game's banner
;;;
;;; The posts are invented, the same for every game, with its name in them.

;;; Definitions

(defstate hubTab hub)

(defstate hubPosts
  [{:kind discussions :key hub.post.firstHours :by 0 :replies 48 :votes 112}
   {:kind guides :key hub.post.allEndings :by 3 :replies 17 :votes 640}
   {:kind discussions :key hub.post.controller :by 1 :replies 9 :votes 21}
   {:kind guides :key hub.post.beginner :by 5 :replies 31 :votes 288}
   {:kind discussions :key hub.post.coop :by 2 :replies 73 :votes 96}
   {:kind guides :key hub.post.secrets :by 4 :replies 12 :votes 154}])

(defcomponent hubPage "The hub of the game on the page: its name, the three sections and the posts of the one chosen."
  {}
  `(column :fill true :scroll vertical :let {:g ~computed.game}
     (row :height 84 :anchors {:left 32 :right 32} :gap 16 :alignItems center
       (image :file ~(str "../assets/art/" (or-else g.art g.id) "/cover.svg") :width 40 :height 60 :radius 6 :fit cover)
       (column :grow 1 :height 48
         (label :font heading :height 26 :anchors {:left 0 :right 0} :text ~g.title :textColor @color.bright)
         (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t "hub.subtitle") :textColor @color.muted))
       {:id hub-back
        :style sectionLink
        :icon arrow-left
        :text ~(t "hub.back")
        :onClick ~(handler openGame g.id)})
     (row#hub-tabs :height 40 :anchors {:left 32 :right 32} :gap 6 :alignItems center :role tablist :arrows horizontal
       ~(repeat [s ["hub" "discussions" "guides"]]
         `{:id ~(str "hub-tab-" s)
           :style pillButton
           :role tab
           :selected ~(= state.hubTab s)
           :color ~(if (= state.hubTab s) "@color.select" "transparent")
           :textColor ~(if (= state.hubTab s) "@color.bright" "@color.text")
           :text ~(t (if (= s "hub") "hub.all" (str "game.section." s)))
           :onClick {:set state.hubTab :to ~s}}))
     (column#hub-posts :anchors {:left 32 :right 32}
       :height ~(+ 8
                   (* (count (filter (fn [it] (or (= state.hubTab "hub") (= it.kind state.hubTab))) state.hubPosts)) 76))
       :gap 8
       :role list
       :label ~(t "hub.subtitle")
       ~(repeat [p (filter (fn [it] (or (= state.hubTab "hub") (= it.kind state.hubTab))) state.hubPosts)]
         (hub-posts-row p)))))

;;; Views

(defview hub-posts-column "#hub-posts-column." [p]
  `(column :grow 1 :height 40
     (label :font
            strong
            :height
            22
            :anchors
            {:left 0 :right 0}
            :text
            ~(t p.key {:game g.title})
            :textColor
            @color.text)
     (label :font
            small
            :height
            18
            :anchors
            {:left 0 :right 0}
            :text
            ~(t "hub.by" {:who who.name})
            :textColor
            @color.muted)))

(defview hub-posts-row "#hub-posts-row." [p]
  `(row :let {:who ~(get data.friends (mod p.by (count data.friends)))}
     :role listitem
     :anchors {:left 0 :right 0}
     :height 68
     :gap 12
     :alignItems center
     :color @color.panel
     :radius 8
     (panel :width 2 :height 1 :color transparent)
     (label :icon
            ~(if (= p.kind "guides") "book-open" "messages-square")
            :text
            ""
            :fitText
            true
            :textColor
            @color.accent)
     ~(hub-posts-column p)
     (label :font small :fitText true :icon thumbs-up :text ~(formatNumber p.votes) :textColor @color.muted)
     (label :font small :fitText true :icon message-circle :text ~(formatNumber p.replies) :textColor @color.muted)
     (panel :width 2 :height 1 :color transparent)))
