;;; image.lisp --- the image Tempera edits, and what its tools do on the canvas
;;;
;;; The pixels live in the library's image doc (modules/imaging): the raster node shows it, the image step changes it
;;; (tempera.lisp), and after each change the library puts its snapshot in state.doc. Each tool is a gesture of the
;;; pointer on the canvas.

;;; The image doc

(defpart image "The image doc, 1200 × 800 to start, and the gesture each tool makes."
  :images {:doc {:width 1200
                 :height 800
                 :into state.doc
                 :tool brush
                 :tools {:move {:gesture move}
                         :marquee {:gesture select-rectangle}
                         :ellipse {:gesture select-ellipse}
                         :lasso {:gesture lasso}
                         :polygon {:gesture polygon-lasso}
                         :wand {:gesture magic-wand}
                         :crop {:gesture crop}
                         :brush {:gesture paint}
                         :pencil {:gesture paint :hardness 100}
                         :eraser {:gesture erase}
                         :clone {:gesture clone}
                         :gradient {:gesture gradient}
                         :fill {:gesture fill}
                         :eyedropper {:gesture sample}
                         :hand {:gesture pan}
                         :zoom {:gesture zoom}}}})
