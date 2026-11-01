{:format affrescoui-adapter
  :version 1
  :feature slideSorter
  :host "app:tempera"
  :note "A broken adapter, for the failing proof: it maps the pages to a scalar and the reorder to an action Tempera doesn't export"
  :bind {:pages {:export doc.active}
    :reorder {:action layers.rotate :with {:args (node #"{{id}}" :onto "{{onto}}")}}
    :select {:action layers.select :with {:args (node #"{{id}}")}}
    :duplicate {:action layers.duplicate :with {:args (node #"{{id}}")}}}}
