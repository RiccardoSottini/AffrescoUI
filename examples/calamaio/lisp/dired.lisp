;;; dired.lisp --- changing a folder from its listing: flags (d, u), deleting (x), renaming (R), copying (C), new folders (+)
;;;
;;; The listing (listings.lisp) keeps the paths flagged for deletion in its buffer's `flagged`, and shows a D at the
;;; start of their lines. x asks before it deletes them; R, C and + ask for a path in the minibuffer. The files are the
;;; sample home folder's (home.lisp), so every change is a change of state.files, and a buffer visiting a renamed file
;;; follows it.

(ns calamaio.dired)

;;; Flags

(defn dired-entry "The path on point's line of a folder's listing, or nil." []
  (let [line (:line (caret))]
    (get computed.buf.entries (- line computed.buf.first))))

(defn dired-next-line "Lists the folder again, point on the line after the one it was on." [line]
  (dired-show computed.buf.dir)
  (set "state.bufs.bd.pt" {:line (inc line) :column 1}))

(defn dired-flag "Flags point's file for deletion, or takes its flag off; then goes to the next line." [on]
  (if-some [path (dired-entry)]
    (let [line (:line (caret))
          others (vec (remove #{path} (or computed.buf.flagged [])))]
      (set "state.bufs.bd.flagged" (if on (conj others path) others))
      (dired-next-line line))
    (message (t "echo.noFileHere"))))

(defcommand dired-flag-file-deletion "In a folder's listing, flags the file on point's line for deletion (x deletes it)."
  :title ~(t "command.dired-flag-file-deletion") :category ~(t "category.files")
  :description ~(t "command.dired-flag-file-deletion.about")
  []
  (dired-flag true))

(defcommand dired-unmark "In a folder's listing, takes the flag off the file on point's line."
  :title ~(t "command.dired-unmark") :category ~(t "category.files")
  :description ~(t "command.dired-unmark.about")
  []
  (dired-flag false))

;;; Deleting

(defn under? "Whether a path is another, or inside it when the other is a folder." [path other]
  (or (= path other) (and (str/ends-with? other "/") (str/starts-with? path other))))

(defn flagged? "Whether a file is one of the flagged paths, or inside a flagged folder." [flagged file]
  (some #(under? (:path file) %) flagged))

(defcommand dired-do-flagged-delete "In a folder's listing, deletes the flagged files, once you answer yes."
  :title ~(t "command.dired-do-flagged-delete") :category ~(t "category.files")
  :description ~(t "command.dired-do-flagged-delete.about")
  []
  (if (empty? computed.buf.flagged)
    (message (t "echo.noneFlagged"))
    (minibuffer-read "dired-delete" (t "prompt.diredDelete" {:n (count computed.buf.flagged)}) "" "")))

(defn mb-dired-delete "Deletes the flagged files (and what flagged folders hold) if the answer is yes." [value]
  (let [flagged (or state.bufs.bd.flagged [])]
    (if (= (str/lower-case (str/trim value)) (t "answer.yes"))
      (do (set "state.files" (vec (remove #(flagged? flagged %) state.files)))
          (set "state.bufs.bd.flagged" [])
          (dired-show state.bufs.bd.dir)
          (message (t "echo.deleted" {:n (count flagged)})))
      (message (t "echo.nothingDeleted")))))

;;; Renaming, copying and new folders

(defn moved "A path once what is under one path is under another." [path from to]
  (if (under? path from)
    (str to (subs path (count from)))
    path))

(defn dired-ask "Asks for a path about point's file, in the minibuffer: a kind of question and its prompt." [kind prompt]
  (if-some [path (dired-entry)]
    (minibuffer-read kind (t prompt {:name (base-name path)}) "" path)
    (message (t "echo.noFileHere"))))

(defcommand dired-do-rename "In a folder's listing, renames or moves the file on point's line, asking for its new path."
  :title ~(t "command.dired-do-rename") :category ~(t "category.files")
  :description ~(t "command.dired-do-rename.about")
  []
  (set "state.mb.from" (dired-entry))
  (dired-ask "dired-rename" "prompt.diredRename"))

(defn mb-dired-rename "Moves the file (or folder) being renamed to the path answered; buffers visiting it follow." [value]
  (let [from state.mb.from
        folder? (and (str/ends-with? from "/") (not (str/ends-with? value "/")))
        to (if folder? (str value "/") value)]
    (cond (#{"" from} to) nil
          (not (neg? (file-index to))) (message (t "echo.fileExists" {:name to}))
          :else
          (do (set "state.files" (vec (map #(assoc % :path (moved (:path %) from to)) state.files)))
              (doseq [b (keys state.bufs)]
                (when-some [file (get-in state.bufs [b :file])]
                  (when (under? file from)
                    (set (str "state.bufs." b ".file") (moved file from to))
                    (set (str "state.bufs." b ".name") (base-name (moved file from to))))))
              (dired-show state.bufs.bd.dir)
              (message (t "echo.renamed" {:from from :to to}))))))

(defcommand dired-do-copy "In a folder's listing, copies the file on point's line to a path you name."
  :title ~(t "command.dired-do-copy") :category ~(t "category.files")
  :description ~(t "command.dired-do-copy.about")
  []
  (set "state.mb.from" (dired-entry))
  (if (str/ends-with? (or (dired-entry) "") "/")
    (message (t "echo.copyFolder"))
    (dired-ask "dired-copy" "prompt.diredCopy")))

(defn mb-dired-copy "Copies the file being copied to the path answered." [value]
  (let [from state.mb.from
        i (file-index from)]
    (cond (or (= value "") (= value from) (neg? i)) nil
          (not (neg? (file-index value))) (message (t "echo.fileExists" {:name value}))
          :else (do (push "state.files" (assoc (get state.files i) :path value))
                    (dired-show state.bufs.bd.dir)
                    (message (t "echo.copied.file" {:from from :to value}))))))

(defcommand dired-create-directory "In a folder's listing, makes a new folder, asking for its path."
  :title ~(t "command.dired-create-directory") :category ~(t "category.files")
  :description ~(t "command.dired-create-directory.about")
  []
  (minibuffer-read "dired-mkdir" (t "prompt.diredMkdir") "" computed.buf.dir))

(defn mb-dired-mkdir "Makes the folder answered." [value]
  (let [path (if (str/ends-with? value "/") value (str value "/"))]
    (cond (= path state.bufs.bd.dir) nil
          (not (neg? (file-index path))) (message (t "echo.fileExists" {:name path}))
          :else (do (push "state.files" {:path path :dir true :mtime 1778749200000})
                    (dired-show state.bufs.bd.dir)))))
