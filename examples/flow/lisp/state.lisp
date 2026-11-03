;;; state.lisp --- what Flow keeps while it runs, and where each value starts
;;;
;;; Each defstate is one key of the state, with the value it starts from and, for some, its type. The board's own state
;;; (the activity feed, the presence, the burndown) is in board.lisp; the issue page's, in issue.lisp; the pages', in
;;; pages.lisp.

;;; The boards and their cards

(defstate theme light :type {:enum [light dark]})

(defstate board "Product launch" :type (string))

(defstate boardId launch :type (string))

(defstate boards
  [{:id launch :label "Product launch" :closable false}
   {:id website :label "Website" :closable false}
   {:id mobile :label "Mobile app" :closable false}])

(defstate view board :type {:enum [board list table timeline]})

(defstate me "RS")

(defstate nextRef 21 :type (number))

(defstate priorities [minus flame chevrons-up chevron-up chevron-down])

(defstate statusIcons [circle loader eye check-circle])

(defstate selected nil)

(defstate peek false)

(defstate sizes [900])

(defstate members
  [{:who "RS" :name "Remy Shaw" :color "#2563eb"}
   {:who "MK" :name "Maya Klein" :color "#9333ea"}
   {:who "AL" :name "Ada Lewis" :color "#0f766e"}
   {:who "JD" :name "Jon Doe" :color "#d97706"}] :type (array :items
                                                              (object :properties
                                                                      {:who (string) :name (string) :color (string)})))

(defstate lanes
  {:launch [{:name "Backlog" :color "#94a3b8"}
            {:name "In progress" :color "#ca8a04" :limit 3}
            {:name "In review" :color "#16a34a" :limit 3}
            {:name "Done" :color "#0d9488"}]
   :website [{:name "Backlog" :color "#94a3b8"}
             {:name "In progress" :color "#ca8a04"}
             {:name "In review" :color "#16a34a"}
             {:name "Done" :color "#0d9488"}]
   :mobile [{:name "Backlog" :color "#94a3b8"}
            {:name "In progress" :color "#ca8a04"}
            {:name "In review" :color "#16a34a"}
            {:name "Done" :color "#0d9488"}]} :type (object))

;;; Filters, search and the Display menu

(defstate filter all :type (string))

(defstate scope all)

(defstate order manual)

(defstate hidden [])

(defstate fStatus -1)

(defstate fPrio -1)

(defstate fProject -1)

(defstate filterProp "")

(defstate filterQuery "")

(defstate filterProps [[status circle] [assignee circle-user] [priority chevrons-up] [label tag]])

(defstate scopes [all active backlog])

(defstate searchOpen false)

(defstate filters [all bug feature design chore] :type (array :items (string)))

(defstate tagIcons {:bug bug :feature sparkles :design palette :chore wrench})

(defstate filterOpen false)

(defstate displayOpen false)

(defstate owner "" :type (string))

(defstate membersOpen false :type (boolean))

(defstate workspaceOpen false)

(defstate tableSort nil)

(defstate columnWidths {:ref 80 :priority 100 :title 300 :column 120 :tag 110 :who 90 :points 80})

(defstate collapsedGroups [])

;;; Menus, pickers and dialogs

(defstate newCard "" :type (string))

(defstate composer {:open false :c 0 :p 0 :who "RS" :tag feature :notes "" :more false})

(defstate cardMenu false :type (boolean))

(defstate columnMenu false)

(defstate menuColumn 0)

(defstate menuAt {:x 0 :y 0})

(defstate edit {:open false :title ""})

(defstate toast {:open false :message "" :undo false})

(defstate toastUndo false)

(defstate quickPick {:open false :query ""})

(defstate propPick {:open false :query ""})

(defstate issuePick {:open false :query ""})

(defstate propKind status)
