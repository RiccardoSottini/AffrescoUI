;;; assistant-lisp.screen.lisp --- The assistant add-on's Lisp version, for the rendering check and its unit test
;;;
;;; A small program that includes the add-on: the person's draft that reads as a form is shown as an evaluation
;;; before it runs, and Enter evaluates it as theirs. test.online stands in for a connected agent, as on
;;; assistant.screen.json, so the panel draws as it does with one.

(include "../../../examples/shared/addons/assistant/assistant.lisp")

;;; Definitions

(defn page-title "The page's title." [] "Notes")

(defpart test
  "The code font, what stands in for what the check's runner has none of (a connected agent), and a draft set by a
step, not typed."
  :onLoad [{:set state.assistant.codeFont :to mono}]
  :actions {:test.online [{:set state.assistant.simulate :to {:online true}}]
            :test.draft [{:set state.assistant.draft :to "(+ 2 2)"}]})

;;; The screen

(defscreen main
  :window {:title "Assistant" :width 800 :height 700}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size 14}
          :mono {:file "../../../examples/shared/fonts/JetBrainsMono-Regular.ttf" :size 13}}
  :permissions {:allow [studio] :reviewers [studio]}
  (node :color "#eef1f4"
    (label#pageTitle :text ~(page-title) :textColor "#1f2328" :anchors {:left 24 :top 20} :fitText true)))
