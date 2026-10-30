{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Images and icons, for the rendering check: one PNG with each fit (contain, cover, fill), the same picture as a JPEG, a small image with transparency (packed into a shared page), a large one (a texture of its own), and labels with Lucide icons, from a fallback of the text font and from a font of their own."
  :window {:title "Images and icons" :width 560 :height 400}
  :fonts {:default {:file "../../../examples/shared/fonts/Roboto-Regular.ttf" :size 16 :fallback [icons]}
    :icons {:file "../../../examples/shared/fonts/Lucide.ttf" :size 16}
    :bigIcons {:file "../../../examples/shared/fonts/Lucide.ttf" :size 32}}
  :colors {:background "#f8fafc" :frame "#cbd5e1" :text "#0f172a" :accent "#7c3aed" :danger "#dc2626"}
  :styles {:picture {:type image :width 150 :height 90 :color @frame}
    :caption {:type label :width 150 :height 20 :textColor @text :align center}
    :button {:type label
      :height 36
      :padding 8
      :color "#ffffff"
      :textColor @text
      :radius 8
      :borderWidth 1
      :borderColor @frame
      :align center
      :hover {:textColor @accent :borderColor @accent}}}
  :root {:color @background
    :children [(image #contain :style picture :x 20 :y 20 :file "images/landscape.png")
      (image #cover :style picture :x 200 :y 20 :file "images/landscape.png" :fit cover)
      (image #fill :style picture :x 380 :y 20 :file "images/landscape.png" :fit fill)
      (node #containCaption :style caption :x 20 :y 114 :text contain)
      (node #coverCaption :style caption :x 200 :y 114 :text cover)
      (node #fillCaption :style caption :x 380 :y 114 :text fill)
      (image #jpeg :style picture :x 20 :y 150 :file "images/landscape.jpg" :fit cover)
      (row #badges :x 200 :y 150 :width 150 :height 48 :gap 6
        (image #badgeSmall :width 24 :height 24 :file "images/badge.png")
        (image #badge :width 48 :height 48 :file "images/badge.png")
        (image #badgeWide :width 60 :height 48 :file "images/badge.png" :fit fill))
      (image #banner :style picture :x 380 :y 150 :file "images/banner.png")
      (row #toolbar :x 20 :y 270 :width 520 :height 36 :gap 8
        (node #search :style button :width 110 :icon search :text Search)
        (node #settings :style button :width 36 :icon settings)
        (node #bell :style button :width 36 :icon bell)
        (node #delete :style button :width 100 :icon trash-2 :text Delete :textColor @danger)
        (node #unknown :style button :width 110 :icon no-such-icon :text "No icon"))
      (label #bigIcon :x 20 :y 324 :width 48 :height 48 :font bigIcons :icon image :textColor @accent :align center)
      (label #iconLine
        :x 80
        :y 336
        :width 460
        :height 24
        :textColor @text
        :icon info
        :text "Icons come from Lucide, a fallback of the text font: one atlas, one batch")]}}
