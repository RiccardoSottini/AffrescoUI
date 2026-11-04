;;; window.lisp --- one window of the frame: its fringes, the buffer's editor, a scroll bar and the mode line
;;;
;;; The window component is drawn once a window of the layout (layout.lisp), `w` its id. Inside it `win` is the
;;; window, `b` the buffer it shows, `on` whether it is the selected one and `v` what its editor shows (the first and
;;; last lines in view, and how many there are).

;;; The window

(defcomponent window
  "A window. Plain keys stay with the editor unless a node around it binds them: the function keys are here."
  {:w w1}
  `(column :grow 1 :basis 0 :alignSelf stretch
     :let {:win ~(get state.win w)
           :b ~(get state.bufs (get-in state.win [w :buf]))
           :on ~(= state.sel w)
           :v ~(get-in state.win [w :view])}
     :keys {:f10 menu-bar-open
            "*" {:when ~(and (= state.sel w) (or state.is.on state.qr.on)) :do search-key}}
     ~(window-body w)
     ~(mode-line)))

(defview window-body "The window's text between its fringes, and its scroll bar." [w]
  `(row :grow 1 :alignSelf stretch :color @bg
     (node :width 8 :alignSelf stretch :color @fringe)
     (node :grow 1 :basis 0 :alignSelf stretch :color @bg
       (node :when ~(= b.kind "splash") :use splash :with {:w ~w})
       (node :when ~(= b.kind "customize") :use settingsPage :anchors {:left 0 :top 0 :right 0 :bottom 0} :color @bg
         :with {:search ~state.customQuery :query state.customQuery})
       ~(buffer-editor w))
     (node :width 8 :alignSelf stretch :color @fringe)
     ~(scroll-bar)))

;;; The buffer

(defview buffer-editor
  "The buffer's text, in the editor: its own point, the variables' looks, and search's matches." [w]
  `(editor :id ~(str "ed" w)
     :when ~(and (not= b.kind "splash") (not= b.kind "customize"))
     :anchors {:left -7 :top -6 :right 0 :bottom 0}
     :bind ~(str "state.bufs." win.buf ".text")
     :caret ~(str "state.win." w ".caret")
     :viewport ~(str "state.win." w ".view")
     :font code
     :textColor @fg
     :color @bg
     :lineColor @line
     :selectionColor ~(if (and on (or state.is.on state.qr.on)) "@isearch" "@region")
     :caretColor @cursor
     :caretStyle ~(get {:box "block" :bar "line" :hbar "underline"} (get settings "cursor-type"))
     :lineNumbers ~(get settings "global-display-line-numbers-mode")
     :lineNumberColor @kit.dim
     :activeLineNumberColor @fg
     :wrap ~(get settings "visual-line-mode")
     :brackets ~(get settings "show-paren-mode")
     :rulers ~(if (get settings "display-fill-column-indicator-mode") [(get settings "fill-column")] [])
     :guideColor @barLine
     :readOnly ~(= b.readOnly true)
     :grammar ~(or-else (get-in state.modes [b.mode :grammar]) "")
     :language ~(or-else (get-in state.modes [b.mode :language]) "")
     :matchColor ~(if (get settings "search-highlight") "@lazy" "transparent")
     :bracketColor @paren
     :onFocus ~(handler select-window w)
     :onChange ~(handler mark-modified w)
     :find ~(when on computed.findRequest)
     :keys ~(editor-keys w)))

(defview editor-keys
  "The keys a window's editor leaves to Calamaio: a listing's RET, g and q, and an argument's digits." [w]
  (merge `{:enter {:when ~(and (= state.sel w) (or (= b.mode "dired") (= b.mode "buffer-menu")) (not state.is.on))
                   :do listing-visit}
           :g {:when ~(and (= state.sel w) (or (= b.mode "dired") (= b.mode "buffer-menu")) (not state.is.on))
               :do listing-revert}
           :q {:when ~(and (= state.sel w) (= b.readOnly true) (not= b.kind "splash") (not= b.mode "messages")
                           (not state.is.on) (not state.qr.on))
               :do quit-window}}
         (zipmap (map str (range 10))
                 (map (fn [d] `{:when ~(and (some? state.arg) state.argDigits) :do digit-argument :with {:d ~d}})
                      (range 10)))))

(defcomputed findRequest
  "What the selected window's editor finds: the search's text, or the replace's, with the request that moves it."
  (cond (and state.is.on (not= state.is.text ""))
        {:text state.is.text :caseSensitive (not= (lower-case state.is.text) state.is.text) :step state.is.step
         :request state.is.req}
        state.qr.on
        {:text state.qr.from :caseSensitive (not= (lower-case state.qr.from) state.qr.from) :step state.qr.step
         :request state.qr.req :replace state.qr.replace}
        :else nil))

(defview scroll-bar "The scroll bar: a thumb as tall as the share of the buffer in view, where that share is." []
  `(node :width 10 :alignSelf stretch :color @track
     :let {:lines ~(max 1 (or-else v.lines 1))
           :shown ~(min 1 (max 0.05 (/ (+ (- (or-else v.last 1) (or-else v.first 1)) 1) lines)))}
     (node :x 2 :width 6 :radius 3 :color @thumb
       :y ~(str (fixed (* 100 (min (- 1 shown) (/ (- (or-else v.first 1) 1) lines))) 2) "%")
       :height ~(str (fixed (* 100 shown) 2) "%"))))

;;; The mode line

(defview mode-line
  "The mode line: the buffer's name in a chip, a lock if it is read-only, its mode, and where point is." []
  `(row :height 26 :alignSelf stretch :alignItems center :gap 8 :color ~(if on "@ml" "@mli")
     :let {:top ~(or-else v.first 1)
           :bottom ~(or-else v.last 1)
           :lines ~(or-else v.lines 1)
           :line ~(if (= b.kind "splash") 1 (or-else win.caret.line 1))
           :column ~(if (= b.kind "splash") 1 (or-else win.caret.column 1))}
     :label ~(str (t "modeLine.label" {:name b.name :mode (get-in state.modes [b.mode :title])})
                  (if b.dirty (t "modeLine.modified") "")
                  (if b.readOnly (t "modeLine.readOnly") ""))
     (node :width 4 :height 1 :color transparent)
     (label :fitText true :height 18 :padding 8 :radius 9
       :color ~(if on "@mlChip" "@mliBorder")
       :textColor ~(if on "@mlFg" "@mliFg")
       :runs [{:text ~(if b.dirty "● " "") :color ~(if on "@mlMark" "@mliFg")} {:text ~b.name :bold true}])
     (label :when ~(= b.readOnly true) :fitText true :runs [{:icon lock :font icons :size 14}]
       :textColor ~(if on "@mlFg" "@mliFg")
       :tooltip ~(t "modeLine.readOnlyTip"))
     (label :fitText true :textColor ~(if on "@mlFg" "@mliFg")
       :text ~(str (get-in state.modes [b.mode :title]) (if (get settings "visual-line-mode") (t "modeLine.wrap") "")))
     (node :grow 1 :height 1 :color transparent)
     (label :fitText true :textColor ~(if on "@mlFg" "@mliFg")
       :text ~(t "modeLine.position"
                 {:line (str line) :column (str (- column 1)) :where (where-in-view top bottom lines)}))
     (node :width 6 :height 1 :color transparent)))

(defmacro where-in-view
  "Where the window is in its buffer: All of it, its Top, its Bottom, or a percentage." [top bottom lines]
  `(cond (and (<= ~top 1) (>= ~bottom ~lines)) (t "modeLine.all")
         (<= ~top 1) (t "modeLine.top")
         (>= ~bottom ~lines) (t "modeLine.bottom")
         :else (str (floor (/ (* 100 (- ~top 1)) ~lines)) "%")))

;;; What its handlers do

(defn select-window "Makes a window the selected one, as its editor takes the focus." [w]
  (set "state.sel" w))

(defn mark-modified "Marks the buffer a window shows as changed." [w]
  (set (str "state.bufs." (get-in state.win [w :buf]) ".dirty") true))
