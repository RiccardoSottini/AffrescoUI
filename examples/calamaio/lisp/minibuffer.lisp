;;; minibuffer.lisp --- the minibuffer: a question in the echo area, its answer, completion and history
;;;
;;; A command asks with minibuffer-read: a kind of question, a prompt, a default and the text to start from. RET
;;; answers (the default if nothing was typed), and the answer goes to the function of that kind (answer, below).
;;; TAB completes what is typed from the kind's candidates: the only match, else the common prefix, else the list in
;;; *Completions*. M-p and M-n (or the arrows) go through the answers given before, one history a kind.

(ns calamaio.minibuffer)

;;; The candidates

(defcomputed mbCands "What an answer of this kind can be: commands, buffers, files, folders, themes or variables."
  (case state.mb.kind
    ("command" "function") computed.commandNames
    ("buffer" "kill") computed.names
    ("file" "write" "insert") (map :path state.files)
    "dired" (concat [state.home] (->> state.files (filter #(= (:dir %) true)) (map :path)))
    "theme" ["default" "inchiostro" "pergamena"]
    "variable" (map :key settingsList)
    []))

(defcomputed mbMatches "The candidates that start with what is typed, sorted."
  (sortText (filter #(= (slice % 0 (count state.mb.text)) state.mb.text) computed.mbCands)))

(defcomputed mbA "The first match." (or-else computed.mbMatches.0 ""))

(defcomputed mbB "The last match." (or-else (get computed.mbMatches (dec (count computed.mbMatches))) ""))

(defcomputed mbPrefix "What every match starts with: the first and the last, sorted, share it."
  (slice computed.mbA
         0
         (count (filter #(= (slice computed.mbA 0 (inc %)) (slice computed.mbB 0 (inc %)))
                        (range (min (count computed.mbA) (count computed.mbB)))))))

;;; Asking

(defn minibuffer-read
  "Opens the minibuffer on a question: its kind, its prompt, its default and the answer it starts from."
  [kind prompt default init]
  (set "state.mb"
       (merge state.mb {:open true :kind kind :prompt prompt :def default :text init :hi -1 :comp nil :note ""}))
  (set "state.echo" "")
  (focus "#mb"))

(defn minibuffer-close "Closes the minibuffer, and gives the selected window back its focus." []
  (set "state.mb.open" false)
  (set "state.mb.comp" nil)
  (focus computed.ed))

(defn answer "Gives an answer to the function its kind of question has." [kind value]
  (case kind
    "buffer" (mb-buffer value)
    "kill" (mb-kill value)
    "file" (mb-file value)
    "write" (mb-write value)
    "insert" (mb-insert value)
    "dired" (mb-dired value)
    "command" (mb-command value)
    "function" (mb-function value)
    "variable" (mb-variable value)
    "theme" (mb-theme value)
    "replace-from" (mb-replace-from value)
    "replace-to" (mb-replace-to value)
    nil))

(defn minibuffer-done
  "Closes the minibuffer, keeps the answer in its kind's history, and gives it to its function." [value kind]
  (minibuffer-close)
  (when (not= value "")
    (set (str "state.mb.hist." kind)
         (concat [value] (remove #{value} (or (get state.mb.hist kind) [])))))
  (answer kind value))

(defn minibuffer-exit "RET in the minibuffer: answers what is typed, or the default if nothing is." []
  (minibuffer-done (if (= state.mb.text "") state.mb.def state.mb.text) state.mb.kind))

(defn minibuffer-choose "A click on a completion: answers it." [value]
  (minibuffer-done value state.mb.kind))

;;; Completing

(defn clear-note "Clears the minibuffer's note, unless another took its place." [note]
  (when (= state.mb.note note) (set "state.mb.note" "")))

(defn minibuffer-note "Shows a note after the answer for a moment: [No match], [No earlier input]..." [note]
  (set "state.mb.note" note)
  (after 1200 clear-note note))

(defn caret-to-end "Puts the minibuffer's caret after its text." []
  (press "cmd+right" :in "#mb"))

(defn minibuffer-set "Replaces the answer typed so far." [value]
  (set "state.mb.text" value)
  (after 0 caret-to-end))

(defn minibuffer-complete "TAB: completes the only match, or the common prefix of the matches, or lists them." []
  (let [matches computed.mbMatches]
    (cond (empty? matches)
          (minibuffer-note (t "minibuffer.noMatch"))
          (= (count matches) 1)
          (do (minibuffer-set computed.mbA)
              (set "state.mb.comp" nil)
              (minibuffer-note (if (= state.mb.text computed.mbA) (t "minibuffer.onlyMatch") "")))
          (> (count computed.mbPrefix) (count state.mb.text))
          (minibuffer-set computed.mbPrefix)
          :else
          (set "state.mb.comp" matches))))

;;; History

(defn minibuffer-history "M-p and M-n: the answer given step answers before (1) or after (-1) the one shown." [step]
  (let [history (or (get state.mb.hist state.mb.kind) [])
        at (+ state.mb.hi step)]
    (cond (>= at (count history)) (minibuffer-note (t "minibuffer.noEarlier"))
          (< at -1) (minibuffer-note (t "minibuffer.noLater"))
          :else (do (set "state.mb.hi" at)
                    (minibuffer-set (if (< at 0) state.mb.def (get history at)))))))

;;; M-x

(defcommand execute-extended-command "Reads a command's name, with completion, and runs it."
  :title ~(t "command.execute-extended-command") :category ~(t "category.help")
  :description ~(t "command.execute-extended-command.about")
  []
  (minibuffer-read "command" "M-x " "" ""))

(defn mb-command "Runs the command named." [value]
  (if (contains computed.commandNames value)
    (run value)
    (message (t "echo.noCommand" {:name value}))))
