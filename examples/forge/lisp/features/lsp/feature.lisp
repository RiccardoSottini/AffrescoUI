{:format affrescoui-feature
  :version 2
  :name lsp
  :release "1.0.0"
  :title "Language servers"
  :description "Diagnostics, hover, completion, signature help, definitions, references, rename, formatting, symbols and semantic tokens from a language server per language (clangd, pyright, typescript-language-server)."
  :license Apache-2.0
  :note "Built into Forge: index.screen.json includes lsp.json, so this manifest names and versions the package and lists no provides; it is not in the catalogue. The host grants the server app.json names, when it is on PATH and the person allows it; without one the editor works as before."
  :requires {:format 2}
  :provides {}}
