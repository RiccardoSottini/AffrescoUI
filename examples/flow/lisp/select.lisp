;;; select.lisp --- Flow's multi-select, included by index.screen.lisp: X marks the selected card, Shift+J and Shift+K
;;; extend the marks, and a bar at the bottom changes the status, priority or assignee of every marked card at once,
;;; or deletes them
;;;
;;; Each bulk change is one undo step.

;;; State

(defstate marked [])

;;; Keys

(defkey "x" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do toggleMark})

(defkey "shift+j" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do markNext})

(defkey "shift+down" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do markNext})

(defkey "shift+k" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do markPrevious})

(defkey "shift+up" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do markPrevious})

(defkey "mod+a" {:when ~(and (not context.textInputFocus) (= state.page "issues")) :do markAll})
