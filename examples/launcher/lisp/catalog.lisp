;;; catalog.lisp --- the Apps page: the person's apps and the examples, as cards
;;;
;;; Each card is tagged Example or Yours; a click or Enter opens its app, and an example a new app can start from has
;;; a button that starts one. The search keeps the apps whose name or tagline holds it.

;;; The page

(defview apps-page "The Apps page: its title, the search, the count, and the cards." []
  `(node#appsPage :when ~(= state.page "apps") :fill true :color transparent :role tabpanel :label ~(t "nav.apps")
     (node#appsTitle :style pageTitle :text ~(t "apps.title") :role heading)
     (node#appsSub :style pageSub :text ~(t "apps.sub") :anchors {:left 36 :right 320 :top 72})
     (node#appSearch :use searchField
       :with {:name query
              :placeholder ~(t "apps.search")
              :label ~(t "apps.search")
              :clearLabel ~(t "apps.clear")
              :width ~(min 280 (- (- window.width computed.sideWidth) 420))
              :height 38
              :color @c.surface}
       :anchors {:right 36 :top 36})
     (label#appsCount :when ~(> (- window.width computed.sideWidth) 860)
       :font small
       :textColor @c.dim
       :align "end center"
       :text ~(t "apps.count" {:n (count computed.shown)})
       :anchors {:right 36 :top 82}
       :height 22
       :width 160)
     (grid#appsGrid :anchors {:left 24 :right 24 :top 116 :bottom 0}
       :columns ~computed.columns
       :gap 20
       :scroll vertical
       :role list
       :label ~(t "nav.apps")
       ~(repeat [app computed.shown :key app.id]
         `(node#appCard :use appCard :with {:app ~app})))
     (label#appsNone :when ~(= (count computed.shown) 0)
       :textColor @c.muted
       :align center
       :wrap true
       :text ~(t "apps.none" {:query (trim state.query)})
       :anchors {:left 36 :right 36 :top 180}
       :height 48
       :live polite)))

;;; A card

(defcomponent appCard "An app as a card: its cover or initial, its name and tagline, its tag, and Start from it."
  {:app {}}
  `(node#appCardPanel :style card
     :height 244
     :focusable true
     :role button
     :cursor hand
     :label ~(if app.mine
               (t "mine.openLabel" {:name app.name})
               (t "apps.openLabel" {:name app.name :tagline (t (str "tagline." app.id))}))
     :hover {:borderColor @c.accent :shadow {:y 8 :blur 22 :color @c.shadow}}
     :focused {:borderColor @c.accent :borderWidth 2}
     :onClick ~(handler openApp app)
     ~(card-cover app)
     (label#cardName :text ~app.name :font strong :textColor @c.text :anchors {:left 16 :right 52 :top 178} :height 22)
     ~(card-tagline app)
     (label#cardKind :font small
       :text ~(if app.mine (t "apps.tagMine") (t "apps.tagExample"))
       :textColor "#ffffff"
       :color ~(if app.mine "@c.accent" "#16142acc")
       :radius 999
       :padding 5
       :anchors {:left 18 :top 18}
       :fitText true
       :height 22)
     (node#cardStart :when ~(and (not app.mine) (= app.program "host") (not= app.starter false))
       :style iconButton
       :icon copy-plus
       :anchors {:right 10 :top 172}
       :tooltip ~(t "apps.startFrom" {:name app.name})
       :label ~(t "apps.startFrom" {:name app.name})
       :onClick ~(handler startFrom app.id))))

(defview card-cover "The card's picture: an example's cover, or the initial of the person's app." [app]
  `(node#cardCoverFrame :anchors {:left 10 :right 10 :top 10} :height 156 :radius 10 :color @c.raised
     (image#cardCover :when ~(not app.mine) :file ~app.cover :fit cover :fill true :label ~app.name)
     (label#cardInitial :when ~app.mine
       :text ~(upper-case (slice app.name 0 1))
       :font cover
       :align center
       :textColor @c.accent
       :color @c.accentSoft
       :radius 10
       :fill true)))

(defview card-tagline "The card's line: an example's tagline, or where the person's app is and when it changed." [app]
  `(label#cardTagline :text ~(if app.mine
                               (if (> app.modified 0)
                                 (t "mine.info" {:folder app.id :date (formatDate app.modified "medium")})
                                 app.id)
                               (t (str "tagline." app.id)))
     :font small
     :textColor @c.muted
     :wrap true
     :anchors {:left 16 :right 16 :top 202}
     :height 32))
