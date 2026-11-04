;;; menus.lisp --- Tempera's menus: the menu bar's, and the submenus they open
;;;
;;; An item runs a command of commands.lisp, whose title it shows unless it gives its own; a submenu names another
;;; menu here. What Tempera can't do yet runs unavailable, shown disabled. The menu bar (menubar.lisp) draws them.

;;; File and Edit

(defmenu file "The File menu: new, open, close, save, export."
  {:new {:run showNewDialog :group "1_new" :order 1}
   :sample {:run newSample :group "1_new" :order 2}
   :open {:run openFile :group "1_new" :order 3}
   :close {:run closeDocument :group "2_close"}
   :save {:run saveProject :group "3_save" :order 1}
   :saveAs {:run saveProjectAs :group "3_save" :order 2}
   :revert {:run revert :group "3_save" :order 3}
   :export {:submenu export :title ~(t "menu.file.export") :group "4_export"}
   :print {:run unavailable :title ~(t "menu.file.print") :group "5_print"}})

(defmenu export "File > Export's formats."
  {:png {:run exportPng :title ~(t "menu.export.png") :order 1}
   :jpeg {:run exportJpeg :title ~(t "menu.export.jpeg") :order 2}})

(defmenu edit "The Edit menu: history, the clipboard, fills, transforms, colours."
  {:undo {:run undo :group "1_history" :order 1}
   :redo {:run redo :group "1_history" :order 2}
   :cut {:run cutPixels :group "2_clipboard" :order 1}
   :copy {:run copyPixels :group "2_clipboard" :order 2}
   :copyMerged {:run copyMerged :group "2_clipboard" :order 3}
   :paste {:run pastePixels :group "2_clipboard" :order 4}
   :clear {:run clearPixels :group "2_clipboard" :order 5}
   :fill {:run showFill :group "3_fill" :order 1}
   :stroke {:run unavailable :title ~(t "menu.edit.stroke") :group "3_fill" :order 2}
   :freeTransform {:run showTransform :group "4_transform" :order 1}
   :transform {:submenu layerTransform :title ~(t "menu.edit.transform") :group "4_transform" :order 2}
   :swap {:run swapColours :group "5_colours" :order 1}
   :reset {:run resetColours :group "5_colours" :order 2}
   :working {:submenu workingSpace :title ~(t "menu.edit.working") :group "6_profiles"}})

;;; Image, its rotations and the colour profiles

(defmenu image "The Image menu: adjustments, sizes, rotation and crop."
  {:mode {:run unavailable :title ~(t "menu.image.mode") :group "1_mode"}
   :adjustments {:submenu adjustmentLayers :title ~(t "menu.image.adjustments") :group "1_mode" :order 1}
   :imageSize {:run showImageSize :group "2_size" :order 1}
   :canvasSize {:run showCanvasSize :group "2_size" :order 2}
   :rotation {:submenu imageRotation :title ~(t "menu.image.rotation") :group "2_size" :order 3}
   :crop {:run cropToSelection :title ~(t "menu.image.crop") :group "2_size" :order 4}
   :trim {:run unavailable :title ~(t "menu.image.trim") :group "2_size" :order 5}})

(defmenu layerTransform "Edit > Transform: the active layer turned or flipped."
  {:rotate180 {:run rotateLayer180 :title ~(t "menu.layerTransform.rotate180") :group "1_rotate" :order 1}
   :rotateRight {:run rotateLayerRight :title ~(t "menu.layerTransform.rotateRight") :group "1_rotate" :order 2}
   :rotateLeft {:run rotateLayerLeft :title ~(t "menu.layerTransform.rotateLeft") :group "1_rotate" :order 3}
   :flipHorizontal {:run flipLayerHorizontal :title ~(t "menu.layerTransform.flipHorizontal") :group "2_flip" :order 1}
   :flipVertical {:run flipLayerVertical :title ~(t "menu.layerTransform.flipVertical") :group "2_flip" :order 2}})

(defmenu imageRotation "Image > Image Rotation: the whole image turned or flipped."
  {:rotate180 {:run rotateCanvas180 :title ~(t "menu.imageRotation.rotate180") :group "1_rotate" :order 1}
   :rotateRight {:run rotateCanvasRight :title ~(t "menu.imageRotation.rotateRight") :group "1_rotate" :order 2}
   :rotateLeft {:run rotateCanvasLeft :title ~(t "menu.imageRotation.rotateLeft") :group "1_rotate" :order 3}
   :flipHorizontal {:run flipCanvasHorizontal :title ~(t "menu.imageRotation.flipHorizontal") :group "2_flip" :order 1}
   :flipVertical {:run flipCanvasVertical :title ~(t "menu.imageRotation.flipVertical") :group "2_flip" :order 2}})

(defmenu workingSpace "Edit > Working Space: the profile the image is in."
  {:srgb {:run setProfile :with {:args {:working srgb}} :title ~(t "menu.workingSpace.srgb") :order 1}
   :displayP3 {:run setProfile :with {:args {:working displayP3}} :title ~(t "menu.workingSpace.displayP3") :order 2}
   :wideRgb {:run setProfile :with {:args {:working wideRgb}} :title ~(t "menu.workingSpace.wideRgb") :order 3}
   :gray {:run setProfile :with {:args {:working gray}} :title ~(t "menu.workingSpace.gray") :order 4}})

(defmenu proofSpace "View > Proof Setup: the profile the colours are shown as."
  {:none {:run setProfile :with {:args {:proof ""}} :title ~(t "menu.proofSpace.none") :group "1_none"}
   :srgb {:run setProfile :with {:args {:proof srgb}} :title ~(t "menu.proofSpace.srgb") :group "2_spaces" :order 1}
   :displayP3 {:run setProfile :with {:args {:proof displayP3}}
               :title ~(t "menu.proofSpace.displayP3") :group "2_spaces" :order 2}
   :gray {:run setProfile :with {:args {:proof gray}} :title ~(t "menu.proofSpace.gray") :group "2_spaces" :order 3}})

;;; Layer

(defmenu layer "The Layer menu: new, delete, masks, groups, arrange, merge."
  {:new {:submenu layerNew :title ~(t "menu.layer.new") :group "1_new" :order 1}
   :duplicate {:run duplicateActive :title ~(t "menu.layer.duplicate") :group "1_new" :order 2}
   :delete {:submenu layerDelete :title ~(t "menu.layer.delete") :group "1_new" :order 3}
   :rename {:run showRename :group "2_props" :order 1}
   :style {:submenu layerStyle :title ~(t "menu.layer.style") :group "2_props" :order 2}
   :fillLayer {:submenu fillLayers :title ~(t "menu.layer.fillLayer") :group "3_adjust" :order 1}
   :adjustment {:submenu adjustmentLayers :title ~(t "menu.layer.adjustment") :group "3_adjust" :order 2}
   :mask {:submenu layerMask :title ~(t "menu.layer.mask") :group "4_mask" :order 1}
   :clip {:run toggleClipping :title ~(t "menu.layer.clip") :group "4_mask" :order 2}
   :group {:run groupLayers :group "5_group" :order 1}
   :ungroup {:run ungroupLayers :group "5_group" :order 2}
   :arrange {:submenu arrange :title ~(t "menu.layer.arrange") :group "6_arrange" :order 1}
   :align {:submenu layerAlign :title ~(t "menu.layer.align") :group "6_arrange" :order 2}
   :lock {:run toggleLockAll :title ~(t "menu.layer.lock") :group "6_arrange" :order 3}
   :rasterize {:run unavailable :title ~(t "menu.layer.rasterize") :group "7_merge" :order 1}
   :mergeDown {:run mergeDown :group "7_merge" :order 2}
   :mergeVisible {:run mergeVisible :group "7_merge" :order 3}
   :flatten {:run flattenImage :group "7_merge" :order 4}})

(defmenu layerNew "Layer > New."
  {:layer {:run newLayer :title ~(t "menu.layerNew.layer") :group "1" :order 1}
   :group {:run newGroup :title ~(t "menu.layerNew.group") :group "1" :order 2}
   :copy {:run layerViaCopy :group "2" :order 1}
   :cut {:run layerViaCut :group "2" :order 2}})

(defmenu layerAlign "Layer > Align: an edge of the active layer to the selection's, or the image's."
  {:left {:run alignActive :with {:args {:edge left}} :title ~(t "menu.layerAlign.left") :group "1_across" :order 1}
   :center {:run alignActive :with {:args {:edge center}}
            :title ~(t "menu.layerAlign.center") :group "1_across" :order 2}
   :right {:run alignActive :with {:args {:edge right}} :title ~(t "menu.layerAlign.right") :group "1_across" :order 3}
   :top {:run alignActive :with {:args {:edge top}} :title ~(t "menu.layerAlign.top") :group "2_down" :order 1}
   :middle {:run alignActive :with {:args {:edge middle}} :title ~(t "menu.layerAlign.middle") :group "2_down" :order 2}
   :bottom {:run alignActive :with {:args {:edge bottom}}
            :title ~(t "menu.layerAlign.bottom") :group "2_down" :order 3}})

(defmenu layerDelete "Layer > Delete."
  {:layer {:run deleteActive :title ~(t "menu.layerDelete.layer") :order 1}
   :hidden {:run deleteHidden :title ~(t "menu.layerDelete.hidden") :order 2}})

(defmenu layerStyle "Layer > Layer Style: not available yet."
  {:options {:run unavailable :title ~(t "menu.layerStyle.options") :group "1"}
   :bevel {:run unavailable :title ~(t "menu.layerStyle.bevel") :group "2" :order 1}
   :stroke {:run unavailable :title ~(t "menu.layerStyle.stroke") :group "2" :order 2}
   :innerShadow {:run unavailable :title ~(t "menu.layerStyle.innerShadow") :group "2" :order 3}
   :outerGlow {:run unavailable :title ~(t "menu.layerStyle.outerGlow") :group "2" :order 4}
   :dropShadow {:run unavailable :title ~(t "menu.layerStyle.dropShadow") :group "2" :order 5}})

(defmenu fillLayers "Layer > New Fill Layer: not available yet."
  {:solid {:run unavailable :title ~(t "menu.fillLayers.solid") :order 1}
   :gradient {:run unavailable :title ~(t "menu.fillLayers.gradient") :order 2}
   :pattern {:run unavailable :title ~(t "menu.fillLayers.pattern") :order 3}})

(defmenu layerMask "Layer > Layer Mask."
  {:reveal {:run addLayerMask :title ~(t "menu.layerMask.reveal") :order 1}
   :delete {:run deleteLayerMask :title ~(t "menu.layerMask.delete") :order 2}})

(defmenu arrange "Layer > Arrange: the active layer up or down."
  {:front {:run layerToFront :order 1}
   :up {:run layerUp :order 2}
   :down {:run layerDown :order 3}
   :back {:run layerToBack :order 4}})

(defmenu adjustmentLayers "The adjustment layers, in Image > Adjustments and Layer > New Adjustment Layer."
  {:brightnessContrast {:run addLayer
                        :with {:args {:kind brightness-contrast :name ~(t "adjustment.brightness-contrast")}}
                        :title ~(t "menu.adjustmentLayers.brightnessContrast")
                        :group "1_tone"
                        :order 1}
   :levels {:run levelsLayer :group "1_tone" :order 2}
   :curves {:run curvesLayer :group "1_tone" :order 3}
   :hueSaturation {:run hueSaturationLayer :group "2_colour" :order 1}
   :colorBalance {:run colorBalanceLayer :group "2_colour" :order 2}
   :blackWhite {:run blackWhiteLayer :group "2_colour" :order 3}
   :invert {:run invertLayer :group "3_special" :order 1}
   :threshold {:run addLayer
               :with {:args {:kind threshold :name ~(t "adjustment.threshold")}}
               :title ~(t "menu.adjustmentLayers.threshold")
               :group "3_special"
               :order 2}
   :gradientMap {:run addLayer
                 :with {:args {:kind gradient-map :name ~(t "adjustment.gradient-map")}}
                 :title ~(t "menu.adjustmentLayers.gradientMap")
                 :group "3_special"
                 :order 3}})

;;; Text, Select and Filter

(defmenu type "The Text menu: not available yet."
  {:panels {:run unavailable :title ~(t "menu.type.panels") :group "1_panels"}
   :workPath {:run unavailable :title ~(t "menu.type.workPath") :group "2_convert" :order 1}
   :rasterize {:run unavailable :title ~(t "menu.type.rasterize") :group "2_convert" :order 2}})

(defmenu select "The Select menu."
  {:all {:run selectAll :group "1_select" :order 1}
   :none {:run deselect :group "1_select" :order 2}
   :reselect {:run unavailable :title ~(t "menu.select.reselect") :group "1_select" :order 3}
   :inverse {:run invertSelection :group "1_select" :order 4}
   :colorRange {:run colorRange :title ~(t "menu.select.colorRange") :group "2_range"}
   :feather {:run featherSelection :title ~(t "menu.select.feather") :group "3_modify" :order 1}
   :transform {:run unavailable :title ~(t "menu.select.transform") :group "3_modify" :order 2}
   :quickMask {:run toggleQuickMask :title ~(t "menu.select.quickMask") :group "4_mask"}})

(defmenu filter "The Filter menu: its kinds of filter."
  {:blur {:submenu blurFilters :title ~(t "menu.filter.blur") :group "2_kinds" :order 1}
   :noise {:submenu noiseFilters :title ~(t "menu.filter.noise") :group "2_kinds" :order 2}
   :sharpen {:submenu sharpenFilters :title ~(t "menu.filter.sharpen") :group "2_kinds" :order 3}})

(defmenu blurFilters "Filter > Blur."
  {:gaussianBlur {:run showFilter :with {:args {:kind gaussian-blur}}
                  :title ~(t "menu.blurFilters.gaussianBlur") :order 1}
   :motionBlur {:run showFilter :with {:args {:kind motion-blur}} :title ~(t "menu.blurFilters.motionBlur") :order 2}})

(defmenu noiseFilters "Filter > Noise."
  {:addNoise {:run showFilter :with {:args {:kind add-noise}} :title ~(t "menu.noiseFilters.addNoise")}})

(defmenu sharpenFilters "Filter > Sharpen."
  {:sharpen {:run showFilter :with {:args {:kind sharpen}} :title ~(t "menu.sharpenFilters.sharpen") :order 1}
   :unsharpMask {:run showFilter :with {:args {:kind unsharp-mask}}
                 :title ~(t "menu.sharpenFilters.unsharpMask") :order 2}})

;;; View, Window and Help

(defmenu view "The View menu: proofing, zoom, rulers, guides, the screen mode and the theme."
  {:proof {:submenu proofSpace :title ~(t "menu.view.proof") :group "1_proof" :order 1}
   :proofColours {:run toggleProof :group "1_proof" :order 2}
   :zoomIn {:run zoomIn :group "2_zoom" :order 1}
   :zoomOut {:run zoomOut :group "2_zoom" :order 2}
   :fit {:run fitOnScreen :group "2_zoom" :order 3}
   :actual {:run actualPixels :group "2_zoom" :order 4}
   :rulers {:run toggleRulers :group "3_show" :order 1}
   :guides {:run toggleGuides :group "3_show" :order 2}
   :clearGuides {:run clearGuides :group "3_show" :order 3}
   :screenMode {:submenu screenMode :title ~(t "menu.view.screenMode") :group "4_theme" :order 1}
   :theme {:run toggleTheme :group "4_theme" :order 2}})

(defmenu screenMode "View > Screen Mode."
  {:standard {:run setScreenMode :with {:args {:mode standard}} :title ~(t "menu.screenMode.standard") :order 1}
   :full {:run setScreenMode :with {:args {:mode full}} :title ~(t "menu.screenMode.full") :order 2}})

(defmenu window "The Window menu: the panels."
  {:adjustments {:run showPanel :with {:args {:slot properties :id adjustments}}
                 :title ~(t "menu.window.adjustments") :order 1}
   :channels {:run showPanel :with {:args {:slot layers :id channels}} :title ~(t "menu.window.channels") :order 2}
   :colour {:run showPanel :with {:args {:slot colour :id colour}} :title ~(t "menu.window.colour") :order 3}
   :history {:run showPopout :with {:args {:id history}} :title ~(t "menu.window.history") :order 4}
   :layers {:run showPanel :with {:args {:slot layers :id layers}} :title ~(t "menu.window.layers") :order 5}
   :navigator {:run showPopout :with {:args {:id navigator}} :title ~(t "menu.window.navigator") :order 6}
   :paths {:run showPanel :with {:args {:slot layers :id paths}} :title ~(t "menu.window.paths") :order 7}
   :properties {:run showPanel :with {:args {:slot properties :id properties}}
                :title ~(t "menu.window.properties") :order 8}
   :swatches {:run showPanel :with {:args {:slot colour :id swatches}} :title ~(t "menu.window.swatches") :order 9}})

(defmenu help "The Help menu." {:about {:run showAbout}})
