;;; hub.lisp --- a game's player hub: what players post about it, as discussions, guides and screenshots
;;;
;;; Opened from the links under a game's banner. The posts, their first replies and the screenshots are invented, the
;;; same for every game, with its name and pictures in them. A post opens on its thread: what it says, its replies,
;;; and a box to reply in. A screenshot opens big, with its caption, its vote and the way to the next one.

(ns arcadia.parts.hub)

;;; The posts

(defstate hubTab hub)

(defstate hubPosts
  [{:id firstHours :kind discussions :key hub.post.firstHours :by 0 :replies 48 :votes 112}
   {:id allEndings :kind guides :key hub.post.allEndings :by 3 :replies 17 :votes 640}
   {:id controller :kind discussions :key hub.post.controller :by 1 :replies 9 :votes 21}
   {:id beginner :kind guides :key hub.post.beginner :by 5 :replies 31 :votes 288}
   {:id coop :kind discussions :key hub.post.coop :by 2 :replies 73 :votes 96}
   {:id secrets :kind guides :key hub.post.secrets :by 4 :replies 12 :votes 154}])

(defstate hubShots
  [{:id shot-1 :file shot1 :key hub.shot.0 :by 2 :votes 214}
   {:id shot-2 :file hero :key hub.shot.1 :by 4 :votes 389}
   {:id shot-3 :file shot2 :key hub.shot.2 :by 0 :votes 57}
   {:id shot-4 :file shot3 :key hub.shot.3 :by 5 :votes 131}])

(defstate hubPost nil)

(defstate hubShot nil)

(defstate hubDraft "")

(defstate hubReplies [])

(defstate hubVoted [])

(defcomputed hubOpen "The post whose thread is open, or nil on the list."
  (find #(= (:id %) state.hubPost) state.hubPosts))

(defcomputed hubThread "The open post's replies: three from players, then the person's own, in the order written."
  (if (some? computed.hubOpen)
    (concat (map (fn [k]
                   {:by (mod (+ computed.hubOpen.by k 1) (count data.friends))
                    :key (str "hub.reply." (mod (+ computed.hubOpen.by k) 4))
                    :mine false})
                 [0 1 2])
            (map #(assoc % :mine true) (filter #(= (:post %) state.hubPost) state.hubReplies)))
    []))

(defmacro hub-posts-shown
  ;; A macro, not a function: the list it writes stays an expression, for a repeat and a height.
  "The posts of the section chosen, or all of them on the hub's own tab."
  []
  `(filter #(or (= state.hubTab :hub) (= (:kind %) state.hubTab)) state.hubPosts))

(defmacro hub-votes
  ;; A macro: the sum stays an expression, read again when a vote changes.
  "A post's votes, with the person's own."
  [p]
  `(+ (:votes ~p) (count (filter #(= % (:id ~p)) state.hubVoted))))

(defcomputed hubWidth "The width of the hub's content: the page's, less its margins."
  (- (or-else (get-in nodes ["hub-page" :width]) 1008) 64))

(defcomputed hubThumbWidth "The width of a screenshot on the hub's shelf: four to a row." (/ (- computed.hubWidth 36) 4))

(defcomputed hubThumbHeight "The height of a screenshot on the hub's shelf, at 16:9." (/ (* computed.hubThumbWidth 9) 16))

(defcomputed hubCardWidth "The width of a screenshot on the screenshots tab: two to a row." (/ (- computed.hubWidth 12) 2))

(defcomputed hubCardHeight "The height of a screenshot's picture on the screenshots tab, at 16:9."
  (/ (* computed.hubCardWidth 9) 16))

(defcomputed hubBigHeight "The height of the open screenshot: the hub's width at 16:9, at most 420."
  (min 420 (/ (* computed.hubWidth 9) 16)))

(defcomputed hubBigWidth "The width of the open screenshot, at 16:9." (/ (* computed.hubBigHeight 16) 9))

(defcomputed hubBigLeft "Where the open screenshot starts, centred in the hub." (/ (- computed.hubWidth computed.hubBigWidth) 2))

(defmacro hub-shot-rows "How many rows the screenshots tab takes, two to a row." []
  `(ceil (/ (count state.hubShots) 2)))

(defmacro hub-browsing
  ;; A macro: the test stays an expression, for each part of the page it shows or hides.
  "Whether the hub shows its sections: no post's thread and no screenshot open."
  []
  `(and (nil? state.hubPost) (nil? state.hubShot)))

(defmacro hub-reply-count
  ;; A macro: the count stays an expression, read again when the person replies.
  "A post's replies, with the person's own."
  [p]
  `(+ (:replies ~p) (count (filter #(= (:post %) (:id ~p)) state.hubReplies))))

;;; The hub page

(defcomponent hubPage
  "The hub of the game on the page: its name, the three sections and the posts of the one chosen."
  {}
  `(column :fill true :scroll vertical :let {:g ~computed.game}
     ~(hub-header)
     ~(hub-tabs)
     ~(hub-thread)
     ~(hub-shot-view)
     ~(hub-shots-shelf)
     ~(hub-shots-grid)
     (column#hub-posts :when ~(and (hub-browsing) (not= state.hubTab "screenshots"))
       :anchors {:left 32 :right 32}
       :height ~(+ 8 (* (count (hub-posts-shown)) 76))
       :gap 8
       :role list
       :label ~(t "hub.subtitle")
       ~(repeat [p (hub-posts-shown)] (hub-post p)))))

(defview hub-header "The game's cover and name over the hub's subtitle, and the way back to the game's page." []
  `(row :height 84 :anchors {:left 32 :right 32} :gap 16 :alignItems center
     (image :file ~(str "../assets/art/" (or-else g.art g.id) "/cover.svg")
       :width 40
       :height 60
       :radius 6
       :fit cover)
     (column :grow 1 :height 48
       (label :font heading :height 26 :anchors {:left 0 :right 0} :text ~g.title :textColor @color.bright)
       (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t "hub.subtitle") :textColor @color.muted))
     (node#hub-back :style sectionLink :icon arrow-left :text ~(t "hub.back") :onClick ~(handler openGame g.id))))

(defview hub-tabs "The hub's sections, everything, the discussions, the guides and the screenshots, the chosen one lit."
  []
  `(row#hub-tabs :when ~(hub-browsing)
     :height 40
     :anchors {:left 32 :right 32} :gap 6 :alignItems center :role tablist :arrows horizontal
     ~(repeat [s ["hub" "discussions" "guides" "screenshots"]]
        `(node :id ~(str "hub-tab-" s)
           :style pillButton
           :role tab
           :selected ~(= state.hubTab s)
           :color ~(if (= state.hubTab s) "@color.select" "transparent")
           :textColor ~(if (= state.hubTab s) "@color.bright" "@color.text")
           :text ~(t (case s "hub" "hub.all" "screenshots" "hub.screenshots" (str "game.section." s)))
           :onClick ~(handler show-hub-tab s)))))

;;; A post

(defview hub-post "A post of the hub: its kind's icon, its title and author, its votes and replies." [p]
  `(row :id ~(str "hub-post-" p.id)
     :let {:who ~(get data.friends (mod p.by (count data.friends)))}
     :role listitem
     :label ~(t p.key {:game g.title})
     :focusable true
     :cursor hand
     :anchors {:left 0 :right 0}
     :height 68
     :gap 12
     :alignItems center
     :color @color.panel
     :radius 8
     :hover {:color @color.raised}
     :onClick ~(handler openHubPost p.id)
     (panel :width 2 :height 1 :color transparent)
     (label :icon ~(if (= p.kind "guides") "book-open" "messages-square")
       :text ""
       :fitText true
       :textColor @color.accent)
     ~(hub-post-title p)
     (label :font small :fitText true :icon thumbs-up :text ~(formatNumber (hub-votes p)) :textColor @color.muted)
     (label :font small :fitText true :icon message-circle :text ~(formatNumber (hub-reply-count p)) :textColor @color.muted)
     (panel :width 2 :height 1 :color transparent)))

(defview hub-post-title "A post's title, with the game's name in it, over who wrote it." [p]
  `(column :grow 1 :height 40
     (label :font strong
       :height 22
       :anchors {:left 0 :right 0}
       :text ~(t p.key {:game g.title})
       :textColor @color.text)
     (label :font small
       :height 18
       :anchors {:left 0 :right 0}
       :text ~(t "hub.by" {:who who.name})
       :textColor @color.muted)))

;;; A post's thread

(defview hub-thread "The open post: the way back to the list, the post itself, its replies, and the box to reply in." []
  `(column#hub-thread :when ~(some? computed.hubOpen)
     :let {:p ~computed.hubOpen}
     :anchors {:left 32 :right 32}
     :height ~(+ 40 196 18 (* (count computed.hubThread) 80) 72 48)
     :gap 12
     (row :height 40 :anchors {:left 0 :right 0} :alignItems center
       (node#hub-thread-back :style sectionLink
         :icon arrow-left
         :text ~(t "hub.backToPosts")
         :onClick ~(handler closeHubPost)))
     ~(hub-thread-post)
     (label :font caps
       :height 18
       :anchors {:left 0 :right 0}
       :text ~(upper-case (t "hub.replies" {:n (hub-reply-count p)}))
       :textColor @color.muted)
     (column#hub-replies :anchors {:left 0 :right 0}
       :height ~(* (count computed.hubThread) 80)
       :gap 8
       :role list
       :label ~(t "hub.replies" {:n (hub-reply-count p)})
       ~(repeat [r computed.hubThread] (hub-reply r)))
     ~(hub-reply-box)))

(defview hub-thread-post "The post: its kind, title and author, what it says, and its vote." []
  `(column#hub-thread-post :let {:who ~(get data.friends (mod p.by (count data.friends)))}
     :anchors {:left 0 :right 0}
     :height 196
     :color @color.panel
     :radius 10
     :gap 8
     (panel :height 6 :anchors {:left 0 :right 0} :color transparent)
     (label :font caps
       :height 18
       :anchors {:left 20 :right 20}
       :icon ~(if (= p.kind "guides") "book-open" "messages-square")
       :text ~(upper-case (t (str "game.section." p.kind)))
       :textColor @color.accent)
     (label#hub-thread-title :font heading
       :height 26
       :anchors {:left 20 :right 20}
       :text ~(t p.key {:game g.title})
       :textColor @color.bright)
     (row :height 24 :anchors {:left 20 :right 20} :gap 8 :alignItems center
       (node :use avatar :with {:avatar ~who.avatar :presence ~who.presence :size 22})
       (label :font small :fitText true :text ~(t "hub.by" {:who who.name}) :textColor @color.muted))
     (label :font default
       :height 40
       :anchors {:left 20 :right 20}
       :wrap true
       :text ~(t (str "hub.body." p.id) {:game g.title})
       :textColor @color.text)
     (row :height 28 :anchors {:left 20 :right 20} :gap 8 :alignItems center
       ~(hub-vote-button "hub-vote" p))))

(defview hub-reply "A reply: who wrote it, the person's own marked as theirs, and what it says." [r]
  `(row :let {:who ~(if r.mine nil (get data.friends r.by))}
     :role listitem
     :anchors {:left 0 :right 0}
     :height 72
     :gap 12
     :alignItems center
     :color ~(if r.mine "@color.raised" "@color.panel")
     :radius 8
     (panel :width 4 :height 1 :color transparent)
     (node :use avatar
       :with {:avatar ~(if r.mine state.user.avatar who.avatar)
              :presence ~(if r.mine state.user.status who.presence)
              :size 36})
     (column :grow 1 :height 44 :gap 4
       (label :font strong
         :height 20
         :anchors {:left 0 :right 0}
         :text ~(if r.mine (t "hub.you" {:name state.user.name}) who.name)
         :textColor ~(if r.mine "@color.accent" "@color.bright"))
       (label :font default
         :height 20
         :anchors {:left 0 :right 0}
         :text ~(if r.mine r.text (t r.key {:game g.title}))
         :textColor @color.text))
     (panel :width 4 :height 1 :color transparent)))

(defview hub-reply-box "The box to reply in, which Enter posts, and Reply, lit once there is something to post." []
  `(row :height 72 :anchors {:left 0 :right 0} :gap 10 :alignItems center
     (textarea#hub-reply-input :grow 1
       :height 38
       :padding 12
       :bind state.hubDraft
       :radius 6
       :color @color.field
       :borderWidth 1
       :borderColor @color.border
       :placeholder ~(t "hub.replyPlaceholder")
       :placeholderColor @color.dim
       :textColor @color.text
       :label ~(t "hub.replyPlaceholder")
       :focused {:borderColor @color.accent}
       :onSubmit ~(handler replyToPost))
     (label#hub-reply-send :font strong
       :fitText true
       :padding 16
       :height 38
       :radius 6
       :icon send
       :text ~(t "hub.reply")
       :focusable true
       :role button
       :cursor hand
       :color ~(if (= (trim state.hubDraft) "") "@color.raised" "@color.accent")
       :textColor ~(if (= (trim state.hubDraft) "") "@color.dim" "@color.onAccent")
       :onClick ~(handler replyToPost))))
;;; Screenshots

(defview hub-vote-button "The Helpful button of a post or a screenshot, with its votes, lit once the person voted." [id p]
  `(label :id ~id
     :font small
     :fitText true
     :padding 10
     :height 26
     :radius 13
     :icon thumbs-up
     :text ~(str (t "hub.helpful") "  " (formatNumber (hub-votes p)))
     :focusable true
     :role button
     :cursor hand
     :selected ~(contains state.hubVoted p.id)
     :color ~(if (contains state.hubVoted p.id) "@color.accent" "@color.raised")
     :textColor ~(if (contains state.hubVoted p.id) "@color.onAccent" "@color.text")
     :hover {:textColor @color.bright}
     :onClick ~(handler voteHubPost p.id)))

(defview hub-shot-picture "A screenshot's picture, which opens it big: a card of the screenshots tab, or a shelf's." [sh n card]
  (let [w (if card 'computed.hubCardWidth 'computed.hubThumbWidth)
        h (if card 'computed.hubCardHeight 'computed.hubThumbHeight)]
    `(image :id ~(str (if card "hub-shot-" "hub-thumb-") n)
       :file ~(str "../assets/art/" (or-else g.art g.id) "/" sh.file ".svg")
       :width ~w
       :height ~h
       :radius 8
       :fit cover
       :focusable true
       :role button
       :label ~(t sh.key)
       :cursor hand
       :hover {:borderWidth 2 :borderColor @color.accent}
       :onClick ~(handler openHubShot n))))

(defview hub-shots-shelf "On the hub's own tab: four screenshots in a row, under a heading with a link to them all." []
  `(column#hub-shots-shelf :when ~(and (hub-browsing) (= state.hubTab "hub"))
     :anchors {:left 32 :right 32}
     :height ~(+ 44 computed.hubThumbHeight 12)
     (row :height 40 :anchors {:left 0 :right 0} :alignItems center
       (label :font caps :fitText true :text ~(upper-case (t "hub.screenshots")) :textColor @color.muted)
       (panel :grow 1 :height 1 :color transparent)
       (node#hub-shots-all :style sectionLink
         :text ~(t "hub.seeAll")
         :onClick ~(handler show-hub-tab "screenshots")))
     (row :height ~computed.hubThumbHeight :anchors {:left 0 :right 0} :gap 12
       ~(repeat [sh state.hubShots :index n]
          (hub-shot-picture sh n false)))))

(defview hub-shots-grid "The screenshots tab: every screenshot, two to a row, each over its caption, author and votes."
  []
  `(row#hub-shots-grid :when ~(and (hub-browsing) (= state.hubTab "screenshots"))
     :wrap true
     :gap 12
     :anchors {:left 32 :right 32}
     :height ~(* (hub-shot-rows) (+ computed.hubCardHeight 64))
     :role list
     :label ~(t "hub.screenshots")
     ~(repeat [sh state.hubShots :index n]
        `(column :let {:who ~(get data.friends (mod sh.by (count data.friends)))}
           :role listitem
           :width ~computed.hubCardWidth
           :height ~(+ computed.hubCardHeight 52)
           :gap 6
           ~(hub-shot-picture sh n true)
           ~(hub-shot-caption sh)))))

(defview hub-shot-caption "A screenshot's caption over who took it, and its votes at the end." [sh]
  `(row :height 40 :anchors {:left 0 :right 0} :gap 8 :alignItems center
     (column :grow 1 :height 40
       (label :font strong :height 20 :anchors {:left 0 :right 0} :text ~(t sh.key) :textColor @color.text)
       (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t "hub.by" {:who who.name}) :textColor @color.muted))
     (label :font small
       :fitText true
       :icon thumbs-up
       :text ~(formatNumber (hub-votes sh))
       :textColor ~(if (contains state.hubVoted sh.id) "@color.accent" "@color.muted"))))

(defview hub-shot-view "An open screenshot, big: the way back, the picture between arrows, its caption and its vote." []
  `(column#hub-shot-view :when ~(some? state.hubShot)
     :let {:sh ~(get state.hubShots state.hubShot)}
     :anchors {:left 32 :right 32}
     :height ~(+ 40 computed.hubBigHeight 64)
     :gap 8
     (row :height 40 :anchors {:left 0 :right 0} :alignItems center
       (node#hub-shot-back :style sectionLink
         :icon arrow-left
         :text ~(t "hub.backToShots")
         :onClick ~(handler closeHubShot))
       (panel :grow 1 :height 1 :color transparent)
       (label :font small
         :fitText true
         :text ~(t "hub.shotOf" {:n (inc state.hubShot) :total (count state.hubShots)})
         :textColor @color.muted))
     (node :anchors {:left ~computed.hubBigLeft :right ~computed.hubBigLeft} :height ~computed.hubBigHeight
       (image#hub-shot-big :fill true
         :file ~(str "../assets/art/" (or-else g.art g.id) "/" sh.file ".svg")
         :radius 10
         :fit cover
         :label ~(t sh.key))
       ~(hub-shot-arrow "prev" "chevron-left" {:left 12})
       ~(hub-shot-arrow "next" "chevron-right" {:right 12}))
     (row :let {:who ~(get data.friends (mod sh.by (count data.friends)))}
       :height 48
       :anchors {:left ~computed.hubBigLeft :right ~computed.hubBigLeft}
       :gap 10
       :alignItems center
       (node :use avatar :with {:avatar ~who.avatar :presence ~who.presence :size 32})
       (column :grow 1 :height 40
         (label#hub-shot-caption :font heading :height 22 :anchors {:left 0 :right 0} :text ~(t sh.key) :textColor @color.bright)
         (label :font small :height 18 :anchors {:left 0 :right 0} :text ~(t "hub.by" {:who who.name}) :textColor @color.muted))
       ~(hub-vote-button "hub-shot-vote" sh))))

(defview hub-shot-arrow "An arrow over the open screenshot that shows the one before or after, round the ends." [dir icon place]
  `(label :id ~(str "hub-shot-" dir)
     :icon ~icon
     :text ""
     :font heading
     :width 40
     :height 40
     :radius 20
     :align center
     :anchors ~(merge place {:top (- (/ computed.hubBigHeight 2) 20)})
     :color "#00000080"
     :textColor "#ffffff"
     :focusable true
     :role button
     :label ~(t (str "hub.shot." dir))
     :cursor hand
     :hover {:color "#000000c0"}
     :onClick ~(handler step-hub-shot (if (= dir "prev") -1 1))))
