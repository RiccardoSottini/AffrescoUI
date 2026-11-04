;;; loop.lisp --- extending Calamaio from inside it, in the Lisp it is written in
;;;
;;; Type a form anywhere and evaluate it: `(+ 1 2)` shows its value, and a definition takes effect at once and lands in
;;; the history, where C-x C-z (`undo-evaluation`) takes it back and `M-x export-init` carries it to another copy. What
;;; you type runs as you. Text you didn't type here, whoever put it in the buffer, a file you opened, what you pasted or
;;; yanked, has none of your authority: C-x C-e, C-M-x, C-j and M-x eval-region ask you first, in a question the window
;;; draws itself with the whole of it, and run it as you, once, only if you say so. M-x eval-buffer doesn't ask: its
;;; definitions are a proposal, in its own name, and anything else in it doesn't run.

;;; Hook points of this editor, which your init.lisp can add to

(defhook after-save "Runs after a buffer is saved, with the buffer's name.")

(defn saved "What C-x C-s runs when it has saved." []
  (run-hooks after-save (:name computed.buf)))

;;; What is at point

(defn text-here "The text of the selected window's buffer." []
  (:text computed.buf))

(defn show-result "Shows what was evaluated in the echo area." [result error]
  (message (if error (t "echo.error" {:error error}) result)))

(defn insert-result "Shows what was evaluated in the buffer, after point, on a line of its own." [result error]
  (if error
    (message (t "echo.error" {:error error}))
    (edit "type" :text (str "\n" result "\n") :in computed.ed)))

(defn evaluate-form "Evaluates a form found at point, whichever way it was found." [form then]
  (if form
    (evaluate (:text form) :file "init.lisp" :confirm true :then then)
    (message (t "echo.noForm"))))

;;; The commands

(defcommand eval-last-sexp "Evaluates the form before point, and shows its value in the echo area."
  :title ~(t "command.eval-last-sexp") :category ~(t "category.lisp")
  :description ~(t "command.eval-last-sexp.about")
  []
  (let [at (caret)]
    (evaluate-form (formBefore (text-here) (:line at) (:column at)) show-result)))

(defcommand eval-print-last-sexp "Evaluates the form before point, and puts its value in the buffer: C-j in *scratch*."
  :title ~(t "command.eval-print-last-sexp") :category ~(t "category.lisp")
  :description ~(t "command.eval-print-last-sexp.about")
  []
  (let [at (caret)]
    (evaluate-form (formBefore (text-here) (:line at) (:column at)) insert-result)))

(defcommand eval-defun "Evaluates the definition around point: the next call uses it, and it is in the history."
  :title ~(t "command.eval-defun") :category ~(t "category.lisp")
  :description ~(t "command.eval-defun.about")
  []
  (let [at (caret)]
    (evaluate-form (formAround (text-here) (:line at) (:column at)) show-result)))

(defcommand eval-region "Evaluates every form in the region."
  :title ~(t "command.eval-region") :category ~(t "category.lisp")
  :description ~(t "command.eval-region.about")
  []
  (if (= (or (:selected (caret)) 0) 0)
    (message (t "echo.regionEmpty"))
    (do (edit "cut" :span "selection" :in computed.ed :into "state.killTmp" :keep true)
        (evaluate state.killTmp :file "init.lisp" :confirm true :then show-result))))

(defcommand eval-buffer "Evaluates every form in this buffer."
  :title ~(t "command.eval-buffer") :category ~(t "category.lisp")
  :description ~(t "command.eval-buffer.about")
  []
  (evaluate (text-here) :file "init.lisp" :then show-result))

;;; Taking back and accepting

(defcomputed proposals "The proposals waiting for review, the first the oldest." proposals)

;; The command undo is the editor's (C-/), so the program's (undo) would be it: taking back an evaluation is the step
;; undo, written as a step.
(defpart undo-evaluation "C-x C-z: the definition last evaluated goes back, exactly. What it did to the state stays."
  :actions {:undo-evaluation {:title ~(t "command.undo-evaluation")
                              :category ~(t "category.lisp")
                              :description ~(t "command.undo-evaluation.about")
                              :do [{:undo true}]}})

(defcommand accept-proposal "Accepts the first proposal waiting, as it stands."
  :title ~(t "command.accept-proposal") :category ~(t "category.lisp")
  :description ~(t "command.accept-proposal.about")
  []
  (let [first-waiting (first computed.proposals)]
    (accept (:id first-waiting) :head (:head first-waiting))))

;;; init.lisp: your own definitions, and what arrives from elsewhere

(def init-starter "What M-x find-init shows before you have defined anything."
  ";;; init.lisp --- your own definitions, in the language Calamaio is written in\n;;\n;; Typed here, a definition lands in the history as yours, with C-x C-e, and C-x C-z takes it\n;; back. M-x eval-buffer on this start, which you didn't type, is a proposal: M-x accept-proposal\n;; takes it, with no authority of yours. A file from elsewhere goes the same way, by M-x load-init.\n\n(defn insert-rule\n  \"Inserts a line of dashes at point.\"\n  []\n  (edit \"type\" :text \"----\\n\" :in computed.ed))\n\n(defcommand say-hello\n  \"Says hello in the echo area.\"\n  :title \"Say Hello\" :category \"Mine\"\n  []\n  (message \"Hello from your init file\"))\n\n(defkey \"ctrl+c h\" say-hello)\n\n(defn note-saved\n  \"Says what was saved, after C-x C-s.\"\n  [& given]\n  (message (str \"Saved \" (:name computed.buf))))\n\n(add-hook after-save note-saved)\n")

(defn on-proposed "Says where a file loaded went." [proposal]
  (message (t "echo.proposed" {:proposal proposal})))

(defn on-refused "Says why a file wasn't taken." [error]
  (message (t "echo.notLoaded" {:error error})))

(defcommand load-init
  "Loads this buffer as a file from elsewhere: a proposal in its own name, with no authority of yours."
  :title ~(t "command.load-init") :category ~(t "category.lisp")
  :description ~(t "command.load-init.about")
  []
  (propose (text-here) :as (:name computed.buf) :file "init.lisp" :then on-proposed :onError on-refused))

(defcommand find-init "Opens your init file: what you have evaluated so far, or else a start to evaluate."
  :title ~(t "command.find-init") :category ~(t "category.lisp")
  :description ~(t "command.find-init.about")
  []
  (let [mine (program-file "init.lisp")]
    (open-buffer {:name "init.lisp" :mode "lisp-interaction" :text (if (= mine "") init-starter mine)})))

(defn shown-changes "Opens a change set as a buffer of its own." []
  (open-buffer {:name "*Exported Init*" :mode "fundamental" :text (json state.exported)}))

(defcommand export-init
  "Exports what you have evaluated as a change set: another copy imports it, under no more authority."
  :title ~(t "command.export-init") :category ~(t "category.lisp")
  :description ~(t "command.export-init.about")
  []
  (exportChanges "init" :only "/program/init.lisp" :into "state.exported")
  (shown-changes))

;;; Asking the environment

(defn one-line "A description as a line." [found]
  (str (:name found) " (" (:kind found) "): " (or (:doc found) "")))

(defcommand apropos-command
  "Lists what is defined whose name holds what is in the minibuffer's last answer, or this buffer's word."
  :title ~(t "command.apropos-command") :category ~(t "category.help")
  :description ~(t "command.apropos-command.about")
  [text]
  (open-buffer {:name "*Apropos*" :mode "fundamental" :text (join "\n" (map one-line (apropos (or text ""))))}))

(defcommand what-is-here "Describes the definition around point, as it is known now."
  :title ~(t "command.what-is-here") :category ~(t "category.help")
  :description ~(t "command.what-is-here.about")
  []
  (let [at (caret)
        form (formAround (text-here) (:line at) (:column at))
        found (and form (describe-function (:name form)))]
    (message (if found (one-line found) (t "echo.notDefined")))))
