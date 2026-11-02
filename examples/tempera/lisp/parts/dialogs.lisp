{:$schema "../../../../schema/affrescoui-screen.schema.json"
 :format affrescoui-screen
 :version 2
 :note "Tempera's dialogs for the commands that take numbers: Free Transform, Image Size, Canvas Size, Rename Layer and Fill, and the question Close asks. Each edits its own part of the state (state.transform, state.resize, state.canvasSize, state.rename, state.fill) and its OK runs the command's apply action."
 :components {:tNumberRow {:params {:title "" :key "" :path "" :unit ""}
                           :node (row :id "{{path}}-{{key}}-row" :width "100%" :height 28 :gap 8 :alignItems center
                                   (node :style dim :fitText false :text "{{title}}" :width 90 :height 20)
                                   (textarea :id "{{path}}-{{key}}-field"
                                     :label "{{title}}"
                                     :width 90
                                     :height 24
                                     :padding 4
                                     :color @input
                                     :textColor @text
                                     :borderWidth 1
                                     :borderColor @border
                                     :radius @kit.radius
                                     :bind "{{'state.' + path + '.' + key}}")
                                   (node :style dim :text "{{unit}}"))}
              :temperaTransformDialog {:node (node :style clear :width 0 :height 0
                                               (node#transform-dialog :use dialog
                                                 :with {:open state.transform.open
                                                        :title "Free Transform"
                                                        :message ""
                                                        :width 340
                                                        :height 270
                                                        :buttons [{:title "Cancel"}
                                                                  {:title "OK" :run applyTransform :default true}]}
                                                 (column#transform-fields :anchors {:left 20
                                                                                    :right 20
                                                                                    :top 44
                                                                                    :bottom 52}
                                                   :gap 4
                                                   (node :use tNumberRow
                                                     :with {:title "Move across" :key x :path transform :unit px})
                                                   (node :use tNumberRow
                                                     :with {:title "Move down" :key y :path transform :unit px})
                                                   (node :use tNumberRow
                                                     :with {:title "Width" :key scaleX :path transform :unit "%"})
                                                   (node :use tNumberRow
                                                     :with {:title "Height" :key scaleY :path transform :unit "%"})
                                                   (node :use tNumberRow
                                                     :with {:title "Angle" :key angle :path transform :unit "°"}))))}
              :temperaImageSizeDialog {:node (node :style clear :width 0 :height 0
                                               (node#image-size-dialog :use dialog
                                                 :with {:open state.resize.open
                                                        :title "Image Size"
                                                        :message ""
                                                        :width 340
                                                        :height 220
                                                        :buttons [{:title "Cancel"}
                                                                  {:title "OK" :run applyImageSize :default true}]}
                                                 (column#image-size-fields :anchors {:left 20
                                                                                     :right 20
                                                                                     :top 44
                                                                                     :bottom 52}
                                                   :gap 4
                                                   (node :use tNumberRow
                                                     :with {:title "Width" :key width :path resize :unit "Pixels"})
                                                   (node :use tNumberRow
                                                     :when "{{!state.resize.linked}}"
                                                     :with {:title "Height" :key height :path resize :unit "Pixels"})
                                                   (node :style dim
                                                     :fitText false
                                                     :when "{{state.resize.linked}}"
                                                     :width "100%"
                                                     :height 28
                                                     :text "Height follows: {{round(state.doc.height * (number(state.resize.width) ?? state.doc.width) / max(state.doc.width, 1))}} Pixels")
                                                   (row :width "100%" :height 28 :gap 8 :alignItems center
                                                     (checkbox#image-size-linked :label "Keep the proportions"
                                                       :width 14
                                                       :height 14
                                                       :bind state.resize.linked
                                                       :activeColor @accent
                                                       :trackColor @border
                                                       :knobColor @input)
                                                     (label :text "Keep the proportions" :textColor @text :fitText true)))))}
              :temperaCanvasSizeDialog {:node (node :style clear :width 0 :height 0
                                                (node#canvas-size-dialog :use dialog
                                                  :with {:open state.canvasSize.open
                                                         :title "Canvas Size"
                                                         :message ""
                                                         :width 340
                                                         :height 262
                                                         :buttons [{:title "Cancel"}
                                                                   {:title "OK" :run applyCanvasSize :default true}]}
                                                  (column#canvas-size-fields :anchors {:left 20
                                                                                       :right 20
                                                                                       :top 44
                                                                                       :bottom 52}
                                                    :gap 4
                                                    (node :use tNumberRow
                                                      :with {:title "Width" :key width :path canvasSize :unit "Pixels"})
                                                    (node :use tNumberRow
                                                      :with {:title "Height"
                                                             :key height
                                                             :path canvasSize
                                                             :unit "Pixels"})
                                                    (row :width "100%" :height 72 :gap 8
                                                      (node :style dim
                                                        :fitText false
                                                        :text "Anchor"
                                                        :width 90
                                                        :height 20)
                                                      (grid#canvas-anchor :columns 3 :width 72 :height 72 :gap 2
                                                        (node :id "anchor-{{a}}"
                                                          :repeat {:for a
                                                                   :in "{{['top-left', 'top', 'top-right', 'left', 'center', 'right', 'bottom-left', 'bottom', 'bottom-right']}}"}
                                                          :role radio
                                                          :label "{{a}}"
                                                          :focusable true
                                                          :checked "{{state.canvasSize.anchor == a}}"
                                                          :width 22
                                                          :height 22
                                                          :radius 4
                                                          :color "{{state.canvasSize.anchor == a ? '@accent' : '@input'}}"
                                                          :borderWidth 1
                                                          :borderColor @border
                                                          :hover {:borderColor @accent}
                                                          :onClick [{:set state.canvasSize.anchor :to "{{a}}"}]))))))}
              :temperaRenameDialog {:node (node :style clear :width 0 :height 0
                                            (node#rename-dialog :use dialog
                                              :with {:open state.rename.open
                                                     :title "Rename Layer"
                                                     :message ""
                                                     :width 340
                                                     :height 150
                                                     :buttons [{:title "Cancel"}
                                                               {:title "OK" :run applyRename :default true}]}
                                              (row#rename-fields :anchors {:left 20 :right 20 :top 48}
                                                :height 28
                                                :gap 8
                                                :alignItems center
                                                (node :style dim :fitText false :text "Name" :width 50 :height 20)
                                                (textarea#rename-name :label "Layer name"
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
                                                  :keys {:enter [{:set state.rename.open :to false} {:run applyRename}]}))))}
              :temperaFillDialog {:node (node :style clear :width 0 :height 0
                                          (node#fill-dialog :use dialog
                                            :with {:open state.fill.open
                                                   :title "Fill"
                                                   :message ""
                                                   :width 340
                                                   :height 150
                                                   :buttons [{:title "Cancel"}
                                                             {:title "OK" :run applyFill :default true}]}
                                            (row#fill-fields :anchors {:left 20 :right 20 :top 48}
                                              :height 28
                                              :gap 8
                                              :alignItems center
                                              (node :style dim :fitText false :text "Contents" :width 70 :height 20)
                                              (node#fill-contents :use tPicker
                                                :with {:title "{{find(computed.fillContents, it.id == state.fill.contents).title}}"
                                                       :tip "What fills the selection"
                                                       :width 200
                                                       :open state.fill.menu
                                                       :inDialog true
                                                       :items "{{map(computed.fillContents, {title: it.title, checked: it.id == state.fill.contents, run: 'setFillContents', with: {args: {id: it.id}}})}}"}))))}
              :temperaCloseDialog {:node (node :style clear :width 0 :height 0
                                           (node#close-dialog :use dialog
                                             :with {:open state.closeAsk
                                                    :title "Close {{state.docName}}?"
                                                    :width 380
                                                    :height 150
                                                    :message "Its changes since it was opened or saved will be lost."
                                                    :buttons [{:title "Cancel" :default true}
                                                              {:title "Close" :run discardDocument}]}))}}}
