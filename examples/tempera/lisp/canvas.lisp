;;; canvas.lisp --- the canvas area: the document's tab, the rulers, the image with its guides, and the status bar
;;;
;;; The image is a raster node showing the image doc (image.lisp): the pointer on it paints, selects or pans as the
;;; tool says, in the library. A click on a ruler adds a guide there. The status bar says what the library last did,
;;; the zoom (type one to change it) and one fact of the document, chosen from its menu.

;;; The area

(defview canvas-area "The canvas area: the document's tab, the rulers, the image, the status bar." []
  `(column#canvas-area :role main
     :label ~(t "canvas.label")
     :grow 1
     :height "100%"
     :color @canvasBack
     :radius @kit.cardRadius
     ~(document-tab)
     ~(top-ruler)
     (row#canvas-row :width "100%" :grow 1
       ~(side-ruler)
       (node#canvas-frame :style clear :grow 1 :height "100%"
         (raster#canvas :label ~(t "canvas.image") :image doc :fill true :checker true)
         ~@(guides)))
     ~(status-bar)))

(defview document-tab "The document's tab: its name, the active layer, the zoom and the mode; closing it closes it." []
  `(tabs#document-tabs :label ~(t "canvas.documents")
     :width "100%"
     :height 26
     :color @header
     :radius {:topLeft @kit.cardRadius :topRight @kit.cardRadius}
     :accentEdge top
     :tabs [{:id doc
             :closable true
             :label ~(str computed.docName " — " (or-else computed.activeLayer.name "") " · "
                          (round computed.zoomPercent) "% · " computed.mode)}]
     :value doc
     :textColor @muted
     :activeTextColor @text
     :activeBackground @panel
     :accentColor @accent
     :onClose closeDocument))

(defview guides "The guides over the image, upright and level, where they are in the image." []
  [(repeat [g state.guides.vertical]
     `(node#guide-v :when ~state.view.guides
        :x ~(+ (/ computed.canvasWidth 2) (* (- g state.doc.view.x) computed.px))
        :width 1
        :anchors {:top 0 :bottom 0}
        :color @guide))
   (repeat [g state.guides.horizontal]
     `(node#guide-h :when ~state.view.guides
        :y ~(+ (/ computed.canvasHeight 2) (* (- g state.doc.view.y) computed.px))
        :height 1
        :anchors {:left 0 :right 0}
        :color @guide))])

;;; The rulers

(defview top-ruler "The horizontal ruler after its corner: small ticks, numbered ticks; a click adds a guide." []
  `(row#ruler-row :when ~state.view.rulers :width "100%" :height 16
     (node#ruler-corner :width 16 :height 16 :color @ruler)
     (node#ruler-top :label ~(t "canvas.rulerAcross")
       :grow 1
       :height 16
       :color @ruler
       :cursor hand
       :tooltip ~(t "canvas.rulerAcross.tip")
       :onClick guideAcross
       ~(repeat [tick computed.rulerMinorX]
         `(node :x ~(+ (/ computed.canvasWidth 2) (* (- tick state.doc.view.x) computed.px)) :y 12 :width 1 :height 4
            :color @muted))
       ~(repeat [tick computed.rulerTicksX]
         `(node :x ~(+ (/ computed.canvasWidth 2) (* (- tick state.doc.view.x) computed.px)) :width 1 :height 16
            :color @muted))
       ~(repeat [tick computed.rulerTicksX]
         `(label :font small
            :text ~tick
            :textColor @muted
            :x ~(+ (/ computed.canvasWidth 2) (* (- tick state.doc.view.x) computed.px) 2)
            :y 0
            :fitText true)))))

(defview side-ruler "The vertical ruler: small ticks, numbered ticks; a click adds a guide." []
  `(node#ruler-left :label ~(t "canvas.rulerDown")
     :when ~state.view.rulers
     :width 16
     :height "100%"
     :color @ruler
     :cursor hand
     :tooltip ~(t "canvas.rulerDown.tip")
     :onClick guideDown
     ~(repeat [tick computed.rulerMinorY]
       `(node :y ~(+ (/ computed.canvasHeight 2) (* (- tick state.doc.view.y) computed.px)) :x 12 :width 4 :height 1
          :color @muted))
     ~(repeat [tick computed.rulerTicksY]
       `(node :y ~(+ (/ computed.canvasHeight 2) (* (- tick state.doc.view.y) computed.px)) :width 16 :height 1
          :color @muted))
     ~(repeat [tick computed.rulerTicksY]
       `(label :align {:horizontal start :vertical center}
          :font small
          :text ~tick
          :textColor @muted
          :y ~(+ (/ computed.canvasHeight 2) (* (- tick state.doc.view.y) computed.px) 2)
          :x 1
          :width 15
          :height 12))))

;;; The status bar

(defview status-bar "The status bar: what the library last did, the zoom, and the fact of the document chosen." []
  `(row#status-bar :role status
     :label ~(t "status.label")
     :style bar
     :height 24
     :gap 12
     :radius {:bottomLeft @kit.cardRadius :bottomRight @kit.cardRadius}
     (node :width 4 :height 1 :style clear)
     (node#status-message :style dim :live polite :text ~state.doc.message)
     (node :grow 1 :height 1 :style clear)
     (textarea#status-zoom :label ~(t "status.zoom")
       :text ~(str computed.zoomPercent "%")
       :width 52
       :height 20
       :radius 999
       :padding 4
       :textColor @text
       :color @panel
       :hover {:color @input}
       :onSubmit zoomTyped)
     ~@(document-facts)
     ~(status-menu)
     (node :width 4 :height 1 :style clear)))

(defview document-facts "The facts the status bar can show, one at a time: sizes, dimensions, profile, undo memory." []
  [`(node#status-doc :style dim
      :when ~(= state.statusInfo "doc")
      :text ~(t "status.docSize" {:size (fixed (/ (* state.doc.width state.doc.height (bytes-per-pixel)) 1048576) 2)}))
   `(node#status-size :style dim
      :when ~(= state.statusInfo "size")
      :text ~(t "status.dimensions" {:width (text state.doc.width) :height (text state.doc.height)}))
   `(node#status-profile :style dim
      :when ~(= state.statusInfo "profile")
      :text ~(str state.doc.profile (if (empty? state.doc.proof) "" (t "status.proof" {:proof state.doc.proof}))))
   `(node#status-memory :style dim
      :when ~(= state.statusInfo "memory")
      :text ~(t "status.undoMemory" {:size (fixed (/ state.doc.memory 1048576) 1)}))])

(defmacro bytes-per-pixel "What a pixel of the image takes, as the status bar counts it: 16-bit float or 8-bit RGB." []
  '(if (= state.doc.format "rgba16f") 6 3))

(defview status-menu "The chevron at the status bar's end, and the menu of what it can show." []
  `(label#status-info :role button
     :label ~(t "status.info")
     :tooltip ~(t "status.info.tip")
     :icon chevron-right
     :width 18
     :height 18
     :align center
     :radius 3
     :cursor hand
     :textColor @muted
     :hover {:color @hover :textColor @text}
     :onClick {:toggle state.statusMenu}
     (node :use tMenu
       :with {:open state.statusMenu
              :placement above
              :width 200
              :items ~(map (fn [it]
                             {:title (t (str "status.show." it))
                              :checked (= state.statusInfo it)
                              :run "setStatusInfo"
                              :with {:args {:id it}}})
                           ["doc" "size" "profile" "memory"])})))
