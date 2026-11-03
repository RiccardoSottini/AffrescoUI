;;; state.lisp --- what Calamaio keeps while it runs: the buffers, the windows, the minibuffer, the mark and the modes
;;;
;;; Each defstate is one key of the state and the value it starts from. The buffers are kept by id, the most recent
;;; first in `order`; the windows by id, laid out in groups across `layout.dir`. The sample home folder is in home.lisp,
;;; the kill ring's own state with the kill ring (killring.lisp). Below, what every command works out from them.

;;; Buffers

(defstate bufs
  {:b0 {:name "*Calamaio*" :mode fundamental :kind splash :text "" :readOnly true}
   :b1 {:name "*scratch*" :mode lisp-interaction :pt {:line 4 :column 1} :text ""}
   :b2 {:name "*Messages*" :mode messages :readOnly true :text ""}}
  "The buffers by id: a name, a major mode, the text and where point was; the start screen is one, of its own kind.")

(defstate order [b0 b1 b2] "The buffers' ids, the most recently shown first.")

(defstate nextBuf 3 "The number of the next buffer's id.")

;;; Windows

(defstate win {:w1 {:buf b0 :caret {:line 1 :column 1 :selected 0} :view {}}}
  "The windows by id: the buffer each shows, its editor's caret and what its editor shows (view).")

(defstate sel w1 "The selected window.")

(defstate nextWin 2 "The number of the next window's id.")

(defstate layout {:dir column :groups [[w1]]}
  "How the windows are laid out: groups across dir (column: one above the other), each group's windows along it.")

(defstate menuBar {} "Which menus of the menu bar are open, by name.")

(defstate dir auto "The frame's direction: auto follows the language.")

;;; The echo area and the minibuffer

(defstate echo "" "What the echo area says.")

(defstate mb {:open false :prompt "" :text "" :kind "" :def "" :hist {} :hi -1 :comp nil :note ""}
  "The minibuffer: its prompt, the answer typed, the kind of question, the default, the histories, completions, a note.")

(defstate keyread {:on false :seq ""} "C-h k reading a key: the keys typed so far.")

;;; The mark, the argument and the kill ring

(defstate mark {:on false :line 1 :column 1} "The mark, and whether the region between it and point is active.")

(defstate arg nil "The numeric argument of the next command, or nil.")

(defstate argDigits false "Whether the argument was typed as digits (M-5), rather than by C-u.")

(defstate argU 0 "How many C-u the argument was typed with.")

(defstate kills [] "The kill ring, the newest kill first.")

(defstate killTmp "" "Where a kill's text lands before it goes in the ring.")

(defstate killAt nil "Where point was after the last kill, so the next one there joins it.")

(defstate yank nil "The last yank: its buffer, its kill's place in the ring, and where the text starts and ends.")

;;; Searching and replacing

(defstate is {:on false :text "" :back false :step first :req 0 :start {:line 1 :column 1} :last nil}
  "The incremental search: what is searched for, which way, its editor request, where it started, and the last search.")

(defstate qr {:on false :from nil :to "" :step first :req 0 :replace "" :done 0 :at nil}
  "The query replace: what is replaced with what, its editor request, how many were replaced and where the last was.")

(defstate customQuery "" "What Customize's search shows.")

;;; Modes

(defstate modes
  {:fundamental {:title "Fundamental"}
   :text {:title "Text" :menu text}
   :lisp-interaction {:title "Lisp Interaction" :menu lisp :language lisp :comment ";; "}
   :lisp {:title "Lisp" :menu lisp :language lisp :comment ";; "}
   :python {:title "Python" :menu python :grammar python :comment "# "}
   :c {:title "C" :menu c :grammar c :comment "// "}
   :markdown {:title "Markdown" :menu outline :grammar markdown}
   :org {:title "Org" :menu outline :language org}
   :messages {:title "Messages"}
   :help {:title "Help"}
   :dired {:title "Folder"}
   :customize {:title "Customize"}
   :buffer-menu {:title "Buffer List"}}
  "The major modes: the mode line's name, the menu, the colours (a grammar or a language) and the comment marker.")

(defstate modeFor {:py python :c c :h c :el lisp :lisp lisp :md markdown :org org :txt text}
  "The major mode a file's extension gives.")

;;; What every command works out

(defcomputed ed "The selected window's editor, as a node." (str "#ed" state.sel))

(defcomputed n "The count of the next command: its argument, else 1." (or-else state.arg 1))

(defcomputed cur "The id of the selected window's buffer." (get-in state.win [state.sel :buf]))

(defcomputed buf "The selected window's buffer." (get state.bufs (get-in state.win [state.sel :buf])))

(defcomputed text "Whether the selected window shows text, rather than the start screen."
  (not= (get-in state.bufs [(get-in state.win [state.sel :buf]) :kind]) "splash"))

(defcomputed wins "The windows, in the order they are on the frame."
  (split (join "," (map (fn [it] (join "," it)) state.layout.groups)) ","))

(defcomputed other "The buffer C-x b offers first: the most recent other than this one."
  (or-else (find (fn [it]
                   (and (not= it (get-in state.win [state.sel :buf])) (not= (get-in state.bufs [it :kind]) "splash")))
                 state.order)
           (get-in state.win [state.sel :buf])))

(defcomputed names "The buffers' names, in their order." (map (fn [it] (get-in state.bufs [it :name])) state.order))

(defcomputed commandNames
  "Every command's name, sorted: what M-x completes." (sortText (map (fn [it] it.name) commands)))
