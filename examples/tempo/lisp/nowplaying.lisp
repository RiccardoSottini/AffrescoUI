;;; nowplaying.lisp --- now playing (the cover, the visualiser of the spectrum of what plays, the lyrics) and the
;;; sound page (the equaliser on a curves control, its presets, the crossfade, the theme and the language)

;;; The song playing

(defcomputed fade "How loud the song playing is through the crossfade: 1, falling to 0 over its last seconds."
  (if (and (> settings.playback.crossfade 0) (> (count state.queue) 0))
      (clamp (/ (- computed.duration state.elapsed) settings.playback.crossfade) 0 1)
      1))

(defcomputed crossfading "Whether the song playing is fading into the next."
  (and state.isPlaying (< computed.fade 1)))

(defcomputed nextTitle "The next song's title."
  (or-else computed.queueSongs.0.title ""))

(defcomputed lyricLine "The line of the lyrics sung now: one every 12 seconds, from a line the song gives."
  (mod (+ (floor (/ state.elapsed 12)) state.playingTrack) 10))

(defcomputed lyricWindow "Five lines of the lyrics round the one sung now, the third the current one."
  (map-indexed (fn [index it]
                 {:text (get state.lyrics (mod (+ computed.lyricLine index 8) 10)) :current (= index 2)})
               (range 5)))

(defcomputed spectrum "The spectrum of the sound playing, as its bands."
  (or-else (if (= state.soundOn "beat") state.beat.spectrum (get-in state.sounds [state.soundOn :spectrum])) []))

(defcomputed barHeights "The visualiser's 32 bars' heights: the spectrum, through the crossfade and the equaliser."
  (map-indexed (fn [index it]
                 (clamp (+ 0.04
                           (* (or-else (get computed.spectrum index) 0)
                              computed.fade
                              (if state.eqOn (* (curve state.eq (/ index 31)) 2) 1)))
                        0.04
                        1))
               (range 32)))

(defcomputed bars "The bars, as a path."
  (join " "
        (map-indexed (fn [index it] (str "M " (+ (* index 10) 2) " " (- 84 (* it 80)) " h 6 V 84 h -6 Z"))
                     computed.barHeights)))

(defcomputed barReflections "The bars' reflections, as a path."
  (join " "
        (map-indexed (fn [index it] (str "M " (+ (* index 10) 2) " 86 h 6 V " (+ 86 (* it 28)) " h -6 Z"))
                     computed.barHeights)))

;;; Now playing

(defcomponent nowPlayingPage "The song playing: its cover, the visualiser (the spectrum of what plays, as bars) and the lyrics, line by line" {}
  `(node :dir auto :color transparent
     (image#nowCover :file ~computed.playing.cover :fit cover :radius 8 :anchors {:left 32 :top 32} :width 160 :height 160 :role img
       :label ~computed.playingName)
     (node#nowCaption :style caption :text ~(if state.isPlaying (t "queue.now") (t "now.paused")) :anchors {:left 216 :top 56})
     (label#nowTitle :text ~computed.track.title :font large :textColor @text :anchors {:left 214 :right 32 :top 76} :height 40)
     (label#nowArtist :text ~(str computed.track.artist "  ·  " computed.track.album)
       :textColor @muted
       :anchors {:left 216 :right 32 :top 122}
       :height 20)
     (node#nowCrossfade :when ~computed.crossfading
       :style caption
       :live polite
       :textColor @accent
       :anchors {:left 216 :top 150}
       :text ~(t "now.crossfading" {:title computed.nextTitle}))
     ~(visualiser)
     (node#lyricsCaption :style caption :text ~(t "now.lyrics") :anchors {:left 32 :top 356})
     (stack#lyrics :anchors {:left 32 :right 32 :top 380} :height 140 :gap 2 :role list :label ~(t "now.lyrics")
       ~(repeat [line computed.lyricWindow] (lyric line)))))

(defview lyric "A line of the lyrics, the current one larger; a click goes to where it is sung." [line]
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
     :onClick ~(handler seekLyric (- index 2))))

;; low and flat when nothing plays, or the host gives no sound
(defview visualiser "32 bars from the spectrum of the sound playing (its bands), and their faint reflection." []
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

;;; The sound page

(defcomponent soundPage "The equaliser on a curves control, its presets and bands, the crossfade, the theme and the language" {}
  `(node :dir auto :color transparent :scroll vertical
     (node#soundTitle :style pageTitle :text ~(t "menu.sound"))
     ~@(equaliser)
     (grid#bands :anchors {:left 32 :right 32 :top 284} :height 20 :columns 5
       ~(repeat [band state.bands] (band-gain band)))
     (stack#presets :direction row :anchors {:left 32 :right 32 :top 314} :height 30 :gap 8 :role toolbar :label ~(t "sound.presets")
       ~(repeat [preset (keys state.presets)]
          `(node#preset :style chip :text ~(t (str "preset." preset)) :onClick ~(handler setPreset preset))))
     ~@(crossfade)
     (node#appearanceCaption :style caption :text ~(t "sound.appearance") :anchors {:left 32 :top 428})
     (stack#themes :direction row :anchors {:left 32 :right 32 :top 452} :height 30 :gap 8 :role toolbar :label ~(t "theme.toggle")
       ~(repeat [theme ["dark" "light"]] (theme-choice theme)))
     (stack#languages :direction row :anchors {:left 32 :right 32 :top 490} :height 30 :gap 8 :role toolbar :label ~(t "language.toggle")
       ~(repeat [language ["en" "es" "fr"]] (language-choice language)))))

(defview equaliser "The equaliser's caption and note, its switch, and its curve: five nodes." []
  [`(node#equaliserCaption :style caption :text ~(t "sound.equaliser") :anchors {:left 32 :top 80})
   `(label#equaliserNote :text ~(t "sound.equaliserNote") :font small :textColor @muted :anchors {:left 140 :top 80} :fitText true)
   `(label#equaliserOnLabel :text ~(t "sound.equaliserOn") :font small :textColor @muted :anchors {:right 84 :top 78} :fitText true)
   `(toggle#equaliserOn :role switch
      :label ~(t "sound.equaliserOn")
      :focusable true
      :anchors {:right 32 :top 76}
      :width 40
      :height 22
      :checked ~state.eqOn
      :trackColor @faint
      :activeColor @accent
      :onChange switchEqualiser)
   `(curves#equaliser :anchors {:left 32 :right 32 :top 108}
      :height 168
      :focusable true
      :label ~(t "sound.curve")
      :tooltip ~(t "sound.equaliserNote")
      :bind state.eq
      :color @surface
      :radius 8
      :accentColor @accent)])

(defmacro gain [band]
  ;; A macro, not a function: what it writes stays an expression.
  "A band's gain in decibels, from -12 to 12, as the curve gives it."
  `(round (* (- (curve state.eq (:at ~band)) 0.5) 24)))

(defview band-gain "A band of the equaliser and its gain." [band]
  `(label#band :font small
     :align center
     :textColor ~(if state.eqOn "@text" "@muted")
     :height 20
     :text ~(t "sound.band" {:label band.label :gain (str (if (> (gain band) 0) "+" "") (gain band))})))

(defview crossfade "The crossfade's caption, slider and seconds: three nodes." []
  [`(node#crossfadeCaption :style caption :text ~(t "sound.crossfade") :anchors {:left 32 :top 364})
   `(slider#crossfade :anchors {:left 32 :top 388}
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
   `(label#crossfadeValue :text ~(t "sound.crossfadeValue" {:n settings.playback.crossfade})
      :textColor @text
      :anchors {:left 288 :top 386}
      :fitText true)])

(defview theme-choice "A theme to choose, dark or light." [theme]
  `(node#themeChoice :style chip
     :text ~(t (str "theme." theme))
     :role radio
     :checked ~(= state.theme theme)
     :color ~(if (= state.theme theme) "@accentSoft" "@button")
     :onClick ~(handler setTheme theme)))

(defview language-choice "A language to choose: English, Spanish or French." [language]
  `(node#languageChoice :style chip
     :text ~(t (str "language." language))
     :role radio
     :checked ~(= settings.ui.language language)
     :color ~(if (= settings.ui.language language) "@accentSoft" "@button")
     :onClick ~(handler setLanguage language)))
