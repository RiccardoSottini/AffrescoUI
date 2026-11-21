;;; bodies.lisp --- where the bodies are: each round its parent, its parent round its own
;;;
;;; A body's place is its parent's place plus its own on its orbit, at the day shown; a chain of parents is at most
;;; three deep (a moon's moon). The browser's tree and the orbit graph read the same list.

(ns orrery.bodies)

;;; Each body's orbit

(defcomputed ids "Every body's id, in order." (map :id state.bodies))

(defcomputed local "Each body with how far round its orbit it is now (f, 0 to 1) and the place of its parent (pi)."
  (map (fn [it]
         (merge it
                {:f (if (= it.parent "")
                      0
                      (mod (+ it.phase (/ state.time it.period)) 1))
                 :pi (indexOf computed.ids it.parent)}))
       state.bodies))

(defcomputed rel "Each body's place relative to its parent (lx, lz)."
  (map #(merge % {:lx (->> (:f %) (* 2 (pi)) cos (* (:radius %)))
                  :lz (->> (:f %) (* 2 (pi)) sin (* (-> % :radius -)))})
       computed.local))

(defcomputed up1 "Each body's parent's place, or -1." (map :pi computed.local))

(defcomputed up2 "Each body's grandparent's place, or -1."
  (map #(if (>= % 0) (get computed.up1 %) (- 1)) computed.up1))

(defcomputed up3 "Each body's great-grandparent's place, or -1."
  (map #(if (>= % 0) (get computed.up1 %) (- 1)) computed.up2))

(defcomputed depth "How many bodies up each body's chain of parents goes (0 for the Sun)."
  (map-indexed (fn [index it]
                 (cond (< it 0) 0 (< (get computed.up2 index) 0) 1 (< (get computed.up3 index) 0) 2 :else 3))
               computed.up1))

(defcomputed below "How many levels of satellites each body has under it."
  (map-indexed (fn [index it]
                 (cond (>= (indexOf computed.up3 index) 0) 3
                       (>= (indexOf computed.up2 index) 0) 2
                       (>= (indexOf computed.up1 index) 0) 1
                       :else 0))
               computed.up1))

(defcomputed positions
  "Each body with the place of the centre it goes round (cx, cz), its parents' places added up."
  (map-indexed (fn [index it]
                 (merge it
                        {:cx (+ (if (>= it.pi 0) (get-in computed.rel [it.pi :lx]) 0)
                                (if (>= (get computed.up2 index) 0)
                                  (get-in computed.rel [(get computed.up2 index) :lx])
                                  0)
                                (if (>= (get computed.up3 index) 0)
                                  (get-in computed.rel [(get computed.up3 index) :lx])
                                  0))
                         :cz (+ (if (>= it.pi 0) (get-in computed.rel [it.pi :lz]) 0)
                                (if (>= (get computed.up2 index) 0)
                                  (get-in computed.rel [(get computed.up2 index) :lz])
                                  0)
                                (if (>= (get computed.up3 index) 0)
                                  (get-in computed.rel [(get computed.up3 index) :lz])
                                  0))}))
               computed.rel))

(defcomputed placed "Each body where it is now (x, z)."
  (map #(merge % {:x (+ (:lx %) (:cx %)) :z (+ (:lz %) (:cz %))}) computed.positions))

;;; The selected and the followed

(defcomputed selectedBody "The selected body." (find #(= (:id %) state.selected) state.bodies))

(defcomputed selectedPosition "Where the selected body is now."
  (find #(= (:id %) state.selected) computed.placed))

(defcomputed followed "Where the body the camera follows is now."
  (find #(= (:id %) state.follow) computed.placed))

(defcomputed flyPosition "Where the body the camera is flying to is now."
  (if (not= state.flying "") (find #(= (:id %) state.flying) computed.placed) nil))

(defcomputed catalogue "Every body for the features: id, name, parent and its thumbnail."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:id it.id :name it.name :parent it.parent :thumbnail (str "body:" (or-else it.look it.id))})
       state.bodies))

;;; The tour

(defn view-image "The thumbnail of the body a saved view looks at: the one it follows, else the Sun." [place]
  (let [follow (or-else (:follow place) "")
        id (if (empty? follow) "sun" follow)
        body (find #(= (:id %) id) state.bodies)]
    (str "body:" (or-else (:look body) id))))

(defcomputed tourViews "The saved views for the tour strip: id, name and the thumbnail of the body each looks at."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:id it.id :name it.label :image (view-image it)}) state.bookmarks))

;;; The search

(defcomputed searchBodies "The bodies, for the search: a row flies to one."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:title it.name :detail (t "search.body") :icon "circle" :run "find-body" :with {:id it.id}})
       state.bodies))

(defcomputed searchEvents "The events, for the search: a row goes to its day."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:title it.label :detail (t "search.event" {:day (fixed it.time 1)}) :icon "calendar" :run "find-event"
                 :with {:id it.id}})
       computed.events))

(defcomputed searchDay "The day the search names after @, a date (2030-06-15) or a day's number, if in the span."
  (let [text (str/trim (slice state.quickPick.query 1))
        since (- (dateMs text) (dateMs "2030-01-01"))
        day (if (isDate text) (/ since 86400000) (number text))]
    (if (and (not (empty? text)) (some? day) (>= day 0) (<= day state.duration))
      [{:title (t "search.goToDay" {:day (fixed day 1)}) :icon "calendar" :run "find-day" :with {:day day}}]
      [])))

;;; The browser's tree

(defcomputed bodyCells "Each body with its depth, as the browser's rows."
  (map-indexed (fn [index it]
                 (if (= (get-in state.bodies [(mod index (count state.bodies)) :parent])
                        (get-in state.bodies [(floor (/ index (count state.bodies))) :id]))
                   (mod index (count state.bodies))
                   (- 1)))
               (range (* (count state.bodies) (count state.bodies)))))

(defcomputed bodyKids "Each body's satellites, as the browser's rows."
  (map-indexed (fn [index it]
                 (filter #(>= % 0)
                         (slice computed.bodyCells
                                (* index (count state.bodies))
                                (* (inc index) (count state.bodies)))))
               state.bodies))

(defcomputed bodyRows "The browser's rows, a body and its satellites."
  (map (fn [it]
         {:id it.id
          :label it.name
          :icon (case it.parent "" "sun" "sun" "circle" "moon")
          :iconColor it.hex})
       state.bodies))

(defcomputed bodyLevel1 "The browser's tree, its deepest level."
  (map-indexed (fn [index it]
                 (if (> (count (get computed.bodyKids index)) 0)
                   (merge it {:children (map #(get computed.bodyRows %) (get computed.bodyKids index))})
                   it))
               computed.bodyRows))

(defcomputed bodyLevel2 "The browser's tree, its next level up."
  (map-indexed (fn [index it]
                 (if (> (count (get computed.bodyKids index)) 0)
                   (merge it {:children (map #(get computed.bodyLevel1 %) (get computed.bodyKids index))})
                   it))
               computed.bodyLevel1))

(defcomputed bodyLevel3 "The browser's tree, the planets' level."
  (map-indexed (fn [index it]
                 (if (> (count (get computed.bodyKids index)) 0)
                   (merge it {:children (map #(get computed.bodyLevel2 %) (get computed.bodyKids index))})
                   it))
               computed.bodyLevel2))

(defcomputed bodyTree "The bodies as a tree, the Sun at its root."
  (map #(get computed.bodyLevel3 %)
       (filter #(= (get-in state.bodies [% :parent]) "") (range (count state.bodies)))))

(defcomputed bookmarkRows "The bookmarks, as the browser's list."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:id it.id :label it.label :icon "bookmark"}) state.bookmarks))

;;; The orbit graph

(defcomputed graphNodes "The bodies as the orbit graph's nodes, each with its orbit in and its satellites out."
  (map-indexed (fn [index it]
                 {:id it.id
                  :title it.name
                  :x (or-else it.gx 400)
                  :y (or-else it.gy (+ 20 (* index 40)))
                  :width 120
                  :inputs (if (= it.parent "") [] [{:id "orbits" :label "orbits" :kind "orbit" :links 1}])
                  :outputs [{:id "satellites" :label "satellites" :kind "orbit"}]})
               state.bodies))

(defcomputed graphLinks "What orbits what, as the graph's links."
  ; style: keep, #(...) can't give a map
  (map (fn [it] {:from it.parent :output "satellites" :to it.id :input "orbits"})
       (filter #(not= (:parent %) "") state.bodies)))
