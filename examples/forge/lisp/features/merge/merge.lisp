;;; merge.lisp --- Forge's merge conflicts: each side tinted, and a bar that resolves the one at the caret
;;;
;;; In a file with git's conflict markers (<<<<<<<, an optional |||||||, =======, >>>>>>>), each side is tinted and
;;; a bar over the editor keeps the current change, the incoming one or both for the conflict at the caret (or the
;;; first), and moves to the next; the file stays unsaved until Save.

;;; Conflicts, and resolving them

(defcomputed conflictLines
  (if (and (some? computed.activeDoc)
             (= computed.activeDoc.kind "file")
             (contains (or-else computed.activeDoc.text "") "<<<<<<<"))
        (split computed.activeDoc.text "\n")
        []))

(defcomputed conflictMarkers
  ; style: keep, a binding's filter takes a function that names its item it
  (filter (fn [it] (some? it))
            (map-indexed (fn [index it]
                           (cond (= (slice it 0 7) "<<<<<<<") {:kind "start" :line index}
                                 (= (slice it 0 7) "|||||||") {:kind "base" :line index}
                                 (= it "=======") {:kind "mid" :line index}
                                 (= (slice it 0 7) ">>>>>>>") {:kind "end" :line index}
                                 :else nil))
                         computed.conflictLines)))

(defcomputed conflictBlocks
  ; style: keep, a binding's filter takes a function that names its item it
  (filter (fn [it] (some? it))
            (map-indexed (fn [index it]
                           (cond (not= it.kind "start") nil
                                 (and (= (:kind (or-else (get computed.conflictMarkers (inc index)) {})) "mid")
                                      (= (:kind (or-else (get computed.conflictMarkers (+ index 2)) {})) "end")) {:start it.line
                                  :base (- 1)
                                  :mid (get-in computed.conflictMarkers [(inc index) :line])
                                  :end (get-in computed.conflictMarkers [(+ index 2) :line])}
                                 (and (= (:kind (or-else (get computed.conflictMarkers (inc index)) {})) "base")
                                      (= (:kind (or-else (get computed.conflictMarkers (+ index 2)) {})) "mid")
                                      (= (:kind (or-else (get computed.conflictMarkers (+ index 3)) {})) "end")) {:start it.line
                                  :base (get-in computed.conflictMarkers [(inc index) :line])
                                  :mid (get-in computed.conflictMarkers [(+ index 2) :line])
                                  :end (get-in computed.conflictMarkers [(+ index 3) :line])}
                                 :else nil))
                         computed.conflictMarkers)))

(defcomputed conflictAtCaret
  (or-else (find (fn [it]
                     (and (>= (dec (or-else state.caret.line 1)) it.start)
                          (<= (dec (or-else state.caret.line 1)) it.end)))
                   computed.conflictBlocks)
             computed.conflictBlocks.0))

(defcomputed conflictNext
  (or-else (find #(> (:start %) (dec (or-else state.caret.line 1))) computed.conflictBlocks)
             computed.conflictBlocks.0))

(defcomputed conflictSummary
  (if (some (fn [it]
                (and (>= (dec (or-else state.caret.line 1)) it.start) (<= (dec (or-else state.caret.line 1)) it.end)))
              computed.conflictBlocks)
        (t "conflict.at" {:n (count (filter #(<= (:start %) (dec (or-else state.caret.line 1))) computed.conflictBlocks))
                          :of (count computed.conflictBlocks)})
        (t "conflict.count" {:n (count computed.conflictBlocks)})))

(defcomputed conflictDecorations
  (filter #(and (>= (:line %) 0) (>= (or-else (:endLine %) (:line %)) (:line %)))
            (flat (map (fn [it]
                         [{:line it.start :kind "background" :color "#7f7f7f40"}
                          {:line (inc it.start)
                           :endLine (dec (if (>= it.base 0) it.base it.mid))
                           :kind "background"
                           :color "#3fa66b40"}
                          {:line it.base :endLine it.mid :kind "background" :color "#7f7f7f40"}
                          {:line it.mid :kind "background" :color "#7f7f7f40"}
                          {:line (inc it.mid) :endLine (dec it.end) :kind "background" :color "#3f86d640"}
                          {:line it.end :kind "background" :color "#7f7f7f40"}])
                       computed.conflictBlocks))))

(defcommand acceptCurrentChange "Keep the lines above the conflict's =======, the file's own, and drop the markers."
  :title ~(t "command.acceptCurrentChange") :category ~(t "category.mergeConflict") :icon "arrow-up"
  :description ~(t "command.acceptCurrentChange.about") :enablement ~(some? computed.conflictAtCaret) []
  (acceptConflict "current"))

(defcommand acceptIncomingChange
  "Keep the lines below the conflict's =======, the ones being merged in, and drop the markers."
  :title ~(t "command.acceptIncomingChange") :category ~(t "category.mergeConflict") :icon "arrow-down"
  :description ~(t "command.acceptIncomingChange.about") :enablement ~(some? computed.conflictAtCaret) []
  (acceptConflict "incoming"))

(defcommand acceptBothChanges "Keep both sides, the current one first, and drop the markers."
  :title ~(t "command.acceptBothChanges") :category ~(t "category.mergeConflict") :icon "list-plus"
  :description ~(t "command.acceptBothChanges.about") :enablement ~(some? computed.conflictAtCaret) []
  (acceptConflict "both"))

(defcommand nextConflict "Next Conflict"
  :title ~(t "command.nextConflict")
  :description ~(t "command.nextConflict") :category ~(t "category.mergeConflict") :icon "chevron-down"
  :enablement ~(some? computed.conflictNext) []
  (set "state.goTo.line" (inc computed.conflictNext.start))
  (set "state.goTo.column" 1)
  (increment "state.goTo.request" :by 1))

(defn acceptConflict "Resolves the conflict at the caret, keeping the current side, the incoming one or both (`keep`)."
  [keep]
  (when-some [conflict computed.conflictAtCaret]
    (setConflictText
      computed.activeDoc.id
      (str/join "\n"
                (concat (slice computed.conflictLines 0 conflict.start)
                        (if (not= keep "incoming")
                          (slice computed.conflictLines
                                 (inc conflict.start)
                                 (if (>= conflict.base 0) conflict.base conflict.mid))
                          [])
                        (if (not= keep "current") (slice computed.conflictLines (inc conflict.mid) conflict.end) [])
                        (slice computed.conflictLines (inc conflict.end)))))))

(defn setConflictText "A file's new text, after a conflict is resolved: dirty while it isn't the one saved." [id text]
  (set "state.docs" (map #(if (= (:id %) id) (merge % {:text text}) %) state.docs))
  (doseq [doc (filter #(= (:id %) id) state.docs)]
    (markDirty id (or (not= doc.text doc.saved) (not= doc.eol doc.savedEol)))))
