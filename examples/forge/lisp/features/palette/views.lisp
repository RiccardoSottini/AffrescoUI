;;; views.lisp --- the palette over the workbench, and the settings and keyboard shortcuts pages

;;; The palette

(defcomponent forgePalette "The quick pick: each provider its prefix, its items, what a pick runs and its hint."
  {}
  `(node#palette :use quickPick
     :with {:width 600
            :color @color.widget
            :textColor @color.text
            :dimColor @color.muted
            :accent @color.accent
            :highlight @color.selection
            :field @color.input
            :providers [{:items ~(or-else state.folder.files []) :run openPicked :placeholder ~(t "palette.searchFilesBy")}
                        {:prefix ">" :items ~computed.commandItems :placeholder ~(t "palette.runACommand")}
                        {:prefix ":" :items ~computed.lineItems :run goToPicked :placeholder ~(t "palette.typeALine")}
                        {:prefix "theme "
                         :items ~state.themes
                         :key name
                         :run setTheme
                         :placeholder ~(t "palette.chooseAColour")}
                        {:prefix "task " :items ~state.tasks.items :run taskStart :placeholder ~(t "palette.chooseATask")}
                        {:prefix "indent " :items ~computed.indentItems :placeholder ~(t "palette.selectTheSpaces")}
                        {:prefix "eol " :items ~computed.eolItems :placeholder ~(t "palette.selectTheEnd")}
                        {:prefix "language " :items ~computed.languageItems :placeholder ~(t "palette.selectALanguage")}]}))

;;; The settings page

(defcomponent forgeSettingsPage "The settings page: its groups on the left, the settings they hold on the right."
  {}
  `(row#settings-editor :width "100%" :grow 1 :color @color.editor
     ~(settings-toc)
     ~(settings-page)))

(defcomponent forgeSettingRow "A setting `s`: its title and what it is, the control its type asks for, and Reset."
  {}
  `(node :label ~s.title
     :width "100%"
     :height ~(if (= s.type "boolean") 58 86)
     :color @color.editor
     :hover {:color @color.hover}
     :radius 4
     (node :when ~s.modified :anchors {:left 0 :top 8 :bottom 8} :width 2 :color @color.accent)
     ~(setting-title)
     ~(setting-check)
     (label#setting-description :text ~(or-else s.description "")
       :textColor @color.muted
       :height 18
       :anchors {:left ~(if (= s.type "boolean") 38 16) :right 90 :top 31})
     ~(setting-choices)
     ~(setting-slider)
     (label#setting-slider-value :when ~(and (not= s.type "boolean") (some? s.minimum) (some? s.maximum))
       :text ~(text s.value)
       :textColor @color.text
       :fitText true
       :anchors {:left 228 :top 58})
     (textarea#setting-text :when ~(and (= s.type "string") (nil? s.enum))
       :label ~s.title
       :bind ~(str "settings." s.key)
       :width 260
       :height 26
       :padding 5
       :anchors {:left 16 :top 52}
       :color @color.input
       :textColor @color.text)
     ~(setting-number)
     ~(setting-reset)))

(defview setting-check "A boolean setting's check box." []
  `(checkbox#setting-check :when ~(= s.type "boolean")
     :label ~s.title
     :bind ~(str "settings." s.key)
     :width 14
     :height 14
     :anchors {:left 16 :top 33}
     :activeColor @color.accent
     :trackColor @color.input
     :knobColor @color.text))

(defview setting-title "A setting's title; its key in the tooltip." []
  `(label#setting-title :text ~s.title :textColor @color.text :fitText true :tooltip ~s.key :anchors {:left 16 :top 8}))

(defview setting-reset "Reset, on a setting changed from its default." []
  `(label#setting-reset :when ~s.modified
     :role button
     :text ~(t "setting-reset")
     :label ~(t "setting-reset.label" {:title s.title})
     :tooltip ~(t "setting-reset.tip")
     :textColor @color.muted
     :hover {:textColor @color.text}
     :cursor hand
     :fitText true
     :anchors {:right 16 :top 8}
     :onClick ~(handler resetSetting s.key)))

(defview setting-slider "A number setting with bounds: a slider." []
  `(slider#setting-slider :when ~(and (not= s.type "boolean") (some? s.minimum) (some? s.maximum))
     :label ~s.title
     :bind ~(str "settings." s.key)
     :min ~(or-else s.minimum 0)
     :max ~(or-else s.maximum 1)
     :step ~(if (= s.type "integer") 1 0)
     :width 200
     :height 20
     :anchors {:left 16 :top 56}
     :activeColor @color.accent
     :trackColor @color.input
     :knobColor @color.text))

(defview setting-number "A number setting without bounds: a field, kept while it reads as a number." []
  `(textarea#setting-number :when ~(and (not= s.type "boolean")
                                        (not= s.type "string")
                                        (or (nil? s.minimum) (nil? s.maximum)))
     :label ~s.title
     :text ~(text s.value)
     :width 120
     :height 26
     :padding 5
     :anchors {:left 16 :top 52}
     :color @color.input
     :textColor @color.text
     :onChange ~(handler setNumberSetting s.key event.value)))

;;; The keyboard shortcuts page

(defcomponent forgeKeybindingsPage "The keyboard shortcuts page: every key, searchable, with its command and when it applies."
  {}
  `(column#keybindings-page :width "100%" :grow 1 :color @color.editor :gap 8
     (textarea#keybindings-search :role searchbox
       :label ~(t "keybindings-search.label")
       :bind state.keysQuery
       :width "100%"
       :height 30
       :padding 7
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "keybindings-search.placeholder"))
     (table#keybindings-table :label ~(t "keybindings-table.label")
       :width "100%"
       :grow 1
       :items ~computed.keyRows
       :rowHeight 24
       :color @color.editor
       :textColor @color.text
       :selectedBackground @color.selection
       :accentColor @color.accent
       :columns [{:id command :title ~(t "keybindings-table.command") :width 280}
                 {:id key :title ~(t "keybindings-table.keybinding") :width 160}
                 {:id when :title ~(t "keybindings-table.when") :width 200}
                 {:id source :title ~(t "keybindings-table.source") :width 100}])))

(defview settings-toc "The settings' groups: a click shows only that group's." []
  `(column#settings-toc :label ~(t "settings-toc.label") :width 176 :height "100%" :color @color.editor :gap 2
     (node :width "100%" :height 6 :color @color.editor)
     ~(repeat [group computed.settingsToc]
       `(node#settings-toc-item :role button
          :label ~group.label
          :width "100%"
          :height 26
          :cursor hand
          :radius 4
          :color ~(if (= state.settingsQuery group.query) "@color.selection" "@color.editor")
          :tooltip ~(if (empty? group.query) (t "settings-toc.every") (t "settings-toc.group" {:group group.label}))
          :onClick ~(handler showSettingsGroup group.query)
          (label :text ~group.label
            :anchors {:left 16 :top 5}
            :fitText true
            :textColor ~(if (= state.settingsQuery group.query) "@color.text" "@color.muted"))))))

(defview settings-page "The settings, searchable: a row for each." []
  `(column#settings-page :height "100%" :grow 1 :color @color.editor :gap 8
     (textarea#settings-search :role searchbox
       :label ~(t "settings-search.label")
       :bind state.settingsQuery
       :width "100%"
       :height 30
       :padding 7
       :color @color.input
       :textColor @color.text
       :placeholderColor @color.muted
       :placeholder ~(t "settings-search.placeholder"))
     (label#settings-none :when ~(empty? computed.settingsRows)
       :text ~(t "settings-none")
       :textColor @color.muted
       :width "100%"
       :height 28
       :padding 8)
     (column#settings-list :label ~(t "settings-list.label") :width "100%" :grow 1 :scroll vertical :gap 2
       ~(repeat [s computed.settingsRows]
         `(node#settings-row :use forgeSettingRow)))))

(defview setting-choices "A setting with a few values: a button for each, the chosen one outlined." []
  `(row#setting-choices :when ~(and (= s.type "string") (some? s.enum))
     :gap 4
     :width 480
     :height 24
     :anchors {:left 16 :top 54}
     :color "#00000000"
     ~(repeat [choice (or-else s.enum [])]
       `(label#setting-choice :role radio
          :label ~choice
          :checked ~(= s.value choice)
          :text ~choice
          :height 24
          :width ~(+ 24 (* 7 (count choice)))
          :align center
          :cursor hand
          :radius 3
          :borderWidth 1
          :borderColor ~(if (= s.value choice) "@color.accent" "@color.border")
          :color ~(if (= s.value choice) "@color.selection" "@color.input")
          :textColor @color.text
          :onClick ~(handler setSetting s.key choice)))))
