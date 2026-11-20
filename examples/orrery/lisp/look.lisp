;;; look.lisp --- Orrery's colours and styles

(ns orrery.look)

;;; Colours

(deftokens tokens "Tokens."
  {:background {:light "#ebf0ee" :dark "#0d1414" :night "#0a0303"}
   :panel {:light "#f7faf9" :dark "#131c1c" :night "#120505"}
   :raised {:light "#dfe8e5" :dark "#1b2827" :night "#1f0808"}
   :border {:light "#c3d0cc" :dark "#263634" :night "#3a1010"}
   :text {:light "#172120" :dark "#e3ecea" :night "#e05a4a"}
   :muted {:light "#536662" :dark "#8fa39f" :night "#9a3a30"}
   :accent {:light "#0f766e" :dark "#2dd4bf" :night "#ff4433"}
   :sky {:light "#0b1213" :dark "#070b0c" :night "#050101"}
   :note {:light "#fcd9b8" :dark "#fcd9b8" :night "#7a2a20"}
   :mark {:light "#e11d74" :dark "#f472b6" :night "#c03020"}
   :onSky {:light "#e3ecea" :dark "#e3ecea" :night "#e05a4a"}
   :onSkyMuted {:light "#93a8a3" :dark "#93a8a3" :night "#9a3a30"}
   :hud {:light "#0b1514e6" :dark "#0b1514e6" :night "#140404e6"}
   :hudBorder {:light "#2dd4bf38" :dark "#2dd4bf38" :night "#ff443338"}
   :hudChecked {:light "#24514b" :dark "#24514b" :night "#4a1410"}
   :skyLabel {:light "#b9c7c2" :dark "#b9c7c2" :night "#b04a3c"}
   :nightFilter {:light "#00000000" :dark "#00000000" :night "#2a0000c4"}
   :assistant.accent @accent
   :assistant.accentHover {:light "#0d9488" :dark "#5eead4" :night "#ff6655"}
   :assistant.onAccent {:light "#f2fffc" :dark "#04201c" :night "#1a0404"}
   :assistant.surface @panel
   :assistant.raised @raised
   :assistant.border @border
   :assistant.text @text
   :assistant.dim @muted})

;;; Styles

(defstyle button
  (label :height 30
    :radius 6
    :padding 8
    :color @raised
    :textColor @text
    :align center
    :cursor hand
    :focusable true
    :hover {:color @border}
    :focused {:borderWidth 2 :borderColor @accent}))

(defstyle heading (label :height 26 :padding 6 :textColor @muted :font small))
