;;; sidebar.lisp --- the Launcher's sidebar: the mark, the pages, and New app
;;;
;;; In a narrow window the sidebar folds to its icons: the names and the keys go, the tooltips say them.

;;; The sidebar

(defview sidebar "The sidebar: the mark and its name, the pages, a note, and New app." []
  `(node#sidebar :anchors {:left 0 :top 0 :bottom 0}
     :width ~computed.sideWidth
     :color @c.side
     :dir auto
     :role navigation
     :label ~(t "nav.label")
     (node#sidebarEdge :anchors {:right 0 :top 0 :bottom 0} :width 1 :color @c.border)
     (image#brandMark :file "../assets/affrescoui-mark.svg"
       :fit contain
       :anchors {:left ~(if computed.wide 20 16) :top 22}
       :width 40
       :height 40
       :label "AffrescoUI")
     (label#brandName :when ~computed.wide
       :text "AffrescoUI"
       :font brand
       :textColor @c.text
       :anchors {:left 72 :top 22}
       :height 22
       :fitText true)
     (label#brandSub :when ~computed.wide :text ~(t "brand.sub") :font small :textColor @c.muted
       :anchors {:left 72 :top 44} :height 18 :fitText true)
     ~(pages-nav)
     ~(new-app-button)
     (label#sideNote :when ~computed.wide
       :font small
       :textColor @c.dim
       :wrap true
       :text ~(t "brand.note")
       :anchors {:left 20 :right 16 :bottom 74}
       :height 52)))

(defmacro key-label
  ;; A macro, so that the label stays an expression of the running app.
  "A key's label, as the platform writes it: with ⌘ on a Mac, Ctrl elsewhere."
  [mac other]
  `(if (= context.platform "mac") ~mac ~other))

(defview pages-nav "The pages, each with its key." []
  `(column#nav :anchors {:left 12 :right 12 :top 90}
     :height 186
     :gap 6
     :arrows vertical
     :role tablist
     :label ~(t "nav.label")
     (node#navApps :use navItem
       :with {:id apps :icon layout-grid :title ~(t "nav.apps") :keyLabel ~(key-label "⌘1" "Ctrl+1")})
     (node#navMine :use navItem
       :with {:id mine :icon folder-heart :title ~(t "nav.mine") :keyLabel ~(key-label "⌘2" "Ctrl+2")})
     (node#navCreate :use navItem
       :with {:id create :icon circle-plus :title ~(t "nav.create") :keyLabel ~(key-label "⌘N" "Ctrl+N")})
     (node#navSettings :use navItem
       :with {:id settings :icon settings :title ~(t "nav.settings") :keyLabel ~(key-label "⌘," "Ctrl+,")})))

(defcomponent navItem "A page of the sidebar: its icon, and its name and key while the sidebar is wide."
  {:id "" :icon "" :title "" :keyLabel ""}
  `(node#navItemPanel :height 42
     :radius 10
     :anchors {:left 0 :right 0}
     :focusable true
     :role tab
     :cursor hand
     :selected ~(= state.page id)
     :label ~title
     :tooltip ~(if computed.wide "" title)
     :color ~(if (= state.page id) "@c.accentSoft" "transparent")
     :hover {:color ~(if (= state.page id) "@c.accentSoft" "@c.hover")}
     :onClick ~(handler go id)
     (label#navIcon :icon ~icon
       :font bigIcons
       :align center
       :textColor ~(if (= state.page id) "@c.accent" "@c.muted")
       :anchors {:left ~(if computed.wide 8 0) :top 0 :bottom 0}
       :width ~(if computed.wide 32 48))
     (label#navTitle :when ~computed.wide
       :text ~title
       :font ~(if (= state.page id) "strong" "default")
       :textColor ~(if (= state.page id) "@c.accent" "@c.text")
       :anchors {:left 48 :top 0 :bottom 0 :right 48}
       :align "start center")
     (label#navKey :when ~computed.wide :text ~keyLabel :font small :textColor @c.dim
       :anchors {:right 12 :top 0 :bottom 0} :width 40 :align "end center")))

(defview new-app-button "New app: a button with its name, or only its icon while the sidebar is narrow." []
  `(node#sideNew :style primary
     :icon plus
     :text ~(if computed.wide (t "nav.newApp") "")
     :font ~(if computed.wide "strong" "bigIcons")
     :label ~(t "nav.newApp")
     :tooltip ~(if computed.wide "" (t "nav.newApp"))
     :anchors {:left 12 :right 12 :bottom 20}
     :height 42
     :onClick showCreate))
