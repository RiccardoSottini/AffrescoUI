;;; registers.lisp --- registers (C-x r s, i, SPC, j) and rectangles (C-x r k, y, t)
;;;
;;; A register is a place named by a character that keeps a text or a position: C-x r s copies the region into one,
;;; C-x r i types its text back, C-x r SPC keeps where point is and C-x r j goes back there, in its buffer. While a
;;; command waits for a register's name, the selected window's editor gives the next key to register-key
;;; (window.lisp). A rectangle is the text between the mark's column and point's on every line from the mark's to
;;; point's: C-x r k kills it (C-x r y yanks it back at point, a line below the other), C-x r t types a text in its place.

(ns calamaio.registers)

;;; What it keeps

(defstate registers {} "The registers by name: {:text \"...\"} or {:buf \"b1\" :pt {:line 3 :column 1}}.")

(defstate regread {:on false :op ""} "A command waiting for a register's name: which one (s, i, SPC, j).")

(defstate regTmp "" "Where the region's text lands before it goes in a register.")

(defstate rect [] "The last rectangle killed, a text per line, each as wide as the rectangle.")

;;; Reading a register's name

(def register-prompts "The echo area's question while each command waits for a register's name."
  {"s" "echo.copyToRegister" "i" "echo.insertRegister" "SPC" "echo.pointToRegister" "j" "echo.jumpToRegister"})

(defn register-read "Waits for a register's name, for one of the register commands." [op]
  (set "state.arg" nil)
  (when computed.text
    (set "state.regread" {:on true :op op})
    (set "state.echo" (t (get register-prompts op)))))

(defn register-key "The key after a register command: a character names the register, C-g cancels." [event]
  (let [op state.regread.op
        r (typed-char event.key)]
    (set "state.regread.on" false)
    (cond (= event.key "ctrl+g") (keyboard-quit)
          (nil? r) (message (t "echo.registerBad" {:key (keyLabel event.key "letters")}))
          :else (do (set "state.echo" "")
                    (case op
                      "s" (copy-to r)
                      "i" (insert-from r)
                      "SPC" (point-to r)
                      "j" (jump-to r)
                      nil)))))

;;; Registers

(defn copy-to "Copies the region's text into a register." [r]
  (let [selected (or (:selected (caret)) 0)]
    (if (zero? selected)
      (message (t "echo.regionEmpty"))
      (do (set "state.regTmp" "")
          (edit "cut" :span "selection" :in computed.ed :into "state.regTmp" :keep true)
          (set (str "state.registers." r) {:text state.regTmp})
          (deactivate-mark)
          (message (t "echo.registerSaved" {:r r}))))))

(defn insert-from "Types a register's text at point." [r]
  (let [it (get state.registers r)]
    (cond (nil? it) (message (t "echo.registerEmpty" {:r r}))
          (nil? (:text it)) (message (t "echo.registerNotText" {:r r}))
          (:readOnly computed.buf) (read-only)
          :else (edit "type" :in computed.ed :text (:text it)))))

(defn point-to "Keeps where point is, and in which buffer, in a register." [r]
  (set (str "state.registers." r) {:buf computed.cur :pt (point)})
  (message (t "echo.registerPoint" {:r r})))

(defn jump-to "Puts point where a register keeps it, showing its buffer if it isn't shown." [r]
  (let [{:keys [buf pt] :as it} (get state.registers r)]
    (cond (nil? it) (message (t "echo.registerEmpty" {:r r}))
          (nil? pt) (message (t "echo.registerNotPoint" {:r r}))
          (nil? (get state.bufs buf)) (message (t "echo.registerGone" {:r r}))
          (= buf computed.cur) (goto-point computed.ed pt)
          :else (do (keep-point)
                    (set (str "state.bufs." buf ".pt") pt)
                    (show buf)))))

(defcommand copy-to-register "Copies the region into a register, asking for its name."
  :title ~(t "command.copy-to-register") :category ~(t "category.registers")
  :description ~(t "command.copy-to-register.about")
  []
  (register-read "s"))

(defcommand insert-register "Types the text a register keeps at point, asking for its name."
  :title ~(t "command.insert-register") :category ~(t "category.registers")
  :description ~(t "command.insert-register.about")
  []
  (register-read "i"))

(defcommand point-to-register "Keeps where point is in a register, asking for its name."
  :title ~(t "command.point-to-register") :category ~(t "category.registers")
  :description ~(t "command.point-to-register.about")
  []
  (register-read "SPC"))

(defcommand jump-to-register "Goes back to the place a register keeps, asking for its name."
  :title ~(t "command.jump-to-register") :category ~(t "category.registers")
  :description ~(t "command.jump-to-register.about")
  []
  (register-read "j"))

;;; Rectangles

(defn rectangle
  "The rectangle between the mark and point: its first and last lines, and its left and right columns (the right one
  just past it), with the buffer's lines."
  []
  (let [p (point) m state.mark]
    {:top (min (:line p) (:line m)) :bottom (max (:line p) (:line m))
     :left (min (:column p) (:column m)) :right (max (:column p) (:column m))
     :lines (str/split (or computed.buf.text "") "\n" -1)}))

(defn type-at "Types a text over columns of a line of the selected window's editor, at once." [l from to text]
  (edit "type" :in computed.ed :line l :column from :toLine l :toColumn to :text text))

(defn rectangle-edit
  "Replaces the rectangle's part of every line with a text (a line too short is first padded with spaces), and keeps
  what each line had there, padded to the rectangle's width, in state.rect when asked." [text keep]
  (when (editable?)
    (let [{:keys [top bottom left right lines]} (rectangle)]
      (when keep
        (set "state.rect"
             (vec (for [l (range top (inc bottom))]
                    (let [s (get lines (dec l) "")
                          piece (subs s (min (dec left) (count s)) (min (dec right) (count s)))]
                      (str piece
                           (spaces (- right left (count piece)))))))))
      (doseq [l (range top (inc bottom))]
        (let [len (count (get lines (dec l) ""))
              pad (spaces (- (dec left) len))]
          (cond (>= len (dec left)) (type-at l left (min right (inc len)) text)
                (not= text "") (type-at l (inc len) (inc len) (str pad text)))))
      (set "state.mark.on" false)
      (edit "goTo" :in computed.ed :line top :column left))))

(defcommand kill-rectangle "Kills the rectangle between the mark and point; C-x r y yanks it back."
  :title ~(t "command.kill-rectangle") :category ~(t "category.registers")
  :description ~(t "command.kill-rectangle.about")
  []
  (set "state.arg" nil)
  (rectangle-edit "" true))

(defcommand string-rectangle "Types a text in place of the rectangle between the mark and point, on every line."
  :title ~(t "command.string-rectangle") :category ~(t "category.registers")
  :description ~(t "command.string-rectangle.about")
  []
  (set "state.arg" nil)
  (when (editable?)
    (minibuffer-read "string-rectangle" (t "prompt.stringRectangle") "" "")))

(defn mb-string-rectangle "Types the answer in place of the rectangle." [value]
  (rectangle-edit value false))

(defcommand yank-rectangle "Yanks the last rectangle killed at point: its first line here, the next ones below."
  :title ~(t "command.yank-rectangle") :category ~(t "category.registers")
  :description ~(t "command.yank-rectangle.about")
  []
  (set "state.arg" nil)
  (cond (empty? state.rect) (message (t "echo.rectangleNone"))
        (editable?)
        (let [{:keys [line column]} (point)
              lines (str/split (or computed.buf.text "") "\n" -1)
              rows (map-indexed vector state.rect)
              inside (filter #(<= (+ line (first %)) (count lines)) rows)
              below (remove #(<= (+ line (first %)) (count lines)) rows)]
          (when (seq below)
            (let [end (end-of-text (or computed.buf.text ""))
                  indent (str "\n" (spaces (dec column)))]
              (type-at (:line end) (:column end) (:column end)
                       (str/join (for [[_ piece] below] (str indent piece))))))
          (doseq [[i piece] inside]
            (let [l (+ line i)
                  len (count (get lines (dec l) ""))
                  pad (spaces (- (dec column) len))]
              (type-at l (min column (inc len)) (min column (inc len)) (str pad piece))))
          (edit "goTo" :in computed.ed :line line :column column))))
