{:format affrescoui-adapter
  :version 1
  :feature slideSorter
  :host "app:orrery"
  :note "Sinopia's slide sorter in Orrery: every body but the Sun is a page, drawn from the thumbnail the screen draws for it, and the sorter's actions are the body actions Orrery's bodies dock receives."
  :bind {:pages {:value "{{map(filter(app.sky.bodies, it.parent != ''), {id: it.id, name: it.name, image: it.thumbnail})}}"}
    :current {:export sky.selected}
    :reorder {:action bodies.reorder :with {:args (node #"{{id}}" :onto "{{onto}}")}}
    :select {:action bodies.select :with {:args (node #"{{id}}")}}
    :duplicate {:action bodies.clone :with {:args (node #"{{id}}")}}}}
