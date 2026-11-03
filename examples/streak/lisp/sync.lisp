;;; sync.lisp --- the document server: Streak's habits in the person's own document (~streak), synced both ways
;;;
;;; The channel itself is data, in server.lisp, so that no one can move where the habits sync (the policy); habits.lisp
;;; has what the channel's events run.

;;; The server

(defpart server "The sync's watch, and what the channel's events run."
  :watch {:computed.syncData [{:debounce 800 :do [{:run syncSend}]}]}
  :actions {"streakServer:open" [{:lisp serverOpened}]
            "streakServer:hello" [{:lisp serverHas}]
            "streakServer:document" [{:lisp serverHas}]
            "streakServer:committed" [{:lisp serverCommitted}]
            "streakServer:error" [{:lisp serverFailed}]
            "streakServer:closed" [{:lisp serverClosed}]})

