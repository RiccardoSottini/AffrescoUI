;;; keys.lisp --- Sinopia's keyboard: editing, the text's style and alignment, and the window
;;;
;;; Most keys act on the selected objects while no text field has the focus; the slide show's own keys are in
;;; parts/show.lisp, and the panes' in parts/views.lisp.

;;; Keys

(defkey "alt+q" {:when ~(not state.show.on) :do [{:focus "#search"}]})

(defkey "mod+f1" {:when ~(not state.show.on) :do [{:toggle state.ribbon}]})

(defkey "mod+z" {:when ~(not context.textInputFocus) :do undo})

(defkey "mod+y" {:when ~(not context.textInputFocus) :do redo})

(defkey "mod+shift+z" {:when ~(not context.textInputFocus) :do redo})

(defkey "mod+m" {:when ~(not context.textInputFocus) :do newSlide :with {:layout content}})

(defkey "mod+d" {:when ~(and (not context.textInputFocus) (> (count computed.selected) 0)) :do duplicateObjects})

(defkey "mod+c" {:when ~(not context.textInputFocus) :do copyObjects})

(defkey "mod+x" {:when ~(not context.textInputFocus) :do cutObjects})

(defkey "mod+v" {:when ~(not context.textInputFocus) :do pasteObjects})

(defkey "mod+a" {:when ~(not context.textInputFocus) :do selectAll})

(defkey "mod+b" {:when ~(not context.textInputFocus) :do toggleText :with {:key bold}})

(defkey "mod+i" {:when ~(not context.textInputFocus) :do toggleText :with {:key italic}})

(defkey "mod+u" {:when ~(not context.textInputFocus) :do toggleText :with {:key underline}})

(defkey "mod+l" {:when ~(not context.textInputFocus) :do setObjects :with {:props {:align left}}})

(defkey "mod+e" {:when ~(not context.textInputFocus) :do setObjects :with {:props {:align center}}})

(defkey "mod+r" {:when ~(not context.textInputFocus) :do setObjects :with {:props {:align right}}})

(defkey "mod+j" {:when ~(not context.textInputFocus) :do setObjects :with {:props {:align justify}}})

(defkey "mod+shift+." {:when ~(not context.textInputFocus) :do growText :with {:by 1}})

(defkey "mod+shift+," {:when ~(not context.textInputFocus) :do growText :with {:by -1}})

(defkey "delete" {:when ~(and (not context.textInputFocus) (> (count computed.selected) 0)) :do deleteObjects})

(defkey "f2"
  {:when ~(and (not context.textInputFocus) (not state.show.on) (= (count computed.selected) 1))
   :do editText
   :with {:id ~state.board.selection.0}})

(defkey "backspace"
  [{:when ~state.show.on :do showPrevious}
   {:when ~(and (not context.textInputFocus) (> (count computed.selected) 0)) :do deleteObjects}])

(defkey "mod+f" {:when ~(not state.show.on) :do ~(handler openFind false)})

(defkey "mod+h" {:when ~(not state.show.on) :do ~(handler openFind true)})

(defkey "escape"
  [{:when ~state.show.on :do showEnd}
   {:when ~(some? state.inkTool) :do inkTool :with {:tool nil}}
   {:when ~(some? state.editing) :do stopEditing}
   {:when ~(and (not context.textInputFocus)
                (not state.drop.open)
                (not state.colors.open)
                (not state.find.open)
                (> (count state.board.selection) 0))
    :do [{:set state.board.selection :to []}]}])
