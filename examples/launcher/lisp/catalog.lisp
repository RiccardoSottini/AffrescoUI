;;; catalog.lisp --- the Apps page: the person's apps and the examples, as cards
;;;
;;; Each card is tagged with its category, or Yours; a click or Enter opens its app, and an example a new app can start from has
;;; a button that starts one. The search keeps the apps whose name or tagline holds it.

(ns launcher.catalog)

;;; The page

(defview apps-page "The Apps page: its title and the search, then, scrolled, the welcome, the categories and the cards." []
  `(node#appsPage :when ~(= state.page :apps) :fill true :color transparent :role tabpanel :label ~(t "nav.apps")
     (node#appsTitle :style pageTitle :text ~(t "apps.title") :role heading)
     (node#appSearch :use searchField
       :with {:name query
              :placeholder ~(t "apps.search")
              :label ~(t "apps.search")
              :clearLabel ~(t "apps.clear")
              :width ~(min 280 (- window.width computed.sideWidth 220))
              :height 38
              :color @c.surface}
       :anchors {:right 48 :top 32})
     (column#appsScroll :anchors {:left 0 :right 0 :top 84 :bottom 0} :scroll vertical :gap 0
       ~(welcome)
       ~(kind-chips)
       (grid#appsGrid :anchors {:left 28 :right 40}
         :height ~computed.gridHeight
         :columns ~computed.columns
         :gap 20
         :role list
         :label ~(t "nav.apps")
         ~(repeat [app computed.shown :key app.id]
           `(node#appCard :use appCard :with {:app ~app}))))
     (label#appsNone :when ~(empty? computed.shown)
       :textColor @c.muted
       :align center
       :wrap true
       :text ~(if (= (trim state.query) "") (t "apps.noneHere") (t "apps.none" {:query (trim state.query)}))
       :anchors {:left 36 :right 48 :top ~(if computed.welcomed 360 180)}
       :height 48
       :live polite)))

(defview welcome "The first thing a newcomer sees: the mark, a welcome, what the apps are, and New app." []
  `(node#welcome :when ~computed.welcomed :anchors {:left 36 :right 48} :height 160 :color transparent
     (node#welcomeCard :anchors {:left 0 :right 0 :top 0}
       :height 136
       :radius 18
       :color @c.accentSoft
       :borderWidth 1
       :borderColor @c.border
       (image#welcomeMark :file "../assets/affrescoui-mark.svg"
         :fit contain
         :anchors {:left 28 :top 32}
         :width 72
         :height 72
         :label "AffrescoUI")
       (label#welcomeTitle :text ~(t "apps.welcome")
         :font heading
         :textColor @c.text
         :anchors {:left 124 :right 224 :top 34}
         :height 26
         :role heading)
       (label#welcomeSub :text ~(t "apps.welcomeSub")
         :textColor @c.muted
         :wrap true
         :anchors {:left 124 :right 224 :top 66}
         :height 42)
       (node#welcomeNew :style primary
         :icon plus
         :text ~(t "nav.newApp")
         :anchors {:right 28 :top 46}
         :width 168
         :height 44
         :onClick showCreate))))

(defview kind-chips "The categories, one chip each, the one shown filled; and how many apps are shown." []
  `(node#kinds :anchors {:left 36 :right 48} :height 58 :color transparent
     (row#kindChips :anchors {:left 0 :right ~(if computed.countShown 120 0) :top 0}
       :height 40
       :gap 8
       :scroll horizontal
       :role radiogroup
       :arrows horizontal
       :label ~(t "apps.kinds")
       ~(repeat [k computed.kinds :key k]
         (kind-chip k)))
     (label#appsCount :when ~computed.countShown
       :font small
       :textColor @c.dim
       :align "end center"
       :text ~(t "apps.count" {:n (count computed.shown)})
       :anchors {:right 0 :top 0}
       :height 34
       :width 120)))

(defview kind-chip "A category of the Apps page: filled while it is the one shown." [k]
  `(label#kindChip :text ~(t (str "kind." k))
     :font small
     :align center
     :fitText true
     :height 34
     :padding 10
     :radius 999
     :focusable true
     :role radio
     :checked ~(= state.kind k)
     :cursor hand
     :color ~(if (= state.kind k) "@c.primary" "@c.surface")
     :textColor ~(if (= state.kind k) "@c.onAccent" "@c.text")
     :borderWidth 1
     :borderColor ~(if (= state.kind k) "@c.primary" "@c.border")
     :hover {:borderColor @c.accent}
     :onClick ~(handler show-kind k)))

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
     :onClick ~(handler open-app app)
     ~(card-cover app)
     (label#cardName :text ~app.name :font strong :textColor @c.text :anchors {:left 16 :right 52 :top 178} :height 22)
     ~(card-tagline app)
     (label#cardKind :font small
       :text ~(if app.mine (t "apps.tagMine") (t (str "kind." app.category)))
       :textColor "#ffffff"
       :color ~(if app.mine "@c.primary" "#16142acc")
       :radius 999
       :padding 5
       :anchors {:left 18 :top 18}
       :fitText true
       :height 22)
     (node#cardStart :when ~(and (not app.mine) (= app.program :host) (not= app.starter false))
       :style iconButton
       :icon copy-plus
       :anchors {:right 10 :top 172}
       :tooltip ~(t "apps.startFrom" {:name app.name})
       :label ~(t "apps.startFrom" {:name app.name})
       :onClick ~(handler start-from app.id))))

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
