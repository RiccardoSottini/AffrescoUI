{:note "Calamaio's own languages, each coloured by the @syntax.* colours of look.lisp; Python, C and Markdown take their grammar. A folder's listing and the buffer list are languages too: their first line dimmed, a flagged line, folders."
 :languages {:lisp {:title "Lisp"
                    :aliases [lisp-interaction]
                    :files ["*.el" "*.lisp"]
                    :comments {:line ";"}
                    :wordPattern "[\\w-]+"
                    :patterns [{:match ";.*" :scope comment}
                               {:match "\"(?:[^\"\\\\]|\\\\.)*\"?" :scope string}
                               {:match "(?<=\\()(?:defun|defmacro|defvar|defcustom|defconst|defgroup|let\\*?|if|when|unless|cond|while|dolist|dotimes|progn|lambda|setq|and|or|require|provide|interactive|save-excursion|condition-case)(?=[\\s()])"
                                :scope keyword}
                               {:match "(?<=\\(defun )[^\\s()]+|(?<=\\(defmacro )[^\\s()]+" :scope function}
                               {:match "(?<=\\(defvar )[^\\s()]+|(?<=\\(defcustom )[^\\s()]+|(?<=\\(defconst )[^\\s()]+"
                                :scope label}
                               {:match "(?<![\\w-]):[\\w-]+" :scope function.builtin}]}
             :org {:title "Org"
                   :files ["*.org"]
                   :wordPattern "[\\w-]+"
                   :patterns [{:match "^#\\+\\w+:.*" :scope comment}
                              {:match "^\\* .*" :scope text.title}
                              {:match "^\\*\\* .*" :scope label}
                              {:match "^\\*{3,} .*" :scope keyword}
                              {:match "\\[\\[[^\\]]*\\](?:\\[[^\\]]*\\])?\\]" :scope text.uri}
                              {:match "\\b(?:TODO|NEXT)\\b" :scope string}
                              {:match "\\bDONE\\b" :scope type}]}
             :dired {:title "Folder"
                     :patterns [{:match "^D .*" :scope constant}
                                {:match "^\\S.*" :scope comment}
                                {:match "\\d+ \\S+ \\d\\d:\\d\\d" :scope comment}
                                {:match "\\S+/$" :scope keyword}]}
             :buffer-menu {:title "Buffer List"
                           :patterns [{:match "^[^\\s%*].*" :scope comment}
                                      {:match "^  -.*" :scope comment}
                                      {:match "^\\* " :scope constant}
                                      {:match "^% " :scope comment}
                                      {:match "(?<=^[ %*] )\\S+" :scope keyword}]}}}
