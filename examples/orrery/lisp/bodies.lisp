;;; bodies.lisp --- where the bodies are: each round its parent, its parent round its own
;;;
;;; A body's place is its parent's place plus its own on its orbit, at the day shown; a chain of parents is at most
;;; three deep (a moon's moon). The browser's tree and the orbit graph read the same list.

;;; Each body's orbit

(defcomputed ids "Every body's id, in order."
  (map (fn [it] it.id) state.bodies))

(defcomputed local
  (map (fn [it]
           (merge it
                  {:f (if (= it.parent "") 0 (mod (+ it.phase (/ state.time it.period)) 1))
                   :pi (indexOf computed.ids it.parent)}))
         state.bodies))

(defcomputed rel
  (map (fn [it] (merge it {:lx (* it.radius (cos (* 2 (pi) it.f))) :lz (* (- it.radius) (sin (* 2 (pi) it.f)))}))
         computed.local))

(defcomputed up1 "Each body's parent's place, or -1."
  (map (fn [it] it.pi) computed.local))

(defcomputed up2 "Each body's grandparent's place, or -1."
  (map (fn [it] (if (>= it 0) (get computed.up1 it) (- 1))) computed.up1))

(defcomputed up3 "Each body's great-grandparent's place, or -1."
  (map (fn [it] (if (>= it 0) (get computed.up1 it) (- 1))) computed.up2))

(defcomputed depth
  (map-indexed (fn [index it]
                   (cond (< it 0) 0 (< (get computed.up2 index) 0) 1 (< (get computed.up3 index) 0) 2 :else 3))
                 computed.up1))

(defcomputed below
  (map-indexed (fn [index it]
                   (cond (>= (indexOf computed.up3 index) 0) 3
                         (>= (indexOf computed.up2 index) 0) 2
                         (>= (indexOf computed.up1 index) 0) 1
                         :else 0))
                 computed.up1))

(defcomputed positions
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
  (map (fn [it] (merge it {:x (+ it.lx it.cx) :z (+ it.lz it.cz)})) computed.positions))

;;; The selected and the followed

(defcomputed selectedBody "The selected body."
  (find (fn [it] (= it.id state.selected)) state.bodies))

(defcomputed selectedPosition "Where the selected body is now."
  (find (fn [it] (= it.id state.selected)) computed.placed))

(defcomputed followed "Where the body the camera follows is now."
  (find (fn [it] (= it.id state.follow)) computed.placed))

(defcomputed flyPosition "Where the body the camera is flying to is now."
  (if (not= state.flying "") (find (fn [it] (= it.id state.flying)) computed.placed) nil))

(defcomputed catalogue
  (map (fn [it] {:id it.id :name it.name :parent it.parent :thumbnail (str "body:" (or-else it.look it.id))})
         state.bodies))

;;; The browser's tree

(defcomputed bodyCells
  (map-indexed (fn [index it]
                   (if (= (get-in state.bodies [(mod index (count state.bodies)) :parent])
                          (get-in state.bodies [(floor (/ index (count state.bodies))) :id]))
                       (mod index (count state.bodies))
                       (- 1)))
                 (range (* (count state.bodies) (count state.bodies)))))

(defcomputed bodyKids
  (map-indexed (fn [index it]
                   (filter (fn [it] (>= it 0))
                           (slice computed.bodyCells
                                  (* index (count state.bodies))
                                  (* (+ index 1) (count state.bodies)))))
                 state.bodies))

(defcomputed bodyRows
  (map (fn [it]
           {:id it.id
            :label it.name
            :icon (cond (= it.parent "") "sun" (= it.parent "sun") "circle" :else "moon")
            :iconColor it.hex})
         state.bodies))

(defcomputed bodyLevel1
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it {:children (map (fn [it] (get computed.bodyRows it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyRows))

(defcomputed bodyLevel2
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it
                              {:children (map (fn [it] (get computed.bodyLevel1 it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyLevel1))

(defcomputed bodyLevel3
  (map-indexed (fn [index it]
                   (if (> (count (get computed.bodyKids index)) 0)
                       (merge it
                              {:children (map (fn [it] (get computed.bodyLevel2 it)) (get computed.bodyKids index))})
                       it))
                 computed.bodyLevel2))

(defcomputed bodyTree
  (map (fn [it] (get computed.bodyLevel3 it))
         (filter (fn [it] (= (get-in state.bodies [it :parent]) "")) (range (count state.bodies)))))

(defcomputed bookmarkRows "The bookmarks, as the browser's list."
  (map (fn [it] {:id it.id :label it.label :icon "bookmark"}) state.bookmarks))

;;; The orbit graph

(defcomputed graphNodes
  (map-indexed (fn [index it]
                   {:id it.id
                    :title it.name
                    :x (or-else it.gx 400)
                    :y (or-else it.gy (+ 20 (* index 40)))
                    :width 120
                    :inputs (if (= it.parent "") [] [{:id "orbits" :label "orbits" :kind "orbit" :links 1}])
                    :outputs [{:id "satellites" :label "satellites" :kind "orbit"}]})
                 state.bodies))

(defcomputed graphLinks
  (map (fn [it] {:from it.parent :output "satellites" :to it.id :input "orbits"})
         (filter (fn [it] (not= it.parent "")) state.bodies)))
