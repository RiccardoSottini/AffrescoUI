;;; columns.lisp --- a sample for the linter's configuration: the board's columns, in a file with an ns label

(ns board.columns)

(defstate columns [{:title "To do" :done false} {:title "Done" :done true}] "The columns, left to right.")

(defcomputed open-columns "The columns not done yet." (filter (fn [column] (not column.done)) state.columns))

(defn column-title "A column's title, marked when it is done." [column]
  (if column.done (str column.title " ✓") column.title))

(defn widest "The longest title's length." []
  (let [titles (map :title state.columns)
        longest (apply max 0 (map count titles))
        shortest (apply min 0 (map count titles))] ; planted: shortest is never read
    longest))

(defview column-list "The columns, side by side." []
  `(row#columns :gap 8
     ~(repeat [column state.columns :index i] `(label :text ~(column-title column) :tooltip ~(str i)))))
