;;; echo.lisp --- the echo area at the foot of the frame, and the *Completions* window above it
;;;
;;; The echo area shows the last message, a key sequence being typed (C-x-), the search or the replace in progress,
;;; or the minibuffer (minibuffer.lisp): a prompt, the answer being typed and a note. `message` is how every
;;; command speaks, and *Messages* keeps what it said.

;;; Messages

(defn message "Shows a text in the echo area, and keeps it in *Messages*." [m]
  (set "state.echo" m)
  (set "state.bufs.b2.text" (str state.bufs.b2.text m "\n")))

(defcomputed echoText
  "The echo area's line, the minibuffer closed: a prefix typed, the search, the replace's question, or the message."
  (cond (some? context.chord) (str (keyLabel context.chord "letters") "-")
        state.is.on (if (and (not= state.is.text "") (= (or-else (get-in state.win [state.sel :caret :matches]) 0) 0))
                      (if state.is.back
                        (t "echo.failingBackward" {:text state.is.text})
                        (t "echo.failing" {:text state.is.text}))
                      (if state.is.back
                        (t "echo.searchBackward" {:text state.is.text})
                        (t "echo.search" {:text state.is.text})))
        state.qr.on (t "echo.replaceQuestion" {:from state.qr.from :to state.qr.to})
        state.keyread.on (t "echo.describeKey"
                            {:keys (if (= state.keyread.seq "") "" (str (keyLabel state.keyread.seq "letters") "-"))})
        :else state.echo))

;;; The echo area

(defcomponent echoArea
  "The echo area: a message, or the minibuffer's prompt, answer and note; and the reader C-h k listens with." {}
  `(node :height 26 :color @bar
     (label :when ~(not state.mb.open) :font code :textColor @fg :anchors {:left 4 :top 5} :fitText true
       :text ~computed.echoText)
     (row :when ~state.mb.open :anchors {:left 4 :right 0 :top 4} :height 19
       (label :font code :textColor @prompt :fitText true :text ~state.mb.prompt)
       ~(minibuffer-answer)
       (label :font code :textColor @prompt :fitText true :text ~state.mb.note))
     (node#keyReader :when ~state.keyread.on :focusable true :width 1 :height 1 :label ~(t "echo.keyReader")
       :keys {"*" key-read})))

(defview minibuffer-answer
  "The answer being typed, as wide as it is: RET answers, Tab completes, M-p and M-n go through the history." []
  `(textarea#mb :grow 0 :height 19 :font code :textColor @fg :color @bar
     :bind state.mb.text
     :autofocus true
     :onSubmit minibuffer-exit
     :keys {:tab minibuffer-complete
            "alt+p" {:do minibuffer-history :with {:step 1}}
            :up {:do minibuffer-history :with {:step 1}}
            "alt+n" {:do minibuffer-history :with {:step -1}}
            :down {:do minibuffer-history :with {:step -1}}
            "ctrl+g" keyboard-quit}
     :onChange {:set state.mb.comp :to nil}
     :width ~(max 30 (+ (* (count state.mb.text) 8.4) 10))))

;;; *Completions*

(defcomponent completions
  "The *Completions* window: the candidates in columns, the typed part in its own face; a click chooses one."
  {}
  `(column :height ~(min 260 (+ 46 (* 19 (ceil (/ (count state.mb.comp) 4))))) :color @bg
     :let {:typed ~(count state.mb.text)}
     (node :grow 1 :alignSelf stretch :color @bg
       (label :font code :textColor @fg :anchors {:left 12 :top 3} :fitText true
         :text ~(t "completions.count" {:n (count state.mb.comp)}))
       ~(repeat [c state.mb.comp :index i] (completion c i)))
     ~(completions-mode-line)))

(defview completion
  "A candidate in its column: the part typed so far dimmed, the next character bold; a click chooses it." [c i]
  `(node :x ~(str (fixed (* 25 (mod i 4)) 2) "%") :y ~(+ 22 (* 19 (floor (/ i 4)))) :width "25%" :height 19
     :hover {:color @hl} :cursor hand :color @bg
     :onClick ~(handler minibuffer-choose c)
     (label :font code :textColor @fg :anchors {:left 12 :top 1} :fitText true
       :runs [{:text ~(slice c 0 typed) :color @compCommon}
              {:text ~(slice c typed (+ typed 1)) :bold true :color @compFirst}
              ~(slice c (+ typed 1))])))

(defview completions-mode-line "The *Completions* window's mode line, as an inactive window's." []
  `(row :height 26 :alignSelf stretch :alignItems center :gap 8 :color @mli
     (node :width 4 :height 1 :color transparent)
     (label :fitText true :height 18 :padding 8 :radius 9 :color @mliBorder :textColor @mliFg
       :runs [{:text "*Completions*" :bold true}])
     (label :fitText true :textColor @mliFg :text ~(t "completions.mode"))
     (node :grow 1 :height 1 :color transparent)
     (label :fitText true :textColor @mliFg :text ~(t "modeLine.all"))
     (node :width 6 :height 1 :color transparent)))
