;;; browser.lisp --- the browser over the view: the bodies, the bookmarked places and the eclipses

;;; The browser

(defview browser "#browser." []
  `(column#browser :anchors {:right 12 :top 66}
     :width ~(if (= state.browserTab "eclipses") 360 220)
     :height ~(clamp (- window.height 526) 150 250)
     :visible ~state.showBrowser
     :color @panel
     :radius 10
     :borderWidth 1
     :borderColor @border
     (tabs#browser-tabs :width ~(if (= state.browserTab "eclipses") 360 220)
       :height 30
       :tabs [{:id bodies :label ~(t "browser.bodies") :closable false}
              {:id places :label ~(t "browser.places") :closable false}
              {:id eclipses :label ~(t "events.eclipses") :closable false}]
       :bind state.browserTab
       :color @panel
       :textColor @muted
       :activeTextColor @text
       :activeBackground @raised
       :accentColor @accent)
     ~(browser-bodies)
     ~(browser-places)
     (list#browser-eclipses :width ~(if (= state.browserTab "eclipses") 360 220)
       :height 220
       :visible ~(= state.browserTab "eclipses")
       :items ~computed.eclipseRows
       :value ~state.eclipse
       :rowHeight 24
       :onChange [~(handler goToEclipse)]
       :onOpen [~(handler goToEclipse)]
       :color @panel
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @raised
       :accentColor @accent)))

(defview browser-bodies "#browser-bodies." []
  `(tree#browser-bodies :width 220
     :height 220
     :visible ~(= state.browserTab "bodies")
     :items ~computed.bodyTree
     :expanded ~state.expandedBodies
     :value ~state.selected
     :rowHeight 24
     :onChange [{:set state.selected :to ~event.value}]
     :onOpen [{:set state.selected :to ~event.value} ~(handler goTo)]
     :onExpand [{:set state.expandedBodies
                 :to ~(if event.expanded
                        (concat (filter (fn [it] (not= it event.id)) state.expandedBodies) [event.id])
                        (filter (fn [it] (not= it event.id)) state.expandedBodies))}]
     :color @panel
     :textColor @text
     :selectedTextColor @text
     :selectedBackground @raised
     :accentColor @accent))

(defview browser-places "#browser-places, in browser." []
  `(column#browser-places :width 220 :height 220 :visible ~(= state.browserTab "places") :color @panel
     (list#browser-bookmarks :width 220
       :height 186
       :items ~computed.bookmarkRows
       :value ~state.bookmark
       :rowHeight 24
       :onChange [~(handler goToBookmark)]
       :onOpen [~(handler goToBookmark)]
       :color @panel
       :textColor @text
       :selectedTextColor @text
       :selectedBackground @raised
       :accentColor @accent)
     ~(places-bar)))

(defview places-bar "#places-bar." []
  `(row#places-bar :width 220 :height 34 :gap 6 :alignItems center
     (label#add-bookmark :style button
       :width 170
       :height 28
       :icon bookmark-plus
       :text ~(t "command.addBookmark")
       :font small
       :role button
       :onClick addBookmark)
     (label#delete-bookmark :style button
       :width 32
       :height 28
       :icon trash-2
       :role button
       :label ~(t "command.deleteBookmark")
       :tooltip ~(t "command.deleteBookmark")
       :onClick deleteBookmark)))
