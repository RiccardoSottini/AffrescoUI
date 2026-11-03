;;; bars.lisp --- the bars: the menus, the toolbar and the status bar

;;; The menus

(defmenu view
  {:goTo {:run goTo :group "1_go" :order 1}
   :centre {:run centre :group "1_go" :order 2}
   :follow {:run follow :group "1_go" :order 3}
   :orbits {:run toggleOrbits :group "2_show" :order 1}
   :labels {:run toggleLabels :group "2_show" :order 2}
   :browser {:run toggleBrowser :group "2_show" :order 3}
   :narrow {:run zoomIn :group "3_fov" :order 1}
   :widen {:run zoomOut :group "3_fov" :order 2}
   :bookmark {:run addBookmark :group "4_places" :order 1}
   :deleteBookmark {:run deleteBookmark :group "4_places" :order 2}
   :eclipses {:run openEclipses :group "4_places" :order 3}
   :renderOptions {:run openRender :group "2_show" :order 7}
   :theme {:run toggleTheme :group "2_show" :order 8}
   :stars {:run toggleStars :group "2_show" :order 4}
   :shading {:run toggleShading :group "2_show" :order 5}
   :glow {:run toggleGlow :group "2_show" :order 6}
   :mark {:run toggleMarker :group "1_go" :order 4}
   :fainter {:run fainter :group "3_fov" :order 3}
   :brighter {:run brighter :group "3_fov" :order 4}})

(defmenu time
  {:faster {:run faster :title ~(t "time.tenFaster") :group "1_rate" :order 1}
   :slower {:run slower :title ~(t "time.tenSlower") :group "1_rate" :order 2}
   :freeze {:run togglePlay :title ~(t "time.freeze") :group "1_rate" :order 3}
   :realTime {:run realTime :group "1_rate" :order 4}
   :reverse {:run reverse :title ~(t "time.reverseTime") :group "1_rate" :order 5}
   :now {:run dayZero :title ~(t "time.dayZero") :group "2_set" :order 1}
   :eclipses {:run openEclipses :group "2_set" :order 2}})

(defcomputed viewMenu
  (map (fn [it]
           (merge it
                  {:checked (cond (= it.run "toggleTheme") (= state.theme "light")
                                  (= it.run "toggleOrbits") state.showOrbits
                                  (= it.run "toggleLabels") state.showLabels
                                  (= it.run "toggleBrowser") state.showBrowser
                                  (= it.run "toggleStars") state.showStars
                                  (= it.run "toggleShading") state.showShading
                                  (= it.run "toggleGlow") state.showGlow
                                  (= it.run "toggleMarker") (>= (indexOf state.markers state.selected) 0)
                                  (= it.run "follow") (and (not= state.follow "") (some? state.follow))
                                  :else false)}))
         menus.view))

(defview menu-bar "#menu-bar." []
  `(node#menu-bar :when ~(not window.nativeMenuBar)
     :use menuBar
     :width 56
     :with {:open state.menuBar
            :menus ~[{:name "view" :title (t "menu.view") :items computed.viewMenu}
                     {:name "time"
                      :title (t "menu.time")
                      :items (map (fn [it]
                                    (merge it
                                           {:checked (cond (= it.run "realTime") (and state.playing computed.realTime)
                                                           (= it.run "reverse") (< state.speed 0)
                                                           (= it.run "togglePlay") (not state.playing)
                                                           :else nil)}))
                                  menus.time)}]
            :width 260
            :color @panel
            :textColor @text
            :dimColor @muted
            :highlight @raised}))

;;; The toolbar and the status bar

(defcomputed toolbarItems
  [{:title (t "time.dayZero") :icon "skip-back" :keyLabel "Ctrl+0" :run "rewind"}
     {:title (if state.playing (t "time.pause") (t "time.play"))
      :icon (if state.playing "pause" "play")
      :keyLabel "Space"
      :run "togglePlay"}
     {:title (if (< state.speed 0) (t "time.previousEvent") (t "time.nextEvent"))
      :icon (if (< state.speed 0) "skip-back" "skip-forward")
      :keyLabel "Ctrl+N"
      :run "nextEvent"}
     {:separator true}
     {:title (t "time.slower") :icon "chevrons-left" :keyLabel "K" :run "slower"}
     {:title (t "time.reverse") :icon "arrow-left-right" :keyLabel "J" :checked (< state.speed 0) :run "reverse"}
     {:title (t "time.faster") :icon "chevrons-right" :keyLabel "L" :run "faster"}
     {:title (t "command.realTime")
      :icon "timer"
      :keyLabel "Shift+K"
      :checked (and state.playing computed.realTime)
      :run "realTime"}
     {:separator true}
     {:title (t "command.goTo") :icon "rocket" :keyLabel "G" :run "goTo"}
     {:title (t "command.centre") :icon "crosshair" :keyLabel "C" :run "centre"}
     {:title (t "command.follow")
      :icon "locate-fixed"
      :keyLabel "F"
      :checked (and (not= state.follow "") (some? state.follow))
      :run "follow"}
     {:title (t "command.toggleMarker")
      :icon "scan"
      :keyLabel "M"
      :checked (>= (indexOf state.markers state.selected) 0)
      :run "toggleMarker"}
     {:separator true}
     {:title (t "view.orbits") :icon "orbit" :keyLabel "O" :checked state.showOrbits :run "toggleOrbits"}
     {:title (t "view.labels") :icon "tag" :keyLabel "P" :checked state.showLabels :run "toggleLabels"}
     {:title (t "view.browser") :icon "list-tree" :keyLabel "B" :checked state.showBrowser :run "toggleBrowser"}
     {:separator true}
     {:title (t "command.openRender") :icon "sliders-horizontal" :keyLabel "Ctrl+," :run "openRender"}])

(defcomputed statusItems
  [{:text (t "status.day" {:day (text (/ (round (* state.time 10)) 10))}) :color "@accent"}
     {:text (cond (not computed.goal) (t "status.noEvents")
                  (< state.speed 0) (t "status.previous"
                                       {:event computed.goal.label
                                        :day (text (round computed.goal.time))
                                        :n (text (round (- state.time computed.goal.time)))})
                  :else (t "status.next"
                           {:event computed.goal.label
                            :day (text (round computed.goal.time))
                            :n (text (round (- computed.goal.time state.time)))}))}
     {:side "end"
      :text (cond (not computed.selectedBody) (t "status.nothing")
                  computed.selectedBody.parent (t "status.orbits"
                                                  {:name computed.selectedBody.name
                                                   :parent (or-else (:name (find (fn [it] (= it.id computed.selectedBody.parent)) state.bodies))
                                                                    computed.selectedBody.parent)
                                                   :days (text computed.selectedBody.period)})
                  :else (t "status.centre" {:name computed.selectedBody.name}))}])

;;; The top bar, the side and the status bar

(defview top-bar "The top bar: the app's name, the menus (when the window has no menu bar of its own) and the toolbar." []
  `(row#top-bar :width "100%" :height 36 :alignItems center :color @panel
     (label#app-title :font title :text ~(t "app.title") :textColor @accent :width 84 :height 30 :align center)
     ~(menu-bar)
     (node#toolbar :use toolbar
       :with {:items ~computed.toolbarItems
              :width ~(- window.width (if window.nativeMenuBar 84 140))
              :label ~(t "toolbar.label")
              :color @panel
              :textColor @text
              :dimColor @muted
              :highlight @raised
              :checkedColor @border})))

(defview side "The side, right of the view: the chart's bar and the chart, then what orbits what." []
  `(column#side :width 460 :height "100%" :gap 1 :color @border
     ~(chart-bar)
     ~(chart-frame)
     (row#graph-bar :width "100%" :height 28 :color @panel
       (label#graph-title :style heading :text ~(t "graph.title") :grow 1))
     ~(orbits-view)))

(defview status-bar "The status bar: the day, the next event and the selected body." []
  `(node#status-bar :use statusBar
     :width "100%"
     :with {:items ~computed.statusItems :label ~(t "status.label") :color @panel :textColor @muted :highlight @raised}))
