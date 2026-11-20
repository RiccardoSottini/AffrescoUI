;;; content.lisp --- What Insert puts on a slide besides text and shapes: tables and pictures
;;;
;;; A table is an object of kind `table` whose `cells` are rows of texts, the first row its header, the others banded
;;; in its `tint` (an accent of the theme). Insert > Table opens a grid of sizes; a double click on a cell edits it, and
;;; Tab and Shift+Tab go to the next and the previous cell (Tab in the last one adds a row). A picture is an object of
;;; kind `picture` whose `src` is the PNG or JPEG the person chose, held in the deck as a data URI, so the saved deck
;;; carries it.

(ns sinopia.parts.content)

;;; Definitions

(defstate tablePick {:open false :rows 0 :cols 0} "The table size grid open, and the size the pointer is over.")

(defstate cell [0 0] "The row and column of the table cell being edited.")

(defstate picture nil "The picture a file held, until it is put on the slide.")

(defcomputed tableSizes "The table grid's sizes, row by row: 6 rows of 8 columns."
  (map (fn [k]
         {:r (inc (floor (/ k 8)))
          :c (inc (mod k 8))})
       (range 48)))

(defcomponent tableGrid "Insert > Table: a grid of sizes; the pointer picks one, a click puts that table on the slide."
  {}
  `(column#table-grid :width 196
     :height 182
     :gap 6
     :radius 10
     :color @card
     :borderWidth 1
     :borderColor @rbLine
     :shadow {:y 4 :blur 14 :color "#00000033"}
     :role menu
     :label ~(t "table.insert")
     :overlay {:anchor "#b-ins-table" :placement below :open state.tablePick.open :dismiss outside :modal true}
     (node#table-grid-top :height 2 :color transparent)
     (label#table-grid-title :height 18
       :anchors {:left 10 :right 10}
       :textColor @rbInk
       :runs [{:text ~(if (pos? state.tablePick.rows)
                        (t "table.size" {:rows state.tablePick.rows :cols state.tablePick.cols})
                        (t "table.insert"))
               :size 12
               :bold true}])
     (grid#table-grid-cells :columns 8 :gap 3 :height 141 :anchors {:left 10 :right 10} :color transparent
       ~(repeat [z computed.tableSizes]
         `(node :id ~(str "table-grid-" z.r "-" z.c)
            :width 18
            :height 18
            :radius 2
            :cursor hand
            :role menuitem
            :label ~(t "table.size" {:rows z.r :cols z.c})
            :let {:on ~(and (<= z.r state.tablePick.rows) (<= z.c state.tablePick.cols))}
            :color ~(if on "@rbChecked" "@card")
            :borderWidth 1
            :borderColor ~(if on "@brand" "@rbFieldLine")
            :onHover ~(handler pick-table-size z.r z.c)
            :onClick ~(handler insert-table z.r z.c))))))

(defcomponent slideTable
  "A table object's cells, in a box `w` by `h`: the header row in its tint, the rows banded; while a cell is edited, an
  editor over it, the same one from cell to cell."
  {:o nil :prefix "" :w 100 :h 100 :edit false}
  `(node :id ~(str prefix o.id "-table")
     :x 0
     :y 0
     :width ~w
     :height ~h
     :let {:rows ~(max (count (or-else o.cells [])) 1)
           :cols ~(max (count (or-else o.cells.0 [])) 1)
           :tint ~(or-else o.tint "accent1")
           :size ~(or-else o.size 16)
           :editing ~(and edit (= state.editing o.id))}
     :color ~(get computed.palette (str tint ".1"))
     :borderWidth 1
     :borderColor ~(get computed.palette tint)
     (column :id ~(str prefix o.id "-rows") :x 0 :y 0 :width ~w :height ~h :color transparent
       ~(repeat [cells (or-else o.cells []) :index r]
         `(row :id ~(str prefix o.id "-r" r)
            :width ~w
            :height ~(/ h rows)
            :color ~(cond (zero? r) (get computed.palette tint)
                          (odd? r) (get computed.palette (str tint ".2"))
                          :else (get computed.palette (str tint ".1")))
            ~(repeat [text cells :index c]
              `(node :id ~(str prefix o.id "-c" r "-" c)
                 :width ~(/ w cols)
                 :height ~(/ h rows)
                 :color transparent
                 :borderWidth 0.5
                 :borderColor ~(get computed.palette "light1")
                 (label :id ~(str prefix o.id "-c" r "-" c "-t")
                   :when ~(not (and editing (= state.cell [r c])))
                   :anchors {:left 7 :right 7 :top 4 :bottom 4}
                   :wrap true
                   :align {:horizontal start :vertical center}
                   :runs [{:text ~text
                           :size ~size
                           :bold ~(zero? r)
                           :color ~(get computed.palette (if (zero? r) "light1" "dark1"))}]))))))
     (editor :id ~(str "edit-" o.id)
       :when ~editing
       :autofocus true
       :wrap true
       :let {:k ~(/ size 48) :r ~state.cell.0 :c ~state.cell.1 :cw ~(/ w cols) :rh ~(/ h rows)}
       :bind ~(str "state.deck.slides[" state.current "].objects["
                   (indexOf (map :id computed.objects) o.id) "].cells[" r "][" c "]")
       :keys {"tab" ~(handler next-cell 1) "shift+tab" ~(handler next-cell -1)}
       :x ~(- (+ (* c cw) 7) (* 12 k))
       :y ~(- (+ (* r rh)
                 (/ (- rh (* size 1.2)) 2))
              (* 6 k))
       :width ~(/ (+ (- cw 14) (* 24 k)) k)
       :height ~(/ (* size 1.7) k)
       :scale ~k
       :font slideText
       :transformOrigin topLeft
       :color transparent
       :lineColor transparent
       :textColor ~(get computed.palette (if (zero? r) "light1" "dark1"))
       :caretColor ~(get computed.palette (if (zero? r) "light1" "dark1"))
       :selectionColor "#2a9d8f55"
       :label ~(t "table.cell" {:row (inc r) :col (inc c)}))))

(defcomponent slidePicture "A picture object: its image, filling its box `w` by `h`."
  {:o nil :prefix "" :w 100 :h 100}
  `(image :id ~(str prefix o.id "-picture")
     :x 0
     :y 0
     :width ~w
     :height ~h
     :source ~(or-else o.src "")
     :fit fill
     :label ~(or-else o.alt (t "picture.name"))))

(defpart content "Insert > Table's grid of sizes, over the window."
         :overlays {:zTable (node :use tableGrid :when ~state.tablePick.open)})

;;; What its commands do

(defn open-table-grid "Opens Insert > Table's grid, with no size picked." []
  (set "state.drop.open" false)
  (set "state.tablePick" {:open true :rows 0 :cols 0}))

(defn pick-table-size "Shows the size the pointer is over in the table grid." [rows cols]
  (set "state.tablePick.rows" rows)
  (set "state.tablePick.cols" cols))

(defn blank-rows "Rows of empty cells." [rows cols]
  (let [row (vec (for [_ (range cols)] ""))]
    (vec (for [_ (range rows)] row))))

(defn insert-table "Puts a table of empty cells in the middle of the slide, and selects it." [rows cols]
  (set "state.tablePick.open" false)
  (let [h (min (* rows 40) 440)]
    (insertObject {:kind "table"
                   :x 80
                   :y (- 270 (/ h 2))
                   :w 800
                   :h h
                   :tint "accent1"
                   :cells (blank-rows rows cols)})))

(defn edit-cell "Edits a cell of a table, in an editor over it, until Escape or a click elsewhere." [id r c]
  (commit)
  (set "state.board.selection" [id])
  (set "state.cell" [r c])
  (set "state.editing" id)
  (focus (str "#edit-" id)))

(defn edit-object
  "A double click on an object: a table edits the cell under the pointer (left and top within its width and height),
  other objects their text."
  [id left top width height]
  (let [cells (:cells (find #(= (:id %) id) computed.objects))
        rows (count cells)
        cols (count (first cells))
        down (/ top (max height 1))
        across (/ left (max width 1))
        r (floor (* down rows))
        c (floor (* across cols))]
    (if (seq cells)
      (edit-cell id (clamp r 0 (dec rows)) (clamp c 0 (dec cols)))
      (edit-text id))))

(defn next-cell "Tab and Shift+Tab in a cell: the next cell (by 1) or the one before (by -1); Tab in the last adds a row."
  [by]
  (let [table (find #(= (:id %) state.editing) computed.objects)
        cells (:cells table)
        cols (count (first cells))
        [r c] state.cell
        at (max (+ (* r cols) c by) 0)]
    (when (>= at (* (count cells) cols))
      (set-objects (map #(if (= (:id %) (:id table))
                           (assoc % :cells (conj cells (vec (for [_ (range cols)] "")))
                                    :h (+ (:h %) (/ (:h %) (count cells))))
                           %)
                        computed.objects)))
    (set "state.cell" [(quot at cols) (mod at cols)])))

(defn set-table-tint "Gives the selected tables a tint: an accent of the theme." [tint]
  (when (seq computed.selected)
    (commit)
    (set-objects (map #(if (and (selected? %) (= (:kind %) "table")) (assoc % :tint tint) %) computed.objects))))

(defn insert-picture "Asks for a PNG or JPEG picture, to put on the slide." []
  (openFile "state.picture" :as "image" :title (t "picture.choose") :onDone picture-opened :onError not-opened))

(defn picture-opened "Puts the picture just opened in the middle of the slide, at most 640 by 400, and selects it."
  [result]
  (let [w (max (or-else result.width 0) 1)
        h (max (or-else result.height 0) 1)
        k (min (/ 640 w) (/ 400 h) 1)
        pw (if (pos? (or-else result.width 0)) (* w k) 400)
        ph (if (pos? (or-else result.height 0)) (* h k) 300)]
    (insertObject {:kind "picture"
                   :x (- 480 (/ pw 2))
                   :y (- 270 (/ ph 2))
                   :w pw
                   :h ph
                   :src state.picture
                   :alt result.name}))
  (set "state.picture" nil)
  (fileStatus (t "picture.inserted" {:name result.name})))
