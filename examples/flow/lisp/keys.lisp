;;; keys.lisp --- Flow's keyboard shortcuts and its context menus
;;;
;;; A key runs a command of flow.lisp by name, some only while no text field has the focus and the issues page is
;;; shown; a menu lists commands by group.

;;; Keys

(defkey "c" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do newCardCommand})

(defkey "mod+n" newCardCommand)

(defkey "mod+p" goToCardCommand)

(defkey "mod+shift+p" commandPalette)

(defkey "mod+k" commandPalette)

(defkey "f"
  {:when ~(and (not context.textInputFocus) (= state.page "issues"))
   :do [{:set state.filterQuery :to ""} {:set state.filterProp :to ""} {:toggle state.filterOpen} {:focus "#filterField"}]})

(defkey "j"
  [{:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do nextNotification}
   {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do selectNext}])

(defkey "k"
  [{:when ~(and (not context.textInputFocus) (= state.page "inbox")) :do previousNotification}
   {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do selectPrev}])

(defkey "s" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changeStatus})

(defkey "p" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changePriority})

(defkey "a" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do changeAssignee})

(defkey "i" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do assignToMe})

(defkey "slash" {:when ~(not context.textInputFocus) :do focusSearch})

(defkey "mod+e" editCard)

(defkey "mod+j" toggleTheme)

(defkey "mod+right" moveRight)

(defkey "mod+left" moveLeft)

(defkey "mod+enter" openIssue)

(defkey "escape"
  [{:when ~state.issueOpen :do closeIssue} {:when ~(> (count state.marked) 0) :do clearMarks} {:do clearFilters}])

;;; Menus

(defmenu card
  {:edit {:run editCard :group "1_edit"}
   :left {:run moveLeft :group "2_move" :order 1}
   :right {:run moveRight :group "2_move" :order 2}
   :delete {:run deleteCard :group "3_delete"}})

(defmenu column {:new {:run columnNew :group "1_add"} :select {:run columnSelect :group "2_select"}})
