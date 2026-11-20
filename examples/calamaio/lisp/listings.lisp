;;; listings.lisp --- the listing buffers: a folder's files (C-x d, the tool bar's folder) and the buffer list (C-x C-b)
;;;
;;; A listing keeps the paths or the ids of its lines in `entries`, from line `first`: RET visits the one on point's
;;; line, g lists again and q leaves it (the keys are the window's, window.lisp).

(ns calamaio.listings)

;;; Showing a listing

(defn listing-show "Shows a listing buffer under its own id, made again each time." [id buffer]
  (set (str "state.bufs." id) buffer)
  (show id))

(defn spaces "A run of spaces." [n]
  (str/join (for [_ (range n)] " ")))

(defn padded "A text, then spaces up to a width." [s width]
  (str s (spaces (- width (count s)))))

;;; Folders

(defcomputed fileNames "Each file's name: a folder's ends with /."
  (map (fn [it]
         (let [parts (str/split it.path "/")]
           (if (= it.dir true)
             (str (get parts (- (count parts) 2)) "/")
             (get parts (dec (count parts))))))
       state.files))

(defcomputed fileInfo "The files, each with its name and the folder it is in."
  (map (fn [it]
         (let [path (get-in state.files [it :path])
               name (get computed.fileNames it)]
           (merge (get state.files it) {:name name :parent (slice path 0 (- (count path) (count name)))})))
       (range (count state.files))))

(defn dired-line
  "A file's line in a folder's listing: D if it is flagged for deletion, d for a folder, its size, when it changed, and
  its name." [flagged file]
  (let [{:keys [dir text mtime name path]} file]
    (str (if (some #{path} flagged) "D " "  ")
         (if dir "d" "-")
         (pad (if dir "" (str (count (or text "")))) 8 " ")
         "  "
         (pad (time mtime "D MMM HH:mm") 12 " ")
         "  "
         name)))

(defn dired-show "Lists a folder's files in its buffer, keeping the flags of the files still there." [dir]
  (let [items (filter #(= (:parent %) dir) computed.fileInfo)
        here (into #{} (map :path items))
        flagged (if (= state.bufs.bd.dir dir) (filter here (or state.bufs.bd.flagged [])) [])]
    (listing-show "bd" {:name dir
                        :mode "dired"
                        :readOnly true
                        :dir dir
                        :first 3
                        :entries (map :path items)
                        :flagged (vec flagged)
                        :text (str (t "dired.header" {:dir dir}) "\n\n"
                                   (str/join "\n" (map #(dired-line flagged %) items))
                                   (when (empty? items) (t "dired.empty"))
                                   "\n")})))

(defcommand dired "Lists a folder's files in a buffer; RET on a line visits that file or folder."
  :title ~(t "command.dired") :category ~(t "category.files")
  :description ~(t "command.dired.about")
  []
  (minibuffer-read "dired" (t "prompt.dired") state.home state.home))

(defn mb-dired "Lists the folder named." [value]
  (dired-show (if (str/ends-with? value "/") value (str value "/"))))

(defcommand dired-home "Lists the home folder's files."
  :title ~(t "command.dired-home") :category ~(t "category.files")
  :description ~(t "command.dired-home.about")
  []
  (dired-show state.home))

;;; The buffer list

(defn buffer-line
  "A buffer's line in the buffer list: * if it changed, % if it is read-only, its name, size, mode and file." [id]
  (let [{:keys [dirty readOnly name text mode file]} (get state.bufs id)]
    (str (cond dirty "* " readOnly "% " :else "  ")
         (padded name 22)
         (padded (str (count (or text ""))) 8)
         (padded (get-in state.modes [mode :title]) 18)
         (or file ""))))

(defn buffer-list-text "The buffer list: a line of help, the columns' titles, then a line a buffer." [ids]
  (str (t "bufferList.header") "\n\n  "
       (padded (t "bufferList.name") 22) (padded (t "bufferList.size") 8) (padded (t "bufferList.mode") 18)
       (t "bufferList.file") "\n  "
       (padded "----" 22) (padded "----" 8) (padded "----" 18) "----\n"
       (str/join "\n" (map buffer-line ids))
       "\n"))

(defcommand list-buffers "Lists the buffers, with their size, mode and file; RET on a line shows that buffer."
  :title ~(t "command.list-buffers") :category ~(t "category.buffers")
  :description ~(t "command.list-buffers.about")
  []
  (let [ids (remove #{"bl"} state.order)]
    (listing-show "bl" {:name "*Buffer List*" :mode "buffer-menu" :readOnly true :first 5 :entries ids
                        :text (buffer-list-text ids)})))

;;; The keys of a listing

(defn listing-visit-entry "Visits a listing's entry: a buffer, a folder or a file." [e mode]
  (cond (nil? e) (message (t "echo.noFileHere"))
        (= mode "buffer-menu") (show e)
        (str/ends-with? e "/") (dired-show e)
        :else (mb-file e)))

(defcommand listing-visit "In a folder or buffer listing, visits the file, folder or buffer on point's line."
  :title ~(t "command.listing-visit") :category ~(t "category.buffers")
  :description ~(t "command.listing-visit.about")
  []
  (listing-visit-entry (get computed.buf.entries (- (:line (caret)) computed.buf.first)) computed.buf.mode))

(defcommand listing-revert "Makes a folder or buffer listing again, with what changed since."
  :title ~(t "command.listing-revert") :category ~(t "category.buffers")
  :description ~(t "command.listing-revert.about")
  []
  (if (= computed.buf.mode "dired")
    (dired-show computed.buf.dir)
    (list-buffers)))
