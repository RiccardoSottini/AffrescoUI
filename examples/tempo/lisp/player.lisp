;;; player.lisp --- TODO

(defview player-view "#player." []
  `(node#player :when ~(not state.mini)
     :anchors {:left 12 :right 12 :bottom 40}
     :height 64
     :radius 16
     :color @bar
     :borderWidth 1
     :borderColor @border
     :role region
     :label ~(t "queue.now")
     {:id shuffleButton
      :style control
      :icon shuffle
      :anchors {:left 12 :top 16}
      :width 32
      :textColor ~(if state.shuffle "@accent" "@muted")
      :checked ~state.shuffle
      :label ~(t "player.shuffle")
      :tooltip ~(t "player.shuffle")
      :onClick toggleShuffle}
     {:id label-previous
      :style control
      :icon skip-back
      :anchors {:left 48 :top 16}
      :width 32
      :label ~(t "player.previous")
      :tooltip ~(t "player.previous")
      :onClick previous}
     {:id playPause
      :style control
      :icon ~(if state.isPlaying "pause" "play")
      :anchors {:left 86 :top 12}
      :width 40
      :height 40
      :radius 12
      :color @accent
      :hover {:color @accentHover}
      :focused {:color @accentHover}
      :textColor @onAccent
      :label ~(t "player.playPause")
      :tooltip ~(t "player.playPause")
      :onClick playPause}
     {:id label-next
      :style control
      :icon skip-forward
      :anchors {:left 132 :top 16}
      :width 32
      :label ~(t "player.next")
      :tooltip ~(t "player.next")
      :onClick next}
     {:id repeatButton
      :style control
      :icon ~(if (= state.repeat "one") "repeat-1" "repeat")
      :anchors {:left 168 :top 16}
      :width 32
      :textColor ~(if (= state.repeat "off") "@muted" "@accent")
      :checked ~(not= state.repeat "off")
      :label ~(t (str "player.repeat." state.repeat))
      :tooltip ~(t (str "player.repeat." state.repeat))
      :onClick cycleRepeat}
     {:id panel-player :anchors {:left 212 :top 14 :bottom 14} :width 1 :color @border}
     ~(now-card)
     (path#levelMeter :d ~computed.meter
       :viewBox "0 0 32 20"
       :fit fill
       :anchors {:right 228 :top 22}
       :width 32
       :height 20
       :stroke @accent
       :strokeWidth 2
       :role img
       :label ~(t "player.level")
       :tooltip ~(t "player.level"))
     {:id miniButton
      :style control
      :icon minimize-2
      :anchors {:right 188 :top 16}
      :width 32
      :radius 10
      :textColor @muted
      :label ~(t "player.mini")
      :tooltip ~(t "player.mini")
      :onClick toggleMini}
     {:id queueButton
      :style control
      :icon list-ordered
      :anchors {:right 152 :top 16}
      :width 32
      :radius 10
      :color ~(if state.queueOpen "@accentSoft" "@bar")
      :textColor ~(if state.queueOpen "@accent" "@muted")
      :checked ~state.queueOpen
      :label ~(t "player.queue")
      :tooltip ~(t "player.queue")
      :onClick toggleQueue}
     {:id muteButton
      :style control
      :icon ~(if state.muted "volume-x" "volume-2")
      :anchors {:right 120 :top 16}
      :width 28
      :radius 10
      :textColor ~(if state.muted "@accent" "@muted")
      :checked ~state.muted
      :label ~(t "player.mute")
      :tooltip ~(t "player.mute")
      :onClick toggleMute}
     (slider#volume :anchors {:right 16 :top 24}
       :width 100
       :height 16
       :step 5
       :focusable true
       :label ~(t "player.volume")
       :bind state.volume
       :trackColor @faint
       :activeColor @muted
       :knobColor @text
       :tooltip ~(t "player.volume"))))

(defview now-card "#nowCard." []
  `(node#nowCard :dir auto :anchors {:left 224 :right 268 :top 0 :bottom 0} :color transparent
     (image#playingCover :file ~computed.playing.cover
       :fit cover
       :radius 10
       :anchors {:left 0 :top 10}
       :width 44
       :height 44
       :cursor hand
       :focusable true
       :role button
       :label ~(t "now.show" {:name computed.playingName})
       :tooltip ~(t "now.show" {:name computed.playingName})
       :onClick [{:if ~(>= state.playingList 0)
                  :then [{:run openList :with {:list ~state.playingList}}]
                  :else [{:set state.page :to library}]}])
     (label#nowPlaying :anchors {:left 56 :right 36 :top 9}
       :height 20
       :runs [{:text ~computed.track.title :color @text} {:text ~(str "  ·  " computed.track.artist) :color @muted :size 12}]
       :label ~(str computed.track.title ", " computed.track.artist))
     {:id likeButton
      :style control
      :icon heart
      :anchors {:right 0 :top 2}
      :width 28
      :color @bar
      :textColor ~(if (contains state.liked computed.playingKey) "@accent" "@muted")
      :checked ~(contains state.liked computed.playingKey)
      :label ~(t "player.like")
      :tooltip ~(t "player.like")
      :onClick likeSong}
     {:id elapsed :style caption :text ~computed.elapsedText :anchors {:left 56 :top 37}}
     (slider#seek :anchors {:left 96 :right 44 :top 36}
       :height 16
       :max ~computed.duration
       :step 1
       :focusable true
       :label ~(t "player.seek")
       :bind state.elapsed
       :trackColor @faint
       :activeColor @accent
       :knobColor @accent)
     {:id label-track-time-2 :style caption :text ~computed.track.time :anchors {:right 4 :top 37}}))

(defcomponent miniPlayer "The compact player: the whole window shows one card with the song, its controls and the seek bar"
  {}
  `(node :dir auto :color @background :role region :label ~(t "player.mini")
     ~(mini-card)))

(defview mini-card "#miniCard." []
  `(node#miniCard :anchors {:left ~(- (floor (/ window.width 2)) 200) :top ~(- (floor (/ window.height 2)) 96)}
     :width 400
     :height 192
     :radius 12
     :color @surface
     (image#miniCover :file
                      ~computed.playing.cover
                      :fit
                      cover
                      :radius
                      6
                      :anchors
                      {:left 20 :top 20}
                      :width
                      112
                      :height
                      112)
     (label#miniTitle :text ~computed.track.title
       :font heading
       :textColor @text
       :anchors {:left 148 :right 52 :top 24}
       :height 24)
     {:id miniArtist :style caption :text ~computed.track.artist :anchors {:left 148 :top 50}}
     {:id miniExpand
      :style control
      :icon maximize-2
      :anchors {:right 12 :top 12}
      :width 32
      :color @surface
      :focusable true
      :role button
      :label ~(t "player.full")
      :tooltip ~(t "player.full")
      :onClick toggleMini}
     {:id miniPrevious
      :style control
      :icon skip-back
      :anchors {:left 148 :top 88}
      :width 32
      :color @surface
      :focusable true
      :role button
      :label ~(t "player.previous")
      :onClick previous}
     {:id miniPlayPause
      :style control
      :icon ~(if state.isPlaying "pause" "play")
      :anchors {:left 192 :top 87}
      :width 34
      :height 34
      :radius 17
      :color @text
      :hover {:color @accentHover}
      :textColor @background
      :focusable true
      :role button
      :label ~(t "player.playPause")
      :onClick playPause}
     {:id miniNext
      :style control
      :icon skip-forward
      :anchors {:left 238 :top 88}
      :width 32
      :color @surface
      :focusable true
      :role button
      :label ~(t "player.next")
      :onClick next}
     {:id miniElapsed :style caption :text ~computed.elapsedText :anchors {:left 20 :top 152}}
     (slider#miniSeek :anchors {:left 64 :right 64 :top 152}
       :height 16
       :max ~computed.duration
       :step 1
       :focusable true
       :label ~(t "player.seek")
       :bind state.elapsed
       :trackColor @faint
       :activeColor @accent
       :knobColor @text)
     {:id miniTime :style caption :text ~computed.track.time :anchors {:right 20 :top 152}}))

