;;; commands.lisp --- the commands people choose from Tempera's menus, keys, bars and the command search
;;;
;;; Each defcommand has a title, a category, a description and an icon for the menus and the search, and some are
;;; enabled only when they can run. What they do is mostly one call of tempera.lisp's functions; the keys are in
;;; keys.lisp, the menus in menus.lisp.

;;; File

(defcommand showNewDialog "Opens the New dialog."
  :title ~(t "command.showNewDialog") :description ~(t "command.showNewDialog.about") :category ~(t "category.file")
  :icon "file-plus"
  []
  (set "state.newDoc.open" true))

(defcommand newSample "Draws the sample again, as a new document."
  :title ~(t "command.newSample") :description ~(t "command.newSample.about") :category ~(t "category.file")
  :icon "sparkles"
  []
  (drawSample))

(defcommand openFile "Opens an image or a Tempera project."
  :title ~(t "command.openFile") :description ~(t "command.openFile.about") :category ~(t "category.file")
  :icon "folder-open"
  []
  (image "open" :with {:projectExtension "tempera"}))

(defcommand closeDocument "Closes the document, asking first if it has changes."
  :title ~(t "command.closeDocument") :description ~(t "command.closeDocument.about") :category ~(t "category.file")
  :icon "x"
  []
  (if state.doc.canUndo (set "state.closeAsk" true) (discardDocument)))

(defcommand saveProject "Saves the document as a Tempera project, its layers kept."
  :title ~(t "command.saveProject") :description ~(t "command.saveProject.about") :category ~(t "category.file")
  :icon "save"
  []
  (save {:format "project" :extension "tempera"}))

(defcommand saveProjectAs "Saves the document as a Tempera project, somewhere else."
  :title ~(t "command.saveProjectAs") :description ~(t "command.saveProjectAs.about") :category ~(t "category.file")
  :icon "save-all"
  []
  (save {:format "project" :extension "tempera"}))

(defcommand revert "Goes back to the document as it was last opened or saved."
  :title ~(t "command.revert") :description ~(t "command.revert.about") :category ~(t "category.file")
  :icon "rotate-ccw"
  []
  (image "revert"))

(defcommand exportPng "Exports the image, flattened, as a PNG."
  :title ~(t "command.exportPng") :description ~(t "command.exportPng.about") :category ~(t "category.file")
  :icon "image"
  []
  (save {:format "png"}))

(defcommand exportJpeg "Exports the image, flattened, as a JPEG."
  :title ~(t "command.exportJpeg") :description ~(t "command.exportJpeg.about") :category ~(t "category.file")
  :icon "image"
  []
  (save {:format "jpeg"}))

;;; Edit: history and the clipboard

(defcommand undo "Takes the last change back."
  :title ~(t "command.undo") :description ~(t "command.undo.about") :category ~(t "category.edit")
  :icon "undo-2" :enablement ~state.doc.canUndo
  []
  (image "undo"))

(defcommand redo "Makes the change undone again."
  :title ~(t "command.redo") :description ~(t "command.redo.about") :category ~(t "category.edit")
  :icon "redo-2" :enablement ~state.doc.canRedo
  []
  (image "redo"))

(defcommand cutPixels "Cuts the selected pixels of the active layer."
  :title ~(t "command.cutPixels") :description ~(t "command.cutPixels.about") :category ~(t "category.edit")
  :icon "scissors" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (image "cut"))

(defcommand copyPixels "Copies the selected pixels of the active layer."
  :title ~(t "command.copyPixels") :description ~(t "command.copyPixels.about") :category ~(t "category.edit")
  :icon "copy" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (image "copy"))

(defcommand copyMerged "Copies the selected pixels of every visible layer, as one."
  :title ~(t "command.copyMerged") :description ~(t "command.copyMerged.about") :category ~(t "category.edit")
  :icon "layers-2"
  []
  (image "copy" :with {:merged true}))

(defcommand pastePixels "Pastes what was copied, as a new layer."
  :title ~(t "command.pastePixels") :description ~(t "command.pastePixels.about") :category ~(t "category.edit")
  :icon "clipboard-paste" :enablement ~state.doc.clipboard
  []
  (image "paste"))

(defcommand clearPixels "Clears the selected pixels of the active layer."
  :title ~(t "command.clearPixels") :description ~(t "command.clearPixels.about") :category ~(t "category.edit")
  :icon "delete" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (image "clear"))

;;; Edit: fills and colours

(defcommand showFill "Opens the Fill dialog."
  :title ~(t "command.showFill") :description ~(t "command.showFill.about") :category ~(t "category.edit")
  :icon "paint-bucket"
  []
  (set "state.fill.open" true))

(defcommand fillForeground "Fills the selection with the foreground colour."
  :title ~(t "command.fillForeground") :description ~(t "command.fillForeground.about") :category ~(t "category.edit")
  :icon "paint-bucket"
  []
  (fillSelection {}))

(defcommand fillBackground "Fills the selection with the background colour."
  :title ~(t "command.fillBackground") :description ~(t "command.fillBackground.about") :category ~(t "category.edit")
  :icon "paint-bucket"
  []
  (fillSelection {:color state.doc.background}))

(defcommand swapColours "Swaps the foreground and background colours."
  :title ~(t "command.swapColours") :description ~(t "command.swapColours.about") :category ~(t "category.edit")
  :icon "arrow-left-right"
  []
  (setOptions {:swap true}))

(defcommand resetColours "Makes the foreground black and the background white."
  :title ~(t "command.resetColours") :description ~(t "command.resetColours.about") :category ~(t "category.edit")
  :icon "rotate-ccw"
  []
  (setOptions {:reset true}))

(defcommand brushSmaller "Makes the brush a step smaller."
  :title ~(t "command.brushSmaller") :description ~(t "command.brushSmaller.about") :category ~(t "category.edit")
  :icon "minus"
  []
  (setOptions {:sizeStep -1}))

(defcommand brushLarger "Makes the brush a step larger."
  :title ~(t "command.brushLarger") :description ~(t "command.brushLarger.about") :category ~(t "category.edit")
  :icon "plus"
  []
  (setOptions {:sizeStep 1}))

;;; Edit: the active layer's transform

(defcommand showTransform "Opens the Free Transform dialog on the active layer."
  :title ~(t "command.showTransform") :description ~(t "command.showTransform.about") :category ~(t "category.edit")
  :icon "scaling" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (set "state.transform" {:open true :x 0 :y 0 :scaleX 100 :scaleY 100 :angle 0}))

(defcommand rotateLayer180 "Turns the active layer half a turn."
  :title ~(t "command.rotateLayer180") :description ~(t "command.rotateLayer180.about") :category ~(t "category.edit")
  :icon "rotate-cw"
  []
  (transformActive {:angle 180}))

(defcommand rotateLayerRight "Turns the active layer a quarter turn clockwise."
  :title ~(t "command.rotateLayerRight") :description ~(t "command.rotateLayerRight.about")
  :category ~(t "category.edit") :icon "rotate-cw"
  []
  (transformActive {:angle 90}))

(defcommand rotateLayerLeft "Turns the active layer a quarter turn anticlockwise."
  :title ~(t "command.rotateLayerLeft") :description ~(t "command.rotateLayerLeft.about")
  :category ~(t "category.edit") :icon "rotate-ccw"
  []
  (transformActive {:angle -90}))

(defcommand flipLayerHorizontal "Flips the active layer left to right."
  :title ~(t "command.flipLayerHorizontal") :description ~(t "command.flipLayerHorizontal.about")
  :category ~(t "category.edit") :icon "flip-horizontal"
  []
  (transformActive {:scaleX -100}))

(defcommand flipLayerVertical "Flips the active layer upside down."
  :title ~(t "command.flipLayerVertical") :description ~(t "command.flipLayerVertical.about")
  :category ~(t "category.edit") :icon "flip-vertical"
  []
  (transformActive {:scaleY -100}))

;;; Image

(defcommand showImageSize "Opens the Image Size dialog."
  :title ~(t "command.showImageSize") :description ~(t "command.showImageSize.about") :category ~(t "category.image")
  :icon "scaling"
  []
  (set "state.resize" {:open true :width state.doc.width :height state.doc.height :linked true}))

(defcommand showCanvasSize "Opens the Canvas Size dialog."
  :title ~(t "command.showCanvasSize") :description ~(t "command.showCanvasSize.about") :category ~(t "category.image")
  :icon "frame"
  []
  (set "state.canvasSize" {:open true :width state.doc.width :height state.doc.height :anchor "center"}))

(defcommand cropToSelection "Crops the image to the selection's box."
  :title ~(t "command.cropToSelection") :description ~(t "command.cropToSelection.about")
  :category ~(t "category.image") :icon "crop" :enablement ~state.doc.selection
  []
  (let [[x y w h] state.doc.bounds]
    (crop {:x x :y y :width w :height h})))

(defcommand cropToRatio "Crops the image to the largest centred box of a width-to-height ratio."
  :title ~(t "command.cropToRatio") :description ~(t "command.cropToRatio.about") :category ~(t "category.image")
  :icon "crop" :enablement ~(> state.doc.width 0)
  [args]
  (let [w state.doc.width
        h state.doc.height
        size (if (> (* w (:height args)) (* h (:width args)))
               [(max 1 (floor (/ (* h (:width args)) (:height args)))) h]
               [w (max 1 (floor (/ (* w (:height args)) (:width args))))])]
    (set "state.cropSize" size)
    (crop {:x (floor (/ (- w (first size)) 2))
           :y (floor (/ (- h (second size)) 2))
           :width (first size)
           :height (second size)})))

(defcommand rotateCanvas180 "Turns the whole image half a turn."
  :title ~(t "command.rotateCanvas180") :description ~(t "command.rotateCanvas180.about")
  :category ~(t "category.image") :icon "rotate-cw"
  []
  (image "rotateCanvas" :with {:angle 180}))

(defcommand rotateCanvasRight "Turns the whole image a quarter turn clockwise."
  :title ~(t "command.rotateCanvasRight") :description ~(t "command.rotateCanvasRight.about")
  :category ~(t "category.image") :icon "rotate-cw-square"
  []
  (image "rotateCanvas" :with {:angle 90}))

(defcommand rotateCanvasLeft "Turns the whole image a quarter turn anticlockwise."
  :title ~(t "command.rotateCanvasLeft") :description ~(t "command.rotateCanvasLeft.about")
  :category ~(t "category.image") :icon "rotate-ccw-square"
  []
  (image "rotateCanvas" :with {:angle 270}))

(defcommand flipCanvasHorizontal "Flips the whole image left to right."
  :title ~(t "command.flipCanvasHorizontal") :description ~(t "command.flipCanvasHorizontal.about")
  :category ~(t "category.image") :icon "flip-horizontal-2"
  []
  (image "flipCanvas" :with {:direction "horizontal"}))

(defcommand flipCanvasVertical "Flips the whole image upside down."
  :title ~(t "command.flipCanvasVertical") :description ~(t "command.flipCanvasVertical.about")
  :category ~(t "category.image") :icon "flip-vertical-2"
  []
  (image "flipCanvas" :with {:direction "vertical"}))

(defcommand toggleProof "Shows the colours as they would print in grey, or as they are."
  :title ~(t "command.toggleProof") :description ~(t "command.toggleProof.about") :category ~(t "category.image")
  :icon "eye"
  []
  (setProfile {:proof (if (empty? state.doc.proof) "gray" "")}))

;;; Image: adjustment layers

(defcommand levelsLayer "Adds a Levels layer: the image's black, white and middle tones."
  :title ~(t "command.levelsLayer") :description ~(t "command.levelsLayer.about") :category ~(t "category.image")
  :icon "sliders-horizontal"
  []
  (addLayer {:kind "levels" :name (t "adjustment.levels")}))

(defcommand curvesLayer "Adds a Curves layer: the image's tones along a curve."
  :title ~(t "command.curvesLayer") :description ~(t "command.curvesLayer.about") :category ~(t "category.image")
  :icon "spline"
  []
  (addLayer {:kind "curves" :name (t "adjustment.curves")}))

(defcommand hueSaturationLayer "Adds a Hue/Saturation layer."
  :title ~(t "command.hueSaturationLayer") :description ~(t "command.hueSaturationLayer.about")
  :category ~(t "category.image") :icon "droplet"
  []
  (addLayer {:kind "hue-saturation" :name (t "adjustment.hue-saturation")}))

(defcommand colorBalanceLayer "Adds a Colour Balance layer."
  :title ~(t "command.colorBalanceLayer") :description ~(t "command.colorBalanceLayer.about")
  :category ~(t "category.image") :icon "scale"
  []
  (addLayer {:kind "color-balance" :name (t "adjustment.color-balance")}))

(defcommand invertLayer "Adds an Invert layer: the colours below, inverted."
  :title ~(t "command.invertLayer") :description ~(t "command.invertLayer.about") :category ~(t "category.image")
  :icon "contrast"
  []
  (addLayer {:kind "invert" :name (t "adjustment.invert")}))

(defcommand blackWhiteLayer "Adds a Black & White layer: a Hue/Saturation layer with no saturation."
  :title ~(t "command.blackWhiteLayer") :description ~(t "command.blackWhiteLayer.about")
  :category ~(t "category.image") :icon "blend"
  []
  (addLayer {:kind "hue-saturation" :name (t "adjustment.blackWhite")})
  (setAdjustment {:saturation -1}))

;;; Layer

(defcommand newLayer "Adds an empty pixel layer above the active one."
  :title ~(t "command.newLayer") :description ~(t "command.newLayer.about") :category ~(t "category.layer")
  :icon "plus"
  []
  (addLayer {:kind "pixel"}))

(defcommand newGroup "Adds an empty group above the active layer."
  :title ~(t "command.newGroup") :description ~(t "command.newGroup.about") :category ~(t "category.layer")
  :icon "folder"
  []
  (addLayer {:kind "group" :name computed.nextGroupName}))

(defcommand duplicateActive "Duplicates the active layer."
  :title ~(t "command.duplicateActive") :description ~(t "command.duplicateActive.about")
  :category ~(t "category.layer") :icon "copy"
  []
  (duplicateLayer {}))

(defcommand deleteActive "Deletes the active layer, unless it is the only one."
  :title ~(t "command.deleteActive") :description ~(t "command.deleteActive.about") :category ~(t "category.layer")
  :icon "trash-2" :enablement ~(> (count state.doc.layers) 1)
  []
  (deleteLayer {}))

(defcommand deleteHidden "Deletes every hidden layer."
  :title ~(t "command.deleteHidden") :description ~(t "command.deleteHidden.about") :category ~(t "category.layer")
  :icon "eye-off" :enablement ~(some (fn [it] (not it.visible)) state.doc.layers)
  []
  (image "deleteHidden"))

(defcommand showRename "Opens the Rename Layer dialog on the active layer."
  :title ~(t "command.showRename") :description ~(t "command.showRename.about") :category ~(t "category.layer")
  :icon "pencil" :enablement ~(some? computed.activeLayer)
  []
  (set "state.rename" {:open true :name computed.activeLayer.name}))

(defcommand layerViaCopy "Copies the selected pixels to a new layer."
  :title ~(t "command.layerViaCopy") :description ~(t "command.layerViaCopy.about") :category ~(t "category.layer")
  :icon "copy-plus" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (image "copy")
  (image "paste" :with {:name (t "layer.copyName" {:name computed.activeLayer.name})}))

(defcommand layerViaCut "Cuts the selected pixels to a new layer."
  :title ~(t "command.layerViaCut") :description ~(t "command.layerViaCut.about") :category ~(t "category.layer")
  :icon "scissors" :enablement ~(= computed.activeLayer.kind "pixel")
  []
  (image "cut")
  (image "paste" :with {:name (t "layer.cutName" {:name computed.activeLayer.name})}))

(defcommand layerUp "Moves the active layer one place up."
  :title ~(t "command.layerUp") :description ~(t "command.layerUp.about") :category ~(t "category.layer")
  :icon "arrow-up"
  []
  (moveLayer {:by 1}))

(defcommand layerDown "Moves the active layer one place down."
  :title ~(t "command.layerDown") :description ~(t "command.layerDown.about") :category ~(t "category.layer")
  :icon "arrow-down"
  []
  (moveLayer {:by -1}))

(defcommand layerToFront "Moves the active layer to the top."
  :title ~(t "command.layerToFront") :description ~(t "command.layerToFront.about") :category ~(t "category.layer")
  :icon "chevrons-up"
  []
  (moveLayer {:by 1000000}))

(defcommand layerToBack "Moves the active layer to the bottom."
  :title ~(t "command.layerToBack") :description ~(t "command.layerToBack.about") :category ~(t "category.layer")
  :icon "chevrons-down"
  []
  (moveLayer {:by -1000000}))

(defcommand toggleLockAll "Locks the active layer whole, or unlocks it."
  :title ~(t "command.toggleLockAll") :description ~(t "command.toggleLockAll.about") :category ~(t "category.layer")
  :icon "lock"
  []
  (setLayer {:lockAll (not computed.activeLayer.lockAll)}))

(defcommand addLayerMask "Adds a mask that reveals all of the active layer."
  :title ~(t "command.addLayerMask") :description ~(t "command.addLayerMask.about") :category ~(t "category.layer")
  :icon "circle-dot" :enablement ~(and computed.activeLayer (not computed.activeLayer.mask))
  []
  (addMask {}))

(defcommand deleteLayerMask "Deletes the active layer's mask."
  :title ~(t "command.deleteLayerMask") :description ~(t "command.deleteLayerMask.about")
  :category ~(t "category.layer") :icon "square-x" :enablement ~(and computed.activeLayer computed.activeLayer.mask)
  []
  (deleteMask {}))

(defcommand toggleClipping "Clips the active layer to the one below, or lets it go."
  :title ~(t "command.toggleClipping") :description ~(t "command.toggleClipping.about")
  :category ~(t "category.layer") :icon "paperclip"
  []
  (setLayer {:clipped (not computed.activeLayer.clipped)}))

(defcommand alignActive "Lines the active layer up with the selection, or the image: args.edge says which edge."
  :title ~(t "command.alignActive") :description ~(t "command.alignActive.about") :category ~(t "category.layer")
  :icon "align-start-vertical" :enablement ~(= computed.activeLayer.kind "pixel")
  [args]
  (image "alignLayer" :with args))

(defcommand groupLayers "Puts the active layer in a new group."
  :title ~(t "command.groupLayers") :description ~(t "command.groupLayers.about") :category ~(t "category.layer")
  :icon "folder-input" :enablement ~(not (empty? state.doc.active))
  []
  (set "state.grouping" state.doc.active)
  (addLayer {:kind "group" :name computed.nextGroupName})
  (moveLayer {:id state.grouping :into state.doc.active}))

(defcommand ungroupLayers "Moves a group's layers out of it, in their order, and deletes it."
  :title ~(t "command.ungroupLayers") :description ~(t "command.ungroupLayers.about")
  :category ~(t "category.layer") :icon "folder-output" :enablement ~(= computed.activeLayer.kind "group")
  []
  (set "state.grouping" state.doc.active)
  (map (fn [child] (moveLayer {:id (:id child) :onto state.grouping})) (reverse computed.groupChildren))
  (deleteLayer {:id state.grouping}))

(defcommand mergeDown "Merges the active layer into the pixel layer below it."
  :title ~(t "command.mergeDown") :description ~(t "command.mergeDown.about") :category ~(t "category.layer")
  :icon "arrow-down-to-line"
  :enablement ~(and (= computed.layerBelow.kind "pixel") (= computed.layerBelow.depth computed.activeLayer.depth))
  []
  (image "mergeDown"))

(defcommand mergeVisible "Merges every visible layer into one."
  :title ~(t "command.mergeVisible") :description ~(t "command.mergeVisible.about") :category ~(t "category.layer")
  :icon "layers-2" :enablement ~(some (fn [it] (and (= it.depth 0) it.visible)) state.doc.layers)
  []
  (image "mergeVisible"))

(defcommand flattenImage "Merges every layer into one, on the background colour."
  :title ~(t "command.flattenImage") :description ~(t "command.flattenImage.about") :category ~(t "category.layer")
  :icon "square-stack"
  []
  (image "flatten" :with {:background state.doc.background}))

;;; Select

(defcommand selectAll "Selects the whole image."
  :title ~(t "command.selectAll") :description ~(t "command.selectAll.about") :category ~(t "category.select")
  :icon "square"
  []
  (select {:kind "all"}))

(defcommand deselect "Selects nothing."
  :title ~(t "command.deselect") :description ~(t "command.deselect.about") :category ~(t "category.select")
  :icon "square-x"
  []
  (select {:kind "none"}))

(defcommand invertSelection "Selects what isn't selected, and nothing of what is."
  :title ~(t "command.invertSelection") :description ~(t "command.invertSelection.about")
  :category ~(t "category.select") :icon "contrast"
  []
  (select {:kind "invert"}))

(defcommand colorRange "Selects the pixels of every layer near the foreground colour."
  :title ~(t "command.colorRange") :description ~(t "command.colorRange.about") :category ~(t "category.select")
  :icon "pipette"
  []
  (select {:kind "colorRange" :color state.doc.foreground :fuzziness 40 :allLayers true}))

(defcommand featherSelection "Softens the selection's edge by 8 pixels."
  :title ~(t "command.featherSelection") :description ~(t "command.featherSelection.about")
  :category ~(t "category.select") :icon "feather" :enablement ~state.doc.selection
  []
  (feather {:radius 8}))

(defcommand toggleQuickMask "Shows the selection as a mask to paint, or as a selection again."
  :title ~(t "command.toggleQuickMask") :description ~(t "command.toggleQuickMask.about")
  :category ~(t "category.select") :icon "scan-line"
  []
  (setOptions {:quickMask (not state.doc.options.quickMask)}))

;;; View

(defcommand zoomIn "Zooms in, twice as close."
  :title ~(t "command.zoomIn") :description ~(t "command.zoomIn.about") :category ~(t "category.view")
  :icon "zoom-in"
  []
  (zoom {:by 2}))

(defcommand zoomOut "Zooms out, twice as far."
  :title ~(t "command.zoomOut") :description ~(t "command.zoomOut.about") :category ~(t "category.view")
  :icon "zoom-out"
  []
  (zoom {:by 0.5}))

(defcommand actualPixels "Shows the image at its own size: one image pixel to one screen pixel."
  :title ~(t "command.actualPixels") :description ~(t "command.actualPixels.about") :category ~(t "category.view")
  :icon "scan"
  []
  (zoom {:to 1}))

(defcommand fitOnScreen "Zooms so that the whole image fits the canvas."
  :title ~(t "command.fitOnScreen") :description ~(t "command.fitOnScreen.about") :category ~(t "category.view")
  :icon "maximize"
  []
  (image "fit"))

(defcommand toggleRulers "Shows the rulers, or hides them."
  :title ~(t "command.toggleRulers") :description ~(t "command.toggleRulers.about") :category ~(t "category.view")
  :icon "ruler"
  []
  (toggle "state.view.rulers"))

(defcommand toggleGuides "Shows the guides, or hides them."
  :title ~(t "command.toggleGuides") :description ~(t "command.toggleGuides.about") :category ~(t "category.view")
  :icon "grid-3-x-3"
  []
  (toggle "state.view.guides"))

(defcommand clearGuides "Takes every guide away."
  :title ~(t "command.clearGuides") :description ~(t "command.clearGuides.about") :category ~(t "category.view")
  :icon "eraser"
  []
  (set "state.guides" {:vertical [] :horizontal []}))

(defcommand cycleScreenMode "Shows the canvas without the panels, or with them again."
  :title ~(t "command.cycleScreenMode") :description ~(t "command.cycleScreenMode.about")
  :category ~(t "category.view") :icon "monitor"
  []
  (set "state.screenMode" (if (= state.screenMode "standard") "full" "standard")))

(defcommand toggleTheme "Switches between the dark and the light colours."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about") :category ~(t "category.view")
  :icon "moon"
  []
  (set "state.theme" (if (= state.theme "dark") "light" "dark")))

;;; Window and Help

(defcommand resetWorkspace "Puts the panels back as they were at first."
  :title ~(t "command.resetWorkspace") :description ~(t "command.resetWorkspace.about")
  :category ~(t "category.window") :icon "layout-panel-left"
  []
  (set "state.panels" {:colour "colour" :properties "properties" :layers "layers"})
  (set "state.popout" {:history false :navigator false})
  (set "state.screenMode" "standard"))

(defcommand showSearch "Searches the commands, and runs the one chosen."
  :title ~(t "command.showSearch") :description ~(t "command.showSearch.about") :category ~(t "category.help")
  :icon "search"
  []
  (set "state.quickPick.open" true))

(defcommand showAbout "Says what Tempera is."
  :title ~(t "command.showAbout") :description ~(t "command.showAbout.about") :category ~(t "category.help")
  :icon "info"
  []
  (set "state.about" true))

(defcommand unavailable "What a menu item Tempera can't do yet runs: it is never enabled."
  :title ~(t "command.unavailable") :description ~(t "command.unavailable.about") :enablement false
  []
  (set "state.doc.message" (t "command.unavailable")))
