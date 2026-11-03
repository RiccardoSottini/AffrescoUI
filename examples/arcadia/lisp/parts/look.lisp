;;; look.lisp --- Arcadia's colours
;;;
;;; Every colour is a token with a dark and a light value; the theme setting chooses. The bars, pages and dialogs name
;;; them (@color.page, @color.accent...); the kit's and the assistant's tokens follow Arcadia's.

;;; The colours

(deftokens arcadia-colours "Arcadia's colours, dark and light, and those it gives the shared kit and the assistant."
  {:color.top {:dark "#16121b" :light "#e8e2ec"}
   :color.page {:dark "#211a28" :light "#f6f2f7"}
   :color.side {:dark "#1d1823" :light "#efe9f2"}
   :color.sideTop {:dark "#262030" :light "#f6f2f7"}
   :color.main {:dark "#231c2a" :light "#faf7fb"}
   :color.panel {:dark "#2a2232" :light "#ffffff"}
   :color.raised {:dark "#352c3f" :light "#ece5f0"}
   :color.hover {:dark "#463a52" :light "#e0d6e8"}
   :color.select {:dark "#54405e" :light "#f3dfc1"}
   :color.field {:dark "#30283a" :light "#ffffff"}
   :color.border {:dark "#3a3045" :light "#dcd2e3"}
   :color.text {:dark "#e2d8e6" :light "#2a2030"}
   :color.bright {:dark "#ffffff" :light "#150f1a"}
   :color.muted {:dark "#a597ad" :light "#6b5d75"}
   :color.dim {:dark "#6e6178" :light "#a796b0"}
   :color.onAccent {:dark "#1b1320" :light "#ffffff"}
   :color.accent {:dark "#f5a524" :light "#a65800"}
   :color.link {:dark "#ffc46b" :light "#94500a"}
   :color.play {:dark "#2ec4a0" :light "#1f9e80"}
   :color.playEnd {:dark "#17866c" :light "#137a62"}
   :color.warm {:dark "#ffc552" :light "#e09422"}
   :color.warmEnd {:dark "#c9560f" :light "#b8520c"}
   :color.ingame {:dark "#a5d66f" :light "#4f8a12"}
   :color.online {:dark "#c9a7ff" :light "#7a4fc0"}
   :color.offline {:dark "#898989" :light "#7a838c"}
   :color.badge {:dark "#1f6b58" :light "#1f6b58"}
   :color.onBadge {:dark "#7ff0cf" :light "#c8ffef"}
   :color.shade {:dark "#100c14cc" :light "#f6f2f7e6"}
   :assistant.accent @color.accent
   :assistant.accentHover @color.warm
   :assistant.surface @color.panel
   :assistant.raised @color.raised
   :assistant.border @color.border
   :assistant.text @color.text
   :assistant.dim @color.muted
   :assistant.dot @color.dim
   :assistant.success @color.play
   :kit.background @color.page
   :kit.surface {:dark "#463a52" :light "#ffffff"}
   :kit.bar @color.top
   :kit.barHover @color.hover
   :kit.field @color.field
   :kit.text {:dark "#e8e0ec" :light "#2a2030"}
   :kit.dim @color.muted
   :kit.body @color.muted
   :kit.selection {:dark "#5e4a6c" :light "#f3dfc1"}
   :kit.border {:dark "#4d4058" :light "#dcd2e3"}
   :kit.primary @color.accent
   :kit.accent @color.link
   :kit.focus @color.accent
   :kit.radius 3
   :kit.cardRadius 4})
