;;; flow.lisp --- what Flow does: its functions and commands
;;;
;;; The screen and its views (index.screen.lisp, toolbar.lisp, columns.lisp...) say what is shown; this file says
;;; what happens. A function reads the running app through paths (`state.selected`, `computed.card`, `data.inbox`) and
;;; changes it only through steps (`set`, `insert`, `update-record`...). A view runs one by name, its parameters
;;; taken by name from the variables around it (`:onClick sortTable`), or with its arguments, (handler moveCard
;;; event.data.from event.data.at c). The keys are in keys.lisp.

;;; The cards

(defn column "A column of the board, with its cards." [c]
  (get computed.columns c))

(defn cardsOf "A column's cards, in their order." [c]
  (:cards (column c)))

(defn nextOrder "The order that puts a card at the end of a column." [c]
  (->> (cardsOf c) (map :order) (reduce max -1) inc))

(defn lastCard "The card at the end of a column." [c]
  (last (cardsOf c)))

(defn columnOf "The column that holds a card, or -1." [ref]
  (or (first (keep-indexed (fn [c col]
                              (when (some #(= (:ref %) ref) (:cards col)) c))
                            computed.columns))
      -1))

(defn remember "Marks a change for undoChange (the last 20): the records' undo takes back the event's writes." []
  (push "state.undoStack" true)
  (set "state.toastUndo" true)
  (when (> (count state.undoStack) 20) (remove-state "state.undoStack" :at 0)))

(defn updateCard "Changes the selected card, as one step undo can take back." [fields]
  (remember)
  (update-record "issues" :id computed.card.id :fields fields))

(defn hideToast "Hides the notice in the corner." [] (set "state.toast.open" false))

(defn notify "A toast in the corner for a few seconds; it offers Undo when what sent it remembered the columns first."
  [message]
  (set "state.toast" {:open true :message message :undo state.toastUndo})
  (set "state.toastUndo" false)
  (debounce 4000 hideToast))

(defn logActivity "The activity feed, newest first, the last 30." [key title column]
  (set "state.activity" (concat [{:who "RS" :key key :title title :column column}] (take 29 state.activity))))

(defn checkLimit "A notice when a column is over its limit." [c]
  (let [{:keys [limit cards name]} (column c)]
    (when (and (true? settings.board.wipWarnings) (some? limit) (> (count cards) limit))
      (notify (t "toast.wip" {:column name :limit limit})))))

(defn insertCard "A new card at the end of column c, with the next id, as one step undo can take back." [title c p who tag notes]
  (remember)
  (insert "issues"
    :value {:id (str "FLO-" state.nextRef) :ref (str "FLO-" state.nextRef) :board state.boardId :status c
            :order (nextOrder c) :priority p :title title :tag tag :who who :points 1 :notes notes})
  (notify (t "toast.added" {:title title :column (:name (column c))}))
  (logActivity "added" title "")
  (set "state.nextRef" (inc state.nextRef)))

(defn addCard "The composer's card, at the end of its column." []
  (let [title (str/trim state.newCard)
        {:keys [c p who tag notes more]} state.composer]
    (when-not (str/blank? title)
      (insertCard title c p who tag (when-not (str/blank? notes) notes))
      (set "state.newCard" "")
      (set "state.composer.notes" "")
      (set "state.composer.open" more))))

(defn openComposer "Opens the composer on a column." [c]
  (set "state.composer" {:open true :c c :p 0 :who state.me :tag (if (= state.filter :all) "feature" state.filter)
                         :notes "" :more state.composer.more}))

(defn moveCard "A card from one column to the end of another." [from at to]
  (when (and (not= from to) (>= to 0) (<= to computed.lastColumn))
    (remember)
    (update-record "issues" :id (-> (cardsOf from) (get at) :id) :fields {:status to :order (nextOrder to)})
    (set "state.selected" {:c to :i (dec (count (cardsOf to)))})
    (logActivity "moved" (:title (lastCard to)) (:name (column to)))
    (checkLimit to)))

(defn moveSelected "The selected card one column on (1) or back (-1)." [by]
  (moveCard state.selected.c state.selected.i (+ state.selected.c by)))

(defn reorderCard "Swaps a card with its neighbour in the column." [c i by]
  (let [cards (cardsOf c)
        at (+ i by)
        {:keys [id order]} (get cards i)
        other (get cards at)]
    (when (and (>= at 0) (< at (count cards)))
      (remember)
      (update-record "issues" :id id :fields {:order (:order other)})
      (update-record "issues" :id (:id other) :fields {:order order})
      (set "state.selected" {:c c :i at}))))

(defn moveToLane "A card dropped in a swimlane takes that column and that owner." [from at to who]
  (if (not= from to)
    (do (moveCard from at to)
        (update-record "issues" :id (:id (lastCard to)) :fields {:who who}))
    (let [card (get (cardsOf from) at)]
      (when (not= (:who card) who)
        (remember)
        (update-record "issues" :id (:id card) :fields {:who who})))))

;;; By ids and names: what the commands an agent runs (cardMove, cardAdd...) do. A card, a column or a member that
;;; isn't there changes nothing, and the notice in the corner says so.

(defn findCard "A card of this board by its id (FLO-12, in any case), with its column `c` and its place `at`; nil if
  there is none." [ref]
  (let [wanted (-> ref str/trim str/upper-case)]
    (find #(= (:ref %) wanted) computed.allCards)))

(defn columnNamed "The column of this board with that name, in any case, or -1." [name]
  (indexOf (map #(str/lower-case (:name %)) computed.columns) (-> name str/trim str/lower-case)))

(defn memberNamed "A member's initials, from their initials, their name or their first name, in any case; nil if
  nobody has it." [name]
  (let [wanted (-> name str/trim str/lower-case)]
    (:who (find (fn [member]
                  (let [full (str/lower-case (:name member))]
                    (some #{wanted} [(str/lower-case (:who member)) full (first (str/split full " "))])))
                state.members))))

(defn moveCardTo "Moves a card, by its id, to the end of the column with that name." [ref status]
  (let [card (findCard ref)
        to (columnNamed status)]
    (cond (nil? card) (notify (t "toast.noCard" {:ref ref}))
          (neg? to) (notify (t "toast.noColumn" {:column status}))
          :else (moveCard (:c card) (:at card) to))))

(defn addCardTo "A card with that title, mine, at the end of the column with that name, or of the first one." [title status]
  (let [typed (str/trim title)
        c (if-some [named status] (columnNamed named) 0)]
    (cond (str/blank? typed) (notify (t "toast.noTitle"))
          (neg? c) (notify (t "toast.noColumn" {:column status}))
          :else (insertCard typed c 0 state.me "feature" nil))))

(defn assignCardTo "Gives a card, by its id, to a member, by their initials or their name." [ref name]
  (let [card (findCard ref)
        who (memberNamed name)]
    (cond (nil? card) (notify (t "toast.noCard" {:ref ref}))
          (nil? who) (notify (t "toast.noMember" {:who name}))
          :else (do (remember) (update-record "issues" :id (:ref card) :fields {:who who})))))

(defn prioritizeCard "Sets a card's priority, by its id, from its name: none, urgent, high, medium or low." [ref priority]
  (if-some [card (findCard ref)]
    (do (remember)
        (update-record "issues" :id (:ref card)
                       :fields {:priority (indexOf ["none" "urgent" "high" "medium" "low"] priority)}))
    (notify (t "toast.noCard" {:ref ref}))))

;;; Every card

(defn cardsWithPlaces
  "The cards of the columns, one list, each with its column `c` and its place `at` in it."
  [columns]
  (for [[c column] (map-indexed vector columns)
        [at card] (map-indexed vector (:cards column))]
    (merge {:c c :at at} (select-keys card [:ref :priority :due :points :start :tag :title :who]))))

;;; Selecting

(defn announcePresence "Tells the other people on this board which card is open." []
  (emit "presence" :data {:who "RS" :card (:title computed.card)}))

(defn selectCard "Selects a card: c its column, i its place." [c i]
  (when-not (and (some? state.selected) (= state.selected.c c) (= state.selected.i i))
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
  (let [[c i] (map number (str/split (or event.id event.value) "-"))]
    (selectCard c i)))

(defn switchBoard "Another board of the sidebar: undo keeps one board's columns." [id]
  (when (not= id state.boardId)
    (set "state.boardId" id)
    (set "state.board" (:label (find #(= (:id %) id) state.boards)))
    (set "state.selected" nil)
    (set "state.undoStack" [])))

;;; Filters

(defn setFilter "A value of the Filter menu: on says it is applied, which removes it." [prop v on]
  (case prop
    :status (set "state.fStatus" (if on -1 v))
    :assignee (set "state.owner" (if on "" v))
    :priority (set "state.fPrio" (if on -1 v))
    :label (set "state.filter" (if on :all v))
    nil)
  (set "state.filterOpen" false)
  (set "state.filterProp" ""))

(defn clearFilter "The filter a chip's x removes." [prop]
  (case prop
    :status (set "state.fStatus" -1)
    :assignee (set "state.owner" "")
    :priority (set "state.fPrio" -1)
    :label (set "state.filter" :all)
    :project (set "state.fProject" -1)
    nil))

(defn showEverything "No filter, search or scope: every card shows." []
  (set "state.filter" :all)
  (set "state.owner" "")
  (set "state.fStatus" -1)
  (set "state.fPrio" -1)
  (set "state.query" "")
  (set "state.fProject" -1)
  (set "state.scope" :all))

(defcommand clearFilters "Clears the filters, the search and the scope."
  :title ~(t "command.clearFilters") :description ~(t "command.clearFilters.about") :category ~(t "category.view") []
  (showEverything)
  (set "state.searchOpen" false))

(defn goToCard "A quick pick row: from another page, back to the issues, in the board, list or table it showed."
  [item]
  (showPage :issues)
  (when (= state.view :timeline) (set "state.view" :board))
  (showEverything)
  (selectCard (:c item) (:at item)))

;;; The selected card's properties

(defn openPropPick "A picker over the selected card's status, priority or assignee, on what it is now." [kind]
  (set "state.propKind" kind)
  (set "state.propPick"
       {:open true :query "" :top 0
        :index (cond (seq state.marked) 0
                     (= kind :status) state.selected.c
                     (= kind :priority) (or computed.card.priority 0)
                     :else (max 0 (indexOf (map :who state.members) computed.card.who)))}))

(defn setPriority "The priority from 0 (none) to 4 (low); every marked card while some are." [p]
  (if (seq state.marked)
    (bulkPriority p)
    (when (some? computed.card)
      (updateCard {:priority p})
      (set "state.issuePriorityOpen" false))))

(defn setEstimate "The points." [n]
  (set "state.issueEstimateOpen" false)
  (when (some? computed.card) (updateCard {:points n})))

(defn setTag "The issue's main label; its project follows it." [tag]
  (set "state.issueTagOpen" false)
  (when (some? computed.card) (updateCard {:tag tag})))

(defn setAssignee "A member's initials; every marked card while some are." [who]
  (if (seq state.marked)
    (bulkAssignee who)
    (when (some? computed.card)
      (updateCard {:who who})
      (set "state.issueWhoOpen" false))))

(defn setStatus "The column the selected card moves to; every marked card while some are." [to]
  (set "state.issueStatusOpen" false)
  (if (seq state.marked)
    (bulkStatus to)
    (when (some? computed.card) (moveCard state.selected.c state.selected.i to))))

(defn setDue "A date, or nil to clear it." [date]
  (when (some? computed.card)
    (updateCard {:due date})
    (set "state.dueOpen" false)))

(defn setCardDates "A card's dates and owner, from the timeline." [c i start due who]
  (remember)
  (update-record "issues" :id (-> (cardsOf c) (get i) :id) :fields {:start start :due due :who who}))

(defn listOf "A list field of the selected card: labels, assignees, checklist, comments, attachments." [field]
  (or (get computed.card field) []))

(defn toggleListItem "Adds a label or an assignee to the selected card, or removes it." [field value]
  (when (some? computed.card)
    (let [items (listOf field)]
      (updateCard {field (if (contains items value) (remove #{value} items) (conj items value))}))))

(defn addToCard "Adds to a list of the selected card; nothing while the text typed is blank." [field value input]
  (when (and (some? computed.card) (not (str/blank? input)))
    (updateCard {field (conj (listOf field) value)})
    (when (= field "comments") (logActivity "commented" computed.card.title ""))))

(defn removeFromCard "Removes an item of a list of the selected card." [field at]
  (let [items (listOf field)]
    (updateCard {field (concat (take at items) (drop (inc at) items))})))

(defn checkItem "Checks or unchecks an item of the checklist." [at value]
  (updateCard {:checklist (map-indexed (fn [index item] (if (= index at) (assoc item :done value) item))
                                       computed.card.checklist)}))

(defn editNotes "Edits the chosen card's description in place." []
  (when (some? computed.card) (set "state.draft" {:editing true :notes (or computed.card.notes "")})))

(defn saveNotes "Keeps the description being edited." []
  (when (some? computed.card)
    (updateCard {:notes state.draft.notes})
    (set "state.draft.editing" false)
    (notify (t "toast.saved"))))

(defn cancelNotes "Leaves the description as it was." [] (set "state.draft.editing" false))

(defn saveCard "The edit dialog's title." []
  (let [title (str/trim state.edit.title)]
    (when (and (some? computed.card) (not (str/blank? title)))
      (updateCard {:title title})
      (set "state.edit.open" false)
      (notify (t "toast.renamed"))
      (logActivity "renamed" title ""))))

(defn closeEdit "Closes the edit dialog." [] (set "state.edit.open" false))

;;; The timeline

(defn clipOf "A card with a due date as a timeline clip, coloured as its column; days are frames." [card columns]
  (let [{:keys [c at title start due]} card
        from (or start due)]
    {:id (str c "-" at) :label title
     :start (max 0 (daysBetween state.sprintStart from)) :duration (max 1 (daysBetween from due))
     :color (get-in columns [c :color])}))

(defn buildTimeline "One track per member, a clip per card with a start and a due date." []
  (let [columns computed.columns
        owned (group-by :who (filter :due computed.allCards))]
    (set "state.timelineTracks"
         (map (fn [{:keys [who name]}]
                {:id who :label name :clips (map #(clipOf % columns) (get owned who []))})
              state.members))))

(defn changeClip "A clip moved or trimmed: its card gets the new dates, and the track's owner." []
  (let [[c i] (map number (str/split event.id "-"))]
    (setCardDates c i
                  (addDays state.sprintStart (round event.start))
                  (addDays state.sprintStart (round (+ event.start event.duration)))
                  event.track)))

;;; Marking several cards

(defn markSelected "Marks the selected card if it isn't marked yet." []
  (when (and (some? computed.card) (not (contains state.marked computed.card.ref)))
    (push "state.marked" computed.card.ref)))

(defn bulkChanged "Says how many marked cards changed." [] (notify (t "bulk.changed" {:n (count state.marked)})))

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
  (doseq [ref state.marked] (moveMarked ref (columnOf ref) to))
  (set "state.selected" nil)
  (bulkChanged))

;;; Pages and the inbox

(defn showPage "issues, inbox, cycles or projects." [page]
  (set "state.page" page)
  (set "state.issueOpen" false))

(defn openProject "A project's issues, behind a Project filter whose chip removes it." [at]
  (set "state.fProject" at)
  (showPage :issues))

(defn pickNotification "A notification among the shown ones, marked read." [at]
  (set "state.inboxAt" at)
  (update-record "inbox" :id (:id (get computed.inboxShown at)) :fields {:read true}))

;;; What the controls of the views do

(defn toggleOwner "Shows only a member's cards, or everyone's again, and closes the members menu." [who]
  (set "state.owner" (if (= state.owner who) "" who))
  (set "state.membersOpen" false))

(defn showMyIssues "The sidebar's My issues: the issues page, only the person's cards, or everyone's again." []
  (showPage :issues)
  (set "state.owner" (if (= state.owner state.me) "" state.me)))

(defn chooseScope "Shows every card, the active ones or the backlog." [scope]
  (set "state.scope" scope))

(defn chooseProjectScope "Shows every project, or only some of them." [scope]
  (set "state.projectScope" scope))

(defn pickFilterProp "Lists the values of one property in the Filter menu." [prop]
  (set "state.filterProp" prop)
  (set "state.filterQuery" "")
  (focus "#filterField"))

(defn submitFilter
  "Enter in the Filter menu's field: the first property listed, or the first value of the one chosen."
  []
  (cond (and (= state.filterProp "") (seq computed.filterPropsShown))
        (pickFilterProp (ffirst computed.filterPropsShown))
        (and (not= state.filterProp "") (seq computed.filterOptionsShown))
        (let [option (first computed.filterOptionsShown)]
          (setFilter state.filterProp (:v option) (:on option)))))

(defn groupBy "The Display menu's grouping: the swimlanes, unless they are the grouping already." [grouped]
  (when-not grouped (toggleSwimlanes)))

(defn orderBy "The Display menu's ordering: manual, by priority or by due date." [order]
  (set "state.order" order))

(defn toggleProperty "Shows or hides one property on the cards." [prop]
  (set "state.hidden" (if (some #{prop} state.hidden) (remove #{prop} state.hidden) (conj state.hidden prop))))

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

(defcommand cardMove "Moves a card, by its id, to the column with that name."
  :title ~(t "command.cardMove") :description ~(t "command.cardMove.about")
  :category ~(t "category.issue") :icon "arrow-right"
  :arguments {:card {:type string :description "The card's id, like FLO-12"}
              :status {:type string :description "The column's name, like In review"}}
  [card status]
  (moveCardTo card status))

(defcommand cardAdd "Adds a card with that title to a column: the first one unless another is named."
  :title ~(t "command.cardAdd") :description ~(t "command.cardAdd.about")
  :category ~(t "category.issue") :icon "plus"
  :arguments {:title {:type string :description "The card's title"}
              :status {:type [string null] :description "The column's name; the first column when left out"}}
  [title status]
  (addCardTo title status))

(defcommand cardAssign "Gives a card, by its id, to a member, by their initials or their name."
  :title ~(t "command.cardAssign") :description ~(t "command.cardAssign.about")
  :category ~(t "category.issue") :icon "user"
  :arguments {:card {:type string :description "The card's id, like FLO-12"}
              :who {:type string :description "A member's initials, name or first name, like MK or Maya"}}
  [card who]
  (assignCardTo card who))

(defcommand cardPriority "Sets a card's priority, by its id."
  :title ~(t "command.cardPriority") :description ~(t "command.cardPriority.about")
  :category ~(t "category.issue") :icon "chevrons-up"
  :arguments {:card {:type string :description "The card's id, like FLO-12"}
              :priority {:enum [none urgent high medium low] :description "The priority"}}
  [card priority]
  (prioritizeCard card priority))

(defcommand columnNew "A new card in the column whose menu is open."
  :title ~(t "command.columnNew")
  :description ~(t "command.columnNew.about") :icon "plus" :enablement ~state.columnMenu []
  (set "state.columnMenu" false)
  (openComposer state.menuColumn))

(defcommand columnSelect "Marks every card shown in the column whose menu is open."
  :title ~(t "command.columnSelect")
  :description ~(t "command.columnSelect.about") :icon "list-checks" :enablement ~state.columnMenu []
  (set "state.columnMenu" false)
  (set "state.marked" (map :ref (filter #(= (:c %) state.menuColumn) computed.visible))))

(defcommand selectNext "Chooses the next card shown."
  :title ~(t "command.selectNext") :description ~(t "command.selectNext.about")
  :category ~(t "category.go") :icon "arrow-down"
  :enablement ~(some? computed.next) []
  (selectCard computed.next.c computed.next.at))

(defcommand selectPrev "Chooses the card shown before."
  :title ~(t "command.selectPrev") :description ~(t "command.selectPrev.about")
  :category ~(t "category.go") :icon "arrow-up"
  :enablement ~(some? computed.prev) []
  (selectCard computed.prev.c computed.prev.at))

(defcommand changeStatus "Opens the status picker for the chosen or marked cards."
  :title ~(t "command.changeStatus") :description ~(t "command.changeStatus.about")
  :category ~(t "category.issue") :icon "circle-dashed"
  :enablement ~(or (some? computed.card) (pos? (count state.marked))) []
  (openPropPick "status"))

(defcommand changePriority "Opens the priority picker for the chosen or marked cards."
  :title ~(t "command.changePriority") :description ~(t "command.changePriority.about")
  :category ~(t "category.issue") :icon "chevrons-up"
  :enablement ~(or (some? computed.card) (pos? (count state.marked))) []
  (openPropPick "priority"))

(defcommand changeAssignee "Opens the assignee picker for the chosen or marked cards."
  :title ~(t "command.changeAssignee") :description ~(t "command.changeAssignee.about")
  :category ~(t "category.issue") :icon "user"
  :enablement ~(or (some? computed.card) (pos? (count state.marked))) []
  (openPropPick "assignee"))

(defcommand assignToMe "Gives the chosen or marked cards to me."
  :title ~(t "command.assignToMe") :description ~(t "command.assignToMe.about")
  :category ~(t "category.issue") :icon "user-check"
  :enablement ~(some? computed.card) []
  (setAssignee state.me))

(defcommand newCardCommand "Opens the composer on the first column."
  :title ~(t "command.newCardCommand") :description ~(t "command.newCardCommand.about")
  :category ~(t "category.issue") :icon "plus" []
  (openComposer 0))

(defcommand goToCardCommand "Opens the list of cards, to go to one."
  :title ~(t "command.goToCardCommand") :description ~(t "command.goToCardCommand.about")
  :category ~(t "category.go") :icon "search" []
  (set "state.issuePick.query" "")
  (set "state.issuePick.open" true))

(defcommand commandPalette "Opens the palette of every command."
  :title ~(t "command.commandPalette") :description ~(t "command.commandPalette.about")
  :category ~(t "category.go") :icon "command" []
  (set "state.quickPick.query" "")
  (set "state.quickPick.open" true))

(defcommand editCard "Renames the chosen card."
  :title ~(t "command.editCard") :description ~(t "command.editCard.about")
  :category ~(t "category.issue") :icon "pencil"
  :enablement ~(some? computed.card) []
  (set "state.edit" {:open true :title computed.card.title}))

(defcommand deleteCard "Deletes the chosen card, which Undo brings back."
  :title ~(t "command.deleteCard") :description ~(t "command.deleteCard.about")
  :category ~(t "category.issue") :icon "trash-2"
  :enablement ~(some? computed.card) []
  (let [card computed.card]
    (remember)
    (notify (t "toast.deleted" {:title (:title card)}))
    (logActivity "deleted" (:title card) "")
    (delete "issues" :id (:id card))
    (set "state.selected" nil)))

(defcommand moveRight "Moves the chosen card to the next column."
  :title ~(t "command.moveRight") :description ~(t "command.moveRight.about")
  :category ~(t "category.issue") :icon "arrow-right"
  :enablement ~(and (some? computed.card) (< state.selected.c computed.lastColumn)) []
  (moveSelected 1))

(defcommand moveLeft "Moves the chosen card to the column before."
  :title ~(t "command.moveLeft") :description ~(t "command.moveLeft.about")
  :category ~(t "category.issue") :icon "arrow-left"
  :enablement ~(and (some? computed.card) (> state.selected.c 0)) []
  (moveSelected -1))

(defcommand moveUp "Moves the chosen card up its column."
  :title ~(t "command.moveUp") :description ~(t "command.moveUp.about")
  :category ~(t "category.issue") :icon "arrow-up"
  :enablement ~(and (some? computed.card) (> state.selected.i 0)) []
  (reorderCard state.selected.c state.selected.i -1))

(defcommand moveDown "Moves the chosen card down its column."
  :title ~(t "command.moveDown") :description ~(t "command.moveDown.about")
  :category ~(t "category.issue") :icon "arrow-down"
  :enablement ~(and (some? computed.card)
                    (< state.selected.i (dec (count (get-in computed.columns [state.selected.c :cards]))))) []
  (reorderCard state.selected.c state.selected.i 1))

(defcommand undoChange "Takes back the last change to the cards."
  :title ~(t "command.undoChange") :description ~(t "command.undoChange.about")
  :category ~(t "category.issue") :icon "undo-2" []
  (if (empty? state.undoStack)
    (notify (t "toast.nothingToUndo"))
    (do (undoData true)
        (remove-state "state.undoStack" :at (dec (count state.undoStack)))
        (set "state.selected" nil)
        (logActivity "undone" "" "")
        (notify (t "toast.undone")))))

(defcommand openIssue "Opens the chosen card as a page."
  :title ~(t "command.openIssue") :description ~(t "command.openIssue.about")
  :category ~(t "category.issue") :icon "maximize-2"
  :enablement ~(some? computed.card) []
  (set "state.issueOpen" true))

(defcommand closeIssue "Closes the issue page."
  :title ~(t "command.closeIssue") :description ~(t "command.closeIssue.about")
  :category ~(t "category.issue") :icon "arrow-left" :enablement ~state.issueOpen []
  (set "state.issueOpen" false)
  (set "state.issueDueOpen" false)
  (set "state.subOpen" false))

(defcommand toggleMark "Marks the chosen card, or unmarks it."
  :title ~(t "command.toggleMark") :description ~(t "command.toggleMark.about")
  :category ~(t "category.issue") :icon "square-check"
  :enablement ~(some? computed.card) []
  (let [ref computed.card.ref]
    (set "state.marked" (if (contains state.marked ref) (remove #{ref} state.marked) (conj state.marked ref)))))

(defcommand markNext "Marks the next card too."
  :title ~(t "command.markNext") :description ~(t "command.markNext.about")
  :category ~(t "category.issue") :icon "chevrons-down"
  :enablement ~(and (some? computed.card) (some? computed.next)) []
  (markSelected)
  (selectNext)
  (markSelected))

(defcommand markPrevious "Marks the card before too."
  :title ~(t "command.markPrevious") :description ~(t "command.markPrevious.about")
  :category ~(t "category.issue") :icon "chevrons-up"
  :enablement ~(and (some? computed.card) (some? computed.prev)) []
  (markSelected)
  (selectPrev)
  (markSelected))

(defcommand clearMarks "Unmarks every card."
  :title ~(t "command.clearMarks") :description ~(t "command.clearMarks.about")
  :category ~(t "category.issue") :icon "x"
  :enablement ~(pos? (count state.marked)) []
  (set "state.marked" []))

(defcommand markAll "Marks every card shown."
  :title ~(t "command.markAll") :description ~(t "command.markAll.about")
  :category ~(t "category.issue") :icon "list-checks" []
  (set "state.marked" (map :ref computed.visible)))

(defcommand bulkDelete "Deletes every marked card."
  :title ~(t "command.bulkDelete") :description ~(t "command.bulkDelete.about")
  :category ~(t "category.issue") :icon "trash-2"
  :enablement ~(pos? (count state.marked)) []
  (remember)
  (notify (t "bulk.deleted" {:n (count state.marked)}))
  (delete "issues" :id state.marked)
  (set "state.marked" [])
  (set "state.selected" nil))

(defcommand showBoard "Shows the cards as a board."
  :title ~(t "command.showBoard") :description ~(t "command.showBoard.about")
  :category ~(t "category.view") :icon "kanban" [] (set "state.view" :board))

(defcommand showList "Shows the cards as a list."
  :title ~(t "command.showList") :description ~(t "command.showList.about")
  :category ~(t "category.view") :icon "layout-list" [] (set "state.view" :list))

(defcommand showTable "Shows the cards as a table."
  :title ~(t "command.showTable") :description ~(t "command.showTable.about")
  :category ~(t "category.view") :icon "table" [] (set "state.view" :table))

(defcommand showTimeline "Shows the cards on a timeline."
  :title ~(t "command.showTimeline") :description ~(t "command.showTimeline.about")
  :category ~(t "category.view") :icon "chart-gantt" []
  (set "state.view" :timeline))

(defcommand toggleSwimlanes "Groups the board's cards by member, or stops."
  :title ~(t "command.toggleSwimlanes") :description ~(t "command.toggleSwimlanes.about")
  :category ~(t "category.view") :icon "rows-2" []
  (set "state.view" :board)
  (toggle "state.swimlanes"))

(defcommand toggleTheme "Switches between the light and the dark look."
  :title ~(t "command.toggleTheme") :description ~(t "command.toggleTheme.about")
  :category ~(t "category.view") :icon "moon" []
  (set "state.theme" (if (= state.theme :light) :dark :light)))

(defcommand togglePeek "Shows the side panel, or hides it."
  :title ~(t "command.togglePeek") :description ~(t "command.togglePeek.about")
  :category ~(t "category.view") :icon "panel-right" []
  (toggle "state.peek"))

(defcommand openSettings "Opens the settings."
  :title ~(t "command.openSettings") :description ~(t "command.openSettings.about")
  :category ~(t "category.view") :icon "settings" [] (set "state.settingsOpen" true))

(defn closeSettings "Closes the settings." [] (set "state.settingsOpen" false))

(defcommand changeLanguage "Shows Flow in the next language."
  :title ~(t "command.changeLanguage") :description ~(t "command.changeLanguage.about") :category ~(t "category.view")
  :icon "languages" []
  (set "settings.appearance.language"
       (case settings.appearance.language :en :es :es :fr :en)))

(defcommand focusSearch "Opens the search, its field focused."
  :title ~(t "command.focusSearch") :description ~(t "command.focusSearch.about")
  :category ~(t "category.go") :icon "search" []
  (set "state.searchOpen" true)
  (focus "#search"))

(defcommand addProject "Adds a project."
  :title ~(t "command.addProject") :description ~(t "command.addProject.about")
  :category ~(t "category.go") :icon "plus" []
  (insert "projects" :value {:name (t "projects.untitled" {:n (inc (count data.projects))}) :icon "box" :color "#0d9488"
                             :health "onTrack" :lead state.me :target state.cycle.end :progress 0 :issues 0})
  (set "state.projectScope" :all))

(defcommand goToInbox "Shows the inbox."
  :title ~(t "command.goToInbox") :description ~(t "command.goToInbox.about")
  :category ~(t "category.go") :icon "inbox" [] (showPage :inbox))

(defcommand goToIssues "Shows the issues."
  :title ~(t "command.goToIssues") :description ~(t "command.goToIssues.about")
  :category ~(t "category.go") :icon "copy" [] (showPage :issues))

(defcommand goToCycles "Shows the sprints."
  :title ~(t "command.goToCycles") :description ~(t "command.goToCycles.about")
  :category ~(t "category.go") :icon "timer" [] (showPage :cycles))

(defcommand goToProjects "Shows the projects."
  :title ~(t "command.goToProjects") :description ~(t "command.goToProjects.about")
  :category ~(t "category.go") :icon "box" [] (showPage :projects))

(defcommand nextNotification "Chooses the next notification."
  :title ~(t "command.nextNotification") :description ~(t "command.nextNotification.about")
  :category ~(t "category.inbox") :icon "arrow-down"
  :enablement ~(and (= state.page :inbox) (< (inc state.inboxAt) (count computed.inboxShown))) []
  (pickNotification (inc state.inboxAt)))

(defcommand previousNotification "Chooses the notification before."
  :title ~(t "command.previousNotification") :description ~(t "command.previousNotification.about")
  :category ~(t "category.inbox") :icon "arrow-up"
  :enablement ~(and (= state.page :inbox) (> state.inboxAt 0)) []
  (pickNotification (dec state.inboxAt)))

(defcommand toggleRead "Marks the chosen notification read, or unread."
  :title ~(t "command.toggleRead") :description ~(t "command.toggleRead.about")
  :category ~(t "category.inbox") :icon "mail-check"
  :enablement ~(some? computed.notification) []
  (update-record "inbox" :id computed.notification.id :fields {:read (not computed.notification.read)}))

(defcommand markAllRead "Marks every notification read."
  :title ~(t "command.markAllRead") :description ~(t "command.markAllRead.about")
  :category ~(t "category.inbox") :icon "check-check" []
  (update-record "inbox" :where {:read false} :fields {:read true}))

(defcommand snoozeNotification "Hides the chosen notification for now."
  :title ~(t "command.snoozeNotification") :description ~(t "command.snoozeNotification.about")
  :category ~(t "category.inbox") :icon "alarm-clock"
  :enablement ~(some? computed.notification) []
  (update-record "inbox" :id computed.notification.id :fields {:snoozed true})
  (set "state.inboxAt" (-> state.inboxAt (min (- (count computed.inboxShown) 2)) (max 0)))
  (notify (t "inbox.snoozed")))

(defcommand openNotification "Opens the issue of the chosen notification."
  :title ~(t "command.openNotification") :description ~(t "command.openNotification.about")
  :category ~(t "category.inbox") :icon "maximize-2"
  :enablement ~(some? computed.notificationCard) []
  (selectCard computed.notificationCard.c computed.notificationCard.at)
  (set "state.page" :issues)
  (set "state.issueOpen" true))
