;;; search.lisp --- Haven's search over every control, and over every command after ">"
;;;
;;; E opens it on the controls: each light, each room's climate and blinds, each device and the alarm; C or Cmd+K
;;; open it on the commands. It is Haven's own quick pick, in its look, with the kit's search and keys: Up and Down
;;; move, Enter runs, and the controls last picked come first.

;;; What it searches

(defcomputed entities "Every control: each room's lights, climate and blinds, each device, and the alarm."
  (concat computed.lightEntities
          (map-indexed (fn [index it]
                         {:title (t "info.climate") :detail it.name :icon "thermometer" :run "openEntity"
                          :with {:room index :kind "climate" :i 0}})
                       state.rooms)
          (map-indexed (fn [index it]
                         {:title (t "info.cover") :detail it.name :icon "blinds" :run "openEntity"
                          :with {:room index :kind "cover" :i 0}})
                       state.rooms)
          (map-indexed (fn [index it]
                         {:title it.name :detail (t "info.home") :icon it.icon :run "openEntity"
                          :with {:room nil :kind "device" :i index}})
                       state.devices)
          [{:title (t "alarm.title") :detail (t "info.home") :icon "shield" :run "openEntity"
            :with {:room nil :kind "alarm" :i 0}}]))

(defmacro room-lights
  ;; A macro, so that what it writes stays an expression, worked out at an expression's speed.
  "The lights of the room at place r, as controls of the search."
  [r]
  `(map-indexed (fn [index it]
                  {:title it.name :detail (get-in state.rooms [~r :name]) :icon "lightbulb" :run "openEntity"
                   :with {:room ~r :kind "light" :i index}})
                (get-in state.rooms [~r :lights])))

(defcomputed lightEntities "Every room's lights, a control each."
  (concat (room-lights 0) (room-lights 1) (room-lights 2) (room-lights 3)))

;;; The dialog

(defview search "The search, over a scrim: the commands after \">\", else the controls." []
  [`(node#scrim-app-4 :use scrim :with {:open state.quickPick.open})
   `(node#quickPick :dir auto
      :y ~(max 48 (* window.height 0.12))
      :use quickPick
      :with {:providers [{:prefix ">" :items ~computed.palette :placeholder ~(t "quick.commands")}
                         {:items ~computed.entities :placeholder ~(t "quick.entities") :key title :run nil}]})])

;; It replaces the kit's quick pick, with the same search and keys: a rounded dialog with a filled field (a magnifier
;; and a clear button), then each result's icon, name and area.
(defcomponent quickPick "Haven's quick pick: a field, and the results of the list its prefix names."
  {:name quickPick :providers [] :rows 7 :rowHeight 56 :width 560 :limit 1000 :recentLimit 10 :highlight @accentSoft}
  `(node :overlay {:placement window :open ~(str "state." name ".open") :dismiss outside :modal true}
     :x ~(max 0 (/ (- window.width width) 2))
     :y 48
     :width ~width
     :height ~(+ 104 (* (max 1 (min total rows)) rowHeight))
     :color @card
     :radius 24
     :shadow {:y 8 :blur 24 :color "#00000047"}
     :role dialog
     :label ~provider.placeholder
     :let ~(search-variables name)
     :keys {:down ~(handler moveSearch base
                            (min (+ sel 1) (max 0 (- total 1)))
                            (max top (+ (- (min (+ sel 1) (- total 1)) rows) 1)))
            :up ~(handler moveSearch base (max (- sel 1) 0) (min top (max (- sel 1) 0)))
            :enter ~(handler runResult base (get results sel) home key qp.recent recentLimit provider.run)}
     ~(search-field)
     {:anchors {:left 0 :right 0 :top 79} :height 1 :color @divider}
     (label :when ~(= total 0) :text ~(t "quick.none") :textColor @muted :anchors {:left 24 :top 100} :fitText true)
     (column :anchors {:left 0 :right 0 :top 88}
       :height ~(* (min total rows) rowHeight)
       :onScroll ~(handler scrollSearch base top (- total rows) event.dy)
       ~(repeat [r (slice results top (+ top rows)) :index i]
         (search-result r i)))))

(defview search-variables
  "What the dialog works out from its text: the provider its prefix names, and the results, the recent first."
  [name]
  `{:base ~(str "state." name)
   :qp ~(get state name)
   :q ~(or-else qp.query "")
   :provider ~(or-else (find (fn [it] (and (> (count (or-else it.prefix "")) 0) (= (indexOf q it.prefix) 0))) providers)
                       (find (fn [it] (= (count (or-else it.prefix "")) 0)) providers))
   :search ~(slice q (count (or-else provider.prefix "")))
   :key ~(or-else provider.key "title")
   :home ~(= (count (or-else provider.prefix "")) 0)
   :recent ~(if home (or-else qp.recent []) [])
   :recentKeys ~(map (fn [it] (get it key)) recent)
   :recentHits ~(fuzzySort recent search key)
   :results ~(slice (concat recentHits
                            (filter (fn [it] (not (contains recentKeys (get it.item key))))
                                    (fuzzySort provider.items search key (+ limit (count recent)))))
                    0
                    limit)
   :total ~(count results)
   :sel ~(clamp (or-else qp.index 0) 0 (max 0 (- total 1)))
   :top ~(clamp (or-else qp.top 0) 0 (max 0 (- total rows)))})

(defview search-field "The search's field: a magnifier, the text, and a button that clears it." []
  `(node :anchors {:left 16 :right 16 :top 16} :height 48 :radius {:topLeft 4 :topRight 4} :color @raised
     (label :font icons :icon search :text "" :textColor @muted :anchors {:left 10 :top 12} :width 24 :height 24
       :align center)
     (textarea :bind ~(str base ".query")
       :autofocus true
       :placeholder ~(or-else provider.placeholder "")
       :label ~(or-else provider.placeholder "")
       :anchors {:left 40 :right 44 :top 8}
       :height 32
       :padding 6
       :color transparent
       :textColor @text
       :placeholderColor @muted
       :onChange ~(handler moveSearch base 0 0))
     (label :when ~(not= q "")
       :font icons
       :icon x
       :text ""
       :textColor @muted
       :anchors {:right 6 :top 6}
       :width 36
       :height 36
       :radius 18
       :align center
       :hover {:color @cardHover}
       :cursor hand
       :role button
       :label ~(t "quick.clear")
       :tooltip ~(t "quick.clear")
       :onClick ~(handler clearSearch base))
     {:anchors {:left 0 :right 0 :bottom 0} :height 1 :color @accent}))

(defview search-result "A result: its icon, its name with the letters that match, and its area, or Recent." [r i]
  `(node :anchors {:left 0 :right 0}
     :height ~rowHeight
     :cursor hand
     :role option
     :label ~r.item.title
     :selected ~(= (+ top i) sel)
     :color ~(if (= (+ top i) sel) highlight "@card")
     :hover {:color ~(if (= (+ top i) sel) highlight "@cardHover")}
     :onClick ~(handler pickResult base (+ top i) r home key qp.recent recentLimit provider.run)
     (label :font icons
       :icon ~(or-else r.item.icon "circle")
       :text ""
       :textColor @muted
       :color @raised
       :anchors {:left 24 :top ~(/ (- rowHeight 40) 2)}
       :width 40
       :height 40
       :radius 20
       :align center)
     (row :anchors {:left 80 :right 24 :top ~(- (/ rowHeight 2) 19)} :height 18
       ~(repeat [p r.parts]
         `(label :text ~p.text :textColor ~(if p.match "@accent" "@text") :fitText true)))
     (label :font small
       :text ~(if (< (+ top i) (count recentHits)) (t "quick.recent") (or-else r.item.detail ""))
       :textColor @muted
       :anchors {:left 80 :top ~(+ (/ rowHeight 2) 1)}
       :fitText true)))

;;; What it does

(defn moveSearch "Highlights a result, and scrolls the list from top." [base index top]
  (set (str base ".top") top)
  (set (str base ".index") index))

(defn clearSearch "Empties the search's text." [base]
  (set (str base ".query") "")
  (moveSearch base 0 0))

(defn scrollSearch "Scrolls the results a row up or down, within the list." [base top last dy]
  (set (str base ".top") (clamp (+ top (cond (< dy 0) 1 (> dy 0) -1 :else 0)) 0 (max 0 last))))

;; The search closes, keeps the result among the recent ones on its home list, and runs its action or the list's.
(defn runResult "Runs a result of the search." [base pick home key recent limit listRun]
  (when (some? pick)
    (let [item (:item pick)]
      (set (str base ".open") false)
      (when home
        (set (str base ".recent")
             (take limit (concat [item] (remove (fn [it] (= (get it key) (get item key))) (or recent []))))))
      (clearSearch base)
      (cond (some? (:run item)) (run (:run item) :with (:with item))
            (some? listRun) (run listRun :with {:item item})))))

(defn pickResult "A click on a result: it is highlighted, then run." [base index pick home key recent limit listRun]
  (set (str base ".index") index)
  (runResult base pick home key recent limit listRun))
