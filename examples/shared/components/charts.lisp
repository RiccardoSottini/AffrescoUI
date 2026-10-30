{:note "Line, bar and area charts drawn with path nodes, to include in a screen (\"include\": [\"charts.json\"]). Each takes a list of numbers (values) and draws it from 0 at the bottom to top (default: the largest value) at the top, stretched to the node. Give the use a width and a height, and an id."
  :components {:lineChart {:params {:values [] :top 0 :color "#2563eb" :lineWidth 2}
      :node {:type path
        :width 240
        :height 120
        :viewBox "0 0 {{max(length(values) - 1, 1)}} 100"
        :fit fill
        :d "{{empty(values) ? '' : 'M ' + join(map(values, index + ' ' + (100 - it * 100 / max(top || max(values), 0.000001))), ' L ')}}"
        :stroke "{{color}}"
        :strokeWidth "{{lineWidth}}"
        :strokeJoin round
        :strokeCap round}}
    :areaChart {:params {:values [] :top 0 :color "#2563eb80"}
      :node {:type path
        :width 240
        :height 120
        :viewBox "0 0 {{max(length(values) - 1, 1)}} 100"
        :fit fill
        :d "{{empty(values) ? '' : 'M 0 100 L ' + join(map(values, index + ' ' + (100 - it * 100 / max(top || max(values), 0.000001))), ' L ') + ' L ' + (length(values) - 1) + ' 100 Z'}}"
        :fillColor "{{color}}"}}
    :barChart {:params {:values [] :top 0 :color "#2563eb" :gap 0.3}
      :node {:type path
        :width 240
        :height 120
        :viewBox "0 0 {{max(length(values), 1)}} 100"
        :fit fill
        :d "{{join(map(values, 'M ' + (index + gap / 2) + ' 100 V ' + (100 - it * 100 / max(top || max(values), 0.000001)) + ' H ' + (index + 1 - gap / 2) + ' V 100 Z'), ' ')}}"
        :fillColor "{{color}}"}}}}
