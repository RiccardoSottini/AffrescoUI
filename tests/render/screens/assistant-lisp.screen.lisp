;;; assistant-lisp.screen.lisp --- The assistant add-on's Lisp version, for the rendering check and its unit test
;;;
;;; A small program that includes the add-on: the person's draft that reads as a form is shown as an evaluation
;;; before it runs, and Enter evaluates it as theirs. test.online stands in for a connected agent, as on
;;; assistant.screen.json, so the panel draws as it does with one.

(include "../../../examples/shared/addons/assistant/assistant.lisp")

;;; Definitions

(defn page-title "The page's title." [] "Notes")

(defpart test
  "The code font, what stands in for what the check's runner has none of (a connected agent, the runs it asked for),
a draft set by a step, not typed, and a command an agent may ask to run."
  :onLoad [{:set state.assistant.codeFont :to mono}]
  :actions {:test.note {:title "Add a note"
                        :description "Writes the page's note."
                        :arguments {:text {:type "string"}}
                        :do [{:set state.note :to "{{text}}"}]}
            :test.online [{:set state.assistant.simulate :to {:online true}}]
            :test.draft [{:set state.assistant.draft :to "(+ 2 2)"}]
            :test.runs [{:set state.assistant.simulate
                         :to {:online true
                              :pending []
                              :runs [{:id 1 :author "agent/claude" :command "addTask"
                                      :title "Add a task"
                                      :arguments {:title "Call the printer people" :due "2026-11-20"}
                                      :state "ran" :reason "" :time 1773501000000
                                      :changes 2 :errors [] :undoable "state" :personOnly false}
                                     {:id 2 :author "agent/claude" :command "archiveDone"
                                      :title "Archive the done tasks"
                                      :arguments {:olderThan 7}
                                      :state "denied" :reason "the person said no" :time 1773501005000
                                      :changes 0 :errors [] :undoable "none" :personOnly false}
                                     {:id 3 :author "agent/claude" :command "sendDigest"
                                      :title "Send the weekly\ndigest"
                                      :arguments {:to "team" :note "Line one\nline two"}
                                      :state "waiting" :reason "it writes to a service" :time 1773501010000
                                      :changes 0 :errors [] :undoable "none" :personOnly false}
                                     {:id 4 :author "agent/claude" :command "deleteAll"
                                      :title "Delete every task"
                                      :arguments {}
                                      :state "waiting" :reason "only the person may allow it" :time 1773501015000
                                      :changes 0 :errors [] :undoable "none" :personOnly true}]}}]})

;;; The screen

(defscreen main
  :window {:title "Assistant" :width 800 :height 700}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size 14}
          :mono {:file "../../../examples/shared/fonts/JetBrainsMono-Regular.ttf" :size 13}}
  :permissions {:allow [studio] :reviewers [studio]}
  (node :color "#eef1f4"
    (label#pageTitle :text ~(page-title) :textColor "#1f2328" :anchors {:left 24 :top 20} :fitText true)))
