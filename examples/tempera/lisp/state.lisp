;;; state.lisp --- what Tempera keeps while it runs, and where each value starts
;;;
;;; The pixels aren't here: they live in the library's image doc (image.lisp), and after each change the library puts
;;; its snapshot in state.doc. The rest is the workspace's own: the tools and the lists the bars and panels show, the
;;; dialogs' fields, which menus are open, and the language setting. Lists people see keep a name for each item, and
;;; the views give its text through (t ...): a preset's own name (`:preset`) is what the image step is told.

(ns tempera.state)

;;; The image's snapshot

(defstate doc
  {:width 0
   :height 0
   :format rgba8
   :profile sRGB
   :display sRGB
   :proof ""
   :revision 0
   :active ""
   :layers []
   :history []
   :snapshots []
   :position 0
   :dropped 0
   :canUndo false
   :canRedo false
   :memory 0
   :selection false
   :bounds [0 0 0 0]
   :foreground "#000000"
   :background "#ffffff"
   :tool brush
   :options {:size 20
             :hardness 80
             :spacing 25
             :opacity 100
             :flow 100
             :mode replace
             :tolerance 32
             :contiguous true
             :allLayers false
             :fillOpacity 100
             :shape linear
             :quickMask false
             :autoSelect false}
   :view {:zoom 1 :x 0 :y 0}
   :message ""
   :previewing false
   :clipboard false}
  "What the library says of the image after each change: its size, layers, history, colours, tool and view.")

(defstate docName "" "The document's name, on its tab: empty for the first untitled one (computed.docName).")

;;; The tools and what they offer

(defstate tools
  [[{:id move :name move :icon move :key "V"}]
   []
   [{:id marquee :name marquee :icon box-select :key "M"}
    {:id ellipse :name ellipse :icon circle-dashed :key "Shift+M"}]
   [{:id lasso :name lasso :icon lasso :key "L"} {:id polygon :name polygon :icon pentagon :key "Shift+L"}]
   [{:id quick-select :name quickSelect :icon wand-2 :key "Shift+W"} {:id wand :name wand :icon wand :key "W"}]
   [{:id crop :name crop :icon crop :key "C"}]
   []
   [{:id eyedropper :name eyedropper :icon pipette :key "I"}]
   [{:id healing :name healing :icon bandage :key "J"}]
   [{:id brush :name brush :icon paintbrush :key "B"} {:id pencil :name pencil :icon pencil :key "Shift+B"}]
   [{:id clone :name clone :icon copy-plus :key "S"}]
   [{:name restore :icon undo-dot}]
   [{:id eraser :name eraser :icon eraser :key "E"}]
   [{:id gradient :name gradient :icon blend :key "G"} {:id fill :name fill :icon paint-bucket :key "Shift+G"}]
   [{:id blur :name blur :icon droplet :key "R"} {:id sharpen :name sharpen :icon triangle}
    {:id smudge :name smudge :icon pointer}]
   [{:id dodge :name dodge :icon sun-dim :key "O"} {:id burn :name burn :icon flame :key "Shift+O"}
    {:id sponge :name sponge :icon droplets}]
   []
   [{:id pen :name pen :icon pen-tool :key "P"}]
   [{:id type :name text :icon type :key "T"}]
   [{:id path-select :name pathSelection :icon mouse-pointer-2 :key "A"}]
   [{:id rectangle-shape :name rectangle :icon square :key "U"}
    {:id ellipse-shape :name ellipseShape :icon circle :key "Shift+U"}
    {:id polygon-shape :name polygonShape :icon hexagon}
    {:id line-shape :name lineShape :icon slash}]
   []
   [{:id hand :name hand :icon hand :key "H"}]
   [{:id zoom :name zoom :icon zoom-in :key "Z"}]]
  "The tools bar's groups, top to bottom (an empty group is a gap): a tool with no id isn't available yet.")

(defstate modeTools
  {:transform-selection {:id transform-selection :name transformSelection :icon scan}
   :free-transform {:id free-transform :name freeTransform :icon scaling}}
  "Tools chosen by a command, not on the tools bar: the handles of Select > Transform Selection and Free Transform.")

(defstate brushHardness 80 "The brush's hardness, kept while the pencil (always hard) is chosen.")

(defstate brushes
  [{:name hardRound5 :preset "Hard Round 5"}
   {:name hardRound20 :preset "Hard Round 20"}
   {:name softRound30 :preset "Soft Round 30"}
   {:name softRound100 :preset "Soft Round 100"}
   {:name airbrush60 :preset "Airbrush 60"}
   {:name pencil1 :preset "Pencil 1"}
   {:name chalk40 :preset "Chalk 40"}]
  "The brush presets: the image step's name for each, and the name of its text.")

(defstate gradients
  [{:name blackWhite :preset "Black, White"}
   {:name spectrum :preset "Spectrum"}
   {:name copper :preset "Copper"}
   {:name blackTransparent :preset "Black, Transparent"}]
  "The gradient presets: the image step's name for each, and the name of its text.")

(defstate gradientEdit
  {:kind linear :angle 90 :stops [{:at 0 :color "#000000ff"} {:at 1 :color "#ffffffff"}]}
  "The gradient the options bar edits and gives the tool.")

(defstate blends
  [normal "-" darken multiply "-" lighten screen "-" overlay soft-light "-" difference "-"
   hue saturation color luminosity]
  "The blend modes, in the Layers panel's menu: \"-\" is a separator.")

(defstate adjustments
  [{:id brightness-contrast :icon sun}
   {:id levels :icon bar-chart-3}
   {:id curves :icon spline}
   {:id hue-saturation :icon palette}
   {:id color-balance :icon scale}
   {:id invert :icon circle-slash}
   {:id threshold :icon contrast}
   {:id gradient-map :icon blend}]
  "The kinds of adjustment layer, in the Adjust panel and its menu.")

(defstate filters
  [{:id gaussian-blur} {:id motion-blur} {:id sharpen} {:id unsharp-mask} {:id add-noise} {:id high-pass}]
  "The filters the Filter menu opens.")

(defstate profiles [{:id srgb} {:id displayP3} {:id wideRgb} {:id gray}] "The colour profiles a document can be in.")

(defstate swatches
  ["#000000" "#ffffff" "#ef4444" "#f97316" "#facc15" "#22c55e" "#14b8a6" "#3b82f6"
   "#6366f1" "#a855f7" "#ec4899" "#78716c" "#9ca3af" "#374151" "#7c2d12" "#14532d"]
  "The Palette panel's colours.")

(defstate newPresets
  [{:name default :width 1200 :height 800}
   {:name square :width 1080 :height 1080}
   {:name hd :width 1920 :height 1080}
   {:name uhd :width 3840 :height 2160}
   {:name a4 :width 2480 :height 3508}
   {:name icon :width 512 :height 512}]
  "The New dialog's sizes.")

;;; The workspace

(defstate theme dark "The colours: dark or light.")

(defstate guides {:vertical [] :horizontal []}
  "The guides, in image pixels, that a click on a ruler adds, or a drag out of one.")

(defstate guideDrag {:axis "" :at 0 :moved false}
  "A drag out of a ruler (the guide's axis while it is over the image, and where) or of a guide (where it was).")

(defstate view {:rulers true :guides true} "Whether the rulers and the guides are shown.")

(defstate panels {:colour colour :properties properties :layers layers} "The tab each group of the dock shows.")

(defstate collapsed {:colour false :properties false :layers false} "The dock's groups folded to their tabs.")

(defstate groupOrder ["colour" "properties" "layers"]
  "The dock's groups from the top: a group's tabs dragged onto another group's take its place.")

(defstate groupHeights {:colour 158 :properties 236}
  "The heights of the dock's first two groups, that the sash under each drags; the Layers group takes the rest.")

(defstate workspace standard "The workspace chosen: standard, painting, or the name of one saved.")

(defstate workspaces [] "The workspaces saved: each a name and the dock's tabs, folds, heights and pop-outs.")

(defstate popout {:history false :navigator false} "Whether the History and the Navigator panels are out.")

(defstate props {:transform true :align true :actions true} "The Properties panel's sections that are open.")

(defstate layerFilter {:on false :kinds []} "The Layers panel's filter: on, and the kinds of layer it shows.")

(defstate foldedGroups [] "The groups folded in the Layers panel.")

(defstate pickedLayers []
  "The layers picked together in the Layers panel (Shift or Cmd and a click): what Distribute spaces.")

(defstate grouping "" "The layer being grouped or ungrouped, while a command moves it.")

(defstate cropSize [0 0] "The size Crop to a Ratio chose.")

(defstate colourTarget foreground "The colour the Colour panel edits: foreground or background.")

(defstate screenMode standard "Standard, with the panels; or full, without them.")

(defstate statusInfo size "What the status bar shows: doc, size, profile or memory.")

(defstate slide 0 "The value a slider was pressed at, that its drag starts from.")

(defstate pop {} "The options' small sliders that are open, by their field.")

(defstate quickPick {:open false :query "" :index 0 :top 0 :recent []} "The command search.")

;;; The dialogs

(defstate lastFilter {} "The filter last applied, with its values: Filter > Last Filter applies it again.")

(defstate filter
  {:open false :kind gaussian-blur :radius 4 :amount 100 :threshold 0 :angle 0 :distance 12 :live true :apply 0
   :cancel 0}
  "The filter dialog: watches preview it while it changes, and apply or cancel it (tempera.lisp).")

(defstate newDoc {:open false :width 1200 :height 800 :format rgba8} "The New dialog.")

(defstate transform {:open false :target layer :x 0 :y 0 :scaleX 100 :scaleY 100 :angle 0}
  "The Free Transform dialog, on the active layer or (target selection) on the selection.")

(defstate resize {:open false :width 0 :height 0 :linked true} "The Image Size dialog.")

(defstate canvasSize {:open false :width 0 :height 0 :anchor center} "The Canvas Size dialog.")

(defstate rename {:open false :name ""} "The Rename Layer dialog.")

(defstate fill {:open false :menu false :contents foreground} "The Fill dialog.")

(defstate closeAsk false "Close asks before it loses the changes.")

(defstate about false "The About dialog is open.")

;;; Menus that are open

(defstate menuBar {} "The menu bar's menus that are open, by their name.")

(defstate flyout {} "The tools bar's groups whose other tools are shown, g and the group's place.")

(defstate newPresetMenu false "The New dialog's sizes are shown.")

(defstate adjustMenu false "The Layers panel's adjustment menu is open.")

(defstate blendMenu false "The blend modes are shown.")

(defstate brushMenu false "The brush presets are shown.")

(defstate curveChannel "rgb" "The channel the Curves editor shows: rgb (the composite), red, green or blue.")

(defstate brushPreset "" "The brush preset last chosen, filled in the Brushes panel.")

(defstate gradientMenu false "The gradient presets are shown.")

(defstate workspaceMenu false "The workspace menu is open.")

(defstate paintModeMenu false "The painting modes are shown.")

(defstate fillSourceMenu false "The fill sources are shown.")

(defstate typeFontMenu false "The Character section's font families are shown.")

(defstate typeFonts [{:id "default" :title "Roboto"} {:id "symbols" :title "JetBrains Mono"}]
  "The font families a type layer can use: the screen's font names, and what the Character section calls them.")

(defstate statusMenu false "The status bar's choices are shown.")

(defstate layerKindMenu false "The Layers panel's filter kinds are shown.")

(defstate screenMenu false "The screen modes are shown.")

(defstate cropMenu false "The crop ratios are shown.")

(defstate cropDragMenu false "The ratios a crop drag can keep are shown.")

(defstate zoomMenu false "The zoom choices are shown.")

;;; Settings

(defcustom appearance.language "en"
  "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "setting.language")
  :description ~(t "setting.language.about"))
