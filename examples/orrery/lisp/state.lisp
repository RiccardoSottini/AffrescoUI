;;; state.lisp --- what Orrery keeps while it runs, and where each value starts
;;;
;;; The bodies (each with its orbit: its parent, radius, period and phase, its size and colour), the day shown and how
;;; fast it runs, the camera, the chart's notes, the bookmarks and what the views show. The sky from the ground keeps
;;; its own, in sky.lisp.

;;; The state

(defstate time 0)

(defstate duration 420)

(defstate playing false)

(defstate speed 0.01)

(defstate theme dark)

(defstate selected earth)

(defstate camera {:tx 0 :tz 0 :yaw 0.4 :pitch 0.75 :distance 15})

(defstate grab nil)

(defstate bodies
  [{:id sun
    :name "Sun"
    :parent ""
    :radius 0
    :period 1
    :phase 0
    :size 0.9
    :hex "#ffc94d"
    :color [1.0 0.5841 0.0742 1]
    :gx 10
    :gy 75
    :km 696000}
   {:id mercury
    :name "Mercury"
    :parent sun
    :radius 2.0
    :period 88
    :phase 0.3
    :size 0.16
    :hex "#b3aca3"
    :color [0.4508 0.4125 0.3663 1]
    :gx 170
    :gy 0
    :km 2440}
   {:id venus
    :name "Venus"
    :parent sun
    :radius 3.0
    :period 224.7
    :phase 0.6
    :size 0.26
    :hex "#e8c27a"
    :color [0.807 0.5395 0.1946 1]
    :gx 170
    :gy 52
    :km 6052}
   {:id earth
    :name "Earth"
    :parent sun
    :radius 4.3
    :period 365.25
    :phase 0
    :size 0.28
    :hex "#3f86e0"
    :color [0.0497 0.2384 0.7454 1]
    :gx 170
    :gy 104
    :km 6371}
   {:id moon
    :name "Moon"
    :parent earth
    :radius 0.65
    :period 27.32
    :phase 0.5
    :nodeDay 36.9
    :eclipseYear 354.4
    :eclipseSeason 18
    :size 0.09
    :hex "#d4d4d4"
    :color [0.6584 0.6584 0.6584 1]
    :gx 330
    :gy 104
    :km 1737}
   {:id mars
    :name "Mars"
    :parent sun
    :radius 5.8
    :period 687
    :phase 0.15
    :size 0.21
    :hex "#d2643c"
    :color [0.6445 0.1274 0.0452 1]
    :gx 170
    :gy 156
    :km 3390}])

(defstate events
  [{:id mercury-1 :time 81.2 :kind conjunction :label "Mercury inferior conjunction"}
   {:id mars-1 :time 116.8 :kind opposition :label "Mars at opposition"}
   {:id mercury-2 :time 197.1 :kind conjunction :label "Mercury inferior conjunction"}
   {:id venus-1 :time 234.6 :kind conjunction :label "Venus inferior conjunction"}
   {:id mercury-3 :time 313.1 :kind conjunction :label "Mercury inferior conjunction"}])

(defstate stars
  [{:id star-0 :x -81 :y -122 :width 4 :height 4 :radius 2.0 :color "#cfd8ff"}
   {:id star-1 :x -197 :y 13 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-2 :x 188 :y -100 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-3 :x -38 :y -91 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-4 :x -203 :y 23 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-5 :x 35 :y -36 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-6 :x 26 :y -128 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-7 :x 19 :y 25 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-8 :x -183 :y 25 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-9 :x -185 :y 74 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-10 :x 55 :y -1 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-11 :x 128 :y -12 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-12 :x -92 :y 103 :width 4 :height 4 :radius 2.0 :color "#fff4d6"}
   {:id star-13 :x -192 :y -70 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-14 :x 106 :y -74 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-15 :x 5 :y -117 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-16 :x 199 :y -27 :width 4 :height 4 :radius 2.0 :color "#cfd8ff"}
   {:id star-17 :x 122 :y 26 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-18 :x 90 :y 33 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-19 :x -198 :y -142 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-20 :x 91 :y -152 :width 4 :height 4 :radius 2.0 :color "#ffffff"}
   {:id star-21 :x 68 :y 173 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-22 :x 100 :y 135 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-23 :x 203 :y -51 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-24 :x -3 :y -99 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-25 :x 110 :y -36 :width 3 :height 3 :radius 1.5 :color "#cfd8ff"}
   {:id star-26 :x -153 :y -34 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-27 :x 147 :y 127 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-28 :x 224 :y 64 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-29 :x -161 :y -113 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-30 :x -224 :y 116 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-31 :x -100 :y -124 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-32 :x 51 :y -63 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-33 :x -20 :y 130 :width 4 :height 4 :radius 2.0 :color "#ffd9c2"}
   {:id star-34 :x -47 :y -37 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-35 :x -201 :y -151 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-36 :x -155 :y -56 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-37 :x -230 :y -122 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-38 :x 52 :y -150 :width 2 :height 2 :radius 1.0 :color "#ffd9c2"}
   {:id star-39 :x -162 :y -87 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-40 :x -12 :y -135 :width 3 :height 3 :radius 1.5 :color "#ffd9c2"}
   {:id star-41 :x -9 :y -66 :width 2 :height 2 :radius 1.0 :color "#cfd8ff"}
   {:id star-42 :x 115 :y 84 :width 3 :height 3 :radius 1.5 :color "#fff4d6"}
   {:id star-43 :x 8 :y -103 :width 3 :height 3 :radius 1.5 :color "#ffffff"}
   {:id star-44 :x -163 :y 15 :width 2 :height 2 :radius 1.0 :color "#ffffff"}
   {:id star-45 :x 220 :y 127 :width 4 :height 4 :radius 2.0 :color "#ffffff"}
   {:id star-46 :x 8 :y 143 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}
   {:id star-47 :x 15 :y 98 :width 2 :height 2 :radius 1.0 :color "#fff4d6"}])

(defstate chart {:camera {:x -215 :y -140 :zoom 1} :selection []})

(defstate notes
  [{:id note-1
    :x -205
    :y 120
    :width 120
    :height 22
    :radius 4
    :color @note
    :text "Drag me; add notes above"
    :textSize 10
    :textColor "#1d2026"}])

(defstate draft "")

(defstate nextId 2)

(defstate follow "")

(defstate fly 0)

(defstate showOrbits true)

(defstate showLabels true)

(defstate showStars true)

(defstate showShading true)

(defstate showGlow true)

(defstate renderOpen false)

(defstate viewMenu false)

(defstate zoom 1.1)

(defstate showBrowser false)

(defstate browserTab bodies)

(defstate expandedBodies [sun earth])

(defstate bookmarks
  [{:id overview
    :label "Whole system"
    :camera {:tx 0 :tz 0 :yaw 0.4 :pitch 0.75 :distance 15}
    :follow ""
    :selected earth
    :time 0
    :zoom 1.1}])

(defstate bookmark overview)

(defstate flying "")

(defstate flights 0)

(defstate landed 0)

(defstate nextPlace 1)

(defstate sceneView
  {:camera {:target [0 0 0] :yaw 0.4 :pitch 0.75 :distance 15 :fieldOfView 0.8533} :selection [earth]})

(defstate limitMag 5)

(defstate markers [])

(defstate muted [])

(defstate locked [])

(defstate eclipse "")

;;; Settings

(defcustom appearance.language "en" "English, Spanish or French."
  :type string
  :enum [en es fr]
  :title ~(t "setting.language")
  :description ~(t "setting.language.about"))
