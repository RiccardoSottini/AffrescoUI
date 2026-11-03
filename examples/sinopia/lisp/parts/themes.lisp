;;; themes.lisp --- Sinopia's design data: the themes (twelve colour slots, four variants, a background and decorations
;;; drawn under every slide, and where a title slide's text goes beside them), the slide layouts and their placeholders
;;; on a 960 by 540 page (points: 13.33 by 7.5 inches), and the colour picker's standard colours
;;;
;;; A deck names its theme and variant; its objects name colours by slot (accent1, dark1, accent1.2 for a tint) or by
;;; value.

;;; Definitions

(defcomputed themes "The themes: their colour slots, variants, background, and the decorations drawn under every slide."
  '[{:id default
     :background light1
     :colors {:dark1 "#000000"
              :light1 "#ffffff"
              :dark2 "#44546a"
              :light2 "#e7e6e6"
              :accent1 "#4472c4"
              :accent2 "#ed7d31"
              :accent3 "#a5a5a5"
              :accent4 "#ffc000"
              :accent5 "#5b9bd5"
              :accent6 "#70ad47"}
     :variants [{}
                {:accent1 "#0f6fc6"
                 :accent2 "#009dd9"
                 :accent3 "#0bd0d9"
                 :accent4 "#10cf9b"
                 :accent5 "#7cca62"
                 :accent6 "#a5c249"}
                {:accent1 "#e48312"
                 :accent2 "#bd582c"
                 :accent3 "#865640"
                 :accent4 "#9b8357"
                 :accent5 "#c2bc80"
                 :accent6 "#94a088"}
                {:dark1 "#ffffff" :light1 "#222a35" :dark2 "#e7e6e6" :light2 "#44546a"}]
     :decor []
     :titleDecor []}
    {:id tide
     :background light1
     :colors {:dark1 "#0b2a35"
              :light1 "#ffffff"
              :dark2 "#14415a"
              :light2 "#e3f1f4"
              :accent1 "#1b8a9e"
              :accent2 "#2bb3a7"
              :accent3 "#f2a541"
              :accent4 "#e4572e"
              :accent5 "#5d7fbd"
              :accent6 "#8cb369"}
     :variants [{}
                {:accent1 "#3b6ea5" :accent2 "#4f9dde"}
                {:accent1 "#7a5195" :accent2 "#bc5090"}
                {:dark1 "#e3f1f4" :light1 "#0b2a35" :light2 "#14415a" :dark2 "#e3f1f4"}]
     :decor [{:id band :kind shape :shape rect :x 0 :y 0 :w 14 :h 540 :fill accent1}]
     :titleDecor [{:id band :kind shape :shape rect :x 0 :y 0 :w 300 :h 540 :fill accent1}
                  {:id edge :kind shape :shape rect :x 300 :y 0 :w 10 :h 540 :fill accent2}]
     :titleArea {:x 350 :w 570}}
    {:id ember
     :background dark2
     :colors {:dark1 "#ffffff"
              :light1 "#2b1d17"
              :dark2 "#2b1d17"
              :light2 "#f6e8dc"
              :accent1 "#f28c28"
              :accent2 "#e0482b"
              :accent3 "#f6c445"
              :accent4 "#b5651d"
              :accent5 "#d9a679"
              :accent6 "#8f3b1b"}
     :variants [{}
                {:accent1 "#e0482b" :accent2 "#f28c28"}
                {:accent1 "#f6c445" :accent2 "#b5651d"}
                {:dark2 "#3a2a22"}]
     :decor [{:id stripe :kind shape :shape rect :x 0 :y 520 :w 960 :h 20 :fill accent1}]
     :titleDecor [{:id stripe :kind shape :shape rect :x 0 :y 440 :w 960 :h 100 :fill accent1}
                  {:id thin :kind shape :shape rect :x 0 :y 430 :w 960 :h 4 :fill accent2}]}
    {:id grove
     :background light1
     :colors {:dark1 "#1d2b1f"
              :light1 "#fbfdf8"
              :dark2 "#2f4a32"
              :light2 "#e8f0e1"
              :accent1 "#4c8c4a"
              :accent2 "#a3c04d"
              :accent3 "#e8b04b"
              :accent4 "#c4643b"
              :accent5 "#6a9fb5"
              :accent6 "#8d6e97"}
     :variants [{}
                {:accent1 "#2e7d6b" :accent2 "#7fb77e"}
                {:accent1 "#a3c04d" :accent2 "#4c8c4a"}
                {:dark1 "#e8f0e1" :light1 "#1d2b1f" :light2 "#2f4a32" :dark2 "#e8f0e1"}]
     :decor [{:id leaf :kind shape :shape ellipse :x 880 :y -60 :w 160 :h 160 :fill accent2.1}]
     :titleDecor [{:id leaf :kind shape :shape ellipse :x 640 :y -160 :w 480 :h 480 :fill accent2.1}
                  {:id seed :kind shape :shape ellipse :x 760 :y 360 :w 140 :h 140 :fill accent1}]}
    {:id slate
     :background dark2
     :colors {:dark1 "#f2f4f7"
              :light1 "#1c2128"
              :dark2 "#262b33"
              :light2 "#c9d1d9"
              :accent1 "#4f9dde"
              :accent2 "#56d4c4"
              :accent3 "#f0b35b"
              :accent4 "#e66b6b"
              :accent5 "#a78bfa"
              :accent6 "#7bd389"}
     :variants [{}
                {:accent1 "#56d4c4" :accent2 "#4f9dde"}
                {:accent1 "#a78bfa" :accent2 "#e66b6b"}
                {:dark2 "#11151a"}]
     :decor [{:id line :kind shape :shape rect :x 66 :y 26 :w 120 :h 4 :fill accent1}]
     :titleDecor [{:id line :kind shape :shape rect :x 120 :y 276 :w 720 :h 3 :fill accent1}]}
    {:id bloom
     :background light1
     :colors {:dark1 "#2d1b33"
              :light1 "#ffffff"
              :dark2 "#4a2545"
              :light2 "#f7e9f2"
              :accent1 "#c2409b"
              :accent2 "#7c4dbd"
              :accent3 "#f08a5d"
              :accent4 "#f9c74f"
              :accent5 "#43aa8b"
              :accent6 "#577590"}
     :variants [{}
                {:accent1 "#7c4dbd" :accent2 "#c2409b"}
                {:accent1 "#f08a5d" :accent2 "#c2409b"}
                {:dark1 "#f7e9f2" :light1 "#2d1b33" :light2 "#4a2545" :dark2 "#f7e9f2"}]
     :decor [{:id wedge :kind shape :shape rtTriangle :flipH true :x 840 :y 420 :w 120 :h 120 :fill accent1.1}]
     :titleDecor [{:id wedge :kind shape :shape rtTriangle :flipH true :x 600 :y 180 :w 360 :h 360 :fill accent1.1}
                  {:id wedge2 :kind shape :shape rtTriangle :flipH true :x 780 :y 360 :w 180 :h 180 :fill accent1}]}
    {:id dune
     :background light2
     :colors {:dark1 "#3b2f25"
              :light1 "#ffffff"
              :dark2 "#5a4632"
              :light2 "#f3ebdd"
              :accent1 "#b07d48"
              :accent2 "#8a9a5b"
              :accent3 "#d4a373"
              :accent4 "#6b705c"
              :accent5 "#a5a58d"
              :accent6 "#cb997e"}
     :variants [{}
                {:accent1 "#8a9a5b" :accent2 "#b07d48"}
                {:accent1 "#cb997e" :accent2 "#6b705c"}
                {:light2 "#e9dcc5"}]
     :decor [{:id base :kind shape :shape rect :x 0 :y 500 :w 960 :h 40 :fill accent1.2}]
     :titleDecor [{:id base :kind shape :shape rect :x 0 :y 380 :w 960 :h 160 :fill accent1.2}
                  {:id rule :kind shape :shape rect :x 0 :y 376 :w 960 :h 4 :fill accent1}]}
    {:id ink
     :background light1
     :colors {:dark1 "#ffffff"
              :light1 "#0d0d0d"
              :dark2 "#e6e6e6"
              :light2 "#262626"
              :accent1 "#22d3ee"
              :accent2 "#f472b6"
              :accent3 "#facc15"
              :accent4 "#4ade80"
              :accent5 "#818cf8"
              :accent6 "#fb923c"}
     :variants [{}
                {:accent1 "#f472b6" :accent2 "#22d3ee"}
                {:accent1 "#facc15" :accent2 "#fb923c"}
                {:light1 "#111827"}]
     :decor [{:id dot :kind shape :shape ellipse :x 900 :y 30 :w 24 :h 24 :fill accent1}]
     :titleDecor [{:id frame :kind shape :shape frame :x 40 :y 40 :w 880 :h 460 :fill accent1}]}])

(defcomputed baseLayouts "The slide layouts and their placeholders, on the 960 by 540 page."
  '[{:id title
     :ph [{:ph ctrTitle :x 120 :y 88 :w 720 :h 188 :size 60 :anchor bottom :align center}
          {:ph subTitle :x 120 :y 284 :w 720 :h 130 :size 24 :anchor top :align center}]}
    {:id content
     :ph [{:ph title :x 66 :y 29 :w 828 :h 105 :size 44 :anchor middle}
          {:ph body :x 66 :y 144 :w 828 :h 344 :size 28 :bullets true :content true}]}
    {:id section
     :ph [{:ph title :x 65 :y 135 :w 828 :h 225 :size 60 :anchor bottom}
          {:ph body :x 65 :y 362 :w 828 :h 118 :size 24 :color dark1.3}]}
    {:id two
     :ph [{:ph title :x 66 :y 29 :w 828 :h 105 :size 44 :anchor middle}
          {:ph body :x 66 :y 144 :w 408 :h 344 :size 28 :bullets true :content true}
          {:ph body2 :x 486 :y 144 :w 408 :h 344 :size 28 :bullets true :content true}]}
    {:id comparison
     :ph [{:ph title :x 66 :y 29 :w 828 :h 105 :size 44 :anchor middle}
          {:ph head :x 66 :y 132 :w 406 :h 65 :size 24 :bold true :anchor bottom}
          {:ph body :x 66 :y 197 :w 406 :h 291 :size 24 :bullets true :content true}
          {:ph head2 :x 486 :y 132 :w 408 :h 65 :size 24 :bold true :anchor bottom}
          {:ph body2 :x 486 :y 197 :w 408 :h 291 :size 24 :bullets true :content true}]}
    {:id titleOnly :ph [{:ph title :x 66 :y 29 :w 828 :h 105 :size 44 :anchor middle}]}
    {:id blank :ph []}
    {:id caption
     :ph [{:ph title :x 66 :y 36 :w 310 :h 126 :size 32 :anchor bottom}
          {:ph body :x 408 :y 78 :w 486 :h 384 :size 32 :bullets true :content true}
          {:ph caption :x 66 :y 162 :w 310 :h 301 :size 16}]}
    {:id picture
     :ph [{:ph title :x 66 :y 36 :w 310 :h 126 :size 32 :anchor bottom}
          {:ph pic :x 408 :y 78 :w 486 :h 384 :size 32}
          {:ph caption :x 66 :y 162 :w 310 :h 301 :size 16}]}])

(defcomputed prompts "The text an empty placeholder shows."
  {:ctrTitle (t "theme.clickToAddTitle")
   :title (t "theme.clickToAddTitle")
   :subTitle (t "theme.clickToAddSubtitle")
   :body (t "theme.clickToAddText")
   :body2 (t "theme.clickToAddText")
   :head (t "theme.clickToAddText")
   :head2 (t "theme.clickToAddText")
   :caption (t "theme.clickToAddText")
   :pic (t "theme.clickIconToAddPicture")})

(defcomputed standardColors "The colour picker's standard colours."
  ["#c00000" "#ff0000" "#ffc000" "#ffff00" "#92d050" "#00b050" "#00b0f0" "#0070c0" "#002060" "#7030a0"])

(defcomputed slotNames "The theme's colour slots."
  '[light1 dark1 light2 dark2 accent1 accent2 accent3 accent4 accent5 accent6])

(defcomputed slotTitles "Each slot's name in the colour picker."
  {:light1 (t "theme.background1")
   :dark1 (t "theme.text1")
   :light2 (t "theme.background2")
   :dark2 (t "theme.text2")
   :accent1 (t "theme.accent1")
   :accent2 (t "theme.accent2")
   :accent3 (t "theme.accent3")
   :accent4 (t "theme.accent4")
   :accent5 (t "theme.accent5")
   :accent6 (t "theme.accent6")})

(defcomputed tints "The tints of a light and of a dark colour, by how far toward black or white."
  {:light [{:to "#000000" :by 0.05 :name (t "theme.darker5")}
           {:to "#000000" :by 0.15 :name (t "theme.darker15")}
           {:to "#000000" :by 0.25 :name (t "theme.darker25")}
           {:to "#000000" :by 0.35 :name (t "theme.darker35")}
           {:to "#000000" :by 0.5 :name (t "theme.darker50")}]
   :dark [{:to "#ffffff" :by 0.5 :name (t "theme.lighter50")}
          {:to "#ffffff" :by 0.35 :name (t "theme.lighter35")}
          {:to "#ffffff" :by 0.25 :name (t "theme.lighter25")}
          {:to "#ffffff" :by 0.15 :name (t "theme.lighter15")}
          {:to "#ffffff" :by 0.05 :name (t "theme.lighter5")}]
   :accent [{:to "#ffffff" :by 0.8 :name (t "theme.lighter80")}
            {:to "#ffffff" :by 0.6 :name (t "theme.lighter60")}
            {:to "#ffffff" :by 0.4 :name (t "theme.lighter40")}
            {:to "#000000" :by 0.25 :name (t "theme.darker25")}
            {:to "#000000" :by 0.5 :name (t "theme.darker50")}]})

(defcomputed theme "The deck's theme."
  (or-else (find (fn [it] (= it.id state.deck.theme)) computed.themes) computed.themes.0))

(defcomputed colors "The deck's colours: its theme's, its variant's, or another theme's."
  (merge computed.theme.colors
         (or-else (get computed.theme.variants (or-else state.deck.variant 0)) {})
         (if (nil? state.deck.colors)
             {}
             (:colors (or-else (find (fn [it] (= it.id state.deck.colors)) computed.themes) {:colors {}})))))

(defcomputed headingFont "The titles' font." (or-else (:heading (or-else state.deck.fonts {})) "default"))

(defcomputed bodyFont "The other text's font." (or-else (:body (or-else state.deck.fonts {})) "default"))

(defcomputed slotTint "Each slot's tints, as colours."
  (map (fn [it]
         (cond (or (= it "light1") (= it "light2")) "light"
               (or (= it "dark1") (= it "dark2")) "dark"
               :else "accent"))
       computed.slotNames))

(defcomputed palette "Every colour by name: each slot and tint."
  (parseJson (str "{"
                  (join ","
                        (map (fn [it]
                               (str (json (+ (get computed.slotNames (floor (/ it 6)))
                                             (if (= (mod it 6) 0) "" (str "." (mod it 6)))))
                                    ":"
                                    (json (if (= (mod it 6) 0)
                                              (get computed.colors (get computed.slotNames (floor (/ it 6))))
                                              (gradientColor {:stops [{:at 0
                                                                       :color (get computed.colors
                                                                                   (get computed.slotNames
                                                                                        (floor (/ it 6))))}
                                                                      {:at 1
                                                                       :color (get-in computed.tints
                                                                                      [(get computed.slotTint
                                                                                            (floor (/ it 6)))
                                                                                       (- (mod it 6) 1)
                                                                                       :to])}]}
                                                             (get-in computed.tints
                                                                     [(get computed.slotTint (floor (/ it 6)))
                                                                      (- (mod it 6) 1)
                                                                      :by]))))))
                             (range 60)))
                  "}")))

(defcomputed layouts "The layouts, their placeholders placed for the deck's theme."
  (map (fn [it]
         (if (and (= it.id "title") (some? computed.theme.titleArea))
             (merge it {:ph (map (fn [it] (merge it computed.theme.titleArea)) it.ph)})
             it))
       computed.baseLayouts))

(defcomputed layout "The layout of the slide shown."
  (or-else (find (fn [it] (= it.id computed.slide.layout)) computed.layouts) computed.layouts.1))

(defcomputed titleLike "Whether the slide shown is a title or section slide."
  (or (= computed.slide.layout "title") (= computed.slide.layout "section")))
