{:note "Calamaio's menus: the menu bar's File, Edit, Options, Tools and Help, and one menu per major mode (shown after Tools while a buffer in that mode is selected). Buffers is made from the buffer list (computed.buffersMenu)."
  :menus {:file {:find {:run find-file :title "Visit New File..." :group "1_open" :order 1}
      :open {:run find-file :title "Open File..." :group "1_open" :order 2}
      :save {:run save-buffer :group "2_save" :order 1}
      :close {:run kill-buffer :group "1_open" :order 5}
      :below {:run split-window-below :group "4_windows" :order 1}
      :right {:run split-window-right :group "4_windows" :order 2}
      :one {:run delete-other-windows :group "4_windows" :order 3}
      :remove {:run delete-window :group "4_windows" :order 4}
      :quit {:run save-buffers-kill-terminal :group "9_quit"}
      :insert {:run insert-file :group "1_open" :order 4}
      :saveas {:run write-file :group "2_save" :order 2}
      :dired {:run dired :group "1_open" :order 3}}
    :edit {:undo {:run undo :group "1_undo" :order 1}
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
      :comment {:run comment-line :group "5_region" :order 5}}
    :options {:lines {:run display-line-numbers-mode :group "1_modes" :order 1}
      :wrap {:run visual-line-mode :group "1_modes" :order 2}
      :paren {:run show-paren-mode :group "1_modes" :order 3}
      :toolbar {:run tool-bar-mode :group "4_frame" :order 1}
      :theme {:run load-theme :title "Load Theme..." :group "5_look"}
      :splash {:run toggle-splash :title "Show the Start Screen at Start" :group "6_start" :order 1}
      :customize {:run customize :title "Customize Calamaio" :group "6_start" :order 2}
      :menubar {:run menu-bar-mode :group "4_frame" :order 0}}
    :tools {:command {:run execute-extended-command :group "1_run"}
      :buffers {:run list-buffers :group "1_run" :order 2}}
    :help {:tutorial {:run help-with-tutorial :group "1_start" :order 1}
      :tour {:run show-tour :group "1_start" :order 2}
      :splash {:run show-splash :title "Start Screen" :group "1_start" :order 3}
      :command {:run execute-extended-command :title "Run a Command by Name..." :group "2_find" :order 1}
      :fn {:run describe-function :group "2_find" :order 2}
      :key {:run describe-key :group "2_find" :order 3}
      :bindings {:run describe-bindings :group "2_find" :order 4}
      :messages {:run show-messages :title Messages :group "3_log"}}
    :lisp {:top {:run beginning-of-buffer :group "1_move" :title "Go to the Start"}
      :bottom {:run end-of-buffer :group "1_move" :title "Go to the End"}}
    :text {:top {:run beginning-of-buffer :group "1_move" :title "Go to the Start"}}
    :python {:top {:run beginning-of-buffer :group "1_move" :title "Go to the Start"}}
    :c {:top {:run beginning-of-buffer :group "1_move" :title "Go to the Start"}}
    :outline {:top {:run beginning-of-buffer :group "1_move" :title "Go to the Start"}}}}
