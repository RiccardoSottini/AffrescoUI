;;; create.lisp --- the Create page: a new app, blank or from an example, and what to ask the assistant
;;;
;;; The name gives the app's folder; it starts blank or as a copy of an example; the person describes what they want
;;; and picks features of other apps, which the assistant is asked for once the app opens. Cmd+Enter creates it.

;;; The page

(defview create-page "The Create page: the form, scrolled." []
  `(node#createPage :when ~(= state.page "create") :fill true :color transparent :role tabpanel :label ~(t "nav.create")
     (column#createScroll :anchors {:left 0 :right 0 :top 0 :bottom 0} :scroll vertical :gap 0
       ~(create-form))))

(defview create-form "The form: the name, what to start from, the description, the features, and Create." []
  `(node#createForm :anchors {:left 36}
     :width ~(min 820 (- (- window.width computed.sideWidth) 72))
     :height 980
     :color transparent
     :keys {"mod+enter" createApp}
     (label#createTitle :font title
       :text ~(t "create.title")
       :textColor @c.text
       :anchors {:left 0 :top 30}
       :fitText true
       :role heading)
     (label#createSub :text ~(t "create.sub") :textColor @c.muted :anchors {:left 0 :right 0 :top 72} :height 20)
     {:id createNameLabel :style sectionTitle :text ~(t "create.name") :anchors {:left 0 :right 0 :top 118}}
     ~(name-field)
     ~(name-hint)
     {:id createFromLabel :style sectionTitle :text ~(t "create.from") :anchors {:left 0 :right 0 :top 236}}
     {:id createFromHint :style hint :text ~(t "create.fromHint") :anchors {:left 0 :right 0 :top 262}}
     ~(start-tiles)
     {:id createDescribeLabel :style sectionTitle :text ~(t "create.describe") :anchors {:left 0 :right 0 :top 566}}
     {:id createDescribeHint :style hint :text ~(t "create.describeHint") :anchors {:left 0 :right 0 :top 592}}
     ~(description-field)
     (label#createDescribePlaceholder :when ~(= state.create.describe "")
       :text ~(t "create.describePlaceholder")
       :textColor @c.dim
       :anchors {:left 12 :right 12 :top 626}
       :height 20
       :disabled true)
     {:id createFeaturesLabel :style sectionTitle :text ~(t "create.features") :anchors {:left 0 :right 0 :top 738}}
     {:id createFeaturesHint :style hint :text ~(t "create.featuresHint") :anchors {:left 0 :right 0 :top 764}}
     ~(feature-chips)
     ~(create-summary)
     ~(create-button)
     ~(create-problem)))

;;; The name

(defview name-field "The new app's name." []
  `{:id createName
    :style field
    :bind state.create.name
    :maxLength 80
    :placeholder ~(t "create.namePlaceholder")
    :label ~(t "create.name")
    :description ~(if (and state.create.tried (not= computed.createError "")) (t computed.createError) "")
    :borderColor ~(if (and state.create.tried (not= computed.createError "")) "@c.danger" "@c.border")
    :anchors {:left 0 :right 0 :top 148}
    :height 42
    :onSubmit createApp})

(defview name-hint "Under the name: what is wrong with it once Create was pressed, else the folder it makes." []
  `(label#createNameHint :font small
     :anchors {:left 2 :right 0 :top 196}
     :height 18
     :live polite
     :textColor ~(if (and state.create.tried (not= computed.createError "")) "@c.danger" "@c.muted")
     :text ~(cond (and state.create.tried (not= computed.createError "")) (t computed.createError)
                  (not= computed.slug "") (t "create.folder" {:folder computed.slug})
                  :else (t "create.nameHint"))))

;;; What it starts from

(defview start-tiles "What the new app starts from: a blank app, or an example." []
  `(grid#createFrom :anchors {:left 0 :right 0 :top 290}
     :height 252
     :columns ~(max 2 (min 5 (floor (/ (+ (min 820 (- (- window.width computed.sideWidth) 72)) 12) 150))))
     :gap 12
     :scroll vertical
     :role radiogroup
     :label ~(t "create.from")
     :arrows horizontal
     (node#startBlankTile :use startTile :with {:id blank :name ~(t "create.blank") :cover ""})
     ~(repeat [app computed.starters :key app.id]
       `(node#startTile :use startTile :with {:id ~app.id :name ~app.name :cover ~app.cover}))))

(defcomponent startTile "A starting point of the new app: its picture and name, outlined while it is the one chosen."
  {:id blank :name "" :cover ""}
  `(node#startTilePanel :height 118
     :radius 12
     :role radio
     :focusable true
     :cursor hand
     :label ~name
     :selected ~(= state.create.from id)
     :color ~(if (= state.create.from id) "@c.accentSoft" "@c.surface")
     :borderWidth ~(if (= state.create.from id) 2 1)
     :borderColor ~(if (= state.create.from id) "@c.accent" "@c.border")
     :hover {:borderColor @c.accent}
     :onClick ~(handler chooseStart id)
     (node#startPicture :anchors {:left 8 :right 8 :top 8} :height 72 :radius 8 :color @c.raised
       (image#startCover :when ~(not= cover "") :file ~cover :fit cover :fill true)
       (label#startBlank :when ~(= cover "") :icon file-plus :font heroIcons :align center :textColor @c.accent
         :fill true))
     (label#startName :text ~name
       :font ~(if (= state.create.from id) "strong" "default")
       :textColor @c.text
       :align center
       :anchors {:left 4 :right 4 :top 86}
       :height 22)))

;;; What the assistant is asked

(defview description-field "What the person asks the assistant for, in their words." []
  `(editor#createDescribe :bind state.create.describe
     :wrap true
     :label ~(t "create.describe")
     :color @c.surface
     :radius 10
     :borderWidth 1
     :borderColor @c.border
     :focused {:borderColor @c.accent}
     :textColor @c.text
     :caretColor @c.accent
     :lineColor transparent
     :selectionColor @c.accentSoft
     :anchors {:left 0 :right 0 :top 618}
     :height 96))

(defview feature-chips "The features of other apps, a chip each." []
  `(row#createFeatures :wrap true
     :gap 8
     :anchors {:left 0 :right 0 :top 792}
     :height 80
     :role group
     :label ~(t "create.features")
     ~(repeat [f state.featureChoices :key f.id]
       (feature-chip f))))

(defview feature-chip "A feature of another app, checked while it is asked for." [f]
  `(label#featureChip :icon ~(if (contains state.create.features f.id) "check" "plus")
     :text ~(str (t (str "feature." f.id)) " · " f.from)
     :font small
     :fitText true
     :height 32
     :padding 10
     :radius 999
     :focusable true
     :role checkbox
     :checked ~(contains state.create.features f.id)
     :cursor hand
     :label ~(t "feature.label" {:feature (t (str "feature." f.id)) :app f.from})
     :color ~(if (contains state.create.features f.id) "@c.accentSoft" "@c.surface")
     :textColor ~(if (contains state.create.features f.id) "@c.accent" "@c.text")
     :borderWidth 1
     :borderColor ~(if (contains state.create.features f.id) "@c.accent" "@c.border")
     :hover {:borderColor @c.accent}
     :onClick ~(handler toggleFeature f.id)))

;;; Create

(defview create-summary "What will be made: its name, and what it starts from." []
  `(label#createSummary :font small
     :textColor @c.muted
     :wrap true
     :text ~(t "create.summary"
               {:name (if (= (trim state.create.name) "") "…" (trim state.create.name)) :from computed.fromName})
     :anchors {:left 0 :right 240 :top 900}
     :height 36))

(defview create-button "Create, which says so while the app is being made." []
  `{:id createSubmit
    :style primary
    :icon ~(if state.create.busy "refresh-cw" "rocket")
    :text ~(if state.create.busy (t "create.busy") (t "create.submit"))
    :disabled ~state.create.busy
    :anchors {:right 0 :top 894}
    :width 200
    :height 44
    :onClick createApp})

(defview create-problem "Why the app couldn't be made, if it couldn't." []
  `(label#createProblem :when ~(not= state.create.error "")
     :font small
     :wrap true
     :role alert
     :live assertive
     :text ~state.create.error
     :textColor @c.danger
     :color @c.dangerSoft
     :radius 8
     :padding 8
     :anchors {:left 0 :right 0 :top 946}
     :height 32))
