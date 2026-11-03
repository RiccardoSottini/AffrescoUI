;;; dialogs.lisp --- Tempera's dialogs: a filter, New, Free Transform, Image Size, Canvas Size, Rename, Fill, Close, About
;;;
;;; Each is the shared kit's dialog, open while its part of the state says so (state.filter, state.newDoc,
;;; state.transform...), with fields bound to that part; its OK runs a function of tempera.lisp that applies it. The
;;; filter dialog only edits state.filter: watches preview, apply or cancel it (tempera.lisp, "The filters").

;;; What the dialogs share

(defcomponent tNumberRow "A number's row in a dialog: its title, its field bound to state.<path>.<key>, its unit."
  {:title "" :key "" :path "" :unit ""}
  `(row :id ~(str path "-" key "-row") :width "100%" :height 28 :gap 8 :alignItems center
     (node :style dim :fitText false :text ~title :width 90 :height 20)
     (textarea :id ~(str path "-" key "-field")
       :label ~title
       :width 90
       :height 24
       :padding 4
       :color @input
       :textColor @text
       :borderWidth 1
       :borderColor @border
       :radius @kit.radius
       :bind ~(str "state." path "." key))
     (node :style dim :text ~unit)))

(defcomponent tFilterSlider "One of the open filter's values, from low to high: a slider, and a field to type it."
  {:title "" :key "" :low 0 :high 100}
  `(row :height 28 :alignItems center :gap 8 :width "100%"
     (node :style dim :fitText false :text ~title :width 80 :height 20)
     (node :id ~(str "filter-" key)
       :use tRange
       :with {:title ~title :key ~key :value ~(get state.filter key) :low ~low :high ~high :run setFilterValue :width 196})
     (textarea :label ~(t "filter.value" {:title title})
       :text ~(round (get state.filter key))
       :width 40
       :height 20
       :padding 3
       :textColor @text
       :color @input
       :borderWidth 1
       :borderColor @border
       :radius @kit.radius
       :onSubmit ~(handler typeFilterValue key low high))))

(defview dialog-host "A dialog, at no size of its own in the tree: what the kit's dialog needs around it." [dialog]
  `(node :style clear :width 0 :height 0 ~dialog))

(defview fields "A dialog's fields, under its title and over its buttons." [id gap & rows]
  `(column :id ~id :anchors {:left 20 :right 20 :top 44 :bottom 52} :gap ~gap ~@rows))

;;; The filter and New

(defview filter-dialog "The open filter's dialog: its values, and Preview." []
  (dialog-host
    `(node#filter-dialog :use dialog
       :with {:open state.filter.open
              :title ~computed.filterTitle
              :message ""
              :width 380
              :height 250
              :buttons ~[{:title (t "dialog.cancel") :run "discardFilter"} {:title (t "dialog.ok") :run "commitFilter" :default true}]}
       ~(fields "filter-fields" 2
          `(node :use tFilterSlider
             :when ~(and (not= state.filter.kind "add-noise") (not= state.filter.kind "motion-blur"))
             :with {:title ~(t "filter.radius") :key radius :low 1 :high 100})
          `(node :use tFilterSlider
             :when ~(and (not= state.filter.kind "gaussian-blur") (not= state.filter.kind "motion-blur"))
             :with {:title ~(t "filter.amount") :key amount :low 1 :high 500})
          `(node :use tFilterSlider
             :when ~(= state.filter.kind "unsharp-mask")
             :with {:title ~(t "filter.threshold") :key threshold :high 255})
          `(node :use tFilterSlider :when ~(= state.filter.kind "motion-blur")
             :with {:title ~(t "filter.angle") :key angle :low -180 :high 180})
          `(node :use tFilterSlider :when ~(= state.filter.kind "motion-blur")
             :with {:title ~(t "filter.distance") :key distance :low 1 :high 200})
          `(row#filter-live-row :width "100%" :height 28 :gap 8 :alignItems center
             (checkbox#filter-live :label ~(t "filter.preview")
               :width 14
               :height 14
               :bind state.filter.live
               :activeColor @accent
               :trackColor @border
               :knobColor @input)
             (label :text ~(t "filter.preview") :textColor @text :fitText true))))))

(defview new-dialog "The New dialog: a preset, or a width and a height, and the bit depth." []
  (dialog-host
    `(node#new-dialog :use dialog
       :with {:open state.newDoc.open
              :title ~(t "newDoc.title")
              :message ""
              :width 360
              :height 264
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "newDoc.create") :run "createDocument" :default true}]}
       ~(fields "new-fields" 6 (new-preset-row) (new-size-row "width") (new-size-row "height") (new-depth-row)))))

(defmacro preset-chosen "The New dialog's sizes are a preset's: it is the one." [preset]
  `(and (= (:width ~preset) (number state.newDoc.width)) (= (:height ~preset) (number state.newDoc.height))))

(defview new-preset-row "The New dialog's preset: the preset the sizes are, or Custom; choosing one sets them." []
  `(row#new-preset-row :width "100%" :height 28 :gap 8 :alignItems center
     (node :style dim :fitText false :text ~(t "newDoc.preset") :width 70 :height 20)
     (node#new-preset :use tPicker
       :with {:title ~(t (str "preset." (or-else (:name (find (fn [it] (preset-chosen it)) state.newPresets)) "custom")))
              :tip ~(t "newDoc.presets")
              :width 200
              :open state.newPresetMenu
              :inDialog true
              :items ~(map (fn [it]
                             {:title (t (str "preset." it.name))
                              :checked (preset-chosen it)
                              :run "newPreset"
                              :with {:args {:width it.width :height it.height}}})
                           state.newPresets)})))

(defview new-size-row "A size of the New dialog, in pixels." [size]
  `(row :width "100%" :height 28 :gap 8 :alignItems center
     (node :style dim :fitText false :text ~(t (str "newDoc." size)) :width 70 :height 20)
     (textarea :id ~(str "new-" size)
       :label ~(t (str "newDoc." size))
       :width 100
       :height 24
       :padding 4
       :color @input
       :textColor @text
       :borderWidth 1
       :borderColor @border
       :radius @kit.radius
       :bind ~(str "state.newDoc." size))
     (node :style dim :text ~(t "unit.pixels"))))

(defview new-depth-row "The New dialog's bit depth: 8 bits, or 16 bits floating point." []
  `(row :width "100%" :height 28 :gap 8 :alignItems center
     (node :style dim :fitText false :text ~(t "newDoc.depth") :width 70 :height 20)
     (tabs#new-format :width 200
       :height 24
       :bind state.newDoc.format
       :tabs [{:id rgba8 :label ~(t "newDoc.8bit") :closable false} {:id rgba16f :label ~(t "newDoc.16bit") :closable false}]
       :textColor @muted
       :activeTextColor @text
       :activeBackground @selected
       :accentColor @accent)))

;;; The image and the active layer

(defview transform-dialog "The Free Transform dialog: move, scale and turn the active layer." []
  (dialog-host
    `(node#transform-dialog :use dialog
       :with {:open state.transform.open :title ~(t "transformDialog.title") :message "" :width 340 :height 270
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "dialog.ok") :run "applyTransform" :default true}]}
       ~(fields "transform-fields" 4
          (number-row "transformDialog.across" "x" "transform" "unit.px")
          (number-row "transformDialog.down" "y" "transform" "unit.px")
          (number-row "transformDialog.width" "scaleX" "transform" "unit.percent")
          (number-row "transformDialog.height" "scaleY" "transform" "unit.percent")
          (number-row "transformDialog.angle" "angle" "transform" "unit.degrees")))))

(defview number-row "A tNumberRow, its title and unit text keys." [title key path unit]
  `(node :use tNumberRow :with {:title ~(t title) :key ~key :path ~path :unit ~(t unit)}))

(defview image-size-dialog "The Image Size dialog: a width, and a height that follows it unless the proportions are free." []
  (dialog-host
    `(node#image-size-dialog :use dialog
       :with {:open state.resize.open :title ~(t "imageSize.title") :message "" :width 340 :height 220
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "dialog.ok") :run "applyImageSize" :default true}]}
       ~(fields "image-size-fields" 4
          (number-row "imageSize.width" "width" "resize" "unit.pixels")
          `(node :use tNumberRow
             :when ~(not state.resize.linked)
             :with {:title ~(t "imageSize.height") :key height :path resize :unit ~(t "unit.pixels")})
          `(node :style dim
             :fitText false
             :when ~state.resize.linked
             :width "100%"
             :height 28
             :text ~(t "imageSize.follows"
                       {:height (text (round (/ (* state.doc.height (or-else (number state.resize.width) state.doc.width))
                                                (max state.doc.width 1))))}))
          `(row :width "100%" :height 28 :gap 8 :alignItems center
             (checkbox#image-size-linked :label ~(t "imageSize.keep")
               :width 14
               :height 14
               :bind state.resize.linked
               :activeColor @accent
               :trackColor @border
               :knobColor @input)
             (label :text ~(t "imageSize.keep") :textColor @text :fitText true))))))

(defview canvas-size-dialog "The Canvas Size dialog: a width and a height, and where the image stays in them." []
  (dialog-host
    `(node#canvas-size-dialog :use dialog
       :with {:open state.canvasSize.open :title ~(t "canvasSize.title") :message "" :width 340 :height 262
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "dialog.ok") :run "applyCanvasSize" :default true}]}
       ~(fields "canvas-size-fields" 4
          (number-row "imageSize.width" "width" "canvasSize" "unit.pixels")
          (number-row "imageSize.height" "height" "canvasSize" "unit.pixels")
          `(row :width "100%" :height 72 :gap 8
             (node :style dim :fitText false :text ~(t "canvasSize.anchor") :width 90 :height 20)
             (grid#canvas-anchor :columns 3 :width 72 :height 72 :gap 2
               ~@(map anchor-cell ["top-left" "top" "top-right" "left" "center" "right" "bottom-left" "bottom" "bottom-right"])))))))

(defview anchor-cell "A place of the anchor's grid: chosen, the image stays there as the canvas grows or shrinks." [place]
  `(node :id ~(str "anchor-" place)
     :role radio
     :label ~(t (str "anchor." place))
     :focusable true
     :checked ~(= state.canvasSize.anchor place)
     :width 22
     :height 22
     :radius 4
     :color ~(if (= state.canvasSize.anchor place) "@accent" "@input")
     :borderWidth 1
     :borderColor @border
     :hover {:borderColor @accent}
     :onClick [{:set state.canvasSize.anchor :to ~place}]))

(defview rename-dialog "The Rename Layer dialog: the active layer's name; Enter renames it." []
  (dialog-host
    `(node#rename-dialog :use dialog
       :with {:open state.rename.open :title ~(t "rename.title") :message "" :width 340 :height 150
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "dialog.ok") :run "applyRename" :default true}]}
       (row#rename-fields :anchors {:left 20 :right 20 :top 48} :height 28 :gap 8 :alignItems center
         (node :style dim :fitText false :text ~(t "rename.name") :width 50 :height 20)
         (textarea#rename-name :label ~(t "rename.label")
           :grow 1
           :height 24
           :padding 4
           :color @input
           :textColor @text
           :borderWidth 1
           :borderColor @border
           :radius @kit.radius
           :bind state.rename.name
           :autofocus true
           :keys {:enter [{:set state.rename.open :to false} {:run applyRename}]})))))

(defview fill-dialog "The Fill dialog: what fills the selection." []
  (dialog-host
    `(node#fill-dialog :use dialog
       :with {:open state.fill.open :title ~(t "fillDialog.title") :message "" :width 340 :height 150
              :buttons ~[{:title (t "dialog.cancel")} {:title (t "dialog.ok") :run "applyFill" :default true}]}
       (row#fill-fields :anchors {:left 20 :right 20 :top 48} :height 28 :gap 8 :alignItems center
         (node :style dim :fitText false :text ~(t "fillDialog.contents") :width 70 :height 20)
         (node#fill-contents :use tPicker
           :with {:title ~(:title (find (fn [it] (= it.id state.fill.contents)) computed.fillContents))
                  :tip ~(t "fillDialog.tip")
                  :width 200
                  :open state.fill.menu
                  :inDialog true
                  :items ~(map (fn [it]
                                 {:title it.title
                                  :checked (= it.id state.fill.contents)
                                  :run "setFillContents"
                                  :with {:args {:id it.id}}})
                               computed.fillContents)})))))

;;; Closing, and About

(defview close-dialog "The question Close asks when the document has changes." []
  (dialog-host
    `(node#close-dialog :use dialog
       :with {:open state.closeAsk
              :title ~(t "closeDialog.title" {:name computed.docName})
              :width 380
              :height 150
              :message ~(t "closeDialog.message")
              :buttons ~[{:title (t "dialog.cancel") :default true} {:title (t "closeDialog.close") :run "discardDocument"}]})))

(defview about-dialog "The About dialog: what Tempera is." []
  `(node#about-dialog :use dialog
     :with {:open state.about :title "Tempera" :message ~(t "about.message") :buttons ~[{:title (t "dialog.ok")}]}))
