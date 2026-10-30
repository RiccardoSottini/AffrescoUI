{:$schema "../../../../schema/affrescoui-screen.schema.json"
  :note "Sinopia's own theme: a sea-green accent on soft green-grey chrome, light and dark. The command bar, the ribbon's cards, the slide rail and the status bar read only these colours and styles."
  :colors {:brand {:light "#0e7c6b" :dark "#3cc2aa"}
    :brandLine {:light "#0e7c6b" :dark "#52cdb7"}
    :brandHover {:light "#0b6a5b" :dark "#62d6c1"}
    :onBrand {:light "#ffffff" :dark "#0b1f1b"}
    :rb {:light "#eef2f0" :dark "#1d2321"}
    :card {:light "#fbfcfb" :dark "#252c2a"}
    :rbInk {:light "#1d2926" :dark "#e3ebe8"}
    :rbMuted {:light "#5a6a65" :dark "#9daea8"}
    :rbHover {:light "#e1e8e5" :dark "#313a37"}
    :rbPressed {:light "#d1dcd8" :dark "#3b4743"}
    :rbChecked {:light "#d3ebe5" :dark "#1f4841"}
    :rbCheckedHover {:light "#c2e2da" :dark "#28574f"}
    :rbDisabled {:light "#a2afab" :dark "#6e7c77"}
    :rbFieldBg {:light "#ffffff" :dark "#161b1a"}
    :rbFieldLine {:light "#c5d0cc" :dark "#42504b"}
    :rbFieldHover {:light "#5a6a65" :dark "#9daea8"}
    :rbLine {:light "#d9e1de" :dark "#343f3b"}
    :work {:light "#e2e7e5" :dark "#131716"}
    :thumbLine {:light "#c5d0cc" :dark "#42504b"}
    :thumbHover {:light "#9fb5ae" :dark "#4f625c"}
    :notes {:light "#fbfcfb" :dark "#1a1f1e"}
    :barLine {:light "#d9e1de" :dark "#343f3b"}
    :handles {:light "#0e7c6b" :dark "#52cdb7"}
    :barInk {:light "#384541" :dark "#cfdad6"}
    :faint {:light "#87918e" :dark "#92a19b"}
    :inkFaded {:light "#1d292666" :dark "#e3ebe866"}
    :record {:light "#c2410c" :dark "#f08a5d"}}
  :styles {:tab {:type label
      :height 26
      :fitText true
      :padding 10
      :radius 13
      :color transparent
      :textColor @rbInk
      :cursor hand
      :focusable true
      :role tab
      :hover {:color @rbHover}}
    :tabOn {:style tab :color @brand :textColor @onBrand :hover {:color @brandHover}}
    :barIcon {:type label
      :width 30
      :height 30
      :radius 15
      :align center
      :color transparent
      :textColor @rbInk
      :cursor hand
      :focusable true
      :hover {:color @rbHover}
      :pressed {:color @rbPressed}}
    :barButton {:type label
      :height 22
      :fitText true
      :padding 5
      :radius 11
      :color transparent
      :textColor @barInk
      :cursor hand
      :hover {:color @rbHover}}
    :barButtonOn {:style barButton :color @rbChecked :textColor @brand}
    :pillButton {:type label
      :height 30
      :fitText true
      :padding 12
      :radius 15
      :color @brand
      :textColor @onBrand
      :cursor hand
      :focusable true
      :hover {:color @brandHover}}
    :rbButton {:type label
      :radius 8
      :color transparent
      :textColor @rbInk
      :cursor hand
      :focusable true
      :hover {:color @rbHover}
      :pressed {:color @rbPressed}}
    :rbButtonOn {:style rbButton :color @rbChecked :textColor @brand :hover {:color @rbCheckedHover}}
    :rbButtonOff {:style rbButton :textColor @rbDisabled :hover {:color transparent} :cursor arrow}
    :rbField {:type label
      :height 22
      :padding 4
      :radius 11
      :color @rbFieldBg
      :borderWidth 1
      :borderColor @rbFieldLine
      :textColor @rbInk
      :cursor hand
      :hover {:borderColor @rbFieldHover}}
    :rbGroupTitle {:type label :height 16 :align "start center" :textColor @rbMuted}}}
