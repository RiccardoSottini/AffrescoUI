{:format affrescoui-adapter
  :version 1
  :feature slideSorter
  :host "app:tempera"
  :note "Sinopia's slide sorter in Tempera: the top-level layers are the pages, each drawn from its layer thumbnail, and the sorter's actions are the layer actions Tempera's panels dock receives."
  :bind {:pages {:value "{{map(filter(app.doc.layers, it.depth == 0), {id: it.id, name: it.name, image: it.thumbnail})}}"}
    :current {:export doc.active}
    :reorder {:action layers.move :with {:args (node #"{{id}}" :onto "{{onto}}")}}
    :select {:action layers.select :with {:args (node #"{{id}}")}}
    :duplicate {:action layers.duplicate :with {:args (node #"{{id}}")}}}}
