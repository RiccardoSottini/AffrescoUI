;;; nowplaying.lisp --- TODO

(defcomputed fade
  (if (and (> settings.playback.crossfade 0) (> (count state.queue) 0))
        (clamp (/ (- computed.duration state.elapsed) settings.playback.crossfade) 0 1)
        1))

(defcomputed crossfading (and state.isPlaying (< computed.fade 1)))

(defcomputed nextTitle (or-else computed.queueSongs.0.title ""))

(defcomputed lyricLine (mod (+ (floor (/ state.elapsed 12)) state.playingTrack) 10))

(defcomputed lyricWindow
  (map-indexed (fn [index it]
                   {:text (get state.lyrics (mod (+ computed.lyricLine index 8) 10)) :current (= index 2)})
                 (range 5)))

(defcomputed spectrum
  (or-else (if (= state.soundOn "beat") state.beat.spectrum (get-in state.sounds [state.soundOn :spectrum])) []))

(defcomputed barHeights
  (map-indexed (fn [index it]
                   (clamp (+ 0.04
                             (* (or-else (get computed.spectrum index) 0)
                                computed.fade
                                (if state.eqOn (* (curve state.eq (/ index 31)) 2) 1)))
                          0.04
                          1))
                 (range 32)))

(defcomputed bars
  (join " "
          (map-indexed (fn [index it] (str "M " (+ (* index 10) 2) " " (- 84 (* it 80)) " h 6 V 84 h -6 Z"))
                       computed.barHeights)))

(defcomputed barReflections
  (join " "
          (map-indexed (fn [index it] (str "M " (+ (* index 10) 2) " 86 h 6 V " (+ 86 (* it 28)) " h -6 Z"))
                       computed.barHeights)))

(defcomponent nowPlayingPage "The song playing: its cover, the visualiser (the spectrum of what plays, as bars) and the lyrics, line by line"
  {}
  `(node :dir auto :color transparent
     (image#nowCover :file ~computed.playing.cover
       :fit cover
       :radius 8
       :anchors {:left 32 :top 32}
       :width 160
       :height 160
       :role img
       :label ~computed.playingName)
     {:id nowCaption
      :style caption
      :text ~(if state.isPlaying (t "queue.now") (t "now.paused"))
      :anchors {:left 216 :top 56}}
     (label#nowTitle :text ~computed.track.title
       :font large
       :textColor @text
       :anchors {:left 214 :right 32 :top 76}
       :height 40)
     (label#nowArtist :text ~(str computed.track.artist "  ·  " computed.track.album)
       :textColor @muted
       :anchors {:left 216 :right 32 :top 122}
       :height 20)
     {:id nowCrossfade
      :when ~computed.crossfading
      :style caption
      :live polite
      :textColor @accent
      :anchors {:left 216 :top 150}
      :text ~(t "now.crossfading" {:title computed.nextTitle})}
     ~(visualiser)
     {:id lyricsCaption :style caption :text ~(t "now.lyrics") :anchors {:left 32 :top 356}}
     (stack#lyrics :anchors {:left 32 :right 32 :top 380} :height 140 :gap 2 :role list :label ~(t "now.lyrics")
       ~(repeat [line computed.lyricWindow]
         `(label#lyric :role listitem
            :text ~line.text
            :font ~(if line.current "heading" "default")
            :textColor ~(if line.current "@text" "@muted")
            :anchors {:left 0 :right 0}
            :height 26
            :align {:horizontal start :vertical center}
            :cursor hand
            :focusable true
            :hover {:textColor @text}
            :tooltip ~(t "now.seekLyric")
            :onClick {:run seekLyric :with {:offset ~(- index 2)}})))))

(defcomponent soundPage "The equaliser on a curves control, its presets and bands, the crossfade, the theme and the language"
  {}
  `(node :dir auto :color transparent :scroll vertical
     {:id soundTitle :style pageTitle :text ~(t "menu.sound")}
     {:id equaliserCaption :style caption :text ~(t "sound.equaliser") :anchors {:left 32 :top 80}}
     (label#equaliserNote :text ~(t "sound.equaliserNote")
       :font small
       :textColor @muted
       :anchors {:left 140 :top 80}
       :fitText true)
     (label#equaliserOnLabel :text ~(t "sound.equaliserOn")
       :font small
       :textColor @muted
       :anchors {:right 84 :top 78}
       :fitText true)
     (toggle#equaliserOn :role switch
       :label ~(t "sound.equaliserOn")
       :focusable true
       :anchors {:right 32 :top 76}
       :width 40
       :height 22
       :checked ~state.eqOn
       :trackColor @faint
       :activeColor @accent
       :onChange {:set state.eqOn :to ~event.value})
     (curves#equaliser :anchors {:left 32 :right 32 :top 108}
       :height 168
       :focusable true
       :label ~(t "sound.curve")
       :tooltip ~(t "sound.equaliserNote")
       :bind state.eq
       :color @surface
       :radius 8
       :accentColor @accent)
     (grid#bands :anchors {:left 32 :right 32 :top 284} :height 20 :columns 5
       ~(repeat [band state.bands]
         `(label#band :font small
            :align center
            :textColor ~(if state.eqOn "@text" "@muted")
            :height 20
            :text ~(str band.label
                        "  "
                        (if (> (round (* (- (curve state.eq band.at) 0.5) 24)) 0) "+" "")
                        (round (* (- (curve state.eq band.at) 0.5) 24))
                        " dB"))))
     (stack#presets :direction row
       :anchors {:left 32 :right 32 :top 314}
       :height 30
       :gap 8
       :role toolbar
       :label ~(t "sound.presets")
       ~(repeat [preset (keys state.presets)]
         `{:id preset :style chip :text ~(t (str "preset." preset)) :onClick {:run setPreset :with {:preset ~preset}}}))
     {:id crossfadeCaption :style caption :text ~(t "sound.crossfade") :anchors {:left 32 :top 364}}
     (slider#crossfade :anchors {:left 32 :top 388}
       :width 240
       :height 16
       :min 0
       :max 12
       :step 1
       :focusable true
       :label ~(t "sound.crossfadeLabel")
       :bind settings.playback.crossfade
       :trackColor @faint
       :activeColor @accent
       :knobColor @text)
     (label#crossfadeValue :text ~(t "sound.crossfadeValue" {:n settings.playback.crossfade})
       :textColor @text
       :anchors {:left 288 :top 386}
       :fitText true)
     {:id appearanceCaption :style caption :text ~(t "sound.appearance") :anchors {:left 32 :top 428}}
     (stack#themes :direction row
       :anchors {:left 32 :right 32 :top 452}
       :height 30
       :gap 8
       :role toolbar
       :label ~(t "theme.toggle")
       ~(repeat [theme ["dark" "light"]]
         `{:id themeChoice
           :style chip
           :text ~(t (str "theme." theme))
           :role radio
           :checked ~(= state.theme theme)
           :color ~(if (= state.theme theme) "@accentSoft" "@button")
           :onClick {:set state.theme :to ~theme}}))
     (stack#languages :direction row
       :anchors {:left 32 :right 32 :top 490}
       :height 30
       :gap 8
       :role toolbar
       :label ~(t "language.toggle")
       ~(repeat [language ["en" "es" "fr"]]
         `{:id languageChoice
           :style chip
           :text ~(t (str "language." language))
           :role radio
           :checked ~(= settings.ui.language language)
           :color ~(if (= settings.ui.language language) "@accentSoft" "@button")
           :onClick {:set settings.ui.language :to ~language}}))))

(defview visualiser "32 bars from the spectrum of the sound playing (its \"bands\"), and their faint reflection; low and flat when nothing plays, or the host gives no sound" []
  `(node#visualiser :note "32 bars from the spectrum of the sound playing (its \"bands\"), and their faint reflection; low and flat when nothing plays, or the host gives no sound"
     :color @surface
     :anchors {:left 32 :right 32 :top 216}
     :height 120
     :radius 8
     :role img
     :label ~(t "now.visualiser")
     (path#visualiserBars :d ~computed.bars
       :viewBox "0 0 320 120"
       :fit fill
       :anchors {:left 8 :right 8 :top 0 :bottom 0}
       :fillGradient {:kind linear :angle 90 :stops [{:at 0 :color @accentHover} {:at 1 :color @accent}]})
     (path#visualiserReflections :d ~computed.barReflections
       :viewBox "0 0 320 120"
       :fit fill
       :anchors {:left 8 :right 8 :top 0 :bottom 0}
       :fillColor @accentSoft)))

