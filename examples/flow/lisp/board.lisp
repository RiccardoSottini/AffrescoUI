{:note "Flow's board, included by index.screen.lisp: its settings and state, the work-in-progress counts, the timeline's watch, presence from the host's channel, and the keys for undo and moves. What they run is in flow.lisp."
 :settings {:appearance.language (string :enum [en es fr]
                                   :default en
                                   :title "Language"
                                   :description "English, Spanish or French.")
            :board.wipWarnings (boolean :default true
                                 :title "Warn over a column's limit"
                                 :description "Show a notice when a move puts a column over its work-in-progress limit.")
            :board.weekStart (number :minimum 0
                               :maximum 6
                               :default 1
                               :title "First day of the week"
                               :description "0 for Sunday, 1 for Monday, up to 6 for Saturday, in the date picker.")}
 :state {:query ""
         :labels [urgent frontend backend docs]
         :swimlanes false
         :undoStack []
         :today "2026-03-16"
         :sprintStart "2026-03-09"
         :burndown [34 33 30 29 27 24 23]
         :activity [{:who "MK" :key moved :title "New colour palette for labels" :column "In progress"}
                    {:who "JD" :key commented :title "Invite teammates by email" :column ""}
                    {:who "AL" :key added :title "Document the board API" :column ""}]
         :presence {}
         :draft {:editing false :notes ""}
         :dueOpen false
         :settingsOpen false
         :newItem ""
         :newComment ""
         :newAttachment ""
         :timelineTracks []
         :settings {}}
 :computed {:remaining "{{sum(filter(computed.allCards, it.c != computed.lastColumn), it.points)}}"
            :burndownValues "{{concat(state.burndown, [computed.remaining])}}"
            :checklistDone "{{computed.card == null ? 0 : count(computed.card.checklist ?? [], it.done)}}"
            :dueDays "{{computed.card == null || computed.card.due == null ? null : daysBetween(state.today, computed.card.due)}}"
            :present "{{filter(keys(state.presence), state.presence[it] != null)}}"}
 :watch {:computed.columns buildTimeline :state.members buildTimeline}
 :onLoad buildTimeline
 :channels {:team {:socket studio.sock :reconnect {:after 2000}}}
 :actions {"team:committed" [{:note "Someone changed the board: the feed shows it and its author is here"}
                             {:set state.activity
                              :to "{{concat([{ who: event.author ?? '?', key: 'revised', title: event.message ?? '', column: '' }], slice(state.activity, 0, 29))}}"}
                             {:set "state.presence[event.author ?? '?']"
                              :to "{{state.presence[event.author ?? '?'] ?? ''}}"}]
           "team:event" [{:note "presence: { who, card } from another person's Flow"}
                         {:if "{{event.name != 'presence' || event.data.who == 'RS'}}" :then [{:stop true}]}
                         {:set "state.presence[event.data.who]" :to "{{event.data.card ?? ''}}"}]
           "team:closed" [{:set state.presence :to {}}]}
 :keys {"mod+z" undoChange
        "mod+l" changeLanguage
        "mod+f" focusSearch
        "mod+comma" openSettings
        "alt+up" {:when "{{!context.textInputFocus}}" :do moveUp}
        "alt+down" {:when "{{!context.textInputFocus}}" :do moveDown}
        "alt+left" {:when "{{!context.textInputFocus}}" :do moveLeft}
        "alt+right" {:when "{{!context.textInputFocus}}" :do moveRight}}}
