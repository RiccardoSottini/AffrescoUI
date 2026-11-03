;;; downloads.lisp --- Arcadia's downloads page
;;;
;;; The current download over its game's art, with its network and disk graph; the queue to reorder; the unscheduled
;;; items and the completed ones. Every second, the download in progress moves on (downloadTick, in arcadia.lisp).

;;; What the page keeps

(defstate dlPaused false)

(defstate dlTick 0)

(defstate dlRate
  [22 31 38 35 41 44 39 36 42 47 45 40 38 43 49 52 46 41 39 44 48 45 42 40
   37 41 46 50 47 43 39 36 40 44 47 45 41 38 42 46 49 45 41 39 43 46 44 38])

(defcomputed dlSize "The size of the download in progress, in GB."
  (if (nil? computed.download) 0 (or-else computed.download.sizeGb computed.downloadGame.sizeGb)))

(defcomputed dlQueue "The downloads waiting, each with its place in the list of downloads."
  (filter (fn [it] (= it.d.status "queued")) (map-indexed (fn [index it] {:d it :at index}) state.downloads)))

(defcomputed dlParked "The downloads taken off the schedule." (filter (fn [it] (= it.status "paused")) state.downloads))

(defcomputed dlDone "The downloads finished." (filter (fn [it] (= it.status "done")) state.downloads))

(defcomputed dlNow "The rate now, in MB/s: the last sample, or 0 while paused or idle."
  (if (or state.dlPaused (nil? computed.download)) 0 state.dlRate.47))

(defcomputed dlPeak "The highest rate of the samples." (max state.dlRate))

(defcomputed dlLeft "The minutes left at the rate now."
  (if (or (nil? computed.download) (= computed.dlNow 0))
    0
    (ceil (/ (/ (* (- computed.dlSize computed.download.doneGb) 1024) computed.dlNow) 60))))

;;; Its looks

(deftokens download-colours "The downloads page's colours: the network and disk graphs, the bands and rows."
  {:dl.net {:dark "#f5a524" :light "#a65800"}
   :dl.netSoft {:dark "#f5a52440" :light "#a6580040"}
   :dl.disk {:dark "#2ec4a0" :light "#1f9e80"}
   :dl.band {:dark "#130f17" :light "#ebe3ef"}
   :dl.shade {:dark "#130f17e0" :light "#f6f2f7e6"}
   :dl.row {:dark "#ffffff08" :light "#ffffffc0"}
   :dl.rowHover {:dark "#ffffff14" :light "#ffffff"}})

(defstyle roundButton "A round icon button of a download's row, raised from it."
  (label :text ""
    :width 30
    :height 30
    :radius 15
    :align center
    :color @color.raised
    :textColor @color.text
    :focusable true
    :role button
    :cursor hand
    :hover {:color @color.hover :textColor @color.bright}))

;;; The page

(defcomponent downloadsPage "Downloads: the current one with its graph, the queue to reorder, unscheduled items and completed ones."
  {}
  `(node#downloads :fill true :color @color.page :every {:ms 1000 :do downloadTick}
     (column#dl-scroll :fill true :scroll vertical
       (column :height ~(+ 420 (* 92 (+ (count computed.dlQueue) (count computed.dlParked) (count computed.dlDone))))
         :anchors {:left 0 :right 0}
         (node :use dlCurrent)
         ~(download-lists)))))

(defview download-lists "The queue, the unscheduled downloads and the completed ones, each under its heading." []
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
     ~(repeat [q computed.dlQueue :index k :key q.d.id]
        (queued-download q k))
     (panel :color transparent :height 8 :width 1)
     (row#dl-parked :height 44 :anchors {:left 0 :right 0} :gap 8 :alignItems center
       (label :text ~(upper-case (t "dl.unscheduled")) :fitText true :font caps :textColor @color.bright)
       (label :text ~(str "(" (count computed.dlParked) ")") :fitText true :font small :textColor @color.muted)
       (panel :color transparent :grow 1 :height 1))
     ~(repeat [d computed.dlParked :key d.id]
        (parked-download d))
     (panel :color transparent :height 8 :width 1)
     ~(done-heading)
     ~(repeat [d computed.dlDone :key d.id]
        (done-download d))))

(defview done-heading "The completed downloads' heading, with Clear all." []
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

;;; The download in progress

(defcomponent dlCurrent "The current download over its art: capsule, title, progress, pause, and the graph with its figures."
  {}
  `(node#dl-current :height 230 :anchors {:left 0 :right 0} :color @dl.band
     (image :when ~(some? computed.download) :fill true :fit cover :file ~(str "../assets/art/" computed.download.id "/hero.svg"))
     (node :fill true :color @dl.shade)
     (label :when ~(nil? computed.download) :font heading :text ~(t "dl.idle") :fill true :align center :textColor @color.muted)
     (row :when ~(some? computed.download) :anchors {:left 24 :right 24 :top 24 :bottom 24} :gap 20
       (image :file ~(str "../assets/art/" computed.download.id "/hero.svg")
         :fit cover
         :width 300
         :height 140
         :radius 2
         :cursor hand
         :onClick ~(handler openGame computed.download.id))
       ~(current-progress)
       ~(current-graph))))

(defview current-progress "The game's title, what it is doing, its bar with Pause, and how much is done and left." []
  `(column :grow 1 :height 182 :gap 6
     (label#dl-title :font title :text ~computed.downloadGame.title :height 36 :anchors {:left 0 :right 0} :textColor @color.bright)
     (label :font caps
       :text ~(upper-case (cond state.dlPaused (t "dl.paused")
                                (= computed.download.kind "update") (t "dl.updating")
                                :else (t "dl.downloading")))
       :height 18
       :anchors {:left 0 :right 0}
       :textColor ~(if state.dlPaused "@color.muted" "@dl.net"))
     (panel :color transparent :height 8 :width 1)
     ~(progress-bar)
     (row :height 20 :anchors {:left 0 :right 0} :gap 16
       (label :text ~(t "dl.sizeOf" {:done (formatNumber computed.download.doneGb 1 1) :size (formatNumber computed.dlSize 1 1)})
         :fitText true
         :font small
         :textColor @color.text)
       (panel :color transparent :grow 1 :height 1)
       (label :text ~(if state.dlPaused "" (t "dl.left" {:n computed.dlLeft})) :fitText true :font small :textColor @color.muted))))

(defview progress-bar "The download's bar, grey while paused, and the button that pauses or resumes it." []
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
     (label#dl-pause :style roundButton
       :icon ~(if state.dlPaused "play" "pause")
       :color @dl.net
       :textColor "#ffffff"
       :label ~(if state.dlPaused (t "dl.resume") (t "dl.pause"))
       :tooltip ~(if state.dlPaused (t "dl.resume") (t "dl.pause"))
       :onClick dlPause)))

(defview current-graph "The graph of the rates, and the figures under it: now, the peak, and the disk." []
  `(column :width 336 :height 182 :gap 8
     (node :use dlGraph)
     (row :height 40 :anchors {:left 0 :right 0} :gap 4
       (node :use dlStat :with {:label ~(t "dl.current") :value ~(t "dl.rate" {:n (formatNumber computed.dlNow 1 1)}) :color @dl.net})
       (node :use dlStat :with {:label ~(t "dl.peak") :value ~(t "dl.rate" {:n (formatNumber computed.dlPeak 1 1)})})
       (node :use dlStat
         :with {:label ~(t "dl.disk") :value ~(t "dl.rate" {:n (formatNumber (* computed.dlNow 0.82) 1 1)}) :color @dl.disk}))))

(defcomponent dlGraph "The network rate as bars and the disk rate as a line over the last samples, newest on the right."
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

;;; A download in the lists

(defview download-art "A download's banner, at the start of its row." [id]
  `(image :file ~(str "../assets/art/" id "/hero.svg") :fit cover :width 150 :height 70 :radius 2))

(defview download-kind "Whether a download is an update or an install." [d]
  `(label :font small
     :text ~(if (= d.kind "update") (t "dl.kindUpdate") (t "dl.kindInstall"))
     :width 90
     :textColor @color.muted
     :height 86
     :align {:horizontal start :vertical center}))

(defview download-name "A download's game, which opens its page, over how much of it is done." [d]
  `(column :grow 1 :height 52 :gap 4
     (label :font strong
       :text ~(:title (find (fn [it] (= it.id d.id)) data.games))
       :height 24
       :anchors {:left 0 :right 0}
       :textColor @color.bright
       :cursor hand
       :onClick ~(handler openGame d.id))
     (label :font small
       :text ~(t "dl.sizeOf"
                 {:done (formatNumber d.doneGb 1 1)
                  :size (formatNumber (or-else d.sizeGb (:sizeGb (find (fn [it] (= it.id d.id)) data.games))) 1 1)})
       :height 18
       :anchors {:left 0 :right 0}
       :textColor @color.muted)))

(defview queued-download "A download in the queue: up, down, download now, unschedule and remove." [q k]
  `(row :id ~(str "dl-q-" q.d.id) :height 86 :anchors {:left 0 :right 0} :gap 16 :alignItems center :color @dl.row :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     ~(download-art 'q.d.id)
     ~(download-name 'q.d)
     ~(download-kind 'q.d)
     (label :id ~(str "dl-up-" q.d.id)
       :style roundButton
       :icon arrow-up
       :color ~(if (> k 0) "@color.raised" "transparent")
       :textColor ~(if (> k 0) "@color.text" "@color.dim")
       :label ~(t "dl.moveUp")
       :tooltip ~(t "dl.moveUp")
       :onClick ~(handler dlUp k))
     (label :id ~(str "dl-down-" q.d.id)
       :style roundButton
       :icon arrow-down
       :color ~(if (< k (- (count computed.dlQueue) 1)) "@color.raised" "transparent")
       :textColor ~(if (< k (- (count computed.dlQueue) 1)) "@color.text" "@color.dim")
       :label ~(t "dl.moveDown")
       :tooltip ~(t "dl.moveDown")
       :onClick ~(handler dlDown k))
     ~(start-button 'q.d)
     (label :id ~(str "dl-unschedule-" q.d.id)
       :style roundButton
       :icon clock-3
       :label ~(t "dl.unschedule")
       :tooltip ~(t "dl.unschedule")
       :onClick ~(handler dlUnschedule q.d.id))
     ~(remove-button 'q.d)
     (panel :color transparent :width 8 :height 1)))

(defview parked-download "An unscheduled download: download now, and remove." [d]
  `(row :id ~(str "dl-p-" d.id) :height 86 :anchors {:left 0 :right 0} :gap 16 :alignItems center :color @dl.row :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     ~(download-art 'd.id)
     ~(download-name d)
     ~(download-kind d)
     ~(start-button d)
     ~(remove-button d)
     (panel :color transparent :width 8 :height 1)))

(defview done-download "A completed download: its size, Play, and remove from the list." [d]
  `(row :id ~(str "dl-d-" d.id) :height 86 :anchors {:left 0 :right 0} :gap 16 :alignItems center :color @dl.row :radius 2
     :hover {:color @dl.rowHover}
     (panel :color transparent :width 8 :height 1)
     ~(download-art 'd.id)
     (column :grow 1 :height 52 :gap 4
       (label :font strong
         :text ~(:title (find (fn [it] (= it.id d.id)) data.games))
         :height 24
         :anchors {:left 0 :right 0}
         :textColor @color.bright
         :cursor hand
         :onClick ~(handler openGame d.id))
       (label :font small
         :text ~(t "dl.doneSize" {:n (formatNumber (or-else d.sizeGb (:sizeGb (find (fn [it] (= it.id d.id)) data.games))) 1 1)})
         :height 18
         :anchors {:left 0 :right 0}
         :textColor @color.muted))
     ~(download-kind d)
     (node :use playButton :with {:id ~d.id})
     (label :id ~(str "dl-forget-" d.id)
       :style roundButton
       :icon x
       :font icons
       :label ~(t "dl.remove")
       :tooltip ~(t "dl.remove")
       :onClick ~(handler dlRemove d.id))
     (panel :color transparent :width 8 :height 1)))

(defview start-button "Download now: the item to the top of the queue, started." [d]
  `(label :id ~(str "dl-start-" d.id)
     :style roundButton
     :icon download
     :color @dl.net
     :textColor "#ffffff"
     :label ~(t "dl.startNow")
     :tooltip ~(t "dl.startNow")
     :onClick ~(handler dlStart d.id)))

(defview remove-button "Remove from the downloads." [d]
  `(label :id ~(str "dl-remove-" d.id)
     :style roundButton
     :icon x
     :font icons
     :label ~(t "dl.remove")
     :tooltip ~(t "dl.remove")
     :onClick ~(handler dlRemove d.id)))

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
