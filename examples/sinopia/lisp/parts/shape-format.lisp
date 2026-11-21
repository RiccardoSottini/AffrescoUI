;;; shape-format.lisp --- the contextual Shape Format tab, shown while an object is selected
;;;
;;; The tab sits after Help in the command bar, in the brand colour, while something on the slide is selected (and
;;; while it is the tab shown). It gathers what formats a shape: the shapes to insert, the shape styles, the order and
;;; the arrangement, and the size, with the proportions kept when they are locked (parts/format.lisp).

(ns sinopia.parts.shape-format)

;;; What the tab keeps

(defcomputed contextTabs "The contextual tabs: Shape Format while an object is selected or the tab is shown."
  (if (or (some? computed.first) (= state.tab "shapeFormat"))
    [{:id "shapeFormat" :title (t "shapeFormat.tab") :contextual true}]
    []))

(defcomputed quickStyleCells "The quick styles the tab's gallery shows: filled, then outlined, each with its colours."
  (concat (map (fn [n]
                 {:id (str "accent" n) :fill (str "accent" n) :line (str "accent" n) :ink "light1" :width 1})
               (range 1 7))
          (map (fn [n]
                 {:id (str "line:accent" n) :fill "light1" :line (str "accent" n) :ink (str "accent" n) :width 2})
               (range 1 7))))

(defcomputed menu-sf-shapes "The tab's shape gallery's whole list: the Shapes menu." (get computed "menu-b-ins-shapes"))

(defcomputed menu-sf-effects "The tab's Shape Effects menu." computed.effectItems)

(defcomputed menu-sf-arrange "The tab's Arrange menu." computed.arrangeItems)

(defcomputed sizeBox "The first selected object's box, its placeholder's where it has none of its own."
  (if (some? computed.first) (merge computed.firstPlaceholder computed.first) {}))

;;; The tab

(defcomponent tabShapeFormat "The Shape Format tab: insert shapes, shape styles, arrange, and size."
  {}
  `(row#tab-shape-format :fitContent width :height 92 :gap 4 :color transparent
     ~(g-sf-shapes)
     ~(g-sf-styles)
     ~(g-sf-arrange)
     ~(g-sf-size)))

(defview g-sf-shapes "The shapes gallery: a click adds one; the last button lists every shape." []
  `(node#g-sf-shapes :use rbGroup :with {:title ~(t "shapeFormat.insertShapes")}
     (node#sf-shapes :use rbGallery :with {:width 168 :more sf-shapes}
       (grid#sf-shapes-grid :columns 6 :width 148 :height 66 :gap 1 :color transparent
         ~(repeat [k computed.galleryShapes]
           (sf-shape k))))))

(defview sf-shape "Shape `k` of the gallery drawn small, or a text box: a click adds one to the slide." [k]
  `(panel :id ~(str "sf-shape-" k)
     :width 24
     :height 21
     :radius 2
     :color transparent
     :cursor hand
     :hover {:color @rbHover}
     :tooltip ~(if (= k "textBox") (t "ribbon.textBox2") (t (str "shape." k)))
     :onClick ~(handler shape-chosen k)
     (label :id ~(str "sf-shape-" k "-tb")
       :when ~(= k "textBox")
       :fill 0
       :align center
       :textColor @rbInk
       :runs [{:icon text-cursor-input :size 14}])
     (node :id ~(str "sf-shape-" k "-box")
       :when ~(not= k "textBox")
       :x 5
       :y 4
       :width 14
       :height 13
       :color transparent
       :overflow visible
       (node :use shapePath
         :with {:o ~{:shape k
                     :w 14
                     :h (if (get-in computed.shapes [k :line]) 0 13)
                     :fill "none"
                     :line "#444444"
                     :lineWidth 1
                     :flipH false
                     :flipV false
                     :gradient nil
                     :dash nil
                     :effect nil}
                :name ~(str "sf-shape-" k)}))))

(defview g-sf-styles "The quick styles as a gallery, the fill and outline in the format pane, and the effects." []
  `(node#g-sf-styles :use rbGroup :with {:title ~(t "shapeFormat.shapeStyles") :launcher "launch-sf-styles"}
     (row#sf-styles :width 196 :height 70 :color @rbFieldBg :borderWidth 1 :borderColor @rbFieldLine :radius 10
       :alignItems center
       (grid#sf-styles-grid :x 6 :columns 6 :width 184 :height 60 :gap 4 :color transparent
         ~(repeat [q computed.quickStyleCells :key q.id]
           (sf-style q))))
     (node#s-sf-styles :use rbStack
       (node#b-sf-fill :use rbSmall
         :with {:icon paint-bucket :title ~(t "ribbon.shapeFill") :enabled ~(some? computed.first)}
         :tooltip ~(t "shapeFormat.inThePane")
         :onClick openFormatPane)
       (node#b-sf-outline :use rbSmall
         :with {:icon pen-tool :title ~(t "ribbon.shapeOutline") :enabled ~(some? computed.first)}
         :tooltip ~(t "shapeFormat.inThePane")
         :onClick openFormatPane)
       (node#sf-effects :use rbSmall
         :with {:icon sparkles :title ~(t "ribbon.shapeEffects") :menu true :enabled ~(some? computed.first)}
         :tooltip ~(t "ribbon.shapeEffects")
         :onClick ~(handler open-drop "sf-effects")))))

(defview sf-style "Quick style `q` as a cell: Abc in its colours on its fill; a click gives it to the selection." [q]
  `(label :id ~(str "sf-style-" q.id)
     :width 27
     :height 28
     :radius 4
     :align center
     :cursor hand
     :color ~(get computed.palette q.fill)
     :borderWidth ~q.width
     :borderColor ~(get computed.palette q.line)
     :textColor ~(get computed.palette q.ink)
     :tooltip ~(t "ribbon.shapeStyles2")
     :disabled ~(nil? computed.first)
     :runs [{:text ~(t "shapeFormat.sample") :size 10}]
     :onClick ~(handler apply-color "b-quick-styles" q.id)))

(defview g-sf-arrange "Forward and backward, the selection pane, and the arrange menu (align, distribute, order)." []
  `(node#g-sf-arrange :use rbGroup :with {:title ~(t "ribbon.arrange")}
     (node#s-sf-arrange :use rbStack
       (node#b-sf-forward :use rbSmall
         :with {:icon arrow-up-from-line :title ~(t "arrange.forward") :enabled ~(some? computed.first)}
         :onClick ~(handler order "forward"))
       (node#b-sf-backward :use rbSmall
         :with {:icon arrow-down-to-line :title ~(t "arrange.backward") :enabled ~(some? computed.first)}
         :onClick ~(handler order "backward"))
       (node#b-sf-selection :use rbSmall
         :with {:icon list :title ~(t "menu.selectionPane") :on ~(= state.sidePane "select")}
         :onClick ~(handler openSidePane "select")))
     (node#sf-arrange :use rbBig
       :with {:icon layers-2 :title ~(t "ribbon.arrange") :menu true :width 52 :enabled ~(some? computed.first)}
       :tooltip ~(t "ribbon.arrange")
       :onClick ~(handler open-drop "sf-arrange"))))

(defview g-sf-size "The selection's height and width, stepped by ten points, and the proportion lock." []
  `(node#g-sf-size :use rbGroup :with {:title ~(t "shapeFormat.size") :launcher "launch-sf-size"}
     (column#s-sf-size :width 150 :height 72 :gap 0 :color transparent :justify space-evenly
       ~(size-row "sf-h" "move-vertical" "view.height" "h" (or-else computed.sizeBox.h 100))
       ~(size-row "sf-w" "move-horizontal" "view.width" "w" (or-else computed.sizeBox.w 100))
       (node#b-sf-ratio :use rbSmall
         :with {:icon ~(if computed.ratioLocked "link-2" "link-2-off")
                :title ~(t "format.lockProportions")
                :on ~computed.ratioLocked
                :enabled ~(some? computed.first)}
         :tooltip ~(t "format.lockProportionsAbout")
         :onClick toggle-ratio))))

(defview size-row "A size of the selection, `key`, its value and the buttons that step it; `title` is a key."
  [prefix icon title key value]
  `(row :id ~prefix :fitContent width :height 22 :gap 2 :alignItems center :color transparent
     (label :id ~(str prefix "-icon") :width 20 :height 22 :align center :textColor @rbInk
       :runs [{:icon ~icon :size 14}]
       :tooltip ~(t title))
     (label :id ~(str prefix "-value")
       :width 70
       :height 20
       :radius 4
       :align end
       :padding 4
       :color @rbFieldBg
       :borderWidth 1
       :borderColor @rbFieldLine
       :textColor ~(if (some? computed.first) "@rbInk" "@rbMuted")
       :label ~(t title)
       :runs [{:text ~(t "shapeFormat.points" {:n (fixed value 1)}) :size 12}])
     (node :id ~(str prefix "-minus")
       :use rbSmall
       :with {:icon minus :enabled ~(some? computed.first)}
       :tooltip ~(t "view.less")
       :onClick ~(handler set-property key (max (- value 10) 1)))
     (node :id ~(str prefix "-plus")
       :use rbSmall
       :with {:icon plus :enabled ~(some? computed.first)}
       :tooltip ~(t "view.more")
       :onClick ~(handler set-property key (+ value 10)))))
