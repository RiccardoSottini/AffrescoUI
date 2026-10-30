{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "JSON: include it to colour and edit JSON in an editor ({ \"language\": \"json\" }); keys are coloured as types."
  :languages {:json {:title JSON
      :aliases [jsonc json5 webmanifest]
      :files ["*.json" "*.jsonc" "*.json5" "*.webmanifest" ".babelrc" ".eslintrc"]
      :brackets [["{" "}"] ["[" "]"]]
      :pairs [["{" "}"] ["[" "]"] ["\"" "\""]]
      :indent {:increase "^.*(?:\\{[^}\\\"]*|\\[[^\\]\\\"]*)$" :decrease "^\\s*[}\\]]"}
      :patterns [{:match "(\"(?:[^\"\\\\]|\\\\.)*\")\\s*(?=:)" :captures {:"1" {:scope type.property}}}
        {:match "\"(?:[^\"\\\\]|\\\\.)*\"?" :scope string.quoted.double}
        {:match "-?\\d[\\w.+-]*|-" :scope number}
        {:match "\\b(?:true|false|null)\\b" :scope keyword.constant}
        {:begin "/\\*" :end "\\*/" :scope comment.block}
        {:match "//.*" :scope comment.line}]}}}
