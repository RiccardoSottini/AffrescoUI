;;; keys.lisp --- Calamaio's keys: C- is ctrl, M- is alt
;;;
;;; A key names the command it runs (or a command with its argument). The echo area and help show them in the
;;; editor's own letters (window.keyLabels: ctrl+x ctrl+f reads C-x C-f). A window's own keys (RET, g and q in a
;;; listing, the digits of an argument, F10) are in window.lisp; the minibuffer's in echo.lisp.

;;; Motion

(defkey "ctrl+f" forward-char)
(defkey "ctrl+b" backward-char)
(defkey "ctrl+n" next-line)
(defkey "ctrl+p" previous-line)
(defkey "alt+f" forward-word)
(defkey "alt+b" backward-word)
(defkey "ctrl+a" move-beginning-of-line)
(defkey "ctrl+e" move-end-of-line)
(defkey "alt+shift+comma" beginning-of-buffer)
(defkey "alt+shift+period" end-of-buffer)
(defkey "ctrl+v" scroll-up-command)
(defkey "alt+v" scroll-down-command)

;;; The mark, the argument and C-g

(defkey "ctrl+space" set-mark-command)
(defkey "ctrl+x h" mark-whole-buffer)
(defkey "ctrl+x ctrl+x" exchange-point-and-mark)
(defkey "ctrl+u" universal-argument)
(defkey "alt+0" {:do digit-argument :with {:d 0}})
(defkey "alt+1" {:do digit-argument :with {:d 1}})
(defkey "alt+2" {:do digit-argument :with {:d 2}})
(defkey "alt+3" {:do digit-argument :with {:d 3}})
(defkey "alt+4" {:do digit-argument :with {:d 4}})
(defkey "alt+5" {:do digit-argument :with {:d 5}})
(defkey "alt+6" {:do digit-argument :with {:d 6}})
(defkey "alt+7" {:do digit-argument :with {:d 7}})
(defkey "alt+8" {:do digit-argument :with {:d 8}})
(defkey "alt+9" {:do digit-argument :with {:d 9}})
(defkey "ctrl+g" keyboard-quit)

;;; Editing

(defkey "ctrl+d" delete-char)
(defkey "ctrl+slash" undo)
(defkey "ctrl+shift+minus" undo)
(defkey "ctrl+x u" undo)
(defkey "ctrl+alt+slash" undo-redo)
(defkey "ctrl+x ctrl+u" upcase-region)
(defkey "ctrl+x ctrl+l" downcase-region)
(defkey "alt+semicolon" comment-line)
(defkey "ctrl+t" transpose-chars)
(defkey "ctrl+o" open-line)

;;; The kill ring

(defkey "ctrl+k" kill-line)
(defkey "alt+d" kill-word)
(defkey "alt+backspace" backward-kill-word)
(defkey "ctrl+shift+backspace" kill-whole-line)
(defkey "ctrl+w" kill-region)
(defkey "alt+w" kill-ring-save)
(defkey "ctrl+y" yank)
(defkey "alt+y" yank-pop)

;;; Searching

(defkey "ctrl+s" isearch-forward)
(defkey "ctrl+r" isearch-backward)
(defkey "alt+shift+5" query-replace)

;;; Windows and the frame

(defkey "ctrl+x 2" split-window-below)
(defkey "ctrl+x 3" split-window-right)
(defkey "ctrl+x 0" delete-window)
(defkey "ctrl+x 1" delete-other-windows)
(defkey "ctrl+x o" other-window)
(defkey "f10" menu-bar-open)
(defkey "ctrl+z" suspend-frame)

;;; Buffers and files

(defkey "ctrl+x b" switch-to-buffer)
(defkey "ctrl+x k" kill-buffer)
(defkey "ctrl+x ctrl+b" list-buffers)
(defkey "ctrl+x ctrl+f" find-file)
(defkey "ctrl+x ctrl+s" save-buffer)
(defkey "ctrl+x ctrl+w" write-file)
(defkey "ctrl+x i" insert-file)
(defkey "ctrl+x d" dired)
(defkey "ctrl+x ctrl+c" save-buffers-kill-terminal)

;;; Help and M-x

(defkey "alt+x" execute-extended-command)
(defkey "ctrl+h f" describe-function)
(defkey "ctrl+h v" describe-variable)
(defkey "ctrl+h k" describe-key)
(defkey "ctrl+h b" describe-bindings)
(defkey "ctrl+h m" describe-mode)
(defkey "ctrl+h e" view-echo-area-messages)
(defkey "ctrl+h t" help-with-tutorial)
(defkey "ctrl+c a" assistant.toggle)

;;; Lisp

(defkey "ctrl+x ctrl+e" eval-last-sexp)
(defkey "ctrl+j" eval-print-last-sexp)
(defkey "ctrl+alt+x" eval-defun)
(defkey "ctrl+x ctrl+z" undo-evaluation)
