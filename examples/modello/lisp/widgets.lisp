;;; widgets.lisp --- TODO

(defcomponent hButton "The hButton component."
  {:icon ""
   :text ""
   :width 24
   :on false
   :chevron false
   :tooltip ""
   :run nil
   :with nil
   :open ""
   :items []
   :menuWidth 200
   :placement below
   :back @menuWidget
   :radius 4
   :textColor @text
   :disabled false}
  `(node :width ~width
     :height 20
     :radius ~radius
     :color ~(if on "@sel" back)
     :hover {:color ~(cond on "@sel" disabled back :else "@widgetHover")}
     :role button
     :focusable true
     :disabled ~disabled
     :tooltip ~tooltip
     :label ~(or text tooltip)
     :selected ~(if on true nil)
     :onClick [{:if ~(not= open "")
                :then [{:toggle ~open}]
                :else [{:if ~(some? run) :then [{:run ~run :with ~with}]}]}]
     (label :when ~(not= icon "")
       :font icons
       :icon ~icon
       :text ""
       :textColor ~(if disabled "@faint" textColor)
       :align center
       :anchors {:left ~(if (and (= text "") (not chevron)) 0 3) :top 0 :bottom 0}
       :width ~(if (and (= text "") (not chevron)) width 18))
     (label :when ~(not= text "")
       :text ~text
       :textColor ~(if disabled "@faint" textColor)
       :align {:horizontal start :vertical center}
       :anchors {:left ~(if (not= icon "") 23 7) :right ~(if chevron 14 4) :top 0 :bottom 0})
     (label :when ~chevron
       :font iconsSmall
       :icon chevron-down
       :text ""
       :textColor @dim
       :align center
       :anchors {:right 2 :top 0 :bottom 0}
       :width 12)
     (node :when ~(not= open "")
       :use menu
       :with {:open ~open
              :items ~items
              :width ~menuWidth
              :placement ~placement
              :icons ~(some (fn [it] (some? it.icon)) items)})))

(defcomponent gap "The gap component."
  {:width 4}
  `{:width ~width :height 1 :color transparent})

(defcomponent searchField "The searchField component."
  {:bind "" :label ""}
  `(node :grow 1 :height 20 :color @field :radius 10
     (label :font iconsSmall :icon search :text "" :textColor @dim :x 6 :width 14 :anchors {:top 0 :bottom 0})
     (textarea :anchors {:left 22 :right 6 :top 1 :bottom 1}
       :bind ~bind
       :placeholder "Search"
       :placeholderColor @faint
       :textColor @text
       :color transparent
       :label ~label)))

(defcomponent panelHeader "The panelHeader component."
  {:title "" :key "" :needs ""}
  `(node :id ~(str "panel-" key)
     :height 24
     :width "100%"
     :radius ~(if (and (= needs "") (get state.panels key)) {:topLeft 4 :topRight 4} 4)
     :color @panel
     :hover {:color ~(if (= needs "") "@widget" "@panel")}
     :role button
     :focusable true
     :label ~title
     :disabled ~(not= needs "")
     :tooltip ~needs
     :isOpen ~(and (= needs "") (= (get state.panels key) true))
     :onClick {:toggle "state.panels[key]"}
     (label :font iconsSmall
       :icon ~(if (and (= needs "") (get state.panels key)) "chevron-down" "chevron-right")
       :text ""
       :textColor ~(if (= needs "") "@dim" "@faint")
       :x 6
       :width 12
       :anchors {:top 0 :bottom 0}
       :align center)
     (label :text ~title
       :textColor ~(if (= needs "") "@text" "@faint")
       :align {:horizontal start :vertical center}
       :anchors {:left 22 :top 0 :bottom 0 :right 8})))

(defcomponent frameField "The frameField component."
  {:label "" :value 0 :width 82 :key "" :radius 4 :ranged false}
  `(node :width ~width
     :height 20
     :radius ~radius
     :color @widget
     :borderWidth ~(if ranged 1 0)
     :borderColor @sel
     :hover {:color @widgetHover}
     :cursor resize-horizontal
     :role slider
     :focusable true
     :label ~label
     :tooltip ~(str label ": drag to change; arrows step it")
     :onPress {:set state.dragStart :to ~value}
     :onDrag {:run setFrameField :with {:key ~key :value ~(+ state.dragStart (* event.dx 0.2))}}
     :keys {:right {:run setFrameField :with {:key ~key :value ~(+ value 1)}}
            :left {:run setFrameField :with {:key ~key :value ~(- value 1)}}}
     (label :when ~(not= key "frame") :text ~label :textColor @dim :x 7 :anchors {:top 0 :bottom 0} :fitText true)
     (label :text ~value
       :textColor @text
       :align ~(if (= key "frame") "center" {:horizontal "end" :vertical "center"})
       :anchors {:left 0 :right ~(if (= key "frame") 0 7) :top 0 :bottom 0})))

(defcomponent numberField "The numberField component."
  {:label ""
   :value 0
   :text ""
   :key ""
   :axis 0
   :step 0.01
   :first false
   :last false
   :path ""
   :min -1000000
   :max 1000000
   :lockable false}
  `(node :height 20 :width "100%" :color transparent
     (label :text ~label
       :textColor @dim
       :align {:horizontal end :vertical center}
       :anchors {:left 0 :top 0 :bottom 0}
       :width "38%")
     ~(field-view label value text key axis step first last path min max)
     ~(lock-view label text key axis lockable)))

(defcomponent checkRow "The checkRow component."
  {:label "" :path "" :checked false :id "" :run nil :with nil}
  `(node :id ~id
     :height 20
     :width "100%"
     :color transparent
     :radius 4
     :hover {:color @widgetHover}
     :role checkbox
     :focusable true
     :label ~label
     :selected ~(= checked true)
     :onClick [{:if ~(some? run) :then [{:run ~run :with ~with}] :else [{:toggle ~path}]}]
     (node :anchors {:left "41%" :top 0 :bottom 0 :right 4} :color transparent
       (node :x 0 :y 3 :width 14 :height 14 :radius 3 :color ~(if checked "@kit.accent" "@widget")
         (label :when ~(= checked true) :font iconsSmall :icon check :text "" :textColor @text :align center :fill true))
       (label :text ~label
         :textColor @text
         :align {:horizontal start :vertical center}
         :anchors {:left 20 :top 0 :bottom 0 :right 0}))))

(defcomponent propPanel "The propPanel component."
  {:key "" :body 32}
  `(column :width "100%"
     :gap 0
     :color transparent
     :height ~(if (and (= (:needs (find (fn [it] (= it.key key)) computed.propPanels)) "") (get state.panels key))
                  (+ 24 body)
                  24)
     (node :use panelHeader
       :with {:title ~(:title (find (fn [it] (= it.key key)) computed.propPanels))
              :key ~key
              :needs ~(:needs (find (fn [it] (= it.key key)) computed.propPanels))})
     (node :id ~(str "panel-" key "-body")
       :when ~(and (= (:needs (find (fn [it] (= it.key key)) computed.propPanels)) "") (= (get state.panels key) true))
       :width "100%"
       :height ~body
       :color @panel
       :radius {:bottomLeft 4 :bottomRight 4}
       (column :fill 6 :gap 4 :color transparent
         {:slot true}))))

(defcomponent infoRow "The infoRow component."
  {:label "" :text "" :id ""}
  `(node :height 20 :width "100%" :color transparent
     (label :text ~label
       :textColor @dim
       :align {:horizontal end :vertical center}
       :anchors {:left 0 :top 0 :bottom 0}
       :width "38%")
     (label :id ~(str "info-" id)
       :text ~text
       :textColor @text
       :align {:horizontal start :vertical center}
       :anchors {:left "41%" :right 4 :top 0 :bottom 0})))

(defcomponent colorRow "The colorRow component."
  {:label "" :value "#000000" :target "" :key ""}
  `(column :width "100%" :height ~(if (get state.panels key) 204 20) :gap 4 :color transparent
     ~(color-row-node label value target key)
     (colorPicker :id ~(str "picker-" target)
       :when ~(= (get state.panels key) true)
       :width "100%"
       :height 180
       :label ~label
       :value ~value
       :fields false
       :alpha false
       :focusable true
       :textColor @text
       :accentColor @kit.accent
       :onChange {:run setColor :with {:target ~target :value ~event.value}})))

(defcomponent vectorField "The vectorField component."
  {:title "" :values [0 0 0] :key "" :unit "" :step 0.01 :digits 4 :exact false :lockable false}
  `(column :width "100%" :height 62 :gap 1 :color transparent
     ~(repeat [v values :index a]
       `(node :use numberField
          :with {:label ~(cond (= a 0) (str title " X") (= a 1) "Y" :else "Z")
                 :value ~v
                 :key ~key
                 :axis ~a
                 :step ~step
                 :first ~(= a 0)
                 :last ~(= a 2)
                 :lockable ~lockable
                 :text ~(+ (if exact (fixed v digits) (formatNumber v 0 digits)) unit)}))))

(defpart pieMenu
  :components {:pieMenu {:runsActions items
             :params {:items [] :open state.pie.open :x 0 :y 0 :title ""}
             :node (node :width 360
                     :height 240
                     :color transparent
                     :role menu
                     :label ~title
                     :arrows vertical
                     :overlay {:placement window :open ~open :dismiss outside :modal true}
                     :x ~(- x 180)
                     :y ~(- y 120)
                     (node :anchors {:left 168 :top 108}
                       :width 24
                       :height 24
                       :radius 12
                       :color @toolGroup
                       :borderWidth 1
                       :borderColor @faint)
                     (label :text ~title :textColor @dim :anchors {:left 0 :right 0 :top 138} :height 16 :align center)
                     (node :id ~(str "pie-" i)
                       :repeat {:for item :in ~items :index i}
                       :role menuitem
                       :label ~item.title
                       :focusable true
                       :disabled ~(= item.enabled false)
                       :selected ~(if (nil? item.checked) nil (= item.checked true))
                       :anchors {:left ~(+ 115 (get [(- 115) 115 0 0 (- 100) 100 (- 100) 100] i))
                                 :top ~(+ 107 (get [0 0 80 (- 80) (- 50) (- 50) 50 50] i))}
                       :width 130
                       :height 26
                       :radius 13
                       :color ~(if (= item.checked true) "@sel" "@toolGroup")
                       :hover {:color ~(if (= item.enabled false) "@toolGroup" "@widgetHover")}
                       :focused {:color @widgetHover}
                       :onClick [{:dismiss true} {:run ~item.run :with ~item.with}]
                       (label :when ~(some? item.icon)
                         :text ""
                         :icon ~item.icon
                         :anchors {:left 10 :top 5}
                         :fitText true
                         :textColor ~(if (= item.enabled false) "@faint" "@text"))
                       (label :text ~item.title
                         :anchors {:left ~(if (some? item.icon) 32 14) :top 5}
                         :fitText true
                         :textColor ~(if (= item.enabled false) "@faint" "@text"))))}})

(defview field-view "#field-." [label value text key axis step first last path min max]
  `(node :id ~(str "field-" (if (not= path "") (slug path) (str key "-" axis)))
     :anchors {:left "41%" :right 22 :top 0 :bottom 0}
     :color @widget
     :hover {:color @widgetHover}
     :cursor resize-horizontal
     :radius ~(cond (and first last) 4 first {:topLeft 4 :topRight 4} last {:bottomLeft 4 :bottomRight 4} :else 0)
     :role slider
     :focusable true
     :label ~label
     :tooltip "Drag to change; arrows step it"
     :onPress [{:set state.dragStart :to ~value}
               {:if ~(if (= key "")
                         (= (slice path 0 6) "scene.")
                         (not (contains ["redo" "lens" "cursor" "whole"] key)))
                :then [{:run snapshot}]}]
     :onDrag {:run setField
              :with {:key ~key
                            :axis ~axis
                            :path ~path
                            :low ~min
                            :high ~max
                            :value ~(+ state.dragStart (* event.dx step))}}
     :keys {:right {:run setField
                    :with {:key ~key
                                  :axis ~axis
                                  :path ~path
                                  :low ~min
                                  :high ~max
                                  :value ~(+ value (* step 10))}}
            :left {:run setField
                   :with {:key ~key
                                 :axis ~axis
                                 :path ~path
                                 :low ~min
                                 :high ~max
                                 :value ~(- value (* step 10))}}}
     (label :text ~text :textColor @text :align center :fill true)))

(defview lock-view "#lock--." [label text key axis lockable]
  `(node :id ~(str "lock-" key "-" axis)
     :when ~lockable
     :anchors {:right 2 :top 0 :bottom 0}
     :width 16
     :color transparent
     :role button
     :focusable true
     :label ~(str "Lock " label)
     :selected ~(= (get-in computed.activeNode [(str "lock_" key) axis]) true)
     :tooltip ~(if (= (get-in computed.activeNode [(str "lock_" key) axis]) true)
                   "Locked: moving with the gizmo or G, R and S leaves it"
                   "Lock it against the gizmo and G, R and S")
     :onClick {:run toggleLock :with {:key ~key :axis ~axis}}
     (label :font iconsSmall
       :text ""
       :align center
       :fill true
       :icon ~(if (= (get-in computed.activeNode [(str "lock_" key) axis]) true) "lock" "lock-open")
       :textColor ~(if (= (get-in computed.activeNode [(str "lock_" key) axis]) true) "@text" "@faint"))))

(defview color-row-node "#color-row-node." [label value target key]
  `(node :height 20 :width "100%" :color transparent
     (label :text ~label
       :textColor @dim
       :align {:horizontal end :vertical center}
       :anchors {:left 0 :top 0 :bottom 0}
       :width "38%")
     {:id ~(str "color-" target)
      :anchors {:left "41%" :right 22 :top 0 :bottom 0}
      :color ~value
      :radius 4
      :borderWidth 1
      :borderColor @widget
      :role button
      :focusable true
      :label ~label
      :tooltip ~(str value ": click to pick another")
      :isOpen ~(= (get state.panels key) true)
      :onClick [{:if ~(and (not= target "world") (not= (get state.panels key) true)) :then [{:run snapshot}]}
                {:toggle "state.panels[key]"}]}))

