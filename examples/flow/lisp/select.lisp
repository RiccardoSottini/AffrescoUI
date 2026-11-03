{:note "Flow's multi-select, included by index.screen.lisp: X marks the selected card, Shift+J and Shift+K extend the marks, and a bar at the bottom changes the status, priority or assignee of every marked card at once, or deletes them. Each bulk change is one undo step."
 :state {:marked []}
 :keys {:x {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do toggleMark}
        "shift+j" {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do markNext}
        "shift+down" {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do markNext}
        "shift+k" {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do markPrevious}
        "shift+up" {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do markPrevious}
        "mod+a" {:when "{{!context.textInputFocus && state.page == 'issues'}}" :do markAll}}}
