;;; queue.lisp --- the queue beside the page: the song playing, its waveform and time, and the songs up next, which
;;; drag or Alt and the arrows reorder

(ns tempo.queue)

;;; The queue

(defview queue-pane "The queue: the song playing, then the songs up next." []
  `(node#queuePane :when ~state.queueOpen :dir auto :color @surface :role complementary :label ~(t "queue.upNext")
     (node#label-now-playing :style caption :text ~(t "queue.now") :anchors {:left 20 :top 22})
     (image#queueCover
       :file ~computed.playing.cover
       :fit cover :color @button
       :radius 12
       :anchors {:left 20 :top 48}
       :width 96
       :height 96)
     (node#queueFrom :style caption :text ~(t "queue.from") :anchors {:left 132 :top 66})
     (label#queueFromName
       :text ~computed.playingName
       :textColor @text
       :anchors {:left 132 :right 20 :top 84}
       :height 20)
     (label#queueTitle
       :text ~computed.track.title
       :font heading
       :textColor @text
       :anchors {:left 20 :top 160}
       :fitText true)
     (label#queueArtist :text ~(str computed.track.artist "  ·  " computed.track.album)
       :textColor @muted
       :anchors {:left 20 :top 184}
       :fitText true)
     ~@(waveform)
     (node#queueTime
       :style caption
       :text ~(str computed.elapsedText " / " computed.track.time)
       :anchors {:left 20 :top 270})
     (node#label-up-next :style caption :text ~(t "queue.upNext") :anchors {:left 20 :top 304})
     (node#queueNote :style caption :live polite :text ~state.queueNote :anchors {:right 56 :top 304})
     (node#clearQueue :style control
       :when ~(seq computed.upNext)
       :icon list-x
       :font small
       :width 28
       :height 28
       :radius 14
       :color @surface
       :anchors {:right 16 :top 296}
       :label ~(t "queue.clear")
       :tooltip ~(t "queue.clear")
       :onClick clearQueue)
     ~(up-next)
     (node#queueEmpty
       :when ~(empty? computed.upNext)
       :style caption
       :text ~(t "queue.empty")
       :anchors {:left 20 :top 330})))

(defview waveform "The song's waveform, the part played in the accent colour: two paths over each other." []
  [`(path#wavePlayed :d ~computed.wavePlayed
      :viewBox "0 0 128 56"
      :fit fill
      :anchors {:left 20 :right 20 :top 216}
      :height 48
      :stroke @accent
      :strokeWidth 2.5
      :strokeCap round)
   `(path#waveLeft :d ~computed.waveLeft
      :viewBox "0 0 128 56"
      :fit fill
      :anchors {:left 20 :right 20 :top 216}
      :height 48
      :stroke @faint
      :strokeWidth 2.5
      :strokeCap round)])

(defview up-next "The songs up next: a song opened plays, a song dragged or moved with Alt and the arrows changes place." []
  `(list#queue :anchors {:left 8 :right 8 :top 326 :bottom 8}
     :color transparent
     :rowHeight 36
     :items ~computed.upNext
     :bind state.queuePick
     :label ~(t "queue.label")
     :textColor @muted
     :selectedTextColor @text
     :selectedBackground @rowHover
     :accentColor @accent
     :drag {}
     :onDrop drop-queued
     :onOpen jump-to
     :onContextMenu queue-menu
     :keys {"alt+up" ~(handler move-queued -1) "alt+down" ~(handler move-queued 1) :delete unqueue :backspace unqueue}))
