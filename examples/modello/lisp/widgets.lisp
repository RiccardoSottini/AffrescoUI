;;; widgets.lisp --- the pieces every editor is made of: header buttons, search fields, panels, number fields, check
;;; rows, colour rows, and the pie menu
;;;
;;; What they do when used is in modello.lisp ("The widgets"): a button opens its dropdown or runs what it was given, a
;;; number field changes what its key or path names.

;;; Buttons and fields

(defcomponent hButton
  "A header's button: an icon, a text or both; it opens a dropdown (open names its state) or runs a function (run, with)."
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
     :onClick ~(handler pressButton open run with)
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
       :with {:open ~open :items ~items :width ~menuWidth :placement ~placement :icons ~(some (fn [it] (some? it.icon)) items)})))

(defcomponent gap "Room between a header's buttons." {:width 4}
  `{:width ~width :height 1 :color transparent})

(defcomponent searchField "A rounded search field, its text in the state that bind names." {:bind "" :label ""}
  `(node :grow 1 :height 20 :color @field :radius 10
     (label :font iconsSmall :icon search :text "" :textColor @dim :x 6 :width 14 :anchors {:top 0 :bottom 0})
     (textarea :anchors {:left 22 :right 6 :top 1 :bottom 1}
       :bind ~bind
       :placeholder ~(t "field.search")
       :placeholderColor @faint
       :textColor @text
       :color transparent
       :label ~label)))

(defcomponent frameField "One of the timeline's frame fields: dragged or stepped with the arrows."
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
     :tooltip ~(t "field.frameTip" {:label label})
     :onPress ~(handler startDrag value)
     :onDrag ~(handler setFrameField key (+ state.dragStart (* event.dx 0.2)))
     :keys {:right ~(handler setFrameField key (+ value 1)) :left ~(handler setFrameField key (- value 1))}
     (label :when ~(not= key "frame") :text ~label :textColor @dim :x 7 :anchors {:top 0 :bottom 0} :fitText true)
     (label :text ~value
       :textColor @text
       :align ~(if (= key "frame") "center" {:horizontal "end" :vertical "center"})
       :anchors {:left 0 :right ~(if (= key "frame") 0 7) :top 0 :bottom 0})))

;; key, axis and path say what the field changes (setField, in modello.lisp); min and max bound it.
(defcomponent numberField "A number field with its label on the left, and a padlock when lockable."
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
     ~(row-label label)
     ~(number-slider label value text key axis step first last path min max)
     ~(lock-button label key axis lockable)))

(defview row-label "A row's label, on the left of its field." [label]
  `(label
     :text ~label
     :textColor @dim
     :align {:horizontal end :vertical center}
     :anchors {:left 0 :top 0 :bottom 0}
     :width "38%"))

(defview caption "A row's label, on the left of its field: the locale's text of a key." [key]
  `(label
     :text ~(t key)
     :textColor @dim
     :align {:horizontal end :vertical center}
     :anchors {:left 0 :top 0 :bottom 0}
     :width "38%"))

(defview number-slider
  "The field itself: dragged across, or stepped with the arrows; its corners round where a vector's fields start and end."
  [label value text key axis step first last path min max]
  `(node :id ~(str "field-" (if (not= path "") (slug path) (str key "-" axis)))
     :anchors {:left "41%" :right 22 :top 0 :bottom 0}
     :color @widget
     :hover {:color @widgetHover}
     :cursor resize-horizontal
     :radius ~(cond (and first last) 4 first {:topLeft 4 :topRight 4} last {:bottomLeft 4 :bottomRight 4} :else 0)
     :role slider
     :focusable true
     :label ~label
     :tooltip ~(t "field.dragTip")
     :onPress ~(handler pressField key path value)
     :onDrag ~(handler setField key axis path min max (+ state.dragStart (* event.dx step)))
     :keys {:right ~(handler setField key axis path min max (+ value (* step 10)))
            :left ~(handler setField key axis path min max (- value (* step 10)))}
     (label :text ~text :textColor @text :align center :fill true)))

(defmacro lock-on [key axis]
  ;; A macro, not a function: what it writes stays an expression.
  "Whether the active object locks an item of its list."
  `(= (get-in computed.activeNode [(str "lock_" ~key) ~axis]) true))

(defview lock-button "The padlock beside a field: locks the item against the gizmo and G, R and S." [label key axis lockable]
  `(node :id ~(str "lock-" key "-" axis)
     :when ~lockable
     :anchors {:right 2 :top 0 :bottom 0}
     :width 16
     :color transparent
     :role button
     :focusable true
     :label ~(t "field.lock" {:label label})
     :selected ~(lock-on key axis)
     :tooltip ~(if (lock-on key axis) (t "field.locked") (t "field.lockIt"))
     :onClick ~(handler toggleLock key axis)
     (label :font iconsSmall
       :text ""
       :align center
       :fill true
       :icon ~(if (lock-on key axis) "lock" "lock-open")
       :textColor ~(if (lock-on key axis) "@text" "@faint"))))

(defcomponent vectorField "Three number fields, X, Y and Z, of a list of the active object (key)."
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
                 :text ~(str (if exact (fixed v digits) (formatNumber v 0 digits)) unit)}))))

;;; Rows of the properties

(defcomponent checkRow "A check box and its label: it switches the state at path, or runs a function (run, with)."
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
     :onClick ~(handler pressCheck path run with)
     (node :anchors {:left "41%" :top 0 :bottom 0 :right 4} :color transparent
       (node :x 0 :y 3 :width 14 :height 14 :radius 3 :color ~(if checked "@kit.accent" "@widget")
         (label
           :when ~(= checked true)
           :font iconsSmall
           :icon check
           :text ""
           :textColor @text
           :align center
           :fill true))
       (label
         :text ~label
         :textColor @text
         :align {:horizontal start :vertical center}
         :anchors {:left 20 :top 0 :bottom 0 :right 0}))))

(defcomponent infoRow "A label and a text beside it, read only." {:label "" :text "" :id ""}
  `(node :height 20 :width "100%" :color transparent
     ~(row-label label)
     (label :id ~(str "info-" id)
       :text ~text
       :textColor @text
       :align {:horizontal start :vertical center}
       :anchors {:left "41%" :right 4 :top 0 :bottom 0})))

(defcomponent colorRow "A colour of the world, a material or a light: a swatch that opens a picker under it."
  {:label "" :value "#000000" :target "" :key ""}
  `(column :width "100%" :height ~(if (get state.panels key) 204 20) :gap 4 :color transparent
     ~(color-swatch label value target key)
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
       :onChange ~(handler setColor target event.value))))

(defview color-swatch "The colour row's label and swatch; a click opens or closes the picker." [label value target key]
  `(node :height 20 :width "100%" :color transparent
     ~(row-label label)
     (node :id ~(str "color-" target)
       :anchors {:left "41%" :right 22 :top 0 :bottom 0}
       :color ~value
       :radius 4
       :borderWidth 1
       :borderColor @widget
       :role button
       :focusable true
       :label ~label
       :tooltip ~(t "field.colorTip" {:value value})
       :isOpen ~(= (get state.panels key) true)
       :onClick ~(handler openColor target key))))

;;; Panels

(defcomponent panelHeader "A panel's header: its title, and a chevron that opens or closes it; one that needs something is greyed."
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
     :onClick ~(handler togglePanel key)
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

(defmacro panel-of [key]
  ;; A macro, not a function: what it writes stays an expression.
  "The panel of the properties with a key (computed.propPanels)."
  `(find (fn [it] (= it.key ~key)) computed.propPanels))

(defcomponent propPanel "A panel of the properties, open or closed, its rows in its slot; body is its open height."
  {:key "" :body 32}
  `(column :width "100%"
     :gap 0
     :color transparent
     :height ~(if (and (= (:needs (panel-of key)) "") (get state.panels key)) (+ 24 body) 24)
     (node :use panelHeader :with {:title ~(:title (panel-of key)) :key ~key :needs ~(:needs (panel-of key))})
     (node :id ~(str "panel-" key "-body")
       :when ~(and (= (:needs (panel-of key)) "") (= (get state.panels key) true))
       :width "100%"
       :height ~body
       :color @panel
       :radius {:bottomLeft 4 :bottomRight 4}
       (column :fill 6 :gap 4 :color transparent
         {:slot true}))))

;;; The pie menu

(defview pie-item "One item of the pie menu, placed round the middle by its index." [item i]
  `(node :id ~(str "pie-" i)
     :role menuitem
     :label ~item.title
     :focusable true
     :disabled ~(= item.enabled false)
     :selected ~(if (nil? item.checked) nil (= item.checked true))
     :anchors {:left ~(+ 115 (get [-115 115 0 0 -100 100 -100 100] i)) :top ~(+ 107 (get [0 0 80 -80 -50 -50 50 50] i))}
     :width 130
     :height 26
     :radius 13
     :color ~(if (= item.checked true) "@sel" "@toolGroup")
     :hover {:color ~(if (= item.enabled false) "@toolGroup" "@widgetHover")}
     :focused {:color @widgetHover}
     :onClick ~(handler runItem item.run item.with)
     (label :when ~(some? item.icon)
       :text ""
       :icon ~item.icon
       :anchors {:left 10 :top 5}
       :fitText true
       :textColor ~(if (= item.enabled false) "@faint" "@text"))
     (label :text ~item.title
       :anchors {:left ~(if (some? item.icon) 32 14) :top 5}
       :fitText true
       :textColor ~(if (= item.enabled false) "@faint" "@text"))))

(defview pie-menu "The pie menu: its items round a ring in the middle, its title under the ring." []
  `(node :width 360
     :height 240
     :color transparent
     :role menu
     :label ~title
     :arrows vertical
     :overlay {:placement window :open ~open :dismiss outside :modal true}
     :x ~(- x 180)
     :y ~(- y 120)
     (node
       :anchors {:left 168 :top 108}
       :width 24
       :height 24
       :radius 12
       :color @toolGroup
       :borderWidth 1
       :borderColor @faint)
     (label :text ~title :textColor @dim :anchors {:left 0 :right 0 :top 138} :height 16 :align center)
     ~(repeat [item items :index i] (pie-item item i))))

;; A part, not a defcomponent: its items run actions (runsActions), which a component's definition says.
(defpart pieMenu "The pie menu, as a component: items, open, and where its middle is."
  :components {:pieMenu {:runsActions items :params {:items [] :open state.pie.open :x 0 :y 0 :title ""} :node ~(pie-menu)}})

;;; Editor types

(defmacro editor-choices [editor]
  ;; A macro, not a function: what it writes stays an expression.
  "The Editor Type dropdown's items for an editor: its own checked, the others swapped in."
  `(map (fn [it] (merge it {:checked (= it.id ~editor) :run "swapEditor" :with {:from ~editor :to it.id}})) computed.editorTypes))
