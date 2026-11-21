;;; sync.lisp --- the document server: Streak's habits in the person's own document (~streak), synced both ways
;;;
;;; The channel itself is data, in server.lisp, so that no one can move where the habits sync (the policy); habits.lisp
;;; has what the channel's events run.

(ns streak.sync)

;;; The server

(defpart server "The sync's watch, and what the channel's events run."
  :watch {:computed.syncData [{:debounce 800 :do [{:run sync-send}]}]}
  :actions {"streakServer:open" [{:lisp server-opened}]
            "streakServer:hello" [{:lisp server-has}]
            "streakServer:document" [{:lisp server-has}]
            "streakServer:committed" [{:lisp server-committed}]
            "streakServer:error" [{:lisp server-failed}]
            "streakServer:closed" [{:lisp server-closed}]})

