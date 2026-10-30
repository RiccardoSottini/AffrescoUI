{:note "The sky from the ground, as a planetarium shows it: the stars, the bodies and invented constellations seen from the Earth at a latitude, in a perspective (gnomonic) view, over the ground and under a sky that brightens by day. Every position is a computed value: a direction (east, north, up) from right ascension and declination and the sidereal time, then projected through the view's azimuth, altitude and field of view. Great circles are straight lines in this projection, so the horizon is a horizontal line and a constellation's lines are straight."
  :state {:skyMode ground
    :sky {:azimuth 3.1416 :altitude 0.15 :fov 1.05 :latitude 45}
    :skyGrab nil
    :skyLayers {:lines true :names true :grid false :ground true :cardinal true :atmosphere true :labels true}
    :constellations [(node #kite :name "The Kite" :u -0.9992 :t 2.5589)
      (node #lantern :name "The Lantern" :u -0.2032 :t 2.5476)
      (node #heron :name "The Heron" :u 0.4002 :t 0.5322)
      (node #anvil :name "The Anvil" :u 0.2723 :t 3.8755)
      (node #harp :name "The Harp" :u 0.6981 :t 4.2281)
      (node #plough-horse :name "The Plough Horse" :u -0.6841 :t 4.0601)
      (node #lighthouse :name "The Lighthouse" :u 0.9577 :t 0.5277)
      (node #fox :name "The Fox" :u -0.4466 :t 4.1576)
      (node #weaver :name "The Weaver" :u 0.192 :t 6.1564)
      (node #bell :name "The Bell" :u -0.1803 :t 1.2753)
      (node #ferry :name "The Ferry" :u -0.8389 :t 5.0192)
      (node #owl :name "The Owl" :u 0.4082 :t 1.0047)
      (node #key :name "The Key" :u -0.7651 :t 2.3502)
      (node #sickle :name "The Sickle" :u -0.0042 :t 5.6924)]
    :constellationLines [[0 184]
      [184 13]
      [0 216]
      [239 360]
      [360 396]
      [239 220]
      [220 362]
      [269 233]
      [269 58]
      [269 206]
      [206 135]
      [86 203]
      [203 307]
      [307 125]
      [103 28]
      [103 72]
      [72 361]
      [28 217]
      [317 143]
      [143 7]
      [7 393]
      [381 208]
      [208 382]
      [382 254]
      [50 350]
      [350 341]
      [50 110]
      [353 29]
      [29 280]
      [280 245]
      [280 99]
      [99 35]
      [209 344]
      [344 181]
      [209 301]
      [123 185]
      [123 309]
      [185 339]
      [25 80]
      [25 351]
      [80 201]
      [101 64]
      [64 172]
      [172 34]
      [227 224]
      [224 15]
      [224 56]
      [56 42]]
    :skyTrack ""}
  :computed {:skyHeight "{{clamp(window.height - 524, 120, 300)}}"
    :skyTracked "{{state.skyTrack == '' ? null : find(computed.skyLocal, it.kind == 'body' && it.id == state.skyTrack)}}"
    :skyView "{{computed.skyTracked ? merge(state.sky, {azimuth: atan2(computed.skyTracked.e, computed.skyTracked.n), altitude: clamp(asin(computed.skyTracked.up), -0.3, 1.45)}) : state.sky}}"
    :skyCamera "{{ {f: 230 / tan(computed.skyView.fov / 2), cx: 230, cy: computed.skyHeight / 2, sinAzimuth: sin(computed.skyView.azimuth), cosAzimuth: cos(computed.skyView.azimuth), sinAltitude: sin(computed.skyView.altitude), cosAltitude: cos(computed.skyView.altitude)} }}"
    :skyHome "{{find(computed.placed, it.id == 'earth') ?? {x: 0, z: 0} }}"
    :skyEcliptic "{{map(filter(computed.placed, it.id != 'earth'), {id: it.id, name: it.name, hex: it.hex, parent: it.parent, longitude: atan2(computed.skyHome.z - it.z, it.x - computed.skyHome.x)})}}"
    :skyEarth "{{ {sinLatitude: sin(radians(state.sky.latitude)), cosLatitude: cos(radians(state.sky.latitude)), sidereal: 2 * pi() * state.time * 1.0027379} }}"
    :skyStars "{{map(range(1200), {kind: 'star', u: abs(sin(index * 12.9898) * 43758.5453) % 1 * 2 - 1, t: abs(sin(index * 78.233) * 12543.917) % 1 * 2 * pi(), mag: 0.5 + 6 * sqrt(abs(sin(index * 39.425) * 24634.633) % 1), tint: floor(abs(sin(index * 7.131) * 9631.77) % 1 * 3)})}}"
    :skyObjects "{{state.skyMode != 'ground' ? [] : concat(map(computed.skyStars, merge(it, {r: round(max(0.5, 2.8 - it.mag * 0.4) * 100) / 100})), map(computed.skyEcliptic, merge(it, {kind: 'body', u: sin(it.longitude) * 0.3978, t: atan2(sin(it.longitude) * 0.9175, cos(it.longitude))})), map(state.constellations, merge(it, {kind: 'name'})), [{kind: 'cardinal', id: 'N', e: 0, n: 1, up: 0}, {kind: 'cardinal', id: 'E', e: 1, n: 0, up: 0}, {kind: 'cardinal', id: 'S', e: 0, n: -1, up: 0}, {kind: 'cardinal', id: 'W', e: -1, n: 0, up: 0}], state.skyLayers.ground ? map(range(73), {kind: 'land', e: sin(radians(5 * it)), n: cos(radians(5 * it)), up: sin(radians(max(0.4, 1.6 + 1.3 * sin(radians(15 * it) + 1) + 0.8 * sin(radians(35 * it)) + 0.5 * sin(radians(85 * it) + 2))))}) : [], state.skyLayers.grid ? map(range(20), {kind: 'gridLabel', id: 'g' + it, text: it < 12 ? 30 * it + '°' : round(30 + 30 * (it % 2)) + '°', e: it < 12 ? sin(radians(30 * it)) : sin(radians(90 * floor((it - 12) / 2))) * cos(radians(30 + 30 * (it % 2))), n: it < 12 ? cos(radians(30 * it)) : cos(radians(90 * floor((it - 12) / 2))) * cos(radians(30 + 30 * (it % 2))), up: it < 12 ? 0.02 : sin(radians(30 + 30 * (it % 2)))}) : [], state.skyLayers.grid ? map(range(111), {kind: 'grid', line: floor(it / 37), e: cos(radians(30 * floor(it / 37))) * sin(radians(10 * (it % 37))), n: cos(radians(30 * floor(it / 37))) * cos(radians(10 * (it % 37))), up: sin(radians(30 * floor(it / 37)))}) : [], state.skyLayers.grid ? map(range(108), {kind: 'grid', line: 3 + floor(it / 9), e: cos(radians(10 * (it % 9))) * sin(radians(30 * floor(it / 9))), n: cos(radians(10 * (it % 9))) * cos(radians(30 * floor(it / 9))), up: sin(radians(10 * (it % 9)))}) : [])}}"
    :skyLocal "{{map(computed.skyObjects, it.kind == 'cardinal' || it.kind == 'grid' || it.kind == 'land' || it.kind == 'gridLabel' ? it : merge(it, {e: -sqrt(1 - it.u * it.u) * sin(computed.skyEarth.sidereal - it.t), n: computed.skyEarth.cosLatitude * it.u - computed.skyEarth.sinLatitude * sqrt(1 - it.u * it.u) * cos(computed.skyEarth.sidereal - it.t), up: computed.skyEarth.sinLatitude * it.u + computed.skyEarth.cosLatitude * sqrt(1 - it.u * it.u) * cos(computed.skyEarth.sidereal - it.t)}))}}"
    :skyDepth "{{map(computed.skyLocal, merge(it, {depth: (it.e * computed.skyCamera.sinAzimuth + it.n * computed.skyCamera.cosAzimuth) * computed.skyCamera.cosAltitude + it.up * computed.skyCamera.sinAltitude, across: it.e * computed.skyCamera.cosAzimuth - it.n * computed.skyCamera.sinAzimuth, rise: it.up * computed.skyCamera.cosAltitude - (it.e * computed.skyCamera.sinAzimuth + it.n * computed.skyCamera.cosAzimuth) * computed.skyCamera.sinAltitude}))}}"
    :skyPlaced "{{map(computed.skyDepth, merge(it, {ahead: it.depth > 0.08, x: it.depth > 0.08 ? computed.skyCamera.cx + computed.skyCamera.f * it.across / it.depth : -1000, y: it.depth > 0.08 ? computed.skyCamera.cy - computed.skyCamera.f * it.rise / it.depth : -1000}))}}"
    :skyShown "{{filter(computed.skyPlaced, it.ahead && it.x > -24 && it.x < 484 && it.y > -24 && it.y < computed.skyHeight + 24)}}"
    :skySun "{{find(computed.skyPlaced, it.id == 'sun')}}"
    :daylight "{{state.skyLayers.atmosphere && computed.skySun ? pow(clamp((computed.skySun.up + 0.21) / 0.31, 0, 1), 1.6) * (1 - 0.92 * computed.eclipseCover) : 0}}"
    :skyLimit "{{state.limitMag - 9 * computed.daylight}}"
    :skyColor "{{gradientColor({kind: 'linear', stops: [{at: 0, color: '#04060cff'}, {at: 0.4, color: '#1b2a4cff'}, {at: 1, color: '#5b8ed4ff'}]}, computed.daylight)}}"
    :groundColor "{{gradientColor({kind: 'linear', stops: [{at: 0, color: '#14110cff'}, {at: 1, color: '#7d6c49ff'}]}, computed.daylight)}}"
    :figureColor "{{gradientColor({kind: 'linear', stops: [{at: 0, color: '#8f78b8ff'}, {at: 1, color: '#8f78b850'}]}, computed.daylight)}}"
    :horizonY "{{computed.skyCamera.cy + computed.skyCamera.f * tan(computed.skyView.altitude)}}"
    :skyFigurePath "{{state.skyLayers.lines && length(computed.skyPlaced) > 1200 ? join(map(filter(state.constellationLines, computed.skyPlaced[it[0]].ahead && computed.skyPlaced[it[1]].ahead), 'M' + fixed(computed.skyPlaced[it[0]].x, 1) + ' ' + fixed(computed.skyPlaced[it[0]].y, 1) + 'L' + fixed(computed.skyPlaced[it[1]].x, 1) + ' ' + fixed(computed.skyPlaced[it[1]].y, 1)), '') : ''}}"
    :skyGridPoints "{{filter(computed.skyPlaced, it.kind == 'grid')}}"
    :skyGridPath "{{join(map(filter(range(max(length(computed.skyGridPoints) - 1, 0)), computed.skyGridPoints[it].line == computed.skyGridPoints[it + 1].line && computed.skyGridPoints[it].ahead && computed.skyGridPoints[it + 1].ahead), 'M' + fixed(computed.skyGridPoints[it].x, 1) + ' ' + fixed(computed.skyGridPoints[it].y, 1) + 'L' + fixed(computed.skyGridPoints[it + 1].x, 1) + ' ' + fixed(computed.skyGridPoints[it + 1].y, 1)), '')}}"
    :skySelected "{{find(computed.skyPlaced, it.kind == 'body' && it.id == state.selected)}}"
    :skyInfo "{{computed.skySelected ? ['Az./Alt.: ' + fixed((degrees(atan2(computed.skySelected.e, computed.skySelected.n)) + 360) % 360, 1) + '° / ' + fixed(degrees(asin(computed.skySelected.up)), 1) + '°', 'RA/Dec.: ' + fixed((degrees(computed.skySelected.t) / 15 + 24) % 24, 2) + 'h / ' + fixed(degrees(asin(computed.skySelected.u)), 1) + '°'] : []}}"
    :skyBar "{{[{title: 'Centre on the selected body', icon: 'locate-fixed', checked: state.skyTrack != '' && state.skyTrack == state.selected, run: 'skyCentre'}, {title: 'Constellation lines', icon: 'waypoints', checked: state.skyLayers.lines, run: 'toggleSkyLines'}, {title: 'Constellation names', icon: 'baseline', checked: state.skyLayers.names, run: 'toggleSkyNames'}, {title: 'Azimuthal grid', icon: 'grid-3-x-3', checked: state.skyLayers.grid, run: 'toggleSkyGrid'}, {title: 'Ground', icon: 'mountain', checked: state.skyLayers.ground, run: 'toggleSkyGround'}, {title: 'Cardinal points', icon: 'compass', checked: state.skyLayers.cardinal, run: 'toggleSkyCardinal'}, {title: 'Atmosphere', icon: 'cloud-sun', checked: state.skyLayers.atmosphere, run: 'toggleSkyAtmosphere'}, {title: 'Planet labels', icon: 'tag', checked: state.skyLayers.labels, run: 'toggleSkyLabels'}]}}"
    :skyStarShapes "{{map(filter(computed.skyShown, it.kind == 'star' && it.mag <= computed.skyLimit), {tint: it.tint, d: 'M' + fixed(it.x - it.r, 1) + ' ' + fixed(it.y, 1) + 'a' + it.r + ' ' + it.r + ' 0 1 0 ' + 2 * it.r + ' 0a' + it.r + ' ' + it.r + ' 0 1 0 ' + -2 * it.r + ' 0'})}}"
    :skyMoon "{{find(computed.skyPlaced, it.kind == 'body' && it.parent == 'earth')}}"
    :eclipseNow "{{find(computed.eclipses, abs(it.time - state.time) < 0.3)}}"
    :eclipseCover "{{computed.eclipseNow && computed.eclipseNow.solar && computed.skySun && computed.skyMoon ? clamp(1 - acos(clamp(computed.skySun.e * computed.skyMoon.e + computed.skySun.n * computed.skyMoon.n + computed.skySun.up * computed.skyMoon.up, -1, 1)) / 0.025, 0, 1) : 0}}"
    :skyLandPath "{{length(filter(computed.skyPlaced, it.kind == 'land' && it.ahead)) < 2 ? (computed.horizonY < computed.skyHeight ? 'M-10 ' + fixed(max(computed.horizonY, -10), 1) + 'L470 ' + fixed(max(computed.horizonY, -10), 1) + 'L470 ' + (computed.skyHeight + 10) + 'L-10 ' + (computed.skyHeight + 10) + 'Z' : '') : 'M' + join(map(sort(filter(computed.skyPlaced, it.kind == 'land' && it.ahead), it.x), fixed(it.x, 1) + ' ' + fixed(it.y, 1)), 'L') + 'L' + fixed(max(sort(filter(computed.skyPlaced, it.kind == 'land' && it.ahead), -it.x)[0].x, 470), 1) + ' ' + (computed.skyHeight + 10) + 'L' + fixed(min(sort(filter(computed.skyPlaced, it.kind == 'land' && it.ahead), it.x)[0].x, -10), 1) + ' ' + (computed.skyHeight + 10) + 'Z'}}"}
  :actions {:toggleSkyLines {:title "Constellation lines" :do [{:toggle state.skyLayers.lines}]}
    :toggleSkyNames {:title "Constellation names" :do [{:toggle state.skyLayers.names}]}
    :toggleSkyGrid {:title "Azimuthal grid" :do [{:toggle state.skyLayers.grid}]}
    :toggleSkyGround {:title Ground :do [{:toggle state.skyLayers.ground}]}
    :toggleSkyCardinal {:title "Cardinal points" :do [{:toggle state.skyLayers.cardinal}]}
    :toggleSkyAtmosphere {:title Atmosphere :do [{:toggle state.skyLayers.atmosphere}]}
    :toggleSkyLabels {:title "Planet labels" :do [{:toggle state.skyLayers.labels}]}
    :skyGround {:title "The sky from the ground" :do [{:set state.skyMode :to ground}]}
    :skyAbove {:title "The sky chart from above" :do [{:set state.skyMode :to above}]}
    :skyPress [{:set state.sky :to "{{computed.skyView}}"}
      {:set state.skyTrack :to ""}
      {:set state.skyGrab :to "{{computed.skyView}}"}]
    :skyDrag [{:if "{{state.skyGrab}}"
        :then [{:set state.sky
            :to "{{merge(state.sky, {azimuth: state.skyGrab.azimuth - event.dx * state.sky.fov / 460, altitude: clamp(state.skyGrab.altitude + event.dy * state.sky.fov / 460, -0.3, 1.45)})}}"}]}]
    :skyScroll [{:set state.sky.fov :to "{{clamp(state.sky.fov * (1 + event.dy * 0.06), 0.09, 1.75)}}"}]
    :skyCentre {:title "Centre on the selected body"
      :do [{:set state.skyTrack
          :to "{{state.skyTrack == state.selected || state.selected == 'earth' ? '' : state.selected}}"}]}}
  :components {:skyFromGround {:node {:type panel
        :fill true
        :color "{{computed.skyColor}}"
        :label "Sky from the ground"
        :onPress skyPress
        :onDrag skyDrag
        :onScroll skyScroll
        :children [(path #sky-grid :fill true :d "{{computed.skyGridPath}}" :stroke "#5c5240" :strokeWidth 1)
          (node :repeat {:for g
              :in "{{filter(computed.skyShown, it.kind == 'gridLabel' && indexOf(['0°', '90°', '180°', '270°'], it.text) < 0)}}"
              :key "{{g.id}}"}
            :type label
            #"{{'sky-grid-label-' + g.id}}"
            :x "{{g.x + 3}}"
            :y "{{g.y - 15}}"
            :height 14
            :fitText true
            :font small
            :text "{{g.text}}"
            :textColor "#a8976e")
          (path #sky-figures
            :fill true
            :d "{{computed.skyFigurePath}}"
            :stroke "{{computed.figureColor}}"
            :strokeWidth 1)
          (path #sky-stars-blue
            :fill true
            :d "{{join(map(filter(computed.skyStarShapes, it.tint == 0), it.d), '')}}"
            :fillColor "#c9d8ff")
          (path #sky-stars-white
            :fill true
            :d "{{join(map(filter(computed.skyStarShapes, it.tint == 1), it.d), '')}}"
            :fillColor "#f4f4ff")
          (path #sky-stars-gold
            :fill true
            :d "{{join(map(filter(computed.skyStarShapes, it.tint == 2), it.d), '')}}"
            :fillColor "#ffe2b8")
          (node :repeat {:for c
              :in "{{state.skyLayers.names ? filter(computed.skyShown, it.kind == 'name') : []}}"
              :key "{{c.id}}"}
            :type label
            #"{{'sky-name-' + c.id}}"
            :x "{{c.x - 60}}"
            :y "{{c.y - 8}}"
            :width 120
            :height 16
            :align center
            :font small
            :text "{{c.name}}"
            :textColor "#b49fd8")
          (path #sky-ground
            :visible "{{state.skyLayers.ground}}"
            :fill true
            :d "{{computed.skyLandPath}}"
            :fillColor "{{computed.groundColor}}"
            :stroke "#6e6347"
            :strokeWidth 1)
          (node :repeat {:for b
              :in "{{filter(computed.skyShown, it.kind == 'body' && (it.up > -0.01 || !state.skyLayers.ground))}}"
              :key "{{b.id}}"}
            :type panel
            #"{{'sky-body-' + b.id}}"
            :x "{{b.x - 12}}"
            :y "{{b.y - 12}}"
            :width 24
            :height 24
            :color "{{b.id == 'sun' ? '#ffd76a30' : '#00000000'}}"
            :radius 12
            :role button
            :label "{{b.name}}"
            :onClick {:set state.selected :to "{{b.id}}"}
            :children [(panel #"{{'sky-disc-' + b.id}}"
                :x "{{b.id == 'sun' || b.parent == 'earth' ? 5 : 9.5}}"
                :y "{{b.id == 'sun' || b.parent == 'earth' ? 5 : 9.5}}"
                :width "{{b.id == 'sun' || b.parent == 'earth' ? 14 : 5}}"
                :height "{{b.id == 'sun' || b.parent == 'earth' ? 14 : 5}}"
                :radius 7
                :color "{{b.parent == 'earth' && computed.eclipseCover > 0 ? '#16181c' : (b.parent == 'earth' && computed.eclipseNow && !computed.eclipseNow.solar ? '#9a4a2c' : b.hex)}}"
                :borderColor "{{b.id == state.selected ? '@accent' : '#00000000'}}"
                :borderWidth 1)]
            :borderWidth "{{b.id == state.selected ? 1.5 : 0}}"
            :borderColor "#2dd4bf")
          (node :repeat {:for b
              :in "{{state.skyLayers.labels ? filter(computed.skyShown, it.kind == 'body' && (it.up > -0.01 || !state.skyLayers.ground)) : []}}"
              :key "{{b.id}}"}
            :type label
            #"{{'sky-label-' + b.id}}"
            :x "{{b.x + 10}}"
            :y "{{b.parent == 'earth' ? b.y + 2 : b.y - 16}}"
            :height 16
            :fitText true
            :font small
            :text "{{b.name}}"
            :textColor "{{b.id == state.selected ? '@accent' : '#e9d9b0'}}")
          (node :repeat {:for p
              :in "{{state.skyLayers.cardinal ? filter(computed.skyShown, it.kind == 'cardinal') : []}}"
              :key "{{p.id}}"}
            :type label
            #"{{'sky-cardinal-' + p.id}}"
            :x "{{p.x - 10}}"
            :y "{{p.y - 24}}"
            :width 20
            :height 22
            :align center
            :font title
            :text "{{p.id}}"
            :textColor "#5eead4")
          (node :use toolbar
            #sky-bar
            :anchors {:right 8 :bottom 6}
            :with {:items "{{computed.skyBar}}"
              :width 272
              :label "Sky layers"
              :color "#0b1514d8"
              :textColor @onSky
              :dimColor @onSkyMuted
              :highlight "#1f3331"
              :checkedColor "#24514b"})
          (label #sky-place
            :anchors {:right 8 :bottom 40}
            :height 18
            :fitText true
            :font small
            :textColor @onSkyMuted
            :text "{{'Earth, ' + abs(state.sky.latitude) + '°' + (state.sky.latitude < 0 ? 'S' : 'N') + '   FOV ' + fixed(degrees(state.sky.fov), 1) + '°   ' + computed.clock}}")]}}}}
