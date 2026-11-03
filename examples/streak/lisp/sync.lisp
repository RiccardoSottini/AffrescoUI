;;; sync.lisp --- the document server: Streak's habits in the person's own document (~streak), synced both ways
;;;
;;; A placeholder provider signs in: put your own here and in the host's allowed origins. habits.lisp has what the
;;; channel's events run.

;;; The server
(defpart server "The server's channel, what it sends, the sync's watch, and the channel's events."
  :connections {:streakServer {:url "ws://127.0.0.1:7420/live"
                               :auth {:flow device
                                      :deviceUrl "https://id.example.com/oauth/device"
                                      :tokenUrl "https://id.example.com/oauth/token"
                                      :clientId streak
                                      :issuer "https://id.example.com"
                                      :scopes [openid profile]
                                      :note "A placeholder provider: put your own here and in the host's allowed origins."}
                               :messages {:open (open :document "~streak")
                                          :revision (revision :id ~id
                                                      :set {"/state/habits" ~data.habits
                                                            "/state/weeks" ~data.weeks
                                                            "/state/notes" ~data.notes
                                                            "/state/dayNotes" ~data.dayNotes})
                                          :getDocument (getDocument)}
                               :reconnect true
                               :note "The person's habits in their own document on the document server (apps/server), ~streak, synced both ways."}}
  :watch {:computed.syncData [{:debounce 800 :do [{:run syncSend}]}]}
  :actions {"streakServer:open" [{:lisp serverOpened}]
            "streakServer:hello" [{:lisp serverHas}]
            "streakServer:document" [{:lisp serverHas}]
            "streakServer:committed" [{:lisp serverCommitted}]
            "streakServer:error" [{:lisp serverFailed}]
            "streakServer:closed" [{:lisp serverClosed}]})

