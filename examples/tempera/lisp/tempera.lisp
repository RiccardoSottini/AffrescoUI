;;; tempera.lisp --- what Tempera does: the image's operations, the dialogs, the panels and the filters' preview
;;;
;;; The views (menubar.lisp, options.lisp, panels.lisp...) say what is shown; this file says what happens. An operation
;;; on the image is the image step, its name written out, `(image "moveLayer" :with args)`, so the revision rules judge
;;; exactly what it reaches; the library then puts the image's snapshot in state.doc. Handlers, keys, menus, scenarios
;;; and a feature's adapter run these functions by name, their arguments in `args`:
;;; {:run moveLayer :with {:args {...}}}. The commands of the menus are in commands.lisp, and call these.

(ns tempera.tempera)

;;; Reading what a dialog was given

(defn typed "A number typed in a dialog, kept between 1 and most; one that isn't a number is fallback."
  [text fallback most]
  (clamp (or-else (number text) fallback) 1 most))

(defn toggled "A list with an item taken out if it is there, else added at its end." [items item]
  (if (some #{item} items) (remove #{item} items) (conj items item)))

;;; The image's operations, one step each

; style: keep, a string of the code names it, and a rename never changes a string
(defn setOptions "Changes the tool's options or the colours: size, hardness, opacity, foreground, a preset..." [args]
  (image "setOptions" :with args))

(defn set-option "Changes one of the tool's options: key is its name." [key value]
  (setOptions {key value}))

(defn select "Selects: a rectangle, an ellipse, a polygon, all, none, the inverse or a colour range." [args]
  (image "select" :with args))

(defn pointer "Plays a gesture of the pointer on the canvas, as the current tool takes it." [args]
  (image "pointer" :with args))

(defn crop "Crops the image to a box." [args]
  (image "crop" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setProfile "Changes the working or the proofing colour profile." [args]
  (image "setProfile" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn fillSelection "Fills the selection with a colour, the foreground's if none is given." [args]
  (image "fillSelection" :with args))

(defn zoom "Zooms by a factor, or to a scale." [args]
  (image "zoom" :with args))

(defn feather "Softens the selection's edge by a radius." [args]
  (image "feather" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setLayer "Changes a layer, the active one if no id is given: its name, visibility, blend, locks, adjustment..."
  [args]
  (image "setLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addLayer "Adds a layer above the active one: pixels, a group or an adjustment." [args]
  (image "addLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn moveLayer "Moves a layer: by places, into a group, or to another layer's place (args.onto)." [args]
  (image "moveLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn deleteLayer "Deletes a layer, the active one if no id is given." [args]
  (image "deleteLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn duplicateLayer "Duplicates a layer, the active one if no id is given." [args]
  (image "duplicateLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn selectLayer "Makes a layer the active one." [args]
  (image "selectLayer" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn addMask "Adds a mask to the active layer." [args]
  (image "addMask" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn deleteMask "Deletes the active layer's mask." [args]
  (image "deleteMask" :with args))

; style: keep, named in examples/tempera/lisp/index.scenarios.json
(defn goToHistory "Goes back, or forward, to a step of the history." [args]
  (image "history" :with args))

(defn new-document "Makes a new, blank image." [args]
  (image "newImage" :with args))

(defn save "Saves the image, as a project or an export, where the person chooses." [args]
  (image "save" :with args))

(defn transform-active "Moves, scales and turns the active layer." [args]
  (image "transformLayer" :with args))

; style: keep, a string of the scenarios names it, and a rename never changes a string
(defn transformSelectionBy "Moves, scales and turns the selection's outline, not its pixels." [args]
  (image "transformSelection" :with args))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setTool "Chooses a tool. The pencil is always hard: the brush's hardness comes back when it is left." [args]
  (let [leavingPencil (and (= state.doc.tool :pencil) (not= args.tool :pencil))]
    (when (and (= args.tool :pencil) (not= state.doc.tool :pencil))
      (set "state.brushHardness" state.doc.options.hardness))
    (image "setTool" :with args)
    (when leavingPencil (setOptions {:hardness state.brushHardness}))))

(defn perform "Runs a function or an action by its name, for the scenarios: event.run with event.args." []
  (run event.run :with {:args event.args}))

;;; The sample

(defn hills-line "The hills' outline across the sample: 49 points along a wave, then the bottom corners." []
  (concat (map (fn [it]
                 {:x (/ (* 1200 it) 48)
                  :y (* 800 (+ 0.66
                               (-> it (* 9) (/ 48) sin (* 0.07))
                               (-> it (* 23) (/ 48) sin (* 0.04))))})
               (range 49))
          [{:x 1200 :y 800} {:x 0 :y 800}]))

(defn draw-sample "The sample, 1200 × 800: a dawn sky, a sun and hills, each a layer; then no history, and the brush."
  []
  (image "newImage" :with {:width 1200 :height 800})
  (image "setLayer" :with {:id state.doc.active :name (t "sample.sky")})
  (image "setTool" :with {:tool "gradient"})
  (image "setOptions" :with {:shape "linear" :gradient {:stops [{:at 0 :color "#5485d1"} {:at 1 :color "#f7d6a8"}]}})
  (image "pointer"
    :with {:points [{:phase "press" :x 600 :y 0} {:phase "move" :x 600 :y 280} {:phase "release" :x 600 :y 560}]})
  (image "addLayer" :with {:kind "pixel" :name (t "sample.sun")})
  (image "select" :with {:kind "ellipse" :from {:x 792 :y 128} :to {:x 952 :y 288}})
  (image "fillSelection" :with {:color "#ffdb5c"})
  (image "addLayer" :with {:kind "pixel" :name (t "sample.hills")})
  (image "select" :with {:kind "polygon" :points (hills-line)})
  (image "fillSelection" :with {:color "#3b784f"})
  (image "select" :with {:kind "none"})
  (image "setOptions" :with {:gradientPreset "Foreground to Background"})
  (image "setTool" :with {:tool "brush"})
  (image "clearHistory"))

(defpart start "Tempera opens on the sample." :onLoad draw-sample)

;;; Documents

; style: keep, a string of the code names it, and a rename never changes a string
(defn createDocument "Makes the document the New dialog describes." []
  (new-document {:width (typed state.newDoc.width 1 20000)
                :height (typed state.newDoc.height 1 20000)
                :format state.newDoc.format})
  (set "state.newDoc.open" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn newPreset "Gives the New dialog a preset's size." [args]
  (set "state.newDoc.width" args.width)
  (set "state.newDoc.height" args.height))

; style: keep, a string of the code names it, and a rename never changes a string
(defn discardDocument "Leaves a blank document in place of this one: Tempera always shows one." []
  (new-document {:width state.doc.width :height state.doc.height})
  (set "state.docName" (t "document.untitled"))
  (set "state.closeAsk" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn applyImageSize "Resizes the image as the Image Size dialog says; linked, the height follows the width." []
  (image "imageSize"
    :with {:width (clamp (or-else (number state.resize.width) state.doc.width) 1 30000)
           :height (if (truthy? state.resize.linked)
                     nil
                     (clamp (or-else (number state.resize.height) state.doc.height) 1 30000))})
  (set "state.resize.open" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn applyCanvasSize "Resizes the canvas as the Canvas Size dialog says, around its anchor." []
  (image "canvasSize"
    :with {:width (clamp (or-else (number state.canvasSize.width) state.doc.width) 1 30000)
           :height (clamp (or-else (number state.canvasSize.height) state.doc.height) 1 30000)
           :anchor state.canvasSize.anchor})
  (set "state.canvasSize.open" false))

;;; Layers

; style: keep, a string of the code names it, and a rename never changes a string
(defn applyRename "Renames the active layer as the dialog says, unless the name is empty." []
  (let [name (str/trim (or-else state.rename.name ""))]
    (when-not (str/blank? name) (setLayer {:id state.doc.active :name name})))
  (set "state.rename.open" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn applyTransform "Moves, scales and turns the active layer, or the selection, as the transform dialog says." []
  (let [args {:x (or-else (number state.transform.x) 0)
              :y (or-else (number state.transform.y) 0)
              :scaleX (or-else (number state.transform.scaleX) 100)
              :scaleY (or-else (number state.transform.scaleY) 100)
              :angle (or-else (number state.transform.angle) 0)}]
    (if (= state.transform.target :selection)
      (transformSelectionBy args)
      (transform-active args)))
  (set "state.transform.open" false))

(defn transform-box
  "The Properties panel's W, H, X and Y: the active layer moves and scales as the box (the selection, or the image)
would."
  [args]
  (let [[x y w h] computed.box]
    (transform-active {:x (- (or-else (:x args) x) x)
                      :y (- (or-else (:y args) y) y)
                      :scaleX (-> (:w args) (or-else w) (* 100) (/ w))
                      :scaleY (-> (:h args) (or-else h) (* 100) (/ h))})))

;;; Colours, adjustments and fills

(defn set-gradient "Edits the gradient, and gives it to the tool." [args]
  (set "state.gradientEdit" args)
  (setOptions {:gradient args}))

(defn choose-brush "Gives the brush a preset, and closes the presets." [preset]
  (setOptions {:preset preset})
  (set "state.brushMenu" false))

(defn choose-gradient "Gives the gradient tool a preset, and chooses the tool." [preset]
  (setOptions {:gradientPreset preset})
  (setTool {:tool "gradient"}))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setFillContents "Chooses what the Fill dialog fills with." [args]
  (set "state.fill.contents" args.id))

; style: keep, a string of the code names it, and a rename never changes a string
(defn applyFill "Fills the selection with what the Fill dialog chose." []
  (let [chosen (find #(= (:id %) state.fill.contents) computed.fillContents)]
    (fillSelection {:color (or-else (:color chosen) state.doc.foreground)})))

; style: keep, named in examples/tempera/lisp/index.scenarios.json
(defn setAdjustment "Changes some of the active adjustment layer's values." [args]
  (setLayer {:adjustment (merge computed.adjustment args)}))

(defn set-balance "Changes one channel of the active Colour Balance layer." [args]
  (let [{:keys [channel value]} args
        balance (:balance computed.adjustment)
        changed (map #(if (= % channel) value (get balance %)) (range 3))]
    (setLayer {:adjustment (assoc computed.adjustment :balance changed)})))

(defn set-curve "The Curves layer's curve, as its editor drew it." [points]
  (setLayer {:adjustment (assoc computed.adjustment :curve points)}))

(defn measure-tones "Levels and Curves show the image's tones; other layers don't need them measured." []
  (when (#{:levels :curves} computed.adjustmentKind)
    (image "histogram" :with {:bins 64})))

;;; The filters: the dialog only edits state.filter, and these watches preview, apply or cancel it

; style: keep, named in examples/tempera/lisp/index.scenarios.json
(defn showFilter "Opens a filter's dialog. It only sets the state, so a revision's button may run it." [args]
  (set "state.filter.kind" args.kind)
  (set "state.filter.open" true))

(defn set-filter-value "Sets one of the open filter's values: args is {name value}." [args]
  (let [key (first (keys args))]
    (set (str "state.filter." key) (get args key))))

(defn type-filter-value "A filter's value typed in its field: a number within its range, or else the value it had."
  [key low high]
  (set-filter-value {key (-> event.text str/trim number (or-else (get state.filter key)) (clamp low high))}))

(defn preview-filter-now "Previews the filter on the canvas while its dialog is open and Preview is on." []
  (when (and (truthy? state.filter.open) (truthy? state.filter.live))
    (image "previewFilter" :with state.filter)))

(defn live-changed "Preview turned on previews; turned off, the canvas goes back." []
  (if (truthy? state.filter.live) (preview-filter-now) (image "cancelFilter")))

; style: keep, a string of the code names it, and a rename never changes a string
(defn commitFilter "Applies the filter to the image, and closes its dialog." []
  (image "applyFilter" :with state.filter)
  (set "state.filter.open" false))

; style: keep, a string of the code names it, and a rename never changes a string
(defn discardFilter "Takes the preview back, and closes the dialog." []
  (image "cancelFilter")
  (set "state.filter.open" false))

(defpart previews "The filter dialog's values preview as they change; the tones are measured for Levels and Curves."
  :watch {:state.filter.open preview-filter-now
          :state.filter.kind preview-filter-now
          :state.filter.radius preview-filter-now
          :state.filter.amount preview-filter-now
          :state.filter.threshold preview-filter-now
          :state.filter.angle preview-filter-now
          :state.filter.distance preview-filter-now
          :state.filter.live live-changed
          :state.filter.apply commitFilter
          :state.filter.cancel discardFilter
          :state.doc.active measure-tones
          :state.doc.revision measure-tones})

;;; The panels and the workspace

(defn show-panel "Shows a panel in its group, unfolding it: args.slot is the group, args.id the panel." [args]
  (set (str "state.panels." args.slot) args.id)
  (set (str "state.collapsed." args.slot) false))

(defn fold-panel-group "Folds a group of the dock to its tabs, or unfolds it." [slot]
  (toggle (str "state.collapsed." slot)))

(defn press-group-sash "A group's sash pressed: its height, kept for the drag." [slot]
  (set "state.slide" (if (= slot :colour) state.groupHeights.colour state.groupHeights.properties)))

(defn drag-group-sash "A group's sash dragged: the group as much taller, or shorter, as the pointer moved." [slot]
  (set (str "state.groupHeights." slot) (-> state.slide (+ event.dy) round (clamp 96 520))))

(defn layout-now "The dock as it is now: what a workspace keeps." []
  {:panels state.panels :collapsed state.collapsed :groupHeights state.groupHeights :popout state.popout})

(defn use-layout "Puts the dock as a workspace keeps it." [layout]
  (set "state.panels" layout.panels)
  (set "state.collapsed" layout.collapsed)
  (set "state.groupHeights" layout.groupHeights)
  (set "state.popout" layout.popout)
  (set "state.screenMode" :standard))

; style: keep, a string of the code names it, and a rename never changes a string
(defn chooseWorkspace "Chooses a workspace: standard, painting (the colours tall, the properties folded), or one saved."
  [args]
  (use-layout (case args.name
                "standard" standard-layout
                "painting" painting-layout
                (saved-layout args.name)))
  (set "state.workspace" args.name)
  (set "state.workspaceMenu" false))

(defn saved-layout "The dock as the workspace saved under a name keeps it." [name]
  (let [saved (find #(= (:name %) name) state.workspaces)]
    (:layout saved)))

(defn save-workspace "Saves the dock as it is as a new workspace, Workspace 1, 2..., and chooses it." []
  (let [name (t "workspace.saved" {:n (inc (count state.workspaces))})]
    (push "state.workspaces" {:name name :layout (layout-now)})
    (set "state.workspace" name)))

(defn show-popout "Pops the History or the Navigator panel out, or back in." [args]
  (set "state.popout" {:history (and (= (:id args) "history") (not state.popout.history))
                       :navigator (and (= (:id args) "navigator") (not state.popout.navigator))}))

(defn toggle-layer-kind "Shows or hides one kind of layer in the Layers panel, and turns the filter on." [args]
  (set "state.layerFilter" {:on true :kinds (toggled state.layerFilter.kinds (:kind args))}))

(defn toggle-layer-filter "Turns the Layers panel's filter on or off." []
  (toggle "state.layerFilter.on"))

(def standard-layout "The dock as it first is."
  {:panels {:colour "colour" :properties "properties" :layers "layers"}
   :collapsed {:colour false :properties false :layers false}
   :groupHeights {:colour 158 :properties 236}
   :popout {:history false :navigator false}})

(def painting-layout "The painting workspace's dock: the palette tall, the properties folded."
  (assoc standard-layout
         :panels {:colour "swatches" :properties "properties" :layers "layers"}
         :collapsed {:colour false :properties true :layers false}
         :groupHeights {:colour 260 :properties 236}))

(defn fold-group "Folds a group in the Layers panel, or unfolds it." [args]
  (set "state.foldedGroups" (toggled state.foldedGroups (:id args))))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setStatusInfo "Chooses what the status bar shows." [args]
  (set "state.statusInfo" args.id))

; style: keep, a string of the code names it, and a rename never changes a string
(defn setScreenMode "Chooses the screen mode: standard, or full without the panels." [args]
  (set "state.screenMode" args.mode))

; style: keep, named in examples/tempera/lisp/index.scenarios.json
(defn addGuide "Adds a guide where a ruler was clicked: args.axis vertical or horizontal, args.at in image pixels."
  [args]
  (push (str "state.guides." args.axis) (round args.at)))

(defn guide-across "A click on the top ruler adds an upright guide there, unless the press dragged a guide out." []
  (when-not state.guideDrag.moved
    (addGuide {:axis "vertical" :at (-> event.left (- (/ event.width 2)) (/ computed.px) (+ state.doc.view.x))})))

(defn guide-down "A click on the side ruler adds a level guide there, unless the press dragged a guide out." []
  (when-not state.guideDrag.moved
    (addGuide {:axis "horizontal" :at (-> event.top (- (/ event.height 2)) (/ computed.px) (+ state.doc.view.y))})))

(defn guide-pressed "A ruler or a guide pressed: nothing dragged yet, and where the guide pressed is." [at]
  (set "state.guideDrag" {:axis "" :at at :moved false}))

(defn guide-out
  "A ruler dragged: past its edge, over the image, a guide follows the pointer (level out of the top ruler, upright out
  of the side one); back on the ruler, there is none."
  [axis]
  (let [level (= axis :horizontal)
        inside (if level (- event.top event.height) (- event.left event.width))]
    (set "state.guideDrag"
         {:axis (if (pos? inside) axis "")
          :at (if level
                (-> inside (- (/ computed.canvasHeight 2)) (/ computed.px) (+ state.doc.view.y))
                (-> inside (- (/ computed.canvasWidth 2)) (/ computed.px) (+ state.doc.view.x)))
          :moved true})))

(defn guide-dropped "A ruler let go: the guide dragged out of it stays where it was let go." []
  (when-not (empty? state.guideDrag.axis)
    (addGuide {:axis state.guideDrag.axis :at state.guideDrag.at}))
  (set "state.guideDrag.axis" ""))

(defn guide-moved "A guide dragged: it follows the pointer, in image pixels." [axis index]
  (let [moved (if (= axis :vertical) event.dx event.dy)]
    (set (str "state.guides." axis "." index) (-> moved (/ computed.px) (+ state.guideDrag.at) round))))

(defn guide-let-go "A guide let go: off the image, onto its ruler, it goes." [axis index]
  (let [upright (= axis :vertical)
        place (if upright (nth state.guides.vertical index) (nth state.guides.horizontal index))
        on-screen (if upright
                    (+ (/ computed.canvasWidth 2) (* (- place state.doc.view.x) computed.px))
                    (+ (/ computed.canvasHeight 2) (* (- place state.doc.view.y) computed.px)))]
    (when (neg? on-screen)
      (let [kept (keep-indexed #(when (not= %1 index) %2)
                               (if upright state.guides.vertical state.guides.horizontal))]
        (set (str "state.guides." axis) kept)))))

(defn zoom-typed "The zoom typed in the status bar, in per cent." []
  (zoom {:to (-> event.text (str/split "%") first str/trim number (or-else computed.zoomPercent) (clamp 1 3200) (/ 100))}))

(defpart review "The scenarios accept an agent's proposal as the person's client would.
A review step runs from a step that the person's input or a scenario plays, not from a function: this one stays a step."
  :actions {:acceptAgentProposal [{:accept ~proposals.0.id :head ~proposals.0.head}]})

;;; What the controls run (controls.lisp, menu.lisp)

(defn run-with "Runs a function or an action by its name, its arguments in args: what a control was told to run."
  [action args]
  (when-not (empty? action)
    (run action :with {:args args})))

(defn toggle-open "Opens what a path of the state says is open, or closes it: a menu, a section, a slider." [path]
  (toggle path))

(defn press-slider "A slider pressed: the value under the pointer, kept for the drag, given to the slider's action."
  [action args key low high scale]
  (let [value (-> event.left (/ (max 1 event.width)) (* (- high low)) (+ low) round (clamp low high))]
    (set "state.slide" value)
    (run-with action (assoc args key (/ value scale)))))

(defn drag-slider "A slider dragged: the value it was pressed at, moved as far as the pointer."
  [action args key low high scale]
  (let [value (-> event.dx (/ (max 1 event.width)) (* (- high low)) (+ state.slide) round (clamp low high))]
    (run-with action (assoc args key (/ value scale)))))

(defn step-slider "The arrow keys on a slider: one less, not under low, or one more, not over high."
  [action args key shown by low high scale]
  (let [moved (+ shown by)
        value (if (neg? by) (max low moved) (min high moved))]
    (run-with action (assoc args key (/ value scale)))))

(defn submit-field "A number typed in an option's field, its unit left out, kept within its range; else it stays shown."
  [action args key shown low high scale]
  (let [typedValue (-> event.text (str/split "%") first (str/split " ") first str/trim number)]
    (run-with action
             (assoc args key (-> typedValue (or-else shown) (clamp low high) (/ scale))))))

(defn choose-menu-item "A menu's item chosen: a submenu opens; anything else closes the menu and runs the item's action."
  [item path close]
  (if (some? item.submenu)
    (toggle path)
    (do (if (empty? close) (dismiss true) (set close false))
        (when-some [action item.run] (run action :with item.with)))))
