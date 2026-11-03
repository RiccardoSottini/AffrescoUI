;;; computed.lisp --- what Tempera works out from the image's snapshot and the workspace
;;;
;;; Each defcomputed is a value the views and the functions read as computed.<name>, worked out again when what it reads
;;; changes: the active layer and its neighbours, the tool, the canvas's size and its rulers, the Layers panel's rows.

;;; Layers

(defcomputed activeLayer "The active layer, or nil." (find (fn [it] (= it.id state.doc.active)) state.doc.layers))

(defcomputed layersAfterActive "The layers under the active one, in the panel's order: a group's are its children first."
  (slice state.doc.layers (+ (indexOf (map (fn [it] it.id) state.doc.layers) state.doc.active) 1)))

(defcomputed layerBelow "The layer Merge Down merges into: the next one at the active layer's depth or above."
  (find (fn [it] (<= it.depth (or-else computed.activeLayer.depth 0))) computed.layersAfterActive))

(defcomputed groupChildren "The active group's own layers: those one level deeper, until the group ends."
  (map (fn [it] it.layer)
       (filter (fn [it] (and (not it.ended) (= it.layer.depth (+ computed.activeLayer.depth 1))))
               (scan computed.layersAfterActive
                     (fn [acc it] {:ended (or acc.ended (<= it.depth computed.activeLayer.depth)) :layer it})
                     {:ended false}))))

(defcomputed nextGroupName "The name a new group takes: Group and how many there will be."
  (t "layer.groupName" {:n (+ (count (filter (fn [it] (= it.kind "group")) state.doc.layers)) 1)}))

(defcomputed adjustment "The active adjustment layer's values, or nil." computed.activeLayer.adjustment)

(defcomputed adjustmentKind "The active adjustment layer's kind, or nothing." (or-else computed.activeLayer.adjustment.kind ""))

(defcomputed blendTitle "The active layer's blend mode, as the Layers panel shows it."
  (t (str "blend." (or-else computed.activeLayer.blend "normal"))))

(defcomputed layerShown "The layers the panel lists: a folded group's layers are hidden."
  (map (fn [it] it.layer)
       (filter (fn [it] (not it.hide))
               (scan state.doc.layers
                     (fn [acc it]
                       (if (< acc.cut it.depth)
                         {:cut acc.cut :hide true :layer it}
                         {:cut (if (and (= it.kind "group") (contains state.foldedGroups it.id)) it.depth 999)
                          :hide false
                          :layer it}))
                     {:cut 999 :hide false}))))

(defcomputed layerRows "The Layers panel's rows: the layers shown, of the kinds its filter lets through."
  (if (and state.layerFilter.on (not (empty? state.layerFilter.kinds)))
    (filter (fn [it] (contains state.layerFilter.kinds it.kind)) computed.layerShown)
    computed.layerShown))

;;; The selection and the image

(defcomputed docName "The document's name: the first one is Untitled 1." (if (empty? state.docName) (t "document.first") state.docName))

(defcomputed box "The selection's box, or else the image's: x, y, width and height."
  (if state.doc.selection state.doc.bounds [0 0 state.doc.width state.doc.height]))

(defcomputed zoomPercent "The zoom, as a percentage to a tenth." (/ (round (* state.doc.view.zoom 1000)) 10))

(defcomputed mode "The image's mode, as the document's tab shows it: RGB or Gray, and its bits."
  (str (if (contains state.doc.profile "Grey") (t "mode.gray") (t "mode.rgb"))
       (if (= state.doc.format "rgba16f") "/16" "/8")))

(defcomputed fillContents "What the Fill dialog can fill with, and its colour."
  [{:id "foreground" :title (t "fill.foreground") :color state.doc.foreground}
   {:id "background" :title (t "fill.background") :color state.doc.background}
   {:id "black" :title (t "fill.black") :color "#000000"}
   {:id "grey" :title (t "fill.grey") :color "#808080"}
   {:id "white" :title (t "fill.white") :color "#ffffff"}])

(defcomputed filterTitle "The open filter's name, the filter dialog's title."
  (if (some (fn [it] (= it.id state.filter.kind)) state.filters) (t (str "filter." state.filter.kind)) (t "filter.any")))

;;; The tool

(defcomputed tool "The current tool, from the tools bar's groups."
  (or-else (find (fn [it] (= it.id state.doc.tool))
                 (or-else (find (fn [group] (some (fn [it] (= it.id state.doc.tool)) group)) state.tools) []))
           {}))

(defcomputed toolTitle "The current tool's name, as its tooltip says it." (t (str "tool." computed.tool.name)))

(defcomputed toolName "The current tool's short name, on the options bar." (t (str "tool." computed.tool.name ".short")))

(defcomputed paints "The tool paints." (contains ["brush" "pencil" "eraser" "clone"] state.doc.tool))

(defcomputed selects "The tool selects." (contains ["marquee" "ellipse" "lasso" "polygon" "wand"] state.doc.tool))

;;; The canvas and its rulers

(defcomputed px "Screen pixels per image pixel." (/ state.doc.view.zoom (or-else window.scale 1)))

(defcomputed canvasWidth "The canvas's width: the node's, or, before the first layout, what the window leaves it."
  (if (> (or-else nodes.canvas.width 0) 0)
    nodes.canvas.width
    (- window.width 6 40 6 6 304 6 (if state.view.rulers 16 0))))

(defcomputed canvasHeight "The canvas's height: the node's, or, before the first layout, what the window leaves it."
  (if (> (or-else nodes.canvas.height 0) 0)
    nodes.canvas.height
    (- window.height 24 6 36 6 6 26 24 (if state.view.rulers 16 0))))

(defcomputed rulerStep "The rulers' step in image pixels: the first that leaves 64 screen pixels between numbers."
  (or-else (find (fn [it] (>= (* it computed.px) 64)) [1 2 5 10 20 50 100 200 500 1000 2000 5000]) 10000))

(defcomputed rulerTicksX "The horizontal ruler's numbered ticks, in image pixels."
  (map (fn [it]
         (+ (* (floor (/ (- state.doc.view.x (/ computed.canvasWidth 2 computed.px)) computed.rulerStep))
               computed.rulerStep)
            (* it computed.rulerStep)))
       (range (min 200 (+ (ceil (/ computed.canvasWidth (* computed.rulerStep computed.px))) 2)))))

(defcomputed rulerTicksY "The vertical ruler's numbered ticks, in image pixels."
  (map (fn [it]
         (+ (* (floor (/ (- state.doc.view.y (/ computed.canvasHeight 2 computed.px)) computed.rulerStep))
               computed.rulerStep)
            (* it computed.rulerStep)))
       (range (min 200 (+ (ceil (/ computed.canvasHeight (* computed.rulerStep computed.px))) 2)))))

(defcomputed rulerMinorX "The horizontal ruler's small ticks, five to a step."
  (map (fn [it] (+ (first computed.rulerTicksX) (/ (* it computed.rulerStep) 5)))
       (range (min 400 (* 5 (count computed.rulerTicksX))))))

(defcomputed rulerMinorY "The vertical ruler's small ticks, five to a step."
  (map (fn [it] (+ (first computed.rulerTicksY) (/ (* it computed.rulerStep) 5)))
       (range (min 400 (* 5 (count computed.rulerTicksY))))))

;;; The command search

(defcomputed searchItems "The command search's items: every command that can run, its category before its title."
  (map (fn [it] {:title (str (if (empty? (or-else it.category "")) "" (str it.category ": ")) it.title) :run it.name})
       (filter (fn [it] (and (not= it.enabled false) (not (empty? (or-else it.title ""))) (not= it.name "unavailable")))
               commands)))
