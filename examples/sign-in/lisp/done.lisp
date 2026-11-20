;;; done.lisp --- the success screen: what just went right, for whom, and the one way on
;;;
;;; A finished flow (a new password, a deleted account) ends here instead of in a notice that leaves after a few
;;; seconds: a tick, a title, what changed and the button that goes on. state.done says which flow finished.

(ns sign-in.done)

;;; The screen

(defview done-card "The success screen: its card, centred, with the tick, the words and the way on." []
  `(node#panel-done :style screen
     (node#card-done :style ~computed.cardStyle :width ~computed.cardWidth :maxHeight ~(- window.height 116)
       (node#card-done-top :height 44 :color transparent)
       ~(done-form)
       (node#card-done-end :height 36 :color transparent))))

(defview done-form "The tick on its soft ring, the title, what changed, the main button and the other way on." []
  `(node#stack-done :style form :role status :live polite :label ~(t "doneTitle" {:kind state.done.kind})
     (row#row-done-badge :height 84 :justify center :anchors {:left 0 :right 10}
       (node#done-ring :style doneRing
         (label#done-badge :style doneBadge :icon ~(if (= state.done.kind "deleted") "user-x" "check"))))
     (node#spacer-done :style spacer :height 10)
     (node#label-done-title :style title :align center :role heading :text ~(t "doneTitle" {:kind state.done.kind}))
     (node#label-done-text :style caption
       :align center
       :wrap true
       :height 44
       :text ~(t "doneText" {:kind state.done.kind :email state.done.email}))
     (node#spacer-done-button :style spacer :height 10)
     (node#doneButton :style primary
       :icon ~(if (= state.done.kind "deleted") "arrow-left" "log-in")
       :text ~(t "doneButton" {:kind state.done.kind})
       :onClick leave-done)
     (node#doneOther :when ~(= state.done.kind "deleted") :style link :text ~(t "doneOther") :onClick toCreate)))

;;; What its buttons run

(defn show-done "The success screen for a flow just finished, by its kind and the email it was for; no notice over it."
  [kind email]
  (set "state.done" {:kind kind :email email})
  (hide-toast)
  (navigate "done")
  (after 0 focus-on "#doneButton"))

(defn leave-done "The success screen's button: the sign-in form, at the step the finished flow left it on." []
  (set "state.done" {:kind "" :email ""})
  (navigate "signInForm")
  (after 0 focus-on "#textarea-placeholder"))
