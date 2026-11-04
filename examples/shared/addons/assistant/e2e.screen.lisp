;;; e2e.screen.lisp --- The assistant add-on end to end (e2e_test.py): a page that includes it, where a timer stands
;;; in for the person
;;;
;;; It asks once before any agent is connected (the offline answer), then, once one is, asks for a new title and
;;; accepts the proposal card that comes back, then asks for a colour and rejects that card, as a person would with
;;; the card's buttons.

(include "assistant.lisp")

;;; Definitions

(defstate step offline)

(defpart e2e "The end-to-end screen's state, its first steps and how the person decides."
  :onLoad [{:run assistant.open} {:run assistant.ask :with {:text "Is anyone there?"}} {:set state.step :to waiting}]
  :actions {:person.decide [{:if ~(= state.step "rename")
                             :then [{:run assistant.accept :with {:id ~card.id}}
                                    {:set state.step :to recolour}
                                    {:run assistant.ask :with {:text "Make the title red"}}]
                             :else [{:run assistant.reject :with {:id ~card.id}} {:set state.step :to done}]}]})

;;; The screen

(defscreen main
  :window {:title "Assistant end to end" :width 800 :height 700}
  :fonts {:default {:file "../../fonts/Roboto-Regular.ttf" :size 14}}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio]}
  (node :color "#ffffff"
    (label#pageTitle :text "Tasks" :textColor "#1f2328" :anchors {:left 24 :top 24} :fitText true)
    (node#person :anchors {:left 0 :top 0}
      :width 1
      :height 1
      :every {:ms 100
              :do [{:if ~(and (= state.step "waiting") (> (count peers) 0))
                    :then [{:set state.step :to rename} {:run assistant.ask :with {:text "Rename the page to Board"}}]}
                   {:forEach ~(filter (fn [it] (= it.kind "card")) state.assistant.messages)
                    :as card
                    :do [{:if ~(some? (find (fn [it] (= it.id card.id)) proposals))
                          :then [{:run person.decide :with {:card ~card}}]}]}]})))
