{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "CMake: include it to colour and edit CMake in an editor ({ \"language\": \"cmake\" })."
  :languages {:cmake {:title CMake
      :aliases [cmake]
      :files [CMakeLists.txt "*.cmake" CMakeCache.txt]
      :comments {:line "#"}
      :brackets [["(" ")"]]
      :pairs [["(" ")"] ["\"" "\""]]
      :indent {:increase "^\\s*(?:(?:if|else|elseif|foreach|while|function|macro|block)\\s*\\(.*|.*\\([^)\\\"]*)$"
        :decrease "^\\s*(?:(?:end\\w+|else|elseif)\\s*\\(|\\))"}
      :wordPattern "[\\w.-]+"
      :patterns [{:begin "#\\[(=*)\\[" :end "\\]\\1\\]" :scope comment.block}
        {:match "#.*" :scope comment.line}
        {:begin "\\[(=*)\\[" :end "\\]\\1\\]" :scope string.bracket}
        {:match "\"(?:[^\"\\\\]|\\\\.)*\"?" :scope string.quoted.double}
        {:match "\\$\\{[^}]*\\}?" :scope type.variable}
        {:match "(?<![\\w:])\\d[\\w.']*" :scope number}
        {:match "(?<![\\w:])[A-Za-z_][\\w:]*(?=\\s*\\()" :scope keyword.command}
        {:match "(?<![\\w:])[A-Z_][A-Z0-9_:]*(?![\\w:])" :scope type.option}]}}}
