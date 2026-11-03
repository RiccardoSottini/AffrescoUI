;;; files.lisp --- visiting, saving and inserting the files of the home folder (home.lisp)
;;;
;;; C-x C-f visits a file in a buffer of its own (the one already visiting it, if there is one), its major mode from
;;; its extension; C-x C-s writes the buffer back, C-x C-w under a new name, and C-x i inserts a file at point.

;;; Paths

(defn file-index "Where a file is in state.files, or -1." [path]
  (indexOf (map (fn [it] it.path) state.files) path))

(defn base-name "The last part of a path: the file's name." [path]
  (last (split path "/")))

;;; Visiting

(defcommand find-file "Visits a file in a buffer of its own, asking for its name."
  :title ~(t "command.find-file") :category ~(t "category.files")
  :description ~(t "command.find-file.about")
  []
  (minibuffer-read "file" (t "prompt.findFile") "" state.home))

(defn visit "Shows the buffer visiting a file, or opens one: the file's text, or an empty new file." [path]
  (let [open (find (fn [b] (= (get-in state.bufs [b :file]) path)) (keys state.bufs))
        i (file-index path)
        name (base-name path)]
    (if (some? open)
      (show open)
      (do (open-buffer {:name name
                        :file path
                        :mode (or (get state.modeFor (last (split name "."))) "fundamental")
                        :text (if (>= i 0) (get-in state.files [i :text]) "")})
          (when (< i 0) (message (t "echo.newFile")))))))

(defn mb-file "Visits the file named." [value]
  (visit value))

;;; Saving

(defn write-file-to "Writes a buffer's text to its file, at place i of the folder, or as a new file." [b i]
  (let [buffer (get state.bufs b)]
    (if (>= i 0)
      (set (str "state.files[" i "].text") (:text buffer))
      (push "state.files" {:path (:file buffer) :text (:text buffer) :mtime 1778749200000}))
    (set (str "state.bufs." b ".dirty") false)
    (message (t "echo.wrote" {:file (:file buffer)}))))

(defcommand save-buffer "Writes the buffer to its file, if it changed."
  :title ~(t "command.save-buffer") :category ~(t "category.files")
  :description ~(t "command.save-buffer.about")
  []
  (cond (nil? computed.buf.file) (message (t "echo.noFile"))
        (not computed.buf.dirty) (message (t "echo.nothingToSave"))
        :else (do (write-file-to computed.cur (file-index computed.buf.file))
                  (saved))))

(defcommand write-file "Writes the buffer to a file you name, which it then visits."
  :title ~(t "command.write-file") :category ~(t "category.files")
  :description ~(t "command.write-file.about")
  []
  (minibuffer-read "write" (t "prompt.writeFile") "" state.home))

(defn mb-write "Makes the buffer visit the file named, and writes it there." [value]
  (set (str "state.bufs." computed.cur ".file") value)
  (set (str "state.bufs." computed.cur ".name") (base-name value))
  (write-file-to computed.cur (file-index value)))

;;; Inserting

(defcommand insert-file "Inserts a file's text at point."
  :title ~(t "command.insert-file") :category ~(t "category.files")
  :description ~(t "command.insert-file.about")
  []
  (minibuffer-read "insert" (t "prompt.insertFile") "" state.home))

(defn insert-at-point "Types a text at a place, and leaves point there, before it." [text pt]
  (edit "type" :text text :in computed.ed)
  (goto-point computed.ed pt))

(defn mb-insert "Inserts the file named at point." [value]
  (let [i (file-index value)]
    (if (< i 0)
      (message (t "echo.noFileNamed" {:name value}))
      (after 0 insert-at-point (get-in state.files [i :text]) (point)))))

;;; Quitting

(defcommand save-buffers-kill-terminal "Offers to save the changed files, then ends the session."
  :title ~(t "command.save-buffers-kill-terminal") :category ~(t "category.files")
  :description ~(t "command.save-buffers-kill-terminal.about")
  []
  (if (some (fn [b] (and (some? (:file b)) (= (:dirty b) true))) (vals state.bufs))
    (message (t "echo.unsaved"))
    (message (t "echo.nothingLeft"))))
