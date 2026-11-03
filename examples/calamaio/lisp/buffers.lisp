;;; buffers.lisp --- showing, opening, switching and killing buffers
;;;
;;; A buffer is an entry of state.bufs, by id; state.order lists them, the most recently shown first. Showing a buffer
;;; in the selected window keeps the point of the one it replaces, and puts the new one's back.

;;; Showing

(defn keep-point "Remembers where point is in the selected window's buffer, for when it is shown again." []
  (when computed.text
    (set (str "state.bufs." computed.cur ".pt") (point))))

(defn show "Shows a buffer in the selected window, as the most recent one, with its point where it was." [b]
  (keep-point)
  (set (str "state.win." state.sel ".buf") b)
  (set "state.order" (concat [b] (filter (fn [it] (not= it b)) state.order)))
  (set "state.mark.on" false)
  (after 20 goto-kept-point b))

(defn goto-kept-point "Puts point in the selected window where it was when the buffer was last shown." [b]
  (goto-point (str "#ed" state.sel) (or (:pt (get state.bufs b)) {:line 1 :column 1})))

(defn open-buffer "Shows a new buffer, under the next id." [buffer]
  (let [id (str "b" state.nextBuf)]
    (set (str "state.bufs." id) buffer)
    (show id)
    (increment "state.nextBuf")))

(defcommand quit-window "Shows the buffer before this one in the window (in a listing or a help buffer)."
  :title ~(t "command.quit-window") :category ~(t "category.buffers")
  :description ~(t "command.quit-window.about")
  []
  (show computed.other))

;;; Switching

(defcommand switch-to-buffer "Shows a buffer in the selected window, asking for its name."
  :title ~(t "command.switch-to-buffer") :category ~(t "category.buffers")
  :description ~(t "command.switch-to-buffer.about")
  []
  (let [name (get-in state.bufs [computed.other :name])]
    (minibuffer-read "buffer" (t "prompt.switchToBuffer" {:name name}) name "")))

(defn mb-buffer "Shows the buffer named, or a new one of that name." [value]
  (if (contains computed.names value)
    (show (get state.order (indexOf computed.names value)))
    (open-buffer {:name value :mode "fundamental" :text ""})))

;;; Killing

(defcommand kill-buffer "Kills a buffer, asking for its name."
  :title ~(t "command.kill-buffer") :category ~(t "category.files")
  :description ~(t "command.kill-buffer.about")
  []
  (minibuffer-read "kill" (t "prompt.killBuffer" {:name computed.buf.name}) computed.buf.name ""))

(defn mb-kill "Kills the buffer named." [value]
  (if (contains computed.names value)
    (kill-buffer-id (get state.order (indexOf computed.names value)))
    (message (t "echo.noBuffer" {:name value}))))

(defn kill-buffer-id "Kills a buffer: the windows that showed it show the most recent other one." [b]
  (when (= (count state.order) 1)
    (open-buffer {:name "*scratch*" :mode "lisp-interaction" :text ""}))
  (set "state.order" (filter (fn [it] (not= it b)) state.order))
  (map (fn [x] (set (str "state.win." x ".buf") (first state.order)))
       (filter (fn [x] (= (get-in state.win [x :buf]) b)) (keys state.win)))
  (remove-state (str "state.bufs." b))
  (focus computed.ed))
