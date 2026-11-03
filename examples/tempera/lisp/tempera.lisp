;;; tempera.lisp --- what Tempera does beyond one operation at a time: dialogs, groups, adjustments and the panels' lists
;;;
;;; The parts (parts/*.lisp) say what is shown, and keep the actions that are one operation of the image step
;;; (`addLayer`, `moveLayer`...), with the sample and the tools. Those stay data on purpose: what an agent's revision may
;;; wire a button to is the same as before, and the slide sorter an agent installs runs the same actions. This file holds
;;; the rest, and reaches the image step only through those actions, by a name `op` is given, which the revision rules
;;; count as reaching anything (VERSION_CONTROL.md, "Programs"). A handler runs a function by name, as it would an action, its arguments in `args`:
;;; {:run setBalance :with {:args {:channel 0 :value 0.2}}}.

;;; What the dialogs and panels share

(defn op "Runs an action that is one operation of the image step (parts/commands.lisp), with its arguments." [action args]
  (run action :with {:args args}))

(defn either "A value, or another when it is nil: an expression's ??." [v otherwise]
  (if (nil? v) otherwise v))

(defn typed "A number typed in a dialog, kept between 1 and most; one that isn't a number is otherwise." [text otherwise most]
  (clamp (either (number text) otherwise) 1 most))

(defn toggled "A list with an item taken out if it is there, else added at its end." [items item]
  (if (contains items item) (filter (fn [x] (not= x item)) items) (conj items item)))

;;; Documents

(defn createDocument "Makes the document the New dialog describes." []
  (op "newDocument" {:width (typed state.newDoc.width 1 20000)
                     :height (typed state.newDoc.height 1 20000)
                     :format state.newDoc.format})
  (set "state.newDoc.open" false))

(defn discardDocument "Leaves a blank document in place of this one: Tempera always shows one." []
  (op "newDocument" {:width state.doc.width :height state.doc.height})
  (set "state.docName" "Untitled")
  (set "state.closeAsk" false))

(defcommand closeDocument "Closes the document, asking first if it has changes." :title "Close" :category "File" :icon x []
  (if state.doc.canUndo (set "state.closeAsk" true) (discardDocument)))

(defcommand cropToRatio "Crops the image to the largest centred box of a width-to-height ratio." :title "Crop to a Ratio"
  :category "Image" :icon crop :enablement "{{state.doc.width > 0}}"
  [args]
  (let [w state.doc.width
        h state.doc.height
        size (if (> (* w (:height args)) (* h (:width args)))
               [(max 1 (floor (/ (* h (:width args)) (:height args)))) h]
               [w (max 1 (floor (/ (* w (:height args)) (:width args))))])]
    (set "state.cropSize" size)
    (op "crop" {:x (floor (/ (- w (first size)) 2))
                :y (floor (/ (- h (second size)) 2))
                :width (first size)
                :height (second size)})))

;;; Layers

(defcommand groupLayers "Puts the active layer in a new group." :title "Group Layers" :category "Layer" :icon folder-input
  :enablement "{{!empty(state.doc.active)}}"
  []
  (set "state.grouping" state.doc.active)
  (op "addLayer" {:kind "group" :name computed.nextGroupName})
  (op "moveLayer" {:id state.grouping :into state.doc.active}))

(defcommand ungroupLayers "Moves a group's layers out of it, in their order, and deletes it." :title "Ungroup Layers"
  :category "Layer" :icon folder-output :enablement "{{computed.activeLayer.kind == 'group'}}"
  []
  (set "state.grouping" state.doc.active)
  (map (fn [child] (op "moveLayer" {:id (:id child) :onto state.grouping})) (reverse computed.groupChildren))
  (op "deleteLayer" {:id state.grouping}))

(defn applyRename "Renames the active layer as the dialog says, unless the name is empty." []
  (let [name (trim (either state.rename.name ""))]
    (when (not= name "") (op "setLayer" {:id state.doc.active :name name})))
  (set "state.rename.open" false))

(defn applyTransform "Moves, scales and turns the active layer as the Free Transform dialog says." []
  (op "transformActive" {:x (either (number state.transform.x) 0)
                         :y (either (number state.transform.y) 0)
                         :scaleX (either (number state.transform.scaleX) 100)
                         :scaleY (either (number state.transform.scaleY) 100)
                         :angle (either (number state.transform.angle) 0)})
  (set "state.transform.open" false))

(defn transformBox "The Properties panel's W, H, X and Y: the active layer moves and scales as the box (the selection, or the image) would."
  [args]
  (let [box computed.box]
    (op "transformActive" {:x (- (either (:x args) (get box 0)) (get box 0))
                           :y (- (either (:y args) (get box 1)) (get box 1))
                           :scaleX (/ (* (either (:w args) (get box 2)) 100) (get box 2))
                           :scaleY (/ (* (either (:h args) (get box 3)) 100) (get box 3))})))

;;; Colours, adjustments and filters

(defn setGradient "Edits the gradient, and gives it to the tool." [args]
  (set "state.gradientEdit" args)
  (op "setOptions" {:gradient args}))

(defn applyFill "Fills the selection with what the Fill dialog chose." []
  (op "fillSelection" {:color (either (:color (find (fn [c] (= (:id c) state.fill.contents)) computed.fillContents))
                                      state.doc.foreground)}))

(defn setAdjustment "Changes some of the active adjustment layer's values." [args]
  (op "setLayer" {:adjustment (merge computed.adjustment args)}))

(defn setBalance "Changes one channel of the active Colour Balance layer." [args]
  (let [balance (:balance computed.adjustment)]
    (op "setLayer" {:adjustment (merge computed.adjustment
                                       {:balance (map (fn [c] (if (= c (:channel args)) (:value args) (get balance c)))
                                                      (range 3))})})))

(defn setFilterValue "Sets one of the open filter's values: args is {name value}." [args]
  (let [key (first (keys args))]
    (set (str "state.filter." key) (get args key))))

;;; The panels

(defn toggleLayerKind "Shows or hides one kind of layer in the Layers panel, and turns the filter on." [args]
  (set "state.layerFilter" {:on true :kinds (toggled state.layerFilter.kinds (:kind args))}))

(defn foldGroup "Folds a group in the Layers panel, or unfolds it." [args]
  (set "state.foldedGroups" (toggled state.foldedGroups (:id args))))

(defn showPopout "Pops the History or the Navigator panel out, or back in." [args]
  (set "state.popout" {:history (and (= (:id args) "history") (not state.popout.history))
                       :navigator (and (= (:id args) "navigator") (not state.popout.navigator))}))
