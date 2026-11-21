;;; done.lisp --- the success screen: what just went right, for whom, and the one way on
;;;
;;; A finished flow (a new password, a deleted account, a new account) ends here instead of in a notice that leaves after a few
;;; seconds: a tick, a title, what changed and the button that goes on. state.done says which flow finished.

(ns sign-in.done)

;;; The screen

(defmacro done-icon "An icon by the kind of flow that finished: a deleted account's, a new account's, else a reset's."
  [kind deleted created other]
  `(case ~kind "deleted" ~deleted "created" ~created ~other))

(defview done-card "The success screen: its card, centred, with the tick, the words and the way on." []
  `(node#panel-done :style screen
     (node#card-done :style ~computed.cardStyle :width ~computed.cardWidth :maxHeight ~(- window.height 116)
       (node#card-done-top :height 44 :color transparent)
       ~(done-form)
       (node#card-done-end :height 36 :color transparent))))

(defview done-form "The tick on its soft ring, the title, what changed, the main button and the other way on." []
  `(node#stack-done :style form
     :role status
     :live polite
     :label ~(t "doneTitle" {:kind state.done.kind :name state.done.name})
     (row#row-done-badge :height 84 :justify center :anchors {:left 0 :right 10}
       (node#done-ring :style doneRing
         (label#done-badge :style doneBadge :icon ~(done-icon state.done.kind "user-x" "party-popper" "check"))))
     (node#spacer-done :style spacer :height 10)
     (node#label-done-title :style title
       :align center
       :role heading
       :text ~(t "doneTitle" {:kind state.done.kind :name state.done.name}))
     (node#label-done-text :style caption
       :align center
       :wrap true
       :height 44
       :text ~(t "doneText" {:kind state.done.kind :email state.done.email :team state.done.team}))
     (node#spacer-done-button :style spacer :height 10)
     (node#doneButton :style primary
       :icon ~(done-icon state.done.kind "arrow-left" "arrow-right" "log-in")
       :text ~(t "doneButton" {:kind state.done.kind})
       :onClick leave-done)
     (node#doneOther :when ~(= state.done.kind "deleted") :style link :text ~(t "doneOther") :onClick toCreate)))

;;; What its buttons run

(def no-done "No flow finished: what the success screen starts from, and what it is reset to on leaving."
  {:email "" :kind "" :name "" :team false})

(defn show-done
  "The success screen for a flow just finished: its kind (reset, deleted or created), its email and, for a new account,
  its name and whether it joined a team. No notice stays over it."
  [done]
  (set "state.done" (merge no-done done))
  (hide-toast)
  (navigate "done")
  (after 0 focus-on "#doneButton"))

(defn leave-done "The success screen's button: a new account's page, else the sign-in form at the step it was left on."
  []
  (let [created (= state.done.kind "created")]
    (set "state.done" no-done)
    (if created
        (navigate "signedIn")
        (do (navigate "signInForm")
            (after 0 focus-on "#textarea-placeholder")))))
