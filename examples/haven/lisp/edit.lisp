;;; edit.lisp --- Haven's dashboard editing: arranging the home page's cards, and a card's editor
;;;
;;; Arrange cards, in the top bar's menu, outlines each card with a pencil; a click on one opens its editor, which
;;; changes the card's configuration in a visual form or as code, beside a preview.

;;; What it keeps

(defstate editing false "Whether the cards are being arranged.")

(defstate dashMenu false "Whether the top bar's menu is open.")

(defstate cards
  {:lights {:type light-tiles :title ""}
   :scenes {:type scene-buttons :title ""}
   :security {:type alarm-panel :title ""}
   :climate {:type thermostat :title "" :modes true}
   :cover {:type cover :title ""}
   :devices {:type device-tiles :title "" :actions true}}
  "Each card's configuration: its kind, its own title (empty for the usual one) and its options.")

(defstate cardEdit {:open false :id climate :code false :draft {} :text "" :error ""}
  "The editor: open, the card, form or code, the configuration being changed, its code, and what is wrong with it.")

;;; What it works out

(defcomputed cardTitle "Each card's title: its own, or the usual one."
  {:lights (or state.cards.lights.title (t "lights.title"))
   :scenes (or state.cards.scenes.title (t "scenes.label"))
   :security (or state.cards.security.title (t "alarm.section"))
   :climate (or state.cards.climate.title (t "climate.title"))
   :cover (or state.cards.cover.title (t "blinds.title"))
   :devices (or state.cards.devices.title (t "devices.title"))})

(defcomputed cardIcon "Each card's icon."
  {:lights "lightbulb" :scenes "clapperboard" :security "shield" :climate "thermometer" :cover "blinds" :devices "cpu"})

(defcomputed cardCode "The configuration being changed, as code."
  (str "{\n"
       (join ",\n" (map (fn [it] (str "  " (json it.key) ": " (json it.value))) (entries state.cardEdit.draft)))
       "\n}"))

;;; Arranging the cards

(defcomponent editFrame "While editing, a card's outline and pencil: a click anywhere on the card opens its editor."
  {:card "" :name ""}
  `(node :when ~state.editing
     :color transparent
     :borderWidth 2
     :borderColor @editLine
     :radius 14
     :hover {:borderColor @accent :color @editVeil}
     :cursor hand
     :focusable true
     :role button
     :label ~(t "edit.card" {:name name})
     :tooltip ~(t "edit.card" {:name name})
     :onClick ~(handler editCard card)
     (label :icon pencil
       :font iconsSmall
       :text ""
       :textColor @onAccent
       :color @accent
       :anchors {:right 8 :top 4}
       :width 28
       :height 28
       :radius 14
       :align center)))

(defcomponent pill "A rounded switch bound to the state path at: a track and a round thumb over a transparent toggle."
  {:at "" :on false :name "" :thumb @accent :track @accentTrack}
  `(node :width 40 :height 24 :color transparent
     {:anchors {:left 3 :right 3 :top 5} :height 14 :radius 7 :color ~(if on track "@off")}
     {:anchors {:left ~(if on 18 0) :top 2}
      :width 20
      :height 20
      :radius 10
      :color ~(if on thumb "@thumbOff")
      :shadow {:y 1 :blur 3 :color "#00000047"}}
     (toggle :fill 0
       :bind ~at
       :label ~name
       :cursor hand
       :trackColor transparent
       :activeColor transparent
       :knobColor transparent)))

;;; The card's editor

(defcomponent cardEditor "A card's configuration as a form or as code; Save keeps it, Cancel or Escape drops it."
  {}
  `(node :dir auto
     :overlay {:placement center :open state.cardEdit.open :dismiss outside :modal true}
     :width 880
     :height 440
     :radius 28
     :color @card
     :shadow {:y 8 :blur 24 :color "#00000040"}
     :role dialog
     :label ~(t "edit.cardTitle" {:kind (t (str "edit.kind." state.cardEdit.id))})
     (label#cardEditorTitle :font heading
       :textColor @text
       :text ~(t "edit.cardTitle" {:kind (t (str "edit.kind." state.cardEdit.id))})
       :anchors {:left 24 :right 24 :top 20}
       :height 24)
     ~(card-form)
     ~(card-code)
     (label#cardCodeError :when ~(and state.cardEdit.code (not (empty? state.cardEdit.error)))
       :font small
       :text ~state.cardEdit.error
       :textColor @alert
       :anchors {:left 24 :bottom 74}
       :fitText true
       :live assertive)
     ~(card-preview)
     {:anchors {:left 0 :right 0 :bottom 64} :height 1 :color @divider}
     ~@(editor-buttons)))

(defview editor-buttons "The editor's buttons: form or code at the start; Cancel and Save at the end." []
  [`{:id cardCodeSwitch
     :style button
     :color transparent
     :hover {:color transparent}
     :textColor @accent
     :padding 12
     :fitText true
     :text ~(t (if state.cardEdit.code "edit.showForm" "edit.showCode"))
     :anchors {:left 16 :bottom 14}
     :height 36
     :onClick switchCardView}
   `{:id cardSave
     :style button
     :color @accent
     :hover {:color @accent}
     :textColor @onAccent
     :text ~(t "edit.save")
     :anchors {:right 16 :bottom 14}
     :width 88
     :height 36
     :radius 18
     :onClick saveCard}
   `{:id cardCancel
     :style button
     :color transparent
     :hover {:color @accentSoft}
     :textColor @accent
     :text ~(t "edit.cancel")
     :anchors {:right 112 :bottom 14}
     :width 88
     :height 36
     :radius 18
     :onClick {:set state.cardEdit.open :to false}}])

(defview card-form "The configuration as a form: its title, and a switch for each option." []
  `(node#cardForm :when ~(not state.cardEdit.code) :anchors {:left 24 :right 360 :top 64 :bottom 72} :color transparent
     (node#cardTitleBox :anchors {:left 0 :right 0 :top 0}
       :height 56
       :color @raised
       :hover {:color @raisedHover}
       :radius {:topLeft 4 :topRight 4}
       (label :font small :text ~(t "edit.title") :textColor @muted :anchors {:left 16 :top 6} :height 16 :fitText true)
       (textarea#cardTitleField :bind state.cardEdit.draft.title
         :placeholder ~(get computed.cardTitle state.cardEdit.id)
         :placeholderColor @muted
         :textColor @text
         :color transparent
         :padding 16
         :anchors {:left 0 :right 0 :top 18 :bottom 0}
         :label ~(t "edit.title"))
       {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @muted})
     ~(repeat [opt (filter (fn [it] (and (not= it "type") (not= it "title"))) (keys state.cardEdit.draft)) :index i]
       (card-option opt i))))

(defview card-option "An option of the card, and its switch." [opt i]
  `(node#cardOption :anchors {:left 0 :right 0 :top ~(+ 76 (* i 52))} :height 44 :color transparent
     (label :text ~(t (str "edit.opt." opt))
       :textColor @text
       :anchors {:left 0 :right 56 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (node#cardOptionSwitch :use pill
       :with {:at ~(str "state.cardEdit.draft." opt) :on ~(get state.cardEdit.draft opt)
              :name ~(t (str "edit.opt." opt))}
       :anchors {:right 0 :top 10})))

(defview card-code "The configuration as code." []
  `(editor#cardCode :when ~state.cardEdit.code
     :bind state.cardEdit.text
     :lineNumbers true
     :font symbols
     :textColor @text
     :lineNumberColor @muted
     :activeLineNumberColor @text
     :lineColor @raised
     :caretColor @accent
     :color @field
     :radius 4
     :dir ltr
     :anchors {:left 24 :right 360 :top 64 :bottom 96}
     :label ~(t "edit.code")))

(defview card-preview "The preview of the card as it would be." []
  `(node#cardPreview :anchors {:right 0 :top 0 :bottom 65}
     :width 336
     :color @background
     :role group
     :label ~(t "edit.preview")
     :radius {:topRight 28}
     {:style caption :text ~(t "edit.preview") :anchors {:left 24 :top 24}}
     ~(preview-card)))

(defview preview-card "The card, drawn from its configuration: its title, and the climate's or the devices' controls."
  []
  `(node :style card
     :anchors {:left 24 :right 24 :top 56}
     :height ~(cond (= state.cardEdit.id "climate") (if state.cardEdit.draft.modes 312 256)
                    state.cardEdit.draft.actions 124
                    :else 68)
     {:style cardTitle
      :icon ~(get computed.cardIcon state.cardEdit.id)
      :text ~(str " " (or state.cardEdit.draft.title (get computed.cardTitle state.cardEdit.id)))
      :anchors {:left 24 :right 16 :top 20}
      :fitText false
      :height 28}
     {:when ~(= state.cardEdit.id "climate")
      :style caption
      :text ~(t "climate.now" {:temp computed.room.temp :humidity computed.room.humidity})
      :anchors {:right 16 :top 26}}
     (node :when ~(= state.cardEdit.id "climate")
       :use thermoDial
       :with {:p preview :small true}
       :anchors {:left 69 :top 44})
     (row :when ~(= state.cardEdit.id "climate") :anchors {:left 0 :right 0 :top 200} :height 40 :gap 12 :justify center
       ~(repeat [m ["minus" "plus"]]
         `(label :icon ~m :text "" :textColor @text :color @raised :width 40 :height 40 :radius 20 :align center)))
     ~(preview-modes)
     (row :when ~(= state.cardEdit.draft.actions true) :anchors {:left 16 :right 16 :bottom 16} :height 40 :gap 8
       ~(repeat [d state.devices]
         `(label :font small :text ~d.name :textColor @text :color @raised :grow 1 :basis 0 :height 40 :radius 8
            :align center)))))

(defview preview-modes "The climate card's modes, in the preview." []
  `(row :when ~(and (= state.cardEdit.id "climate") (= state.cardEdit.draft.modes true))
     :anchors {:left 0 :right 0 :bottom 16}
     :height 44
     :gap 12
     :justify center
     ~(repeat [mode state.modes]
       `(label :icon ~(cond (= mode "Heat") "flame" (= mode "Cool") "snowflake" :else "sun-snow")
          :text ""
          :color ~(if (= mode state.mode) computed.modeSoft "@raised")
          :textColor ~(if (= mode state.mode) computed.modeColor "@muted")
          :width 44
          :height 44
          :radius 22
          :align center))))
