[(node #e1 :day today :time "21:42" :icon lamp-floor :name "Floor lamp" :rule "" :what on)
  (node #e2 :day today :time "21:40" :icon door-open :name "Front door" :rule "" :what opened)
  (node #e3 :day today :time "21:40" :icon workflow :name "" :rule welcome :what triggered)
  (node #e4 :day today :time "19:05" :icon bot :name Vacuum :rule "" :what docked)
  (node #e5 :day today :time "07:00" :icon blinds :name "Bedroom blinds" :rule "" :what opened)
  (node #e6 :day yesterday :time "23:00" :icon workflow :name "" :rule goodNight :what triggered)
  (node #e7 :day yesterday :time "23:00" :icon lock :name "Front door" :rule "" :what locked)
  (node #e8 :day yesterday :time "22:58" :icon lightbulb :name Ceiling :rule "" :what off)
  (node #e9 :day yesterday :time "18:12" :icon speaker :name Speaker :rule "" :what playing)]
