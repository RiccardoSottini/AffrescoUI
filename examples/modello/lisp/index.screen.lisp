{:$schema "../../../schema/affrescoui-screen.schema.json"
  :format affrescoui-screen
  :version 2
  :note "Modello, a 3D modeller, written only in the format: the stock host runs it. This file names the window, the fonts, the colours, the state and the areas; parts/commands.json holds the actions, keys and menus, parts/viewport.json the 3D viewport and its header, parts/editors.json the other editors (outliner, properties, timeline) and the bars, parts/shader.json the Shader Editor."
  :window {:title Modello :width 1440 :height 900 :minWidth 1000 :minHeight 640}
  :fonts {:default {:file "../../shared/fonts/Roboto-Regular.ttf" :size 12 :fallback [icons]}
    :small {:file "../../shared/fonts/Roboto-Regular.ttf" :size 11 :fallback [icons]}
    :icons {:file "../../shared/fonts/Lucide.ttf" :size 14}
    :iconsSmall {:file "../../shared/fonts/Lucide.ttf" :size 12}}
  :include ["parts/commands.lisp"
    "parts/viewport.lisp"
    "parts/editors.lisp"
    "parts/shader.lisp"
    "../../shared/addons/assistant/assistant.lisp"]
  :theme "{{state.theme}}"
  :tokens {:gap {:dark "#0e1116" :light "#9aa3ae"}
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
    :kit.cardRadius 6}
  :permissions {:allow [studio] :ask [agent] :reviewers [studio user]}
  :state {:theme dark
    :workspace Layout
    :mode object
    :tool select
    :menuBar {}
    :open {}
    :view {:camera {:target [0 0 0] :yaw 0.7 :pitch 0.58 :distance 17.5 :fieldOfView 0.68} :selection [cube]}
    :active cube
    :scene {:nodes [(node #camera
          :name Camera
          :kind camera
          :camera {:fieldOfView 0.69}
          :translation [7.36 -6.93 4.96]
          :rotationDegrees [63.6 0 46.7]
          :scale [1 1 1])
        (node #cube
          :name Cube
          :kind mesh
          :mesh cube
          :material material
          :translation [0 0 0]
          :rotationDegrees [0 0 0]
          :scale [1 1 1])
        (node #light
          :name Light
          :kind light
          :light {:type point :intensity 60}
          :translation [4.08 1.0 5.9]
          :rotationDegrees [37.3 3.2 106.9]
          :scale [1 1 1])]
      :materials [(node #material :name Material :baseColor [0.8 0.8 0.8 1] :roughness 0.5)]
      :collections [(node #collection :name Collection)]}
    :cursor [0 0 0]
    :pivot median
    :orientation global
    :snap false
    :snapStep unit
    :shading solid
    :sceneLights false
    :sceneWorld false
    :world {:color "#3a3f47"}
    :render {:exposure 1}
    :propertiesFilter ""
    :overlays true
    :overlay {:grid true :axes true :cursor true :text true :stats false :extras true}
    :gizmos true
    :navGizmo true
    :toolGizmos true
    :objectGizmo nil
    :hiddenKinds []
    :activeCollection collection
    :outlinerMode layer
    :outlinerHide []
    :outlinerSelectedOnly false
    :outlinerOpen {:root true :collection true}
    :renaming nil
    :unselectableKinds []
    :measure {:points [] :screen [] :camera nil}
    :xray false
    :modal nil
    :modalText ""
    :undoStack []
    :redoStack []
    :last ""
    :added 0
    :frame 1
    :start 1
    :end 250
    :playing false
    :autoKey false
    :maximized nil
    :hoverSlot nil
    :previewRange nil
    :markers []
    :timelineOffset -0.4
    :propTab object
    :panels {:transform true
      :activeTool true
      :viewPanel true
      :cursorPanel true
      :colorManagement true
      :frameRange true
      :scenePanel true
      :surfaceWorld true
      :restrictions true
      :dataPanel true
      :surfaceMaterial true}
    :sidebar false
    :sidebarTab Item
    :ctx {:open false :menu "" :items nil :x 0 :y 0}
    :pie {:open false :menu "" :x 0 :y 0}
    :about false
    :lastOp nil
    :lastBase []
    :redoOpen true
    :quickPick {:open false :query ""}
    :fileStatus ""
    :copied []
    :shaderSelected []
    :rendering false
    :opened nil
    :openedName nil
    :sizes {:main [1102] :left [720] :right [262]}
    :areas {:a viewport :b timeline :c outliner :d properties}
    :sceneName Scene
    :layerName "Main Layer"}
  :computed {:activeNode "{{find(state.scene.nodes, it.id == state.active)}}"
    :selectedNodes "{{filter(state.scene.nodes, contains(state.view.selection, it.id))}}"
    :gizmo "{{merge({ visible: state.gizmos && (contains(['move', 'rotate', 'scale'], state.tool) && state.toolGizmos != false || state.tool == 'select' && state.objectGizmo != null), mode: state.tool == 'rotate' ? 'rotate' : state.tool == 'scale' ? 'scale' : state.tool == 'select' && state.objectGizmo != null ? state.objectGizmo : 'translate', space: state.orientation == 'local' || state.orientation == 'normal' ? 'local' : 'world', snap: state.snap ? computed.snapSteps : {} }, state.pivot == 'cursor' ? { pivot: state.cursor } : state.pivot == 'active' && computed.activeNode ? { pivot: computed.activeNode.translation } : state.pivot == 'bounds' && computed.boundsCentre != null ? { pivot: computed.boundsCentre } : {})}}"}
  :onLoad {:set state.startScene :to "{{state.scene}}"}
  :root (node #root :color @gap
    (column #workspace :fill true
      {:use topBar}
      (node :when "{{state.maximized == null}}"
        :type split
        #areas
        :width "100%"
        :grow 1
        :bind state.sizes.main
        :minSize 240
        :sashSize 4
        :defaultSizes [1102]
        (split #left-areas :direction column :bind state.sizes.left :minSize [200 60] :sashSize 4 :defaultSizes [720]
          {:use area :with {:slot a}}
          {:use area :with {:slot b}})
        (split #right-areas :direction column :bind state.sizes.right :minSize [80 160] :sashSize 4 :defaultSizes [262]
          {:use area :with {:slot c}}
          {:use area :with {:slot d}}))
      {:when "{{state.maximized != null}}" :use area :with {:slot "{{state.maximized}}"} :width "100%" :grow 1}
      {:use modelloStatusBar})
    (node #playback :when "{{state.playing != false}}" :width 0 :height 0 :every {:ms 42 :do tick})
    (node :use contextMenu
      #popup
      :with {:open state.ctx.open
        :x "{{state.ctx.x}}"
        :y "{{state.ctx.y}}"
        :items "{{state.ctx.items ?? menus[state.ctx.menu] ?? []}}"
        :icons "{{some(state.ctx.items ?? menus[state.ctx.menu] ?? [], it.icon != null)}}"})
    (node :use pieMenu
      #pie
      :with {:open state.pie.open
        :x "{{state.pie.x}}"
        :y "{{state.pie.y}}"
        :items "{{computed.pieItems}}"
        :title "{{state.pie.menu == 'view' ? 'View' : 'Shading'}}"})
    (node :use dialog
      #about-dialog
      :with {:open state.about
        :title "Modello 1.0"
        :width 360
        :height 170
        :message "A 3D modeller written only as AffrescoUI screen documents: the stock host runs it."
        :buttons [{:title Close}]})
    (node :use dialog
      #render-window
      :with {:open state.rendering
        :title Render
        :width 760
        :height 518
        :buttons [{:title Close} {:title "Save As Image…" :run saveRender :keepOpen true}]}
      (scene #render-image
        :anchors {:left 20 :right 20 :top 44 :bottom 56}
        :scene "{{merge(state.scene, { nodes: map(state.scene.nodes, it.render == false || contains(computed.collectionsOff.render, it.collection ?? 'collection') ? merge(it, { visible: false }) : it) })}}"
        :view "{{ { camera: computed.cameraView, selection: [] } }}"
        :up z
        :extras false
        :shading material
        :lighting scene
        :exposure "{{state.render.exposure}}"
        :grid false
        :axes false
        :background "{{state.world.color}}"
        :controls {:orbit none :pan none :select none :zoom false :selects false}
        :label "{{'Render of ' + (computed.cameraNode.name ?? 'the camera')}}"
        :role img))
    (node :when "{{state.quickPick.open}}"
      :use quickPick
      #search
      :with {:width 460
        :rows 12
        :rowHeight 22
        :providers [{:items "{{computed.searchItems}}" :placeholder "Search for a command"}]}))}
