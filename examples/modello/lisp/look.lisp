;;; look.lisp --- Modello's colours, dark and light: the gaps, bars and headers, the regions and panels, the widgets, the
;;; selection, the gizmo's axes, and the shared kit's and the assistant's colours
;;;
;;; A token with {:dark :light} follows the theme (state.theme); @name reads another.

;;; Colours

(deftokens tokens "Every colour of the app, by name: @name in a view."
  {:gap {:dark "#0e1116" :light "#9aa3ae"}
   :bar {:dark "#161a21" :light "#c9d0d8"}
   :header {:dark "#222833" :light "#d6dce3"}
   :view {:dark "#2b323d" :light "#bcc4cd"}
   :region {:dark "#1c2129" :light "#e3e8ed"}
   :rowAlt {:dark "#212730" :light "#dbe1e7"}
   :panel {:dark "#29303b" :light "#e9edf1"}
   :panelBack {:dark "#20262f" :light "#d9dfe5"}
   :widget {:dark "#38404d" :light "#f4f6f8"}
   :widgetHover {:dark "#465061" :light "#ffffff"}
   :menuWidget {:dark "#1f252e" :light "#e6eaee"}
   :field {:dark "#12161c" :light "#ffffff"}
   :text {:dark "#e3e8ef" :light "#161b22"}
   :dim {:dark "#93a0b0" :light "#4f5966"}
   :faint {:dark "#535e6c" :light "#96a0ac"}
   :sel {:dark "#1f7f74" :light "#1f8a7d"}
   :selRow {:dark "#1b3f3d" :light "#a9dcd4"}
   :active {:dark "#6ff2df" :light "#007f72"}
   :selected {:dark "#22b8a6" :light "#00695e"}
   :grid {:dark "#3d4654" :light "#98a1ab"}
   :toolGroup {:dark "#141820e6" :light "#e9edf1eb"}
   :navButton {:dark "#14182080" :light "#ffffff80"}
   :navHover {:dark "#465061cc" :light "#ffffffcc"}
   :navBall {:dark "#ffffff12" :light "#ffffff40"}
   :object {:dark "#c3ccd8" :light "#3a4350"}
   :data {:dark "#7d8a9b" :light "#66717e"}
   :socketColor "#d1c33f"
   :socketFloat "#a0a8b3"
   :axisX "#e8604c"
   :axisY "#4cbf7a"
   :axisZ "#5b74f0"
   :kit.background {:dark "#222833" :light "#d6dce3"}
   :kit.surface {:dark "#151920" :light "#eef1f4"}
   :kit.bar {:dark "#161a21" :light "#c9d0d8"}
   :kit.barHover {:dark "#2e3540" :light "#e3e8ed"}
   :kit.field {:dark "#12161c" :light "#ffffff"}
   :kit.text {:dark "#e3e8ef" :light "#161b22"}
   :kit.dim {:dark "#8794a5" :light "#5d6774"}
   :kit.body {:dark "#b8c1cd" :light "#353c46"}
   :kit.selection {:dark "#1f7f74" :light "#1f8a7d"}
   :kit.border {:dark "#262d37" :light "#a3acb6"}
   :kit.primary {:dark "#1f7f74" :light "#1f8a7d"}
   :kit.secondary {:dark "#38404d" :light "#f4f6f8"}
   :kit.onSecondary {:dark "#e3e8ef" :light "#161b22"}
   :kit.accent {:dark "#5fd6c6" :light "#0f6e64"}
   :assistant.accent @sel
   :assistant.accentHover {:dark "#24897d" :light "#1f7f74"}
   :assistant.onAccent "#ffffff"
   :assistant.surface @panelBack
   :assistant.raised @panel
   :assistant.border @gap
   :assistant.text @text
   :assistant.dim @dim
   :kit.radius 6
   :kit.cardRadius 6})

