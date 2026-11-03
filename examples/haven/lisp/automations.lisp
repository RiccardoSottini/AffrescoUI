;;; automations.lisp --- Haven's automations: rules as a list and an editor, as a node graph, and the day's schedules
;;;
;;; The list tab shows every rule, its last run and a menu; opening one shows its parts as cards (when, and if, then
;;; do) beside the trace of its last run. The flow tab shows the rules as a node graph, and the schedules of the day
;;; on a timeline whose clips move and stretch by the hour.

;;; What the page keeps

(defstate autoTab list "The tab shown: the list, or the flow.")

(defstate autoOpen -1 "The automation open in the editor, by its place, or -1 for the list.")

(defstate autoQuery "" "The text the list is filtered by.")

(defstate autoMenu false "Whether the editor's menu is open.")

(defstate autoConfirm false "Whether the question before a delete is open.")

(defstate autoTarget -1 "The automation the question is about, by its place.")

(defstate autoRowMenu {} "Which of the list's row menus is open, by the automation's id.")

(defstate stepMenu {} "Which of the editor's step menus is open, by s and the step's place.")

(defstate automations
  [{:id welcome
    :icon door-open
    :on true
    :ran min7
    :steps [{:part when :icon door-open :key frontOpens}
            {:part if :icon sunset :key afterDark}
            {:part do :icon lightbulb :key hallOn}
            {:part do :icon music :key playMusic}]}
   {:id goodNight
    :icon moon
    :on true
    :ran h9
    :steps [{:part when :icon clock-3 :key time2300}
            {:part if :icon users :key allHome}
            {:part do :icon lightbulb-off :key lightsOff}
            {:part do :icon lock :key lockDoors}
            {:part do :icon thermometer :key heatNight}]}
   {:id motionAway
    :icon siren
    :on true
    :ran d2
    :steps [{:part when :icon activity :key hallMotion}
            {:part if :icon shield :key alarmAway}
            {:part do :icon bell :key sendNote}
            {:part do :icon lightbulb :key hallOn}]}
   {:id morning
    :icon sunrise
    :on false
    :ran never
    :steps [{:part when :icon clock-3 :key time0630}
            {:part do :icon blinds :key blindsOpen}
            {:part do :icon thermometer :key heatDay}]}]
  "The automations: each one's name (its id, or a title), icon, last run, and steps."
  :type (array :items (object :properties {:id (string) :title (string) :icon (string) :on (boolean) :ran (string)
                                           :steps (array)})))

(defstate autoPresets
  {:when {:part when :icon clock-3 :key time0700}
   :if {:part if :icon user-check :key someoneHome}
   :do {:part do :icon bell :key sendNote}}
  "The step each part's add button adds.")

(defstate autoSelected [] "The flow's nodes chosen, by their ids.")

(defstate autoNodes
  [{:id motion :kind motion :x 20 :y 30 :outputs [{:id when :kind event}]}
   {:id sunset :kind sunset :x 20 :y 190 :outputs [{:id when :kind event}]}
   {:id armed :kind armed :x 230 :y 30 :inputs [{:id if :kind event}] :outputs [{:id then :kind event}]}
   {:id hallLight :kind hallLight :x 440 :y 10 :inputs [{:id do :kind event}]}
   {:id notify :kind notify :x 440 :y 100 :inputs [{:id do :kind event}]}
   {:id blindsDown :kind blindsDown :x 440 :y 190 :inputs [{:id do :kind event}]}]
  "The flow's nodes: what starts a rule, what it checks and what it does, each with its ports.")

(defstate autoLinks
  [{:from motion :output when :to armed :input if}
   {:from armed :output then :to notify :input do}
   {:from armed :output then :to hallLight :input do}
   {:from sunset :output when :to blindsDown :input do}]
  "The flow's links, from a node's output to another's input.")

(defstate schedules
  [{:id heating
    :clips [{:id heatMorning :key morning :start 6 :duration 3} {:id heatEvening :key evening :start 17 :duration 6}]}
   {:id lights :clips [{:id lightsNight :key night :start 19 :duration 4}]}
   {:id vacuum :clips [{:id vacuumClean :key clean :start 10 :duration 2}]}]
  "The day's schedules: a track each, its clips by the hour.")

;;; What the page works out

(defmacro rule-name
  ;; A macro, so that what it writes stays an expression of the binding that uses it.
  "An automation's name, in the language shown: its title's, or its id's."
  [rule]
  `(t (str "rule." (or-else (:title ~rule) (:id ~rule)))))

(defmacro matches-search "Whether an automation's name holds the list's search, or there is none." [rule]
  `(or (= state.autoQuery "") (contains (lower-case (rule-name ~rule)) (lower-case (trim state.autoQuery)))))

(defcomputed openAuto "The automation open in the editor." (get state.automations (max state.autoOpen 0)))

(defcomputed autoOnCount "How many automations are on." (count (filter (fn [it] it.on) state.automations)))

(defcomputed autoShown "How many automations the list shows."
  (count (filter (fn [it] (matches-search it)) state.automations)))

(defcomputed autoGraph "The flow's nodes, named in the language shown, as the graph draws them."
  (map (fn [it]
         (merge it
                {:title (t (str "auto." it.kind))
                 :width 170
                 :inputs (map (fn [it] (merge it {:label (t (str "port." it.id))})) (or-else it.inputs []))
                 :outputs (map (fn [it] (merge it {:label (t (str "port." it.id))})) (or-else it.outputs []))}))
       state.autoNodes))

(defcomputed scheduleTracks "The schedules, named in the language shown, as the timeline draws them."
  (map (fn [it]
         {:id it.id
          :label (t (str "schedule." it.id))
          :clips (map (fn [it] {:id it.id :label (t (str "schedule." it.key)) :start it.start
                                :duration it.duration}) it.clips)})
       state.schedules))

;;; The page

;; The list tab: the automations, and an editor of cards (when, and if, then do) beside the last run. The flow tab:
;; the rules as a node graph, and the schedules of the day on a timeline.
(defcomponent automationsPage "The automations: the list and its editor, or the flow and the schedules."
  {}
  `(node :color transparent
     {:id automationsBar :anchors {:left 0 :right 0 :top 0} :height 56 :color @header :role banner}
     (label#automationsTitle :when ~(or (< state.autoOpen 0) (= state.autoTab "flow"))
       :font heading
       :text ~(t "page.automations")
       :textColor @onHeader
       :anchors {:left 24 :right 24 :top 0}
       :height 56
       :align {:horizontal start :vertical center})
     ~(page-tabs)
     ~(list-search)
     ~(new-button)
     (node :use scrim :with {:open state.autoConfirm})
     ~(delete-question)
     ~(rule-list)
     ~(rule-editor)
     {:id automationsSummary :when ~(= state.autoTab "flow") :style caption :text ~(t "auto.summary")
      :anchors {:left 308 :top 73}}
     ~(add-node-buttons)
     (node#autoCard :when ~(= state.autoTab "flow")
       :style card
       :anchors {:left 32 :right 32 :top 104 :bottom "#scheduleCard.top - 16"}
       ~(rule-graph))
     ~(schedule-card)))

(defview page-tabs "The two tabs: the list and the flow." []
  `(row#autoTabs :when ~(or (< state.autoOpen 0) (= state.autoTab "flow"))
     :anchors {:left 32 :top 68}
     :width 260
     :height 28
     :gap 4
     :role toolbar
     :arrows horizontal
     ~(page-tab "autoTabList" "list" "list" "auto.tabList")
     ~(page-tab "autoTabFlow" "flow" "git-fork" "auto.tabFlow")))

(defview page-tab "A tab of the page, filled while it is the one shown." [id tab icon key]
  `{:id ~id
    :style ~(if (= state.autoTab tab) "choiceOn" "choice")
    :font small
    :icon ~icon
    :text ~(t key)
    :grow 1
    :basis 0
    :anchors {:top 0 :bottom 0}
    :selected ~(= state.autoTab tab)
    :onClick ~(handler showAutomationTab tab)})

(defview list-search "The search over the list's names." []
  `(node#autoSearchBox :when ~(and (= state.autoTab "list") (< state.autoOpen 0))
     :anchors {:left 308 :top 64}
     :width 300
     :height 36
     :radius 18
     :color @field
     (label :icon search :text "" :textColor @muted :anchors {:left 10 :top 0 :bottom 0} :width 24 :align center)
     (textarea#autoSearch :bind state.autoQuery
       :placeholder ~(t "auto.search")
       :placeholderColor @muted
       :textColor @text
       :color transparent
       :anchors {:left 38 :right 12 :top 0 :bottom 0}
       :label ~(t "auto.search"))))

(defview new-button "The floating button that makes a new automation." []
  `{:id autoNew
    :when ~(and (= state.autoTab "list") (< state.autoOpen 0))
    :style button
    :icon plus
    :text ~(t "auto.new")
    :color @accent
    :hover {:color @accent}
    :textColor @onAccent
    :anchors {:right 32 :bottom 32}
    :width 208
    :height 52
    :radius 16
    :shadow {:y 4 :blur 12 :color "#00000040"}
    :onClick newAutomation})

(defview delete-question "The question before an automation is deleted." []
  `(node#autoConfirm :dir auto
     :overlay {:placement center :open state.autoConfirm :dismiss outside :modal true}
     :width 400
     :height 156
     :radius 28
     :color @card
     :shadow {:y 8 :blur 24 :color "#00000040"}
     :role dialog
     :label ~(t "auto.deleteTitle")
     (label :font heading :textColor @text :text ~(t "auto.deleteTitle") :anchors {:left 24 :right 24 :top 24}
       :height 24)
     (label :wrap true
       :textColor @muted
       :text ~(t "auto.deleteBody" {:name (rule-name (get state.automations state.autoTarget))})
       :anchors {:left 24 :right 24 :top 60 :bottom 60})
     {:style button
      :color transparent
      :textColor @accent
      :hover {:color @accentSoft}
      :text ~(t "auto.cancel")
      :anchors {:right 112 :bottom 16}
      :width 88
      :height 36
      :radius 18
      :onClick {:set state.autoConfirm :to false}}
     {:style button
      :color transparent
      :textColor @alert
      :hover {:color @alertSoft}
      :text ~(t "auto.delete")
      :anchors {:right 16 :bottom 16}
      :width 88
      :height 36
      :radius 18
      :onClick deleteAutomation}))

;;; The list

(defview rule-list "The automations, a row each, under the columns' names." []
  `(node#autoList :when ~(and (= state.autoTab "list") (< state.autoOpen 0))
     :style card
     :anchors {:left 32 :right 32 :top 112}
     :height ~(min (+ (* (max 1 computed.autoShown) 64) 56) (if computed.compact 420 520))
     {:style caption :text ~(t "auto.colName") :anchors {:left 88 :top 14}}
     {:style caption
      :text ~(t "auto.colRan")
      :anchors {:right 128 :top 14}
      :width 180
      :height 16
      :fitText false
      :align {:horizontal start :vertical center}}
     {:anchors {:left 0 :right 0 :top 44} :height 1 :color @border}
     {:style caption :when ~(= computed.autoShown 0) :text ~(t "auto.noMatch") :anchors {:left 88 :top 70}}
     (column :anchors {:left 0 :right 0 :top 48 :bottom 6} :scroll vertical :role list :label ~(t "page.automations")
       ~(repeat [auto state.automations :index i :key auto.id]
         `(node#autoRow :anchors {:left 0 :right 0}
            :when ~(matches-search auto)
            :height 64
            :color transparent
            :role listitem
            ~(rule-row auto i)
            ~(rule-menu auto i)
            {:when ~(< i (- (count state.automations) 1)) :anchors {:left 24 :right 24 :bottom 0} :height 1
             :color @border})))))

(defview rule-row "A row of the list: the automation's icon, name and last run; it opens the editor." [auto i]
  `(node#autoRowOpen :anchors {:left 8 :right 8 :top 4 :bottom 4}
     :radius 8
     :color transparent
     :hover {:color @raised}
     :cursor hand
     :focusable true
     :role button
     :label ~(rule-name auto)
     :onClick ~(handler openAutomation i)
     (label :icon ~auto.icon
       :text ""
       :anchors {:left 12 :top 8}
       :width 40
       :height 40
       :radius 20
       :align center
       :color ~(if auto.on "@accentSoft" "@raised")
       :textColor ~(if auto.on "@accent" "@muted"))
     (label :text ~(rule-name auto)
       :textColor ~(if auto.on "@text" "@muted")
       :anchors {:left 68 :right 320 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label :font small
       :text ~(t (str "ran." auto.ran))
       :textColor @muted
       :anchors {:right 120 :top 0 :bottom 0}
       :width 180
       :align {:horizontal start :vertical center})))

(defview rule-menu "A row's menu: run, duplicate, enable or disable, delete." [auto i]
  `(node#autoRowMore :style button
     :icon ellipsis-vertical
     :text ""
     :color transparent
     :hover {:color @raised}
     :textColor @muted
     :anchors {:right 16 :top 12}
     :width 40
     :height 40
     :radius 20
     :label ~(str (t "auto.more") ": " (rule-name auto))
     :tooltip ~(t "auto.more")
     :isOpen ~(= (get state.autoRowMenu auto.id) true)
     :onClick ~(handler toggleRowMenu auto.id)
     (node :use popMenu
       :with {:open ~(str "state.autoRowMenu." auto.id)
              :width 200
              :items [{:title ~(t "auto.run") :icon play :run rowAction :with {:i ~i :what run}}
                      {:title ~(t "auto.duplicate") :icon copy :run rowAction :with {:i ~i :what duplicate}}
                      {:title ~(t (if auto.on "auto.disable" "auto.enable"))
                       :icon ~(if auto.on "circle-off" "check-circle")
                       :run rowAction
                       :with {:i ~i :what enable}}
                      {:separator true}
                      {:title ~(t "auto.delete") :icon trash-2 :run rowAction :with {:i ~i :what delete}}]})))

;;; The editor

(defview rule-editor "The automation open: its bar and menu, a banner while it is off, its parts, and its last run." []
  `(node#autoEditor :when ~(and (= state.autoTab "list") (>= state.autoOpen 0))
     :anchors {:left 0 :right 0 :top 0 :bottom 0}
     :color transparent
     {:id autoBack
      :style button
      :icon arrow-left
      :mirror true
      :text ""
      :color transparent
      :hover {:color @headerHover}
      :textColor @onHeader
      :anchors {:left 12 :top 8}
      :width 40
      :height 40
      :radius 20
      :label ~(t "auto.back")
      :tooltip ~(t "auto.back")
      :onClick ~(handler openAutomation -1)}
     (label#autoName :font heading
       :icon ~computed.openAuto.icon
       :text ~(rule-name computed.openAuto)
       :textColor @onHeader
       :anchors {:left 64 :right 72 :top 0}
       :height 56
       :align {:horizontal start :vertical center})
     ~(editor-menu)
     ~(editor-menu-button)
     ~(off-banner)
     ~(rule-parts)
     ~(rule-trace)))

(defview editor-menu-button "The button of the editor's menu." []
  `{:id autoMore
    :style button
    :icon ellipsis-vertical
    :text ""
    :color transparent
    :hover {:color @headerHover}
    :textColor @onHeader
    :anchors {:right 16 :top 8}
    :width 40
    :height 40
    :radius 20
    :label ~(t "auto.more")
    :tooltip ~(t "auto.more")
    :isOpen ~state.autoMenu
    :onClick {:toggle state.autoMenu}})

(defview editor-menu "The editor's menu: run now, enable or disable, duplicate, delete." []
  `(node#autoMoreAnchor :anchors {:right 16 :top 8} :width 40 :height 40 :color transparent
     (node#autoMenu :use popMenu
       :with {:open state.autoMenu
              :width 200
              :items [{:title ~(t "auto.run") :icon play :run runAutomation :with nil}
                      {:title ~(t (if computed.openAuto.on "auto.disable" "auto.enable"))
                       :icon ~(if computed.openAuto.on "circle-off" "check-circle")
                       :run rowAction
                       :with {:i ~state.autoOpen :what enable}}
                      {:title ~(t "auto.duplicate") :icon copy :run duplicateAutomation}
                      {:separator true}
                      {:title ~(t "auto.delete") :icon trash-2 :run confirmDelete}]})))

(defview off-banner "While the automation is off, a banner that says so and turns it on." []
  `(node#autoOffBanner :when ~(not computed.openAuto.on)
     :anchors {:left 32 :right ~(if computed.compact 32 364) :top 80}
     :height 48
     :radius 8
     :color @warmSoft
     :role status
     (label :icon info
       :text ~(t "auto.offBanner")
       :textColor @text
       :anchors {:left 16 :right 120 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     {:id autoOffEnable
      :style button
      :text ~(t "auto.enable")
      :color transparent
      :hover {:color @accentSoft}
      :textColor @accent
      :anchors {:right 8 :top 6}
      :padding 12
      :fitText true
      :height 36
      :radius 18
      :onClick ~(handler rowAction state.autoOpen "enable")}))

(defview rule-parts "The automation's three parts, a card each: when, and if, then do." []
  `(column#autoParts :anchors {:left 32 :right ~(if computed.compact 32 364) :top ~(if computed.openAuto.on 88 144)
                               :bottom 0}
     :gap 12
     :scroll vertical
     (node :use autoSection :with {:part when :title ~(t "auto.when") :hint ~(t "auto.whenHint")
                                   :addLabel ~(t "auto.addWhen")})
     (node :use autoSection :with {:part if :title ~(t "auto.if") :hint ~(t "auto.ifHint") :addLabel ~(t "auto.addIf")})
     (node :use autoSection :with {:part do :title ~(t "auto.then") :hint ~(t "auto.thenHint")
                                   :addLabel ~(t "auto.addDo")})
     {:anchors {:left 0 :right 0} :height 12 :color transparent}))

(defmacro part-steps "The open automation's steps of a part." [part]
  `(filter (fn [it] (= it.part ~part)) computed.openAuto.steps))

(defcomponent autoSection "A part of the automation editor as a card: its steps, each with its menu, and an add button."
  {:part when :title "" :hint "" :addLabel ""}
  `(node :style card
     :anchors {:left 0 :right 12}
     :height ~(+ (* (max 1 (count (part-steps part))) 54) 116)
     :role group
     :label ~title
     {:style cardTitle :text ~title :anchors {:left 20 :top 16}}
     {:style caption :text ~hint :anchors {:left 20 :top 42}}
     {:style caption :when ~(= (count (part-steps part)) 0) :text ~(t "auto.empty") :anchors {:left 24 :top 80}}
     (column :anchors {:left 12 :right 12 :top 68} :height ~(* (count (part-steps part)) 54) :gap 6 :role list
       :label ~title
       ~(repeat [step computed.openAuto.steps :index s]
         (part-step part step s)))
     {:id ~(str "autoAdd-" part)
      :style button
      :font small
      :icon plus
      :text ~addLabel
      :color transparent
      :textColor @accent
      :borderWidth 1
      :borderColor @border
      :anchors {:left 20 :bottom 10}
      :width 196
      :height 36
      :radius 18
      :onClick ~(handler addStep part)}))

(defview part-step "A step of a part: its icon and words, and its menu." [part step s]
  `(node#autoStep :when ~(= step.part part)
     :anchors {:left 0 :right 0}
     :height 48
     :radius 12
     :color @card
     :borderWidth 1
     :borderColor @border
     :role listitem
     (label :icon ~step.icon :text "" :textColor @accent :anchors {:left 16 :top 0 :bottom 0} :width 24 :align center)
     (label :text ~(t (str "rule." step.key))
       :textColor @text
       :anchors {:left 48 :right 96 :top 0 :bottom 0}
       :align {:horizontal start :vertical center})
     (label :icon chevron-down :text "" :textColor @muted :anchors {:right 56 :top 12} :width 24 :height 24
       :align center)
     ~(step-menu step s)))

(defview step-menu "A step's menu: duplicate, delete." [step s]
  `(node#autoStepMore :style button
     :icon ellipsis-vertical
     :text ""
     :color transparent
     :hover {:color @raised}
     :textColor @muted
     :anchors {:right 8 :top 4}
     :width 40
     :height 40
     :radius 20
     :label ~(str (t "auto.more") ": " (t (str "rule." step.key)))
     :tooltip ~(t "auto.more")
     :isOpen ~(= (get state.stepMenu (str "s" s)) true)
     :onClick ~(handler toggleStepMenu s)
     (node :use popMenu
       :with {:open ~(str "state.stepMenu.s" s)
              :width 200
              :items [{:title ~(t "auto.duplicate") :icon copy :run stepAction :with {:s ~s :what duplicate}}
                      {:separator true}
                      {:title ~(t "auto.delete") :icon trash-2 :run stepAction :with {:s ~s :what delete}}]})))

(defview rule-trace "The automation's last run, step by step." []
  `(node#autoTrace :when ~(not computed.compact)
     :style card
     :anchors {:right 32 :top 88 :bottom 24}
     :width 316
     :role group
     :label ~(t "auto.trace")
     {:style cardTitle :icon history :text ~(t "auto.trace") :anchors {:left 20 :top 16}}
     {:style caption
      :text ~(if (= computed.openAuto.ran "never") (t "auto.traceNever") (t (str "ran." computed.openAuto.ran)))
      :anchors {:left 20 :top 44}}
     (column :when ~(not= computed.openAuto.ran "never")
       :anchors {:left 20 :right 16 :top 78 :bottom 12}
       :scroll vertical
       :role list
       :label ~(t "auto.trace")
       ~(repeat [step computed.openAuto.steps :index s]
         (trace-step step s)))))

(defview trace-step "A step of the last run: a tick, its words, its part and when it ran." [step s]
  `(node :anchors {:left 0 :right 0} :height 52 :color transparent :role listitem
     {:when ~(< s (- (count computed.openAuto.steps) 1)) :anchors {:left 11 :top 26 :bottom 0} :width 2 :color @border}
     (label :icon check
       :text ""
       :font small
       :anchors {:left 0 :top 0}
       :width 24
       :height 24
       :radius 12
       :align center
       :color @accentSoft
       :textColor @accent)
     (label :font small
       :text ~(t (str "rule." step.key))
       :textColor @text
       :anchors {:left 36 :right 0 :top 3}
       :height 18
       :align {:horizontal start :vertical center})
     {:style caption
      :text ~(t "trace.at" {:part (t (str "trace." step.part)) :seconds (fixed (* s 0.12) 2)})
      :anchors {:left 36 :top 25}}))

;;; The flow and the schedules

(defview add-node-buttons "The buttons that add a node to the flow: a time, a check of the dark, a scene." []
  `(row#autoAdd :when ~(= state.autoTab "flow")
     :anchors {:right 32 :top 28}
     :width 330
     :height 30
     :gap 6
     :role toolbar
     :arrows horizontal
     {:id autoAddLabel :style caption :text ~(t "auto.add") :anchors {:top 8}}
     ~(add-node-button "autoAddTime" "clock-3" "auto.addTime" {:kind "time" :outputs [{:id "when" :kind "event"}]})
     ~(add-node-button "autoAddDark" "moon" "auto.addDark"
                       {:kind "dark" :inputs [{:id "if" :kind "event"}] :outputs [{:id "then" :kind "event"}]})
     ~(add-node-button "autoAddScene" "sparkles" "auto.addScene" {:kind "scene" :inputs [{:id "do" :kind "event"}]})))

(defview add-node-button "A button that adds a node to the flow." [id icon key node]
  `{:id ~id
    :style button
    :font small
    :icon ~icon
    :text ~(t key)
    :grow 1
    :anchors {:top 0 :bottom 0}
    :onClick ~(handler addAutoNode node)})

(defview rule-graph "The rules as a node graph: links drawn between ports, nodes moved and deleted." []
  `(graph#autoGraph :fill 8
     :focusable true
     :label ~(t "auto.graph")
     :nodes ~computed.autoGraph
     :links ~state.autoLinks
     :color @field
     :minimap ~(not computed.compact)
     :nodeColor @raised
     :headerColor @raisedHover
     :textColor @text
     :accentColor @accent
     :kindColors {:event @warm}
     :onConnect linkNodes
     :onDisconnect unlinkNodes
     :onMove moveNodes
     :onSelect selectNodes
     :onDelete deleteNodes))

(defview schedule-card "The day's schedules on a timeline: a track each, the hours across." []
  `(node#scheduleCard :when ~(= state.autoTab "flow")
     :style card
     :anchors {:left 32 :right 32 :bottom 24}
     :height ~(if computed.compact 156 196)
     {:id scheduleTitle
      :style caption
      :icon calendar-clock
      :text ~(t "schedule.title")
      :anchors {:left 16 :top 12}}
     (timeline#schedule :anchors {:left 8 :right 8 :top 34 :bottom 8}
       :tracks ~computed.scheduleTracks
       :duration 24
       :fps 1
       :pixelsPerSecond ~(max 16 (/ (- (- (- (- window.width computed.sideWidth) 64) 16) 140) 24))
       :offset 0
       :time 14
       :headerWidth 140
       :trackHeight ~(if computed.compact 30 40)
       :label ~(t "schedule.title")
       :color @card
       :clipColor @accent
       :headerColor @field
       :textColor @text
       :accentColor @accent
       :onClipChange changeSchedule
       ~(repeat [track computed.scheduleTracks]
         `(label#scheduleHeader :font small
            :text ~track.label
            :textColor @text
            :fill 0
            :padding 10
            :align {:horizontal start :vertical center})))))
