;;; flow.lisp --- what Flow does: its functions and commands
;;;
;;; The screen and its parts (index.screen.lisp, board.lisp, issue.lisp...) say what is shown; this file says what
;;; happens. A function reads the running app through paths (`state.selected`, `computed.card`, `data.inbox`) and changes
;;; it only through steps (`set`, `insert`, `update-record`...). A handler runs one by name, its parameters by name:
;;; {:run moveCard :with {:from 0 :at 2 :to 1}}. The keys stay in the parts, as data.

;;; The cards

(defn column "A column of the board, with its cards." [c]
  (get computed.columns c))

(defn cardsOf "A column's cards, in their order." [c]
  (:cards (column c)))

(defn nextOrder "The order that puts a card at the end of a column." [c]
  (inc (reduce max -1 (map :order (cardsOf c)))))

(defn lastCard "The card at the end of a column." [c]
  (last (cardsOf c)))

(defn columnOf "The column that holds a card, or -1." [ref]
  (or (first (filter some? (map-indexed (fn [c col] (when (some (fn [card] (= (:ref card) ref)) (:cards col)) c))
                                        computed.columns)))
      -1))

(defn remember "Marks a change for undoChange (the last 20): the records' undo takes back the event's writes." []
  (push "state.undoStack" true)
  (set "state.toastUndo" true)
  (when (> (count state.undoStack) 20) (remove-state "state.undoStack" :at 0)))

(defn updateCard "Changes the selected card, as one step undo can take back." [fields]
  (remember)
  (update-record "issues" :id computed.card.id :fields fields))

(defn hideToast "" [] (set "state.toast.open" false))

(defn notify "A toast in the corner for a few seconds; it offers Undo when what sent it remembered the columns first." [message]
  (set "state.toast" {:open true :message message :undo state.toastUndo})
  (set "state.toastUndo" false)
  (debounce 4000 hideToast))

(defn logActivity "The activity feed, newest first, the last 30." [key title column]
  (set "state.activity" (concat [{:who "RS" :key key :title title :column column}] (take 29 state.activity))))

(defn checkLimit "A notice when a column is over its limit." [c]
  (let [col (column c)]
    (when (and settings.board.wipWarnings (some? (:limit col)) (> (count (:cards col)) (:limit col)))
      (notify (t "toast.wip" {:column (:name col) :limit (:limit col)})))))

(defn addCard "The composer's card, at the end of its column." []
  (let [title (trim state.newCard)
        composer state.composer
        c (:c composer)]
    (when (not= title "")
      (remember)
      (insert "issues"
        :value {:id (str "FLO-" state.nextRef) :ref (str "FLO-" state.nextRef) :board state.boardId :status c
                :order (nextOrder c) :priority (:p composer) :title title :tag (:tag composer) :who (:who composer)
                :points 1 :notes (if (not= (trim (:notes composer)) "") (:notes composer) nil)})
      (notify (t "toast.added" {:title title :column (:name (column c))}))
      (logActivity "added" title "")
      (set "state.nextRef" (inc state.nextRef))
      (set "state.newCard" "")
      (set "state.composer.notes" "")
      (set "state.composer.open" (:more composer)))))

(defn openComposer "Opens the composer on a column." [c]
  (set "state.composer" {:open true :c c :p 0 :who state.me :tag (if (= state.filter "all") "feature" state.filter)
                         :notes "" :more state.composer.more}))

(defn moveCard "A card from one column to the end of another." [from at to]
  (when (and (not= from to) (>= to 0) (<= to computed.lastColumn))
    (remember)
    (update-record "issues" :id (:id (get (cardsOf from) at)) :fields {:status to :order (nextOrder to)})
    (set "state.selected" {:c to :i (dec (count (cardsOf to)))})
    (logActivity "moved" (:title (lastCard to)) (:name (column to)))
    (checkLimit to)))

(defn moveSelected "The selected card one column on (1) or back (-1)." [by]
  (moveCard state.selected.c state.selected.i (+ state.selected.c by)))

(defn reorderCard "Swaps a card with its neighbour in the column." [c i by]
  (let [cards (cardsOf c)
        card (get cards i)
        other (get cards (+ i by))]
    (when (and (>= (+ i by) 0) (< (+ i by) (count cards)))
      (remember)
      (update-record "issues" :id (:id card) :fields {:order (:order other)})
      (update-record "issues" :id (:id other) :fields {:order (:order card)})
      (set "state.selected" {:c c :i (+ i by)}))))

(defn moveToLane "A card dropped in a swimlane takes that column and that owner." [from at to who]
  (if (not= from to)
    (do (moveCard from at to)
        (update-record "issues" :id (:id (lastCard to)) :fields {:who who}))
    (let [card (get (cardsOf from) at)]
      (when (not= (:who card) who)
        (remember)
        (update-record "issues" :id (:id card) :fields {:who who})))))

;;; Selecting

(defn announcePresence "Tells the other people on this board which card is open." []
  (emit "presence" :data {:who "RS" :card (if (nil? computed.card) nil computed.card.title)}))

(defn selectCard "Selects a card: c its column, i its place." [c i]
  (when (not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i)))
    (set "state.selected" {:c c :i i})
    (set "state.draft" {:editing false :notes ""})
    (set "state.dueOpen" false)
    (announcePresence)))

(defn openCardMenu "A card's menu, at the pointer." [c i]
  (selectCard c i)
  (set "state.menuAt" {:x event.x :y event.y})
  (set "state.cardMenu" true))

(defn openColumnMenu "A column's menu, below its button; from the keyboard, at the board's top start corner." [c]
  (set "state.menuColumn" c)
  (set "state.menuAt" {:x (or event.x 244) :y (+ (or event.y 108) 12)})
  (set "state.columnMenu" true))

(defn openRow "A table row chosen: its id is column-index." []
  (let [parts (split (or event.id event.value) "-")]
    (selectCard (number (first parts)) (number (second parts)))))

(defn switchBoard "Another board of the sidebar: undo keeps one board's columns." [id]
  (when (not= id state.boardId)
    (set "state.boardId" id)
    (set "state.board" (:label (find (fn [board] (= (:id board) id)) state.boards)))
    (set "state.selected" nil)
    (set "state.undoStack" [])))

;;; Filters

(defn setFilter "A value of the Filter menu: on says it is applied, which removes it." [prop v on]
  (cond (= prop "status") (set "state.fStatus" (if on -1 v))
        (= prop "assignee") (set "state.owner" (if on "" v))
        (= prop "priority") (set "state.fPrio" (if on -1 v))
        (= prop "label") (set "state.filter" (if on "all" v)))
  (set "state.filterOpen" false)
  (set "state.filterProp" ""))

(defn clearFilter "The filter a chip's x removes." [prop]
  (cond (= prop "status") (set "state.fStatus" -1)
        (= prop "assignee") (set "state.owner" "")
        (= prop "priority") (set "state.fPrio" -1)
        (= prop "label") (set "state.filter" "all")
        (= prop "project") (set "state.fProject" -1)))

(defn showEverything "No filter, search or scope: every card shows." []
  (set "state.filter" "all")
  (set "state.owner" "")
  (set "state.fStatus" -1)
  (set "state.fPrio" -1)
  (set "state.query" "")
  (set "state.fProject" -1)
  (set "state.scope" "all"))

(defcommand clearFilters "Clears the filters, the search and the scope." :title "Clear the filters" :category "View" []
  (showEverything)
  (set "state.searchOpen" false))

(defn goToCard "A quick pick row: from another page, back to the issues, in the board, list or table it showed." [item]
  (showPage "issues")
  (when (= state.view "timeline") (set "state.view" "board"))
  (showEverything)
  (selectCard (:c item) (:at item)))

;;; The selected card's properties

(defn openPropPick "A picker over the selected card's status, priority or assignee, on what it is now." [kind]
  (set "state.propKind" kind)
  (set "state.propPick"
       {:open true :query "" :top 0
        :index (cond (> (count state.marked) 0) 0
                     (= kind "status") state.selected.c
                     (= kind "priority") (or computed.card.priority 0)
                     :else (max 0 (indexOf (map :who state.members) computed.card.who)))}))

(defn setPriority "The priority from 0 (none) to 4 (low); every marked card while some are." [p]
  (if (> (count state.marked) 0)
    (bulkPriority p)
    (when computed.card
      (updateCard {:priority p})
      (set "state.issuePriorityOpen" false))))

(defn setEstimate "The points." [n]
  (set "state.issueEstimateOpen" false)
  (when computed.card (updateCard {:points n})))

(defn setTag "The issue's main label; its project follows it." [tag]
  (set "state.issueTagOpen" false)
  (when computed.card (updateCard {:tag tag})))

(defn setAssignee "A member's initials; every marked card while some are." [who]
  (if (> (count state.marked) 0)
    (bulkAssignee who)
    (when computed.card
      (updateCard {:who who})
      (set "state.issueWhoOpen" false))))

(defn setStatus "The column the selected card moves to; every marked card while some are." [to]
  (set "state.issueStatusOpen" false)
  (if (> (count state.marked) 0)
    (bulkStatus to)
    (when computed.card (moveCard state.selected.c state.selected.i to))))

(defn setDue "A date, or nil to clear it." [date]
  (when computed.card
    (updateCard {:due date})
    (set "state.dueOpen" false)))

(defn setCardDates "A card's dates and owner, from the timeline." [c i start due who]
  (remember)
  (update-record "issues" :id (:id (get (cardsOf c) i)) :fields {:start start :due due :who who}))

(defn listOf "A list field of the selected card: labels, assignees, checklist, comments, attachments." [field]
  (or (get computed.card field) []))

(defn toggleListItem "Adds a label or an assignee to the selected card, or removes it." [field value]
  (when computed.card
    (let [items (listOf field)]
      (updateCard (assoc {} field (if (contains items value)
                                    (filter (fn [item] (not= item value)) items)
                                    (concat items [value])))))))

(defn addToCard "Adds to a list of the selected card; nothing while the text typed is blank." [field value input]
  (when (and (some? computed.card) (not= (trim (or input "")) ""))
    (updateCard (assoc {} field (concat (listOf field) [value])))
    (when (= field "comments") (logActivity "commented" computed.card.title ""))))

(defn removeFromCard "Removes an item of a list of the selected card." [field at]
  (let [items (listOf field)]
    (updateCard (assoc {} field (concat (take at items) (drop (inc at) items))))))

(defn checkItem "Checks or unchecks an item of the checklist." [at value]
  (updateCard {:checklist (map-indexed (fn [index item] (if (= index at) (merge item {:done value}) item))
                                       computed.card.checklist)}))

(defn editNotes "" []
  (when computed.card (set "state.draft" {:editing true :notes (or computed.card.notes "")})))

(defn saveNotes "" []
  (when computed.card
    (updateCard {:notes state.draft.notes})
    (set "state.draft.editing" false)
    (notify (t "toast.saved"))))

(defn cancelNotes "" [] (set "state.draft.editing" false))

(defn saveCard "The edit dialog's title." []
  (let [title (trim state.edit.title)]
    (when (and (some? computed.card) (not= title ""))
      (updateCard {:title title})
      (set "state.edit.open" false)
      (notify (t "toast.renamed"))
      (logActivity "renamed" title ""))))

(defn closeEdit "" [] (set "state.edit.open" false))

;;; The timeline

(defn clipOf "A card with a due date as a timeline clip, coloured as its column; days are frames." [card columns]
  (let [from (or (:start card) (:due card))]
    {:id (str (:c card) "-" (:at card)) :label (:title card)
     :start (max 0 (daysBetween state.sprintStart from)) :duration (max 1 (daysBetween from (:due card)))
     :color (:color (get columns (:c card)))}))

(defn buildTimeline "One track per member, a clip per card with a start and a due date." []
  (let [columns computed.columns
        cards (filter (fn [card] (some? (:due card))) computed.allCards)]
    (set "state.timelineTracks"
         (map (fn [m]
                {:id (:who m) :label (:name m)
                 :clips (map (fn [card] (clipOf card columns)) (filter (fn [card] (= (:who card) (:who m))) cards))})
              state.members))))

(defn changeClip "A clip moved or trimmed: its card gets the new dates, and the track's owner." []
  (let [parts (split event.id "-")]
    (setCardDates (number (first parts)) (number (second parts))
                  (addDays state.sprintStart (round event.start))
                  (addDays state.sprintStart (round (+ event.start event.duration)))
                  event.track)))

;;; Marking several cards

(defn markSelected "Marks the selected card if it isn't marked yet." []
  (when (and (some? computed.card) (not (contains state.marked computed.card.ref)))
    (push "state.marked" computed.card.ref)))

(defn bulkChanged "" [] (notify (t "bulk.changed" {:n (count state.marked)})))

(defn bulkPriority "Every marked card's priority." [p]
  (remember)
  (update-record "issues" :id state.marked :fields {:priority p})
  (bulkChanged))

(defn bulkAssignee "Assigns every marked card." [who]
  (remember)
  (update-record "issues" :id state.marked :fields {:who who})
  (bulkChanged))

(defn moveMarked "One card of a bulk move, without its own undo step." [ref from to]
  (when (and (>= from 0) (not= from to))
    (update-record "issues" :id ref :fields {:status to :order (nextOrder to)})))

(defn bulkStatus "Moves every marked card to the end of a column, in the order they were marked." [to]
  (remember)
  (map (fn [ref] (moveMarked ref (columnOf ref) to)) state.marked)
  (set "state.selected" nil)
  (bulkChanged))

;;; Pages and the inbox

(defn showPage "issues, inbox, cycles or projects." [page]
  (set "state.page" page)
  (set "state.issueOpen" false))

(defn openProject "A project's issues, behind a Project filter whose chip removes it." [at]
  (set "state.fProject" at)
  (showPage "issues"))

(defn pickNotification "A notification among the shown ones, marked read." [at]
  (set "state.inboxAt" at)
  (update-record "inbox" :id (:id (get computed.inboxShown at)) :fields {:read true}))

;;; What the controls of the views do

(defn toggleOwner "Shows only a member's cards, or everyone's again, and closes the members menu." [who]
  (set "state.owner" (if (= state.owner who) "" who))
  (set "state.membersOpen" false))

(defn showMyIssues "The sidebar's My issues: the issues page, only the person's cards, or everyone's again." []
  (showPage "issues")
  (set "state.owner" (if (= state.owner state.me) "" state.me)))

(defn chooseScope "Shows every card, the active ones or the backlog." [scope]
  (set "state.scope" scope))

(defn chooseProjectScope "Shows every project, or only some of them." [scope]
  (set "state.projectScope" scope))

(defn pickFilterProp "Lists the values of one property in the Filter menu." [prop]
  (set "state.filterProp" prop)
  (set "state.filterQuery" "")
  (focus "#filterField"))

(defn submitFilter "Enter in the Filter menu's field: the first property listed, or the first value of the one chosen." []
  (cond (and (= state.filterProp "") (> (count computed.filterPropsShown) 0))
        (pickFilterProp (first (first computed.filterPropsShown)))
        (and (not= state.filterProp "") (> (count computed.filterOptionsShown) 0))
        (let [option (first computed.filterOptionsShown)]
          (setFilter state.filterProp (:v option) (:on option)))))

(defn groupBy "The Display menu's grouping: the swimlanes, unless they are the grouping already." [grouped]
  (when (not grouped) (toggleSwimlanes)))

(defn orderBy "The Display menu's ordering: manual, by priority or by due date." [order]
  (set "state.order" order))

(defn toggleProperty "Shows or hides one property on the cards." [prop]
  (set "state.hidden" (if (some (fn [hidden] (= hidden prop)) state.hidden)
                        (filter (fn [hidden] (not= hidden prop)) state.hidden)
                        (conj state.hidden prop))))

(defn sortTable "A table header clicked: sorts the table by that column." []
  (set "state.tableSort" {:column event.column :descending event.descending}))

(defn resizeTableColumn "A table column's edge dragged: its width is kept." []
  (set (str "state.columnWidths." event.column) event.width))

(defn nextComposerColumn "The composer's status chip: the next column, round to the first." []
  (set "state.composer.c" (mod (inc state.composer.c) (count computed.columns))))

(defn nextComposerPriority "The composer's priority chip: the next priority, round to none." []
  (set "state.composer.p" (mod (inc state.composer.p) (count state.priorities))))

(defn copyLink "A link clicked in a description: copied, and a notice says so." []
  (copy event.link)
  (notify (t "toast.linkCopied" {:link event.link})))

;;; What other people's Flow says, through the host's channel (board.lisp)

(defn teamCommitted "Someone changed the board: the feed shows it, and its author is here." []
  (let [who (or event.author "?")]
    (set "state.activity" (concat [{:who who :key "revised" :title (or event.message "") :column ""}]
                                  (take 29 state.activity)))
    (set (str "state.presence." who) (or (get state.presence who) ""))))

(defn teamEvent "Another person's Flow says which card they have open." []
  (when (and (= event.name "presence") (not= event.data.who "RS"))
    (set (str "state.presence." event.data.who) (or event.data.card ""))))

(defn teamClosed "The channel closed: nobody else is here." []
  (set "state.presence" {}))

;;; Commands: the palette (Cmd+Shift+P), the menus and the keys run these

(defcommand columnNew "A new card in the column whose menu is open."
  :title "New issue in this status" :icon "plus" :enablement ~state.columnMenu []
  (set "state.columnMenu" false)
  (openComposer state.menuColumn))

(defcommand columnSelect "Marks every card shown in the column whose menu is open."
  :title "Select all in this status" :icon "list-checks" :enablement ~state.columnMenu []
  (set "state.columnMenu" false)
  (set "state.marked" (map :ref (filter (fn [card] (= (:c card) state.menuColumn)) computed.visible))))

(defcommand selectNext "" :title "Select the next issue" :category "Go" :icon "arrow-down"
  :enablement ~(some? computed.next) []
  (selectCard computed.next.c computed.next.at))

(defcommand selectPrev "" :title "Select the previous issue" :category "Go" :icon "arrow-up"
  :enablement ~(some? computed.prev) []
  (selectCard computed.prev.c computed.prev.at))

(defcommand changeStatus "" :title "Change status…" :category "Issue" :icon "circle-dashed"
  :enablement ~(or (some? computed.card) (> (count state.marked) 0)) []
  (openPropPick "status"))

(defcommand changePriority "" :title "Change priority…" :category "Issue" :icon "chevrons-up"
  :enablement ~(or (some? computed.card) (> (count state.marked) 0)) []
  (openPropPick "priority"))

(defcommand changeAssignee "" :title "Assign to…" :category "Issue" :icon "user"
  :enablement ~(or (some? computed.card) (> (count state.marked) 0)) []
  (openPropPick "assignee"))

(defcommand assignToMe "" :title "Assign to me" :category "Issue" :icon "user-check"
  :enablement ~(some? computed.card) []
  (setAssignee state.me))

(defcommand newCardCommand "" :title "New issue" :category "Issue" :icon "plus" []
  (openComposer 0))

(defcommand goToCardCommand "" :title "Go to issue…" :category "Go" :icon "search" []
  (set "state.issuePick.query" "")
  (set "state.issuePick.open" true))

(defcommand commandPalette "" :title "Show all commands" :category "Go" :icon "command" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand editCard "" :title "Edit issue…" :category "Issue" :icon "pencil"
  :enablement ~(some? computed.card) []
  (set "state.edit" {:open true :title computed.card.title}))

(defcommand deleteCard "" :title "Delete issue" :category "Issue" :icon "trash-2"
  :enablement ~(some? computed.card) []
  (let [card computed.card]
    (remember)
    (notify (t "toast.deleted" {:title (:title card)}))
    (logActivity "deleted" (:title card) "")
    (delete "issues" :id (:id card))
    (set "state.selected" nil)))

(defcommand moveRight "" :title "Move issue right" :category "Issue" :icon "arrow-right"
  :enablement ~(and (some? computed.card) (< state.selected.c computed.lastColumn)) []
  (moveSelected 1))

(defcommand moveLeft "" :title "Move issue left" :category "Issue" :icon "arrow-left"
  :enablement ~(and (some? computed.card) (> state.selected.c 0)) []
  (moveSelected -1))

(defcommand moveUp "" :title "Move issue up" :category "Issue" :icon "arrow-up"
  :enablement ~(and (some? computed.card) (> state.selected.i 0)) []
  (reorderCard state.selected.c state.selected.i -1))

(defcommand moveDown "" :title "Move issue down" :category "Issue" :icon "arrow-down"
  :enablement ~(and (some? computed.card)
                    (< state.selected.i (- (count (get-in computed.columns [state.selected.c :cards])) 1))) []
  (reorderCard state.selected.c state.selected.i 1))

(defcommand undoChange "" :title "Undo" :category "Issue" :icon "undo-2" []
  (if (= (count state.undoStack) 0)
    (notify (t "toast.nothingToUndo"))
    (do (undoData true)
        (remove-state "state.undoStack" :at (dec (count state.undoStack)))
        (set "state.selected" nil)
        (logActivity "undone" "" "")
        (notify (t "toast.undone")))))

(defcommand openIssue "" :title "Open issue" :category "Issue" :icon "maximize-2"
  :enablement ~(some? computed.card) []
  (set "state.issueOpen" true))

(defcommand closeIssue "" :title "Close issue" :category "Issue" :icon "arrow-left" :enablement ~state.issueOpen []
  (set "state.issueOpen" false)
  (set "state.issueDueOpen" false)
  (set "state.subOpen" false))

(defcommand toggleMark "" :title "Select or deselect the issue" :category "Issue" :icon "square-check"
  :enablement ~(some? computed.card) []
  (let [ref computed.card.ref]
    (set "state.marked" (if (contains state.marked ref)
                          (filter (fn [marked] (not= marked ref)) state.marked)
                          (concat state.marked [ref])))))

(defcommand markNext "" :title "Extend the selection down" :category "Issue" :icon "chevrons-down"
  :enablement ~(and (some? computed.card) (some? computed.next)) []
  (markSelected)
  (selectNext)
  (markSelected))

(defcommand markPrevious "" :title "Extend the selection up" :category "Issue" :icon "chevrons-up"
  :enablement ~(and (some? computed.card) (some? computed.prev)) []
  (markSelected)
  (selectPrev)
  (markSelected))

(defcommand clearMarks "" :title "Clear the selection" :category "Issue" :icon "x"
  :enablement ~(> (count state.marked) 0) []
  (set "state.marked" []))

(defcommand markAll "" :title "Select all issues" :category "Issue" :icon "list-checks" []
  (set "state.marked" (map :ref computed.visible)))

(defcommand bulkDelete "" :title "Delete the selected issues" :category "Issue" :icon "trash-2"
  :enablement ~(> (count state.marked) 0) []
  (remember)
  (notify (t "bulk.deleted" {:n (count state.marked)}))
  (delete "issues" :id state.marked)
  (set "state.marked" [])
  (set "state.selected" nil))

(defcommand showBoard "" :title "Show the board" :category "View" :icon "kanban" [] (set "state.view" "board"))

(defcommand showList "" :title "Show the list" :category "View" :icon "layout-list" [] (set "state.view" "list"))

(defcommand showTable "" :title "Show the table" :category "View" :icon "table" [] (set "state.view" "table"))

(defcommand showTimeline "" :title "Show the timeline" :category "View" :icon "chart-gantt" []
  (set "state.view" "timeline"))

(defcommand toggleSwimlanes "" :title "Swimlanes by member" :category "View" :icon "rows-2" []
  (set "state.view" "board")
  (toggle "state.swimlanes"))

(defcommand toggleTheme "" :title "Switch light and dark" :category "View" :icon "moon" []
  (set "state.theme" (if (= state.theme "light") "dark" "light")))

(defcommand togglePeek "" :title "Show or hide the side panel" :category "View" :icon "panel-right" []
  (toggle "state.peek"))

(defcommand openSettings "" :title "Settings…" :category "View" :icon "settings" [] (set "state.settingsOpen" true))

(defn closeSettings "" [] (set "state.settingsOpen" false))

(defcommand changeLanguage "" :title "Change the language" :category "View"
  :icon "languages" []
  (set "settings.appearance.language"
       (cond (= settings.appearance.language "en") "es" (= settings.appearance.language "es") "fr" :else "en")))

(defcommand focusSearch "" :title "Search issues" :category "Go" :icon "search" []
  (set "state.searchOpen" true)
  (focus "#search"))

(defcommand addProject "" :title "New project" :category "Go" :icon "plus" []
  (insert "projects" :value {:name (t "projects.untitled" {:n (inc (count data.projects))}) :icon "box" :color "#0d9488"
                             :health "onTrack" :lead state.me :target state.cycle.end :progress 0 :issues 0})
  (set "state.projectScope" "all"))

(defcommand goToInbox "" :title "Go to inbox" :category "Go" :icon "inbox" [] (showPage "inbox"))

(defcommand goToIssues "" :title "Go to issues" :category "Go" :icon "copy" [] (showPage "issues"))

(defcommand goToCycles "" :title "Go to sprints" :category "Go" :icon "timer" [] (showPage "cycles"))

(defcommand goToProjects "" :title "Go to projects" :category "Go" :icon "box" [] (showPage "projects"))

(defcommand nextNotification "" :title "Next notification" :category "Inbox" :icon "arrow-down"
  :enablement ~(and (= state.page "inbox") (< (+ state.inboxAt 1) (count computed.inboxShown))) []
  (pickNotification (inc state.inboxAt)))

(defcommand previousNotification "" :title "Previous notification" :category "Inbox" :icon "arrow-up"
  :enablement ~(and (= state.page "inbox") (> state.inboxAt 0)) []
  (pickNotification (dec state.inboxAt)))

(defcommand toggleRead "" :title "Mark read or unread" :category "Inbox" :icon "mail-check"
  :enablement ~(some? computed.notification) []
  (update-record "inbox" :id computed.notification.id :fields {:read (not computed.notification.read)}))

(defcommand markAllRead "" :title "Mark all notifications read" :category "Inbox" :icon "check-check" []
  (update-record "inbox" :where {:read false} :fields {:read true}))

(defcommand snoozeNotification "" :title "Snooze notification" :category "Inbox" :icon "alarm-clock"
  :enablement ~(some? computed.notification) []
  (update-record "inbox" :id computed.notification.id :fields {:snoozed true})
  (set "state.inboxAt" (max (min state.inboxAt (- (count computed.inboxShown) 2)) 0))
  (notify (t "inbox.snoozed")))

(defcommand openNotification "" :title "Open the notification's issue" :category "Inbox" :icon "maximize-2"
  :enablement ~(some? computed.notificationCard) []
  (selectCard computed.notificationCard.c computed.notificationCard.at)
  (set "state.page" "issues")
  (set "state.issueOpen" true))
