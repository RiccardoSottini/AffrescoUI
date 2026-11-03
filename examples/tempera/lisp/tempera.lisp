;;; tempera.lisp --- what Tempera does: the image's operations, the dialogs, the panels and the filters' preview
;;;
;;; The views (menubar.lisp, options.lisp, panels.lisp...) say what is shown; this file says what happens. An operation
;;; on the image is the image step, its name written out, `(image "moveLayer" :with args)`, so the revision rules judge
;;; exactly what it reaches; the library then puts the image's snapshot in state.doc. Handlers, keys, menus, scenarios
;;; and a feature's adapter run these functions by name, their arguments in `args`:
;;; {:run moveLayer :with {:args {...}}}. The commands of the menus are in commands.lisp, and call these.

;;; Reading what a dialog was given

(defn typed "A number typed in a dialog, kept between 1 and most; one that isn't a number is fallback."
  [text fallback most]
  (clamp (or-else (number text) fallback) 1 most))

(defn toggled "A list with an item taken out if it is there, else added at its end." [items item]
  (if (contains items item) (filter (fn [x] (not= x item)) items) (conj items item)))

;;; The image's operations, one step each

(defn setOptions "Changes the tool's options or the colours: size, hardness, opacity, foreground, a preset..." [args]
  (image "setOptions" :with args))

(defn setOption "Changes one of the tool's options: key is its name." [key value]
  (setOptions {key value}))

(defn select "Selects: a rectangle, an ellipse, a polygon, all, none, the inverse or a colour range." [args]
  (image "select" :with args))

(defn pointer "Plays a gesture of the pointer on the canvas, as the current tool takes it." [args]
  (image "pointer" :with args))

(defn crop "Crops the image to a box." [args]
  (image "crop" :with args))

(defn setProfile "Changes the working or the proofing colour profile." [args]
  (image "setProfile" :with args))

(defn fillSelection "Fills the selection with a colour, the foreground's if none is given." [args]
  (image "fillSelection" :with args))

(defn zoom "Zooms by a factor, or to a scale." [args]
  (image "zoom" :with args))

(defn feather "Softens the selection's edge by a radius." [args]
  (image "feather" :with args))

(defn setLayer "Changes a layer, the active one if no id is given: its name, visibility, blend, locks, adjustment..."
  [args]
  (image "setLayer" :with args))

(defn addLayer "Adds a layer above the active one: pixels, a group or an adjustment." [args]
  (image "addLayer" :with args))

(defn moveLayer "Moves a layer: by places, into a group, or to another layer's place (args.onto)." [args]
  (image "moveLayer" :with args))

(defn deleteLayer "Deletes a layer, the active one if no id is given." [args]
  (image "deleteLayer" :with args))

(defn duplicateLayer "Duplicates a layer, the active one if no id is given." [args]
  (image "duplicateLayer" :with args))

(defn selectLayer "Makes a layer the active one." [args]
  (image "selectLayer" :with args))

(defn addMask "Adds a mask to the active layer." [args]
  (image "addMask" :with args))

(defn deleteMask "Deletes the active layer's mask." [args]
  (image "deleteMask" :with args))

(defn goToHistory "Goes back, or forward, to a step of the history." [args]
  (image "history" :with args))

(defn newDocument "Makes a new, blank image." [args]
  (image "newImage" :with args))

(defn save "Saves the image, as a project or an export, where the person chooses." [args]
  (image "save" :with args))

(defn transformActive "Moves, scales and turns the active layer." [args]
  (image "transformLayer" :with args))

(defn setTool "Chooses a tool. The pencil is always hard: the brush's hardness comes back when it is left." [args]
  (let [leavingPencil (and (= state.doc.tool "pencil") (not= args.tool "pencil"))]
    (when (and (= args.tool "pencil") (not= state.doc.tool "pencil"))
      (set "state.brushHardness" state.doc.options.hardness))
    (image "setTool" :with args)
    (when leavingPencil (setOptions {:hardness state.brushHardness}))))

(defn perform "Runs a function or an action by its name, for the scenarios: event.run with event.args." []
  (run event.run :with {:args event.args}))

;;; The sample

(defn hillsLine "The hills' outline across the sample: 49 points along a wave, then the bottom corners." []
  (concat (map (fn [it]
                 {:x (/ (* 1200 it) 48)
                  :y (* 800 (+ 0.66 (* 0.07 (sin (/ (* 9 it) 48))) (* 0.04 (sin (/ (* 23 it) 48)))))})
               (range 49))
          [{:x 1200 :y 800} {:x 0 :y 800}]))

(defn drawSample "The sample, 1200 × 800: a dawn sky, a sun and hills, each a layer; then no history, and the brush."
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
  (image "select" :with {:kind "polygon" :points (hillsLine)})
  (image "fillSelection" :with {:color "#3b784f"})
  (image "select" :with {:kind "none"})
  (image "setOptions" :with {:gradientPreset "Foreground to Background"})
  (image "setTool" :with {:tool "brush"})
  (image "clearHistory"))

(defpart start "Tempera opens on the sample." :onLoad drawSample)

;;; Documents

(defn createDocument "Makes the document the New dialog describes." []
  (newDocument {:width (typed state.newDoc.width 1 20000)
                :height (typed state.newDoc.height 1 20000)
                :format state.newDoc.format})
  (set "state.newDoc.open" false))

(defn newPreset "Gives the New dialog a preset's size." [args]
  (set "state.newDoc.width" args.width)
  (set "state.newDoc.height" args.height))

(defn discardDocument "Leaves a blank document in place of this one: Tempera always shows one." []
  (newDocument {:width state.doc.width :height state.doc.height})
  (set "state.docName" (t "document.untitled"))
  (set "state.closeAsk" false))

(defn applyImageSize "Resizes the image as the Image Size dialog says; linked, the height follows the width." []
  (image "imageSize"
    :with {:width (clamp (or-else (number state.resize.width) state.doc.width) 1 30000)
           :height (if (truthy? state.resize.linked)
                     nil
                     (clamp (or-else (number state.resize.height) state.doc.height) 1 30000))})
  (set "state.resize.open" false))

(defn applyCanvasSize "Resizes the canvas as the Canvas Size dialog says, around its anchor." []
  (image "canvasSize"
    :with {:width (clamp (or-else (number state.canvasSize.width) state.doc.width) 1 30000)
           :height (clamp (or-else (number state.canvasSize.height) state.doc.height) 1 30000)
           :anchor state.canvasSize.anchor})
  (set "state.canvasSize.open" false))

;;; Layers

(defn applyRename "Renames the active layer as the dialog says, unless the name is empty." []
  (let [name (trim (or-else state.rename.name ""))]
    (when (not= name "") (setLayer {:id state.doc.active :name name})))
  (set "state.rename.open" false))

(defn applyTransform "Moves, scales and turns the active layer as the Free Transform dialog says." []
  (transformActive {:x (or-else (number state.transform.x) 0)
                    :y (or-else (number state.transform.y) 0)
                    :scaleX (or-else (number state.transform.scaleX) 100)
                    :scaleY (or-else (number state.transform.scaleY) 100)
                    :angle (or-else (number state.transform.angle) 0)})
  (set "state.transform.open" false))

(defn transformBox
  "The Properties panel's W, H, X and Y: the active layer moves and scales as the box (the selection, or the image)
would."
  [args]
  (let [[x y w h] computed.box]
    (transformActive {:x (- (or-else (:x args) x) x)
                      :y (- (or-else (:y args) y) y)
                      :scaleX (/ (* (or-else (:w args) w) 100) w)
                      :scaleY (/ (* (or-else (:h args) h) 100) h)})))

;;; Colours, adjustments and fills

(defn setGradient "Edits the gradient, and gives it to the tool." [args]
  (set "state.gradientEdit" args)
  (setOptions {:gradient args}))

(defn chooseBrush "Gives the brush a preset, and closes the presets." [preset]
  (setOptions {:preset preset})
  (set "state.brushMenu" false))

(defn chooseGradient "Gives the gradient tool a preset, and chooses the tool." [preset]
  (setOptions {:gradientPreset preset})
  (setTool {:tool "gradient"}))

(defn setFillContents "Chooses what the Fill dialog fills with." [args]
  (set "state.fill.contents" args.id))

(defn applyFill "Fills the selection with what the Fill dialog chose." []
  (fillSelection {:color (or-else (:color (find (fn [c] (= (:id c) state.fill.contents)) computed.fillContents))
                                  state.doc.foreground)}))

(defn setAdjustment "Changes some of the active adjustment layer's values." [args]
  (setLayer {:adjustment (merge computed.adjustment args)}))

(defn setBalance "Changes one channel of the active Colour Balance layer." [args]
  (let [balance (:balance computed.adjustment)]
    (setLayer {:adjustment (merge computed.adjustment
                                  {:balance (map (fn [c] (if (= c (:channel args)) (:value args) (get balance c)))
                                                 (range 3))})})))

(defn setCurve "The Curves layer's curve, as its editor drew it." [points]
  (setLayer {:adjustment (merge computed.adjustment {:curve points})}))

(defn measureTones "Levels and Curves show the image's tones; other layers don't need them measured." []
  (when (contains ["levels" "curves"] computed.adjustmentKind)
    (image "histogram" :with {:bins 64})))

;;; The filters: the dialog only edits state.filter, and these watches preview, apply or cancel it

(defn showFilter "Opens a filter's dialog. It only sets the state, so a revision's button may run it." [args]
  (set "state.filter.kind" args.kind)
  (set "state.filter.open" true))

(defn setFilterValue "Sets one of the open filter's values: args is {name value}." [args]
  (let [key (first (keys args))]
    (set (str "state.filter." key) (get args key))))

(defn typeFilterValue "A filter's value typed in its field: a number within its range, or else the value it had."
  [key low high]
  (setFilterValue {key (clamp (or-else (number (trim event.text)) (get state.filter key)) low high)}))

(defn previewFilterNow "Previews the filter on the canvas while its dialog is open and Preview is on." []
  (when (and (truthy? state.filter.open) (truthy? state.filter.live))
    (image "previewFilter" :with state.filter)))

(defn liveChanged "Preview turned on previews; turned off, the canvas goes back." []
  (if (truthy? state.filter.live) (previewFilterNow) (image "cancelFilter")))

(defn commitFilter "Applies the filter to the image, and closes its dialog." []
  (image "applyFilter" :with state.filter)
  (set "state.filter.open" false))

(defn discardFilter "Takes the preview back, and closes the dialog." []
  (image "cancelFilter")
  (set "state.filter.open" false))

(defpart previews "The filter dialog's values preview as they change; the tones are measured for Levels and Curves."
  :watch {:state.filter.open previewFilterNow
          :state.filter.kind previewFilterNow
          :state.filter.radius previewFilterNow
          :state.filter.amount previewFilterNow
          :state.filter.threshold previewFilterNow
          :state.filter.angle previewFilterNow
          :state.filter.distance previewFilterNow
          :state.filter.live liveChanged
          :state.filter.apply commitFilter
          :state.filter.cancel discardFilter
          :state.doc.active measureTones
          :state.doc.revision measureTones})

;;; The panels and the workspace

(defn showPanel "Shows a panel in its group: args.slot is the group, args.id the panel." [args]
  (set (str "state.panels." args.slot) args.id))

(defn showPopout "Pops the History or the Navigator panel out, or back in." [args]
  (set "state.popout" {:history (and (= (:id args) "history") (not state.popout.history))
                       :navigator (and (= (:id args) "navigator") (not state.popout.navigator))}))

(defn toggleLayerKind "Shows or hides one kind of layer in the Layers panel, and turns the filter on." [args]
  (set "state.layerFilter" {:on true :kinds (toggled state.layerFilter.kinds (:kind args))}))

(defn toggleLayerFilter "Turns the Layers panel's filter on or off." []
  (toggle "state.layerFilter.on"))

(defn foldGroup "Folds a group in the Layers panel, or unfolds it." [args]
  (set "state.foldedGroups" (toggled state.foldedGroups (:id args))))

(defn setStatusInfo "Chooses what the status bar shows." [args]
  (set "state.statusInfo" args.id))

(defn setScreenMode "Chooses the screen mode: standard, or full without the panels." [args]
  (set "state.screenMode" args.mode))

(defn addGuide "Adds a guide where a ruler was clicked: args.axis vertical or horizontal, args.at in image pixels."
  [args]
  (push (str "state.guides." args.axis) (round args.at)))

(defn guideAcross "A click on the top ruler adds an upright guide there." []
  (addGuide {:axis "vertical" :at (+ state.doc.view.x (/ (- event.left (/ event.width 2)) computed.px))}))

(defn guideDown "A click on the side ruler adds a level guide there." []
  (addGuide {:axis "horizontal" :at (+ state.doc.view.y (/ (- event.top (/ event.height 2)) computed.px))}))

(defn zoomTyped "The zoom typed in the status bar, in per cent." []
  (zoom {:to (/ (clamp (or-else (number (trim (first (split event.text "%")))) computed.zoomPercent) 1 3200) 100)}))

(defpart review "The scenarios accept an agent's proposal as the person's client would.
A review step runs from a step that the person's input or a scenario plays, not from a function: this one stays a step."
  :actions {:acceptAgentProposal [{:accept ~proposals.0.id :head ~proposals.0.head}]})

;;; What the controls run (controls.lisp, menu.lisp)

(defn runWith "Runs a function or an action by its name, its arguments in args: what a control was told to run."
  [action args]
  (when (and (some? action) (not= action ""))
    (run action :with {:args args})))

(defn toggleOpen "Opens what a path of the state says is open, or closes it: a menu, a section, a slider." [path]
  (toggle path))

(defn pressSlider "A slider pressed: the value under the pointer, kept for the drag, given to the slider's action."
  [action args key low high scale]
  (let [value (clamp (round (+ low (* (/ event.left (max 1 event.width)) (- high low)))) low high)]
    (set "state.slide" value)
    (runWith action (assoc args key (/ value scale)))))

(defn dragSlider "A slider dragged: the value it was pressed at, moved as far as the pointer."
  [action args key low high scale]
  (let [value (clamp (round (+ state.slide (* (/ event.dx (max 1 event.width)) (- high low)))) low high)]
    (runWith action (assoc args key (/ value scale)))))

(defn stepSlider "The arrow keys on a slider: one less, not under low, or one more, not over high."
  [action args key shown by low high scale]
  (runWith action (assoc args key (/ (if (< by 0) (max low (+ shown by)) (min high (+ shown by))) scale))))

(defn submitField "A number typed in an option's field, its unit left out, kept within its range; else it stays shown."
  [action args key shown low high scale]
  (let [typedValue (number (trim (first (split (first (split event.text "%")) " "))))]
    (runWith action (assoc args key (/ (clamp (or-else typedValue shown) low high) scale)))))

(defn chooseMenuItem "A menu's item chosen: a submenu opens; anything else closes the menu and runs the item's action."
  [item path close]
  (if (some? item.submenu)
    (toggle path)
    (do (if (empty? close) (dismiss true) (set close false))
        (when (some? item.run) (run item.run :with item.with)))))
