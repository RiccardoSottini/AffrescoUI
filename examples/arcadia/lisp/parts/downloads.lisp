;;; downloads.lisp --- Arcadia's downloads: the current download with its network and disk graph, the queue to reorder, unscheduled items and completed ones.

;;; Definitions

(defstate dlPaused false)

(defstate dlTick 0)

(defstate dlRate
  [22
   31
   38
   35
   41
   44
   39
   36
   42
   47
   45
   40
   38
   43
   49
   52
   46
   41
   39
   44
   48
   45
   42
   40
   37
   41
   46
   50
   47
   43
   39
   36
   40
   44
   47
   45
   41
   38
   42
   46
   49
   45
   41
   39
   43
   46
   44
   38])

(defcomputed dlSize (if (nil? computed.download) 0 (or-else computed.download.sizeGb computed.downloadGame.sizeGb)))

(defcomputed dlQueue
  (filter (fn [it] (= it.d.status "queued")) (map-indexed (fn [index it] {:d it :at index}) state.downloads)))

(defcomputed dlParked (filter (fn [it] (= it.status "paused")) state.downloads))

(defcomputed dlDone (filter (fn [it] (= it.status "done")) state.downloads))

(defcomputed dlNow (if (or state.dlPaused (nil? computed.download)) 0 state.dlRate.47))

(defcomputed dlPeak (max state.dlRate))

(defcomputed dlLeft
  (if (or (nil? computed.download) (= computed.dlNow 0))
    0
    (ceil (/ (/ (* (- computed.dlSize computed.download.doneGb) 1024) computed.dlNow) 60))))

(defcomponent dlGraph
  "The network rate as bars and the disk rate as a line over the last samples, newest on the right."
  {}
  `(row#dl-graph :width 336 :height 110 :gap 1 :alignItems end :color "#00000033"
     ~(repeat [r state.dlRate :index k]
        `(node :width 6 :height 110 :color transparent
           (node :anchors {:left 0 :right 0 :bottom 0}
             :height ~(round (* r 1.8))
             :color transparent
             :gradient {:kind linear :stops [@dl.net @dl.netSoft]})
           (node :anchors {:left 0 :right 0 :bottom ~(round (* r 1.8 0.82))} :height 2 :color @dl.disk)))))

(defcomponent dlStat "One figure under the graph: a caps label over a value."
  {:label "" :value "" :color @color.bright}
  `(column :width 110 :height 38
     (label :font small :text ~(upper-case label) :height 16 :anchors {:left 0 :right 0} :textColor @color.muted)
     (label :font strong :text ~value :height 20 :anchors {:left 0 :right 0} :textColor ~color)))

(defcomponent dlCurrent
  "The current download over its art: capsule, title, progress, pause, and the graph with its figures."
  {}
  `(node#dl-current :height 230 :anchors {:left 0 :right 0} :color @dl.band
     (image :when ~(some? computed.download)
       :fill true
       :fit cover
       :file ~(str "../assets/art/" computed.download.id "/hero.svg"))
     (node :fill true :color @dl.shade)
     (label :when ~(nil? computed.download)
       :font heading
       :text ~(t "dl.idle")
       :fill true
       :align center
       :textColor @color.muted)
     ~(dl-current-row-view)))

(defcomponent downloadsPage
  "Downloads: the current one with its graph, the queue to reorder, unscheduled items and completed ones."
  {}
  `(node#downloads :fill true
     :color @color.page
     :every {:ms 1000
             :do [{:if ~(and (some? computed.download)
                             (not state.dlPaused)
                             (or settings.downloads.whilePlaying (nil? state.running)))
                   :then [{:set state.dlTick :to ~(+ state.dlTick 1)}
                          {:set state.dlRate
                           :to ~(concat (slice state.dlRate 1)
                                        [(if (> settings.downloads.limit 0)
                                           (min settings.downloads.limit
                                                (- (+ 34 (* state.dlTick 7))
                                                   (* (floor (/ (* state.dlTick 7) 19)) 19)))
                                           (- (+ 34 (* state.dlTick 7)) (* (floor (/ (* state.dlTick 7) 19)) 19)))])}
                          {:forEach state.downloads
                           :as d
                           :index i
                           :do [{:if ~(= d.status "downloading")
                                 :then [{:set "state.downloads[i].doneGb"
                                         :to ~(min computed.dlSize (+ d.doneGb (/ state.dlRate.47 1024)))}]}]}
                          {:if ~(>= computed.download.doneGb computed.dlSize)
                           :then [~(handler dlFinish computed.download.id)]}]}]}
     (column#dl-scroll :fill true :scroll vertical
       (column :height ~(+ 420
                           (* 92 (+ (count computed.dlQueue) (count computed.dlParked) (count computed.dlDone))))
         :anchors {:left 0 :right 0}
         (node :use dlCurrent)
         ~(dl-scroll-column)))))

(defcomponent playButton "A small green Play for a finished download."
  {:id ""}
  `(label :icon play
     :text ~(str " " (t "dl.play"))
     :fitText true
     :padding 8
     :radius 2
     :font strong
     :textColor "#ffffff"
     :color transparent
     :focusable true
     :role button
     :cursor hand
     :gradient {:kind linear :angle 90 :stops [@color.play @color.playEnd]}
     :hover {:shadow {:blur 10 :color "#2ec4a066"}}
     :onClick ~(handler play id)))

(deftokens download-colours "The downloads page's colours: the network and disk graphs, the bands and rows."
  {:dl.net {:dark "#f5a524" :light "#a65800"}
   :dl.netSoft {:dark "#f5a52440" :light "#a6580040"}
   :dl.disk {:dark "#2ec4a0" :light "#1f9e80"}
   :dl.band {:dark "#130f17" :light "#ebe3ef"}
   :dl.shade {:dark "#130f17e0" :light "#f6f2f7e6"}
   :dl.row {:dark "#ffffff08" :light "#ffffffc0"}
   :dl.rowHover {:dark "#ffffff14" :light "#ffffff"}})

;;; Views

(defview dl-current-row "#dl-current-row." []
  `(row :height 30 :anchors {:left 0 :right 0} :gap 12 :alignItems center
     (node#dl-progress :grow 1
       :height 8
       :radius 4
       :color @color.raised
       :role progressbar
       :label ~(t "dl.progress")
       :value ~(/ computed.download.doneGb computed.dlSize)
       (node :anchors {:left 0 :top 0}
         :height 8
         :radius 4
         :width ~(str (text (floor (/ (* 100 computed.download.doneGb) computed.dlSize))) "%")
         :color ~(if state.dlPaused "@color.muted" "@dl.net")))
     (label#dl-pause :icon ~(if state.dlPaused "play" "pause")
       :text ""
       :width 30
       :height 30
       :radius 15
       :align center
       :color ~(if true "@dl.net" "transparent")
       :textColor ~(if true "#ffffff" "@color.dim")
       :focusable true
       :role button
       :cursor hand
       :label ~(if state.dlPaused (t "dl.resume") (t "dl.pause"))
       :tooltip ~(if state.dlPaused (t "dl.resume") (t "dl.pause"))
       :hover {:color @color.hover :textColor @color.bright}
       :onClick dlPause)))

(defview dl-current-column "#dl-current-column." []
  `(column :grow 1 :height 182 :gap 6
     (label#dl-title :font title
       :text ~computed.downloadGame.title
       :height 36
       :anchors {:left 0 :right 0}
       :textColor @color.bright)
     (label :font caps
       :text ~(upper-case (cond state.dlPaused (t "dl.paused")
                                (= computed.download.kind "update") (t "dl.updating")
                                :else (t "dl.downloading")))
       :height 18
       :anchors {:left 0 :right 0}
       :textColor ~(if state.dlPaused "@color.muted" "@dl.net"))
     (panel :color transparent :height 8 :width 1)
     ~(dl-current-row)
     (row :height 20 :anchors {:left 0 :right 0} :gap 16
       (label :text ~(str (formatNumber computed.download.doneGb 1 1)
                          " GB / "
                          (formatNumber computed.dlSize 1 1)
                          " GB")
         :fitText true
         :font small
         :textColor @color.text)
       (panel :color transparent :grow 1 :height 1)
       (label :text ~(if state.dlPaused "" (t "dl.left" {:n computed.dlLeft}))
         :fitText true
         :font small
         :textColor @color.muted))))

(defview dl-current-row-view "#dl-current-row-view." []
  `(row :when ~(some? computed.download) :anchors {:left 24 :right 24 :top 24 :bottom 24} :gap 20
     (image :file ~(str "../assets/art/" computed.download.id "/hero.svg")
       :fit cover
       :width 300
       :height 140
       :radius 2
       :cursor hand
       :onClick ~(handler openGame computed.download.id))
     ~(dl-current-column)
     (column :width 336 :height 182 :gap 8
       (node :use dlGraph)
       (row :height 40 :anchors {:left 0 :right 0} :gap 4
         (node :use dlStat
           :with {:label ~(t "dl.current") :value ~(str (formatNumber computed.dlNow 1 1) " MB/s") :color @dl.net})
         (node :use dlStat :with {:label ~(t "dl.peak") :value ~(str (formatNumber computed.dlPeak 1 1) " MB/s")})
         (node :use dlStat
           :with {:label ~(t "dl.disk")
                  :value ~(str (formatNumber (* computed.dlNow 0.82) 1 1) " MB/s")
                  :color @dl.disk})))))

(defview downloads-column "#downloads-column." [q]
  `(column :grow 1 :height 52 :gap 4
     (label :font strong
       :text ~(:title (find (fn [it] (= it.id q.d.id)) data.games))
       :height 24
       :anchors {:left 0 :right 0}
       :textColor @color.bright
       :cursor hand
       :onClick ~(handler openGame q.d.id))
     (label :font small
       :text ~(str (formatNumber q.d.doneGb 1 1)
                   " GB / "
                   (formatNumber (or-else q.d.sizeGb (:sizeGb (find (fn [it] (= it.id q.d.id)) data.games))) 1 1)
                   " GB")
       :height 18
       :anchors {:left 0 :right 0}
       :textColor @color.muted)))

(defview downloads-view "#." [q k]
  `(label :id ~(str "dl-up-" q.d.id)
     :icon arrow-up
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if (> k 0) "@color.raised" "transparent")
     :textColor ~(if (> k 0) "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.moveUp")
     :tooltip ~(t "dl.moveUp")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick {:if ~(> k 0) :then [~(handler dlMove q.at (get-in computed.dlQueue [(- k 1) :at]))]}))

(defview downloads-view-view "#." [q k]
  `(label :id ~(str "dl-down-" q.d.id)
     :icon arrow-down
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if (< k (- (count computed.dlQueue) 1)) "@color.raised" "transparent")
     :textColor ~(if (< k (- (count computed.dlQueue) 1)) "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.moveDown")
     :tooltip ~(t "dl.moveDown")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick {:if ~(< k (- (count computed.dlQueue) 1))
               :then [~(handler dlMove q.at (get-in computed.dlQueue [(+ k 1) :at]))]}))

(defview downloads-view-2 "#." [q]
  `(label :id ~(str "dl-start-" q.d.id)
     :icon download
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@dl.net" "transparent")
     :textColor ~(if true "#ffffff" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.startNow")
     :tooltip ~(t "dl.startNow")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlStart q.d.id)))

(defview downloads-view-3 "#." [q]
  `(label :id ~(str "dl-unschedule-" q.d.id)
     :icon clock-3
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@color.raised" "transparent")
     :textColor ~(if true "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.unschedule")
     :tooltip ~(t "dl.unschedule")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlUnschedule q.d.id)))

(defview downloads-view-4 "#." [q]
  `(label :id ~(str "dl-remove-" q.d.id)
     :icon x
     :font icons
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@color.raised" "transparent")
     :textColor ~(if true "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.remove")
     :tooltip ~(t "dl.remove")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlRemove q.d.id)))

(defview downloads-view-5 "#." [q k]
  `(row :id ~(str "dl-q-" q.d.id)
     :height 86
     :anchors {:left 0 :right 0}
     :gap 16
     :alignItems center
     :color @dl.row
     :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     (image :file ~(str "../assets/art/" q.d.id "/hero.svg") :fit cover :width 150 :height 70 :radius 2)
     ~(downloads-column q)
     (label :font small
       :text ~(if (= q.d.kind "update") (t "dl.kindUpdate") (t "dl.kindInstall"))
       :width 90
       :textColor @color.muted
       :height 86
       :align {:horizontal start :vertical center})
     ~(downloads-view q k)
     ~(downloads-view-view q k)
     ~(downloads-view-2 q)
     ~(downloads-view-3 q)
     ~(downloads-view-4 q)
     (panel :color transparent :width 8 :height 1)))

(defview downloads-column-view "#downloads-column-view." [d]
  `(column :grow 1 :height 52 :gap 4
     (label :font strong
       :text ~(:title (find (fn [it] (= it.id d.id)) data.games))
       :height 24
       :anchors {:left 0 :right 0}
       :textColor @color.bright
       :cursor hand
       :onClick ~(handler openGame d.id))
     (label :font small
       :text ~(str (formatNumber d.doneGb 1 1)
                   " GB / "
                   (formatNumber (or-else d.sizeGb (:sizeGb (find (fn [it] (= it.id d.id)) data.games))) 1 1)
                   " GB")
       :height 18
       :anchors {:left 0 :right 0}
       :textColor @color.muted)))

(defview downloads-view-6 "#." [d]
  `(label :id ~(str "dl-start-" d.id)
     :icon download
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@dl.net" "transparent")
     :textColor ~(if true "#ffffff" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.startNow")
     :tooltip ~(t "dl.startNow")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlStart d.id)))

(defview downloads-view-7 "#." [d]
  `(label :id ~(str "dl-remove-" d.id)
     :icon x
     :font icons
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@color.raised" "transparent")
     :textColor ~(if true "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.remove")
     :tooltip ~(t "dl.remove")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlRemove d.id)))

(defview downloads-view-8 "#." [d]
  `(row :id ~(str "dl-p-" d.id)
     :height 86
     :anchors {:left 0 :right 0}
     :gap 16
     :alignItems center
     :color @dl.row
     :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     (image :file ~(str "../assets/art/" d.id "/hero.svg") :fit cover :width 150 :height 70 :radius 2)
     ~(downloads-column-view d)
     (label :font small
       :text ~(if (= d.kind "update") (t "dl.kindUpdate") (t "dl.kindInstall"))
       :width 90
       :textColor @color.muted
       :height 86
       :align {:horizontal start :vertical center})
     ~(downloads-view-6 d)
     ~(downloads-view-7 d)
     (panel :color transparent :width 8 :height 1)))

(defview downloads-view-9 "#." [d]
  `(label :id ~(str "dl-forget-" d.id)
     :icon x
     :font icons
     :text ""
     :width 30
     :height 30
     :radius 15
     :align center
     :color ~(if true "@color.raised" "transparent")
     :textColor ~(if true "@color.text" "@color.dim")
     :focusable true
     :role button
     :cursor hand
     :label ~(t "dl.remove")
     :tooltip ~(t "dl.remove")
     :hover {:color @color.hover :textColor @color.bright}
     :onClick ~(handler dlRemove d.id)))

(defview downloads-view-10 "#." [d]
  `(row :id ~(str "dl-d-" d.id)
     :height 86
     :anchors {:left 0 :right 0}
     :gap 16
     :alignItems center
     :color @dl.row
     :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     (image :file ~(str "../assets/art/" d.id "/hero.svg") :fit cover :width 150 :height 70 :radius 2)
     (column :grow 1 :height 52 :gap 4
       (label :font strong
         :text ~(:title (find (fn [it] (= it.id d.id)) data.games))
         :height 24
         :anchors {:left 0 :right 0}
         :textColor @color.bright
         :cursor hand
         :onClick ~(handler openGame d.id))
       (label :font small
         :text ~(t "dl.doneSize"
                   {:n (formatNumber (or-else d.sizeGb (:sizeGb (find (fn [it] (= it.id d.id)) data.games))) 1 1)})
         :height 18
         :anchors {:left 0 :right 0}
         :textColor @color.muted))
     (label :font small
       :text ~(if (= d.kind "update") (t "dl.kindUpdate") (t "dl.kindInstall"))
       :width 90
       :textColor @color.muted
       :height 86
       :align {:horizontal start :vertical center})
     (node :use playButton :with {:id ~d.id})
     ~(downloads-view-9 d)
     (panel :color transparent :width 8 :height 1)))

(defview dl-scroll-column "#dl-scroll-column." []
  `(column :height ~(+ 180 (* 92 (+ (count computed.dlQueue) (count computed.dlParked) (count computed.dlDone))))
     :anchors {:left 24 :right 24}
     :gap 6
     (panel :color transparent :height 8 :width 1)
     (row#dl-next :height 44 :anchors {:left 0 :right 0} :gap 8 :alignItems center
       (label :text ~(upper-case (t "dl.upNext")) :fitText true :font caps :textColor @color.bright)
       (label :text ~(str "(" (count computed.dlQueue) ")") :fitText true :font small :textColor @color.muted)
       (panel :color transparent :grow 1 :height 1)
       (label :text "" :fitText true :icon settings :font small :textColor @color.muted)
       (label :text ~(t "dl.autoUpdates") :fitText true :font small :textColor @color.muted))
     (label :when ~(= (count computed.dlQueue) 0)
       :font small
       :text ~(t "dl.queueEmpty")
       :height 30
       :anchors {:left 0 :right 0}
       :textColor @color.dim)
     ~(repeat [q computed.dlQueue :index k :key q.d.id] (downloads-view-5 q k))
     (panel :color transparent :height 8 :width 1)
     (row#dl-parked :height 44 :anchors {:left 0 :right 0} :gap 8 :alignItems center
       (label :text ~(upper-case (t "dl.unscheduled")) :fitText true :font caps :textColor @color.bright)
       (label :text ~(str "(" (count computed.dlParked) ")") :fitText true :font small :textColor @color.muted)
       (panel :color transparent :grow 1 :height 1))
     ~(repeat [d computed.dlParked :key d.id] (downloads-view-8 d))
     (panel :color transparent :height 8 :width 1)
     ~(dl-done)
     ~(repeat [d computed.dlDone :key d.id] (downloads-view-10 d))))

(defview dl-done "#dl-done, in dl-scroll-column." []
  `(row#dl-done :height 44 :anchors {:left 0 :right 0} :gap 8 :alignItems center
     (label :text ~(upper-case (t "dl.completed")) :fitText true :font caps :textColor @color.bright)
     (label :text ~(str "(" (count computed.dlDone) ")") :fitText true :font small :textColor @color.muted)
     (panel :color transparent :grow 1 :height 1)
     (label#dl-clear :font small
       :text ~(t "dl.clearAll")
       :fitText true
       :padding 6
       :radius 2
       :textColor @color.link
       :focusable true
       :role button
       :cursor hand
       :hover {:color @color.hover}
       :onClick dlClear)))
