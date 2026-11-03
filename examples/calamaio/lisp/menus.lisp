;;; menus.lisp --- Calamaio's menus: the menu bar's File, Edit, Options, Tools and Help, and one per major mode
;;;
;;; An item runs a command, and shows the command's title unless it has its own. A major mode's menu is shown after
;;; Tools while a buffer in that mode is selected; Buffers is made from the buffer list (bars.lisp).

;;; The menu bar

(defmenu file "The File menu: visiting, saving and closing, then the windows."
  {:find {:run find-file :title ~(t "menu.file.find") :group "1_open" :order 1}
   :open {:run find-file :title ~(t "menu.file.open") :group "1_open" :order 2}
   :save {:run save-buffer :group "2_save" :order 1}
   :close {:run kill-buffer :group "1_open" :order 5}
   :below {:run split-window-below :group "4_windows" :order 1}
   :right {:run split-window-right :group "4_windows" :order 2}
   :one {:run delete-other-windows :group "4_windows" :order 3}
   :remove {:run delete-window :group "4_windows" :order 4}
   :quit {:run save-buffers-kill-terminal :group "9_quit"}
   :insert {:run insert-file :group "1_open" :order 4}
   :saveas {:run write-file :group "2_save" :order 2}
   :dired {:run dired :group "1_open" :order 3}})

(defmenu edit "The Edit menu: undo, the kill ring, search, the mark, the region."
  {:undo {:run undo :group "1_undo" :order 1}
   :redo {:run undo-redo :group "1_undo" :order 2}
   :mark {:run set-mark-command :group "3_mark" :order 1}
   :top {:run beginning-of-buffer :group "4_go" :order 1}
   :bottom {:run end-of-buffer :group "4_go" :order 2}
   :search {:run isearch-forward :group "2_search" :order 1}
   :searchBack {:run isearch-backward :group "2_search" :order 2}
   :replace {:run query-replace :group "2_search" :order 3}
   :cut {:run kill-region :group "1b_kill" :order 1}
   :copy {:run kill-ring-save :group "1b_kill" :order 2}
   :paste {:run yank :group "1b_kill" :order 3}
   :pasteOlder {:run yank-pop :group "1b_kill" :order 4}
   :all {:run mark-whole-buffer :group "3_mark" :order 2}
   :exchange {:run exchange-point-and-mark :group "3_mark" :order 3}
   :upcase {:run upcase-region :group "5_region" :order 1}
   :downcase {:run downcase-region :group "5_region" :order 2}
   :capitalize {:run capitalize-region :group "5_region" :order 3}
   :sort {:run sort-lines :group "5_region" :order 4}
   :comment {:run comment-line :group "5_region" :order 5}})

(defmenu options "The Options menu: the minor modes, the frame's bars, the theme and what happens at start."
  {:lines {:run display-line-numbers-mode :group "1_modes" :order 1}
   :wrap {:run visual-line-mode :group "1_modes" :order 2}
   :paren {:run show-paren-mode :group "1_modes" :order 3}
   :toolbar {:run tool-bar-mode :group "4_frame" :order 1}
   :theme {:run load-theme :title ~(t "menu.options.theme") :group "5_look"}
   :splash {:run toggle-splash :title ~(t "menu.options.splash") :group "6_start" :order 1}
   :customize {:run customize :title ~(t "menu.options.customize") :group "6_start" :order 2}
   :menubar {:run menu-bar-mode :group "4_frame" :order 0}})

(defmenu tools "The Tools menu."
  {:command {:run execute-extended-command :group "1_run"}
   :buffers {:run list-buffers :group "1_run" :order 2}})

(defmenu help "The Help menu: the guides, the describe commands and the messages."
  {:tutorial {:run help-with-tutorial :group "1_start" :order 1}
   :tour {:run show-tour :group "1_start" :order 2}
   :splash {:run show-splash :title ~(t "menu.help.splash") :group "1_start" :order 3}
   :command {:run execute-extended-command :title ~(t "menu.help.command") :group "2_find" :order 1}
   :fn {:run describe-function :group "2_find" :order 2}
   :key {:run describe-key :group "2_find" :order 3}
   :bindings {:run describe-bindings :group "2_find" :order 4}
   :messages {:run show-messages :title ~(t "menu.help.messages") :group "3_log"}})

;;; The major modes' menus

(defmenu lisp "The Lisp modes' menu."
  {:top {:run beginning-of-buffer :group "1_move" :title ~(t "menu.goToStart")}
   :bottom {:run end-of-buffer :group "1_move" :title ~(t "menu.goToEnd")}})

(defmenu text "Text mode's menu."
  {:top {:run beginning-of-buffer :group "1_move" :title ~(t "menu.goToStart")}})

(defmenu python "Python mode's menu."
  {:top {:run beginning-of-buffer :group "1_move" :title ~(t "menu.goToStart")}})

(defmenu c "C mode's menu."
  {:top {:run beginning-of-buffer :group "1_move" :title ~(t "menu.goToStart")}})

(defmenu outline "The outline modes' menu (Markdown, Org)."
  {:top {:run beginning-of-buffer :group "1_move" :title ~(t "menu.goToStart")}})
