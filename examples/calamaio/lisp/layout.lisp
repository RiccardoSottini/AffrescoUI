;;; layout.lisp --- the windows on the frame: how they are laid out, split, removed and selected
;;;
;;; state.layout holds groups of window ids: with dir column the groups are one above the other and each group's
;;; windows side by side; with dir row the other way round. C-x 2 and C-x 3 split the selected window, C-x 0 removes
;;; it, C-x 1 keeps only it, and C-x o selects the next. Each window keeps its own point, put back after a change of
;;; size.

;;; The windows, in their groups

(defview windows-in-rows "The groups one above the other, each a row of windows." []
  `(column#column-root-2 :when ~(= state.layout.dir "column") :grow 1 :gap 6 :alignSelf stretch
     ~(repeat [g state.layout.groups :key (join "," g)]
        `(row#row-g :grow 1 :basis 0 :gap 6 :alignSelf stretch
           ~(repeat [w g :key w] `(node#window-w :use window :with {:w ~w}))))))

(defview windows-in-columns "The groups side by side, each a column of windows." []
  `(row#row-root :when ~(= state.layout.dir "row") :grow 1 :gap 6 :alignSelf stretch
     ~(repeat [g state.layout.groups :key (join "," g)]
        `(column#column-g :grow 1 :basis 0 :gap 6 :alignSelf stretch
           ~(repeat [w g :key w] `(node#window-w-2 :use window :with {:w ~w}))))))

;;; Point, kept per window

(defn point "Where point is in the selected window: its line and column." []
  (let [at (caret)] {:line (:line at) :column (:column at)}))

(defn has-editor?
  "Whether a window shows an editor (not the start screen or Customize), from its editor's node, #ed<window>." [e]
  (let [kind (get-in state.bufs [(get-in state.win [(subs e 3) :buf]) :kind])]
    (and (some? (get state.win (subs e 3))) (not= kind "splash") (not= kind "customize"))))

(defn goto-point "Puts an editor's point at a line and a column, by its keys." [e pt]
  (focus computed.ed)
  (when (has-editor? e)
    (press "cmd+up" :in e)
    (when (> (:line pt) 1) (press "down" :in e :times (min 1000 (dec (:line pt)))))
    (press "cmd+left" :in e)
    (when (> (:column pt) 1) (press "right" :in e :times (min 1000 (dec (:column pt)))))))

(defn relayout "Keeps each window's point, and puts it back once the windows have their new sizes." []
  (map (fn [x] (set (str "state.win." x ".pt") (:caret (get state.win x)))) (keys state.win))
  (after 40 restore-points))

(defn restore-points "Puts each window's point back where relayout kept it." []
  (map (fn [x] (goto-point (str "#ed" x) (or (:pt (get state.win x)) {:line 1 :column 1}))) (keys state.win))
  (focus computed.ed))

;;; Splitting

(defn group-of "The place in the layout of the group that holds a window." [w]
  (indexOf (map (fn [g] (contains g w)) state.layout.groups) true))

(defn split-at
  "Adds a window n showing what the selected one shows: in a group of its own after group gi, or beside it."
  [n gi across]
  (let [here (get state.win state.sel)
        groups state.layout.groups]
    (set (str "state.win." n) {:buf (:buf here) :caret (:caret here) :pt (:caret here) :view {}})
    (increment "state.nextWin")
    (set "state.layout.groups"
         (if across
           (concat (take (inc gi) groups) [[n]] (drop (inc gi) groups))
           (map-indexed (fn [i g]
                          (if (= i gi)
                            (let [at (inc (indexOf g state.sel))] (concat (take at g) [n] (drop at g)))
                            g))
                        groups)))))

(defn split-window "Splits the selected window in two: one below the other, or side by side." [below]
  (if (>= (count computed.wins) 8)
    (message (t "echo.noRoom"))
    (do (when (= (count computed.wins) 1)
          (set "state.layout.dir" (if below "column" "row")))
        (split-at (str "w" state.nextWin) (group-of state.sel) (= below (= state.layout.dir "column")))
        (relayout))))

(defcommand split-window-below "Splits the selected window in two, one above the other."
  :title ~(t "command.split-window-below") :category ~(t "category.windows")
  :description ~(t "command.split-window-below.about")
  []
  (split-window true))

(defcommand split-window-right "Splits the selected window in two, side by side."
  :title ~(t "command.split-window-right") :category ~(t "category.windows")
  :description ~(t "command.split-window-right.about")
  []
  (split-window false))

;;; Removing

(defn delete-window-at "Removes a window, selecting the next one." [s next]
  (relayout)
  (set "state.layout.groups"
       (filter (fn [g] (> (count g) 0)) (map (fn [g] (filter (fn [x] (not= x s)) g)) state.layout.groups)))
  (remove-state (str "state.win." s))
  (set "state.sel" next)
  (focus (str "#ed" next)))

(defcommand delete-window "Removes the selected window; the others take its room."
  :title ~(t "command.delete-window") :category ~(t "category.windows")
  :description ~(t "command.delete-window.about")
  []
  (let [wins computed.wins
        before (get wins (max 0 (dec (indexOf wins state.sel))))]
    (if (= (count wins) 1)
      (message (t "echo.onlyWindow"))
      (delete-window-at state.sel (if (= before state.sel) (get wins 1) before)))))

(defcommand delete-other-windows "Makes the selected window fill the frame."
  :title ~(t "command.delete-other-windows") :category ~(t "category.windows")
  :description ~(t "command.delete-other-windows.about")
  []
  (relayout)
  (map (fn [x] (remove-state (str "state.win." x))) (filter (fn [x] (not= x state.sel)) (keys state.win)))
  (set "state.layout" {:dir "column" :groups [[state.sel]]}))

;;; Selecting

(defcommand other-window "Selects the next window, in the order they are on the frame."
  :title ~(t "command.other-window") :category ~(t "category.windows")
  :description ~(t "command.other-window.about")
  []
  (let [wins computed.wins]
    (set "state.sel" (get wins (mod (inc (indexOf wins state.sel)) (count wins)))))
  (set "state.mark.on" false)
  (focus computed.ed))
