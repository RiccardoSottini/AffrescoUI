;;; geometry.lisp --- Shapes as path nodes, from a preset at the object's size
;;;
;;; A preset is a polygon (subpaths of points [ax, ay, bx, by]: x = ax·w + bx·m, y = ay·h + by·m, m the shorter side,
;;; so corners and arrowheads keep their size as the shape stretches), a path in a 100 by 100 box stretched to the
;;; object, a rectangle rounded by a fraction of m, an ellipse, a regular polygon or a star (sides, inner ratio), or a
;;; line from corner to corner with its ends. `light` and `dark` are faces (a cube's top and side) tinted over the
;;; fill. `text` is the text frame's inset on each side (left, top, right, bottom) as fractions of the size, `pad` how
;;; far a part (a callout's tail) passes the box, `open` an outline only. The path node is grown by half the outline
;;; and the pad on each side, past the object, which lets it overflow.

;;; Definitions

(defcomputed shapes
  '{:line {:line true}
   :lineArrow {:line true :tail arrow}
   :lineDoubleArrow {:line true :head arrow :tail arrow}
   :lineDot {:line true :head oval :tail oval}
   :rect {:round 0}
   :roundRect {:round 0.1667}
   :snip1Rect {:poly [[[0 0] [1 0 -0.1667 0] [1 0 0 0.1667] [1 1] [0 1]]]}
   :snip2SameRect {:poly [[[0 0 0.1667 0] [1 0 -0.1667 0] [1 0 0 0.1667] [1 1] [0 1] [0 0 0 0.1667]]]}
   :snip2DiagRect {:poly [[[0 0] [1 0 -0.1667 0] [1 0 0 0.1667] [1 1] [0 1 0.1667 0] [0 1 0 -0.1667]]]}
   :stadium {:round 0.5}
   :ellipse {:ellipse true :text [0.146 0.146 0.146 0.146]}
   :triangle {:poly [[[0.5 0] [1 1] [0 1]]] :text [0.25 0.5 0.25 0]}
   :rtTriangle {:poly [[[0 0] [1 1] [0 1]]] :text [0.08 0.5 0.5 0.08]}
   :parallelogram {:poly [[[0 0 0.25 0] [1 0] [1 1 -0.25 0] [0 1]]] :text [0.12 0 0.12 0]}
   :trapezoid {:poly [[[0 1] [0 0 0.25 0] [1 0 -0.25 0] [1 1]]] :text [0.12 0 0.12 0]}
   :diamond {:poly [[[0.5 0] [1 0.5] [0.5 1] [0 0.5]]] :text [0.25 0.25 0.25 0.25]}
   :pentagonReg {:sides 5 :text [0.2 0.2 0.2 0.1]}
   :hexagon {:poly [[[0 0.5] [0 0 0.25 0] [1 0 -0.25 0] [1 0.5] [1 1 -0.25 0] [0 1 0.25 0]]]
             :text [0.15 0.1 0.15 0.1]}
   :heptagon {:sides 7 :text [0.15 0.15 0.15 0.15]}
   :octagon {:poly [[[0 0 0.2929 0]
                     [1 0 -0.2929 0]
                     [1 0 0 0.2929]
                     [1 1 0 -0.2929]
                     [1 1 -0.2929 0]
                     [0 1 0.2929 0]
                     [0 1 0 -0.2929]
                     [0 0 0 0.2929]]]
             :text [0.1 0.1 0.1 0.1]}
   :decagon {:sides 10 :text [0.12 0.12 0.12 0.12]}
   :dodecagon {:sides 12 :text [0.12 0.12 0.12 0.12]}
   :pie {:d "M50 50L100 50A50 50 0 1 1 50 0Z"}
   :chord {:d "M85.3553 85.3553A50 50 0 1 1 50.0 0.0Z"}
   :teardrop {:d "M50 0H100V50A50 50 0 1 1 50 0Z" :text [0.146 0.146 0.146 0.146]}
   :frame {:poly [[[0 0] [1 0] [1 1] [0 1]] [[0 0 0.125 0.125] [0 1 0.125 -0.125] [1 1 -0.125 -0.125] [1 0 -0.125 0.125]]]}
   :halfFrame {:poly [[[0 0] [1 0] [1 0 -0.15 0.15] [0 0 0.15 0.15] [0 1 0.15 -0.15] [0 1]]]}
   :corner {:poly [[[0 0] [0 0 0.5 0] [0 1 0.5 -0.5] [1 1 0 -0.5] [1 1] [0 1]]]}
   :diagStripe {:poly [[[0 0.5] [0.5 0] [1 0] [0 1]]]}
   :plus {:poly [[[0 0 0.25 0]
                  [1 0 -0.25 0]
                  [1 0 -0.25 0.25]
                  [1 0 0 0.25]
                  [1 1 0 -0.25]
                  [1 1 -0.25 -0.25]
                  [1 1 -0.25 0]
                  [0 1 0.25 0]
                  [0 1 0.25 -0.25]
                  [0 1 0 -0.25]
                  [0 0 0 0.25]
                  [0 0 0.25 0.25]]]
          :text [0.12 0.12 0.12 0.12]}
   :can {:d "M0 12C0 -4 100 -4 100 12V88C100 104 0 104 0 88ZM0 12C0 28 100 28 100 12"
         :text [0 0.25 0 0.1]}
   :cube {:poly [[[0 0 0 0.25] [0 0 0.25 0] [1 0] [1 1 0 -0.25] [1 1 -0.25 0] [0 1]]
                 [[0 0 0 0.25] [1 0 -0.25 0.25] [1 1 -0.25 0]]
                 [[1 0 -0.25 0.25] [1 0]]]
          :text [0 0.25 0.25 0]
          :light [[[0 0 0 0.25] [0 0 0.25 0] [1 0] [1 0 -0.25 0.25]]]
          :dark [[[1 0 -0.25 0.25] [1 0] [1 1 0 -0.25] [1 1 -0.25 0]]]}
   :bevel {:poly [[[0 0] [1 0] [1 1] [0 1]]
                  [[0 0 0.125 0.125] [1 0 -0.125 0.125] [1 1 -0.125 -0.125] [0 1 0.125 -0.125]]
                  [[0 0] [0 0 0.125 0.125]]
                  [[1 0] [1 0 -0.125 0.125]]
                  [[1 1] [1 1 -0.125 -0.125]]
                  [[0 1] [0 1 0.125 -0.125]]]
           :text [0.125 0.125 0.125 0.125]
           :light [[[0 0] [1 0] [1 0 -0.125 0.125] [0 0 0.125 0.125]] [[0 0] [0 0 0.125 0.125] [0 1 0.125 -0.125] [0 1]]]
           :dark [[[1 0] [1 1] [1 1 -0.125 -0.125] [1 0 -0.125 0.125]]
                  [[0 1] [0 1 0.125 -0.125] [1 1 -0.125 -0.125] [1 1]]]}
   :donut {:d "M0 50A50 50 0 1 1 100 50A50 50 0 1 1 0 50ZM25 50A25 25 0 1 0 75 50A25 25 0 1 0 25 50Z"
           :evenodd true}
   :noSmoking {               :d "M0 50A50 50 0 1 1 100 50A50 50 0 1 1 0 50ZM30 23L77 70A33 33 0 0 0 30 23ZM23 30A33 33 0 0 0 70 77Z"
               :evenodd true}
   :blockArc {:d "M0 50A50 50 0 0 1 100 50H75A25 25 0 0 0 25 50Z"}
   :foldedCorner {:poly [[[0 0] [1 0] [1 1 0 -0.1667] [1 1 -0.1667 0] [0 1]]
                         [[1 1 0 -0.1667] [1 1 -0.1333 -0.1333] [1 1 -0.1667 0]]]
                  :dark [[[1 1 0 -0.1667] [1 1 -0.1333 -0.1333] [1 1 -0.1667 0]]]}
   :smileyFace {:d "M0 50A50 50 0 1 1 100 50A50 50 0 1 1 0 50ZM30 36A6 6 0 1 0 42 36A6 6 0 1 0 30 36ZM58 36A6 6 0 1 0 70 36A6 6 0 1 0 58 36ZM27 64Q50 84 73 64Q50 76 27 64Z"
                :evenodd true}
   :heart {:d "M50 24C50 8 26 -2 11 9C-5 21 3 47 50 100C97 47 105 21 89 9C74 -2 50 8 50 24Z"
           :text [0.15 0.15 0.15 0.3]}
   :lightningBolt {:poly [[[0.39 0]
                           [0.6 0.3]
                           [0.52 0.34]
                           [0.77 0.58]
                           [0.69 0.62]
                           [1 1]
                           [0.43 0.69]
                           [0.52 0.65]
                           [0.22 0.45]
                           [0.3 0.41]
                           [0 0.18]]]}
   :sun {:d "M28 50A22 22 0 1 1 72 50A22 22 0 1 1 28 50ZM80.529 44.6169L100.0 50.0L80.529 55.3831ZM75.3937 67.7809L85.3553 85.3553L67.7809 75.3937ZM55.3831 80.529L50.0 100.0L44.6169 80.529ZM32.2191 75.3937L14.6447 85.3553L24.6063 67.7809ZM19.471 55.3831L0.0 50.0L19.471 44.6169ZM24.6063 32.2191L14.6447 14.6447L32.2191 24.6063ZM44.6169 19.471L50.0 0.0L55.3831 19.471ZM67.7809 24.6063L85.3553 14.6447L75.3937 32.2191Z"
         :text [0.3 0.3 0.3 0.3]}
   :moon {:d "M100 0A100 50 0 0 0 100 100A60 50 0 0 1 100 0Z"}
   :cloud {:d "M22 88C5 88 0 70 10 62C0 50 10 34 24 38C26 20 46 14 56 26C64 12 86 16 86 34C100 38 102 58 92 66C100 78 88 92 74 86C66 98 46 98 40 88C34 92 26 92 22 88Z"
           :text [0.15 0.25 0.15 0.2]}
   :arc {:d "M50 0A50 50 0 0 1 100 50" :open true}
   :bracketPair {:d "M15 0Q0 0 0 15V85Q0 100 15 100M85 0Q100 0 100 15V85Q100 100 85 100"
                 :text [0.08 0.08 0.08 0.08]
                 :open true}
   :bracePair {:d "M20 0Q10 0 10 8V42Q10 50 0 50Q10 50 10 58V92Q10 100 20 100M80 0Q90 0 90 8V42Q90 50 100 50Q90 50 90 58V92Q90 100 80 100"
               :text [0.12 0.08 0.12 0.08]
               :open true}
   :leftBracket {:d "M100 0Q0 0 0 15V85Q0 100 100 100" :open true}
   :rightBracket {:d "M0 0Q100 0 100 15V85Q100 100 0 100" :open true}
   :leftBrace {:d "M100 0Q50 0 50 10V40Q50 50 0 50Q50 50 50 60V90Q50 100 100 100" :open true}
   :rightBrace {:d "M0 0Q50 0 50 10V40Q50 50 100 50Q50 50 50 60V90Q50 100 0 100" :open true}
   :rightArrow {:poly [[[0 0.25] [1 0.25 -0.5 0] [1 0 -0.5 0] [1 0.5] [1 1 -0.5 0] [1 0.75 -0.5 0] [0 0.75]]]
                :text [0 0.25 0.25 0.25]}
   :leftArrow {:poly [[[1 0.25 0 0] [0 0.25 0.5 0] [0 0 0.5 0] [0 0.5 0 0] [0 1 0.5 0] [0 0.75 0.5 0] [1 0.75 0 0]]]
               :text [0.25 0.25 0 0.25]}
   :upArrow {:poly [[[0.25 1 0 0] [0.25 0 0 0.5] [0 0 0 0.5] [0.5 0 0 0] [1 0 0 0.5] [0.75 0 0 0.5] [0.75 1 0 0]]]
             :text [0.25 0.25 0.25 0]}
   :downArrow {:poly [[[0.25 0 0 0] [0.25 1 0 -0.5] [0 1 0 -0.5] [0.5 1 0 0] [1 1 0 -0.5] [0.75 1 0 -0.5] [0.75 0 0 0]]]
               :text [0.25 0 0.25 0.25]}
   :leftRightArrow {:poly [[[0 0.5]
                            [0 0 0.5 0]
                            [0 0.25 0.5 0]
                            [1 0.25 -0.5 0]
                            [1 0 -0.5 0]
                            [1 0.5]
                            [1 1 -0.5 0]
                            [1 0.75 -0.5 0]
                            [0 0.75 0.5 0]
                            [0 1 0.5 0]]]
                    :text [0.25 0.25 0.25 0.25]}
   :upDownArrow {:poly [[[0.5 0 0 0]
                         [0 0 0 0.5]
                         [0.25 0 0 0.5]
                         [0.25 1 0 -0.5]
                         [0 1 0 -0.5]
                         [0.5 1 0 0]
                         [1 1 0 -0.5]
                         [0.75 1 0 -0.5]
                         [0.75 0 0 0.5]
                         [1 0 0 0.5]]]
                 :text [0.25 0.25 0.25 0.25]}
   :quadArrow {:poly [[[0 0.5]
                       [0 0.5 0.2 -0.2]
                       [0 0.5 0.2 -0.1]
                       [0.5 0.5 -0.1 -0.1]
                       [0.5 0 -0.1 0.2]
                       [0.5 0 -0.2 0.2]
                       [0.5 0]
                       [0.5 0 0.2 0.2]
                       [0.5 0 0.1 0.2]
                       [0.5 0.5 0.1 -0.1]
                       [1 0.5 -0.2 -0.1]
                       [1 0.5 -0.2 -0.2]
                       [1 0.5]
                       [1 0.5 -0.2 0.2]
                       [1 0.5 -0.2 0.1]
                       [0.5 0.5 0.1 0.1]
                       [0.5 1 0.1 -0.2]
                       [0.5 1 0.2 -0.2]
                       [0.5 1]
                       [0.5 1 -0.2 -0.2]
                       [0.5 1 -0.1 -0.2]
                       [0.5 0.5 -0.1 0.1]
                       [0 0.5 0.2 0.1]
                       [0 0.5 0.2 0.2]]]}
   :notchedRightArrow {:poly [[[0 0.25]
                               [1 0.25 -0.5 0]
                               [1 0 -0.5 0]
                               [1 0.5]
                               [1 1 -0.5 0]
                               [1 0.75 -0.5 0]
                               [0 0.75]
                               [0 0.5 0.25 0]]]
                       :text [0.12 0.25 0.25 0.25]}
   :stripedRightArrow {:poly [[[0 0.25] [0 0.25 0.06 0] [0 0.75 0.06 0] [0 0.75]]
                              [[0 0.25 0.12 0] [0 0.25 0.18 0] [0 0.75 0.18 0] [0 0.75 0.12 0]]
                              [[0 0.25 0.24 0]
                               [1 0.25 -0.5 0]
                               [1 0 -0.5 0]
                               [1 0.5]
                               [1 1 -0.5 0]
                               [1 0.75 -0.5 0]
                               [0 0.75 0.24 0]]]
                       :text [0.15 0.25 0.25 0.25]}
   :homePlate {:poly [[[0 0] [1 0 -0.5 0] [1 0.5] [1 1 -0.5 0] [0 1]]] :text [0 0 0.25 0]}
   :chevron {:poly [[[0 0] [1 0 -0.5 0] [1 0.5] [1 1 -0.5 0] [0 1] [0 0.5 0.5 0]]]
             :text [0.25 0 0.25 0]}
   :uturnArrow {:d "M0 100V40A40 40 0 0 1 80 40V55H100L70 85L40 55H60V40A20 20 0 0 0 20 40V100Z"}
   :bentArrow {:d "M0 100V50A35 35 0 0 1 35 15H75V0L100 25L75 50V35H35A15 15 0 0 0 20 50V100Z"}
   :circularArrow {:d "M8 60A42 42 0 1 1 92 60H100L84 80L68 60H76A26 26 0 1 0 24 60Z"}
   :curvedRightArrow {:d "M0 50C0 20 40 5 75 5V0L100 20L75 40V35C45 35 20 45 20 70V100H0Z"}
   :mathPlus {:poly [[[0.12 0.5 0 -0.12]
                      [0.5 0.5 -0.12 -0.12]
                      [0.5 0.12 -0.12 0]
                      [0.5 0.12 0.12 0]
                      [0.5 0.5 0.12 -0.12]
                      [0.88 0.5 0 -0.12]
                      [0.88 0.5 0 0.12]
                      [0.5 0.5 0.12 0.12]
                      [0.5 0.88 0.12 0]
                      [0.5 0.88 -0.12 0]
                      [0.5 0.5 -0.12 0.12]
                      [0.12 0.5 0 0.12]]]}
   :mathMinus {:poly [[[0.12 0.5 0 -0.12] [0.88 0.5 0 -0.12] [0.88 0.5 0 0.12] [0.12 0.5 0 0.12]]]}
   :mathEqual {:poly [[[0.12 0.5 0 -0.22] [0.88 0.5 0 -0.22] [0.88 0.5 0 -0.06] [0.12 0.5 0 -0.06]]
                      [[0.12 0.5 0 0.06] [0.88 0.5 0 0.06] [0.88 0.5 0 0.22] [0.12 0.5 0 0.22]]]}
   :mathMultiply {:poly [[[0.5 0.5 0 -0.1198]
                          [0.2 0.2 0 -0.1198]
                          [0.2 0.2 -0.1198 0]
                          [0.5 0.5 -0.1198 0]
                          [0.2 0.8 -0.1198 0]
                          [0.2 0.8 0 0.1198]
                          [0.5 0.5 0 0.1198]
                          [0.8 0.8 0 0.1198]
                          [0.8 0.8 0.1198 0]
                          [0.5 0.5 0.1198 0]
                          [0.8 0.2 0.1198 0]
                          [0.8 0.2 0 -0.1198]]]}
   :mathDivide {:d "M12 44H88V56H12ZM42 26A8 8 0 1 0 58 26A8 8 0 1 0 42 26ZM42 74A8 8 0 1 0 58 74A8 8 0 1 0 42 74Z"}
   :mathNotEqual {:poly [[[0.12 0.5 0 -0.22]
                          [0.58 0.5 0 -0.22]
                          [0.66 0.15]
                          [0.76 0.15]
                          [0.7 0.5 0 -0.22]
                          [0.88 0.5 0 -0.22]
                          [0.88 0.5 0 -0.06]
                          [0.66 0.5 0 -0.06]
                          [0.63 0.5 0 0.06]
                          [0.88 0.5 0 0.06]
                          [0.88 0.5 0 0.22]
                          [0.58 0.5 0 0.22]
                          [0.34 0.85]
                          [0.24 0.85]
                          [0.48 0.5 0 0.22]
                          [0.12 0.5 0 0.22]
                          [0.12 0.5 0 0.06]
                          [0.53 0.5 0 0.06]
                          [0.56 0.5 0 -0.06]
                          [0.12 0.5 0 -0.06]]]}
   :flowChartProcess {:round 0}
   :flowChartAlternateProcess {:round 0.1667}
   :flowChartDecision {:poly [[[0.5 0] [1 0.5] [0.5 1] [0 0.5]]] :text [0.25 0.25 0.25 0.25]}
   :flowChartInputOutput {:poly [[[0.2 0] [1 0] [0.8 1] [0 1]]] :text [0.2 0 0.2 0]}
   :flowChartPredefinedProcess {:poly [[[0 0] [1 0] [1 1] [0 1]] [[0.125 0] [0.125 1]] [[0.875 0] [0.875 1]]]
                                :text [0.125 0 0.125 0]}
   :flowChartInternalStorage {:poly [[[0 0] [1 0] [1 1] [0 1]] [[0.2 0] [0.2 1]] [[0 0.2] [1 0.2]]]
                              :text [0.2 0.2 0 0]}
   :flowChartDocument {:d "M0 0H100V86C70 70 30 110 0 92Z" :text [0 0 0 0.17]}
   :flowChartMultidocument {:d "M0 14H86V88C60 76 26 108 0 92ZM6 14V7H93V80L86 82M86 14V7M13 7V0H100V74L93 76"
                            :text [0 0.14 0.14 0.17]}
   :flowChartTerminator {:round 0.5}
   :flowChartPreparation {:poly [[[0 0.5] [0.2 0] [0.8 0] [1 0.5] [0.8 1] [0.2 1]]]
                          :text [0.2 0 0.2 0]}
   :flowChartManualInput {:poly [[[0 0.2] [1 0] [1 1] [0 1]]] :text [0 0.2 0 0]}
   :flowChartManualOperation {:poly [[[0 0] [1 0] [0.8 1] [0.2 1]]]
                              :text [0.2 0 0.2 0]}
   :flowChartConnector {:ellipse true :text [0.146 0.146 0.146 0.146]}
   :flowChartOffpageConnector {:poly [[[0 0] [1 0] [1 0.8] [0.5 1] [0 0.8]]]
                               :text [0 0 0 0.2]}
   :flowChartPunchedCard {:poly [[[0.2 0] [1 0] [1 1] [0 1] [0 0.2]]] :text [0 0.2 0 0]}
   :flowChartPunchedTape {:d "M0 10C25 30 25 -10 50 10C75 30 75 -10 100 10V90C75 70 75 110 50 90C25 70 25 110 0 90Z"
                          :text [0 0.2 0 0.2]}
   :flowChartSummingJunction {:poly [[[0.146 0.146] [0.854 0.854]] [[0.854 0.146] [0.146 0.854]]]
                              :ellipse true}
   :flowChartCollate {:poly [[[0 0] [1 0] [0 1] [1 1]]]}
   :flowChartSort {:poly [[[0.5 0] [1 0.5] [0.5 1] [0 0.5]] [[0 0.5] [1 0.5]]]}
   :flowChartExtract {:poly [[[0.5 0] [1 1] [0 1]]] :text [0.25 0.5 0.25 0]}
   :flowChartMerge {:poly [[[0 0] [1 0] [0.5 1]]] :text [0.25 0 0.25 0.5]}
   :flowChartOnlineStorage {:d "M17 0H100A17 50 0 0 0 100 100H17A17 50 0 0 1 17 0Z"
                            :text [0.17 0 0.17 0]}
   :flowChartDelay {:d "M0 0H50A50 50 0 0 1 50 100H0Z" :text [0 0.146 0.146 0.146]}
   :flowChartMagneticDisk {:d "M0 12C0 -4 100 -4 100 12V88C100 104 0 104 0 88ZM0 12C0 28 100 28 100 12"
                           :text [0 0.25 0 0.1]}
   :flowChartDisplay {:d "M0 50L17 0H83A17 50 0 0 1 83 100H17Z" :text [0.17 0 0.17 0]}
   :star4 {:sides 4 :inner 0.25 :text [0.25 0.25 0.25 0.25]}
   :star5 {:sides 5 :inner 0.382 :text [0.25 0.25 0.25 0.25]}
   :star6 {:sides 6 :inner 0.577 :text [0.25 0.25 0.25 0.25]}
   :star7 {:sides 7 :inner 0.692 :text [0.25 0.25 0.25 0.25]}
   :star8 {:sides 8 :inner 0.75 :text [0.25 0.25 0.25 0.25]}
   :star10 {:sides 10 :inner 0.85 :text [0.25 0.25 0.25 0.25]}
   :star12 {:sides 12 :inner 0.75 :text [0.25 0.25 0.25 0.25]}
   :star16 {:sides 16 :inner 0.75 :text [0.25 0.25 0.25 0.25]}
   :star24 {:sides 24 :inner 0.75 :text [0.25 0.25 0.25 0.25]}
   :star32 {:sides 32 :inner 0.75 :text [0.25 0.25 0.25 0.25]}
   :irregularSeal1 {:poly [[[0.5 0.0]
                            [0.6 0.27]
                            [0.83 0.08]
                            [0.77 0.36]
                            [1 0.38]
                            [0.83 0.55]
                            [0.98 0.76]
                            [0.72 0.7]
                            [0.7 1]
                            [0.55 0.78]
                            [0.38 0.96]
                            [0.33 0.73]
                            [0.08 0.82]
                            [0.2 0.6]
                            [0 0.48]
                            [0.2 0.38]
                            [0.06 0.14]
                            [0.32 0.27]]]
                    :text [0.25 0.3 0.25 0.3]}
   :wave {:d "M0 12C30 -10 70 34 100 12V88C70 110 30 66 0 88Z" :text [0 0.15 0 0.15]}
   :doubleWave {:d "M0 8C17 -6 33 22 50 8C67 -6 83 22 100 8V92C83 106 67 78 50 92C33 106 17 78 0 92Z"
                :text [0 0.1 0 0.1]}
   :ribbon2 {:poly [[[0 0.25]
                     [0.2 0.25]
                     [0.2 0]
                     [0.8 0]
                     [0.8 0.25]
                     [1 0.25]
                     [0.9 0.5]
                     [1 0.75]
                     [0.8 0.75]
                     [0.8 1]
                     [0.2 1]
                     [0.2 0.75]
                     [0 0.75]
                     [0.1 0.5]]]
             :text [0.2 0 0.2 0]}
   :verticalScroll {:d "M10 10A8 8 0 0 1 18 2H92A8 8 0 0 1 92 18H82V90A8 8 0 0 1 74 98H8A8 8 0 0 1 8 82H18V18Z"
                    :text [0.18 0.18 0.18 0.18]}
   :horizontalScroll {:d "M2 18A8 8 0 0 1 10 10H90V8A8 8 0 0 1 98 8V82A8 8 0 0 1 90 90H18V92A8 8 0 0 1 2 92Z"
                      :text [0.1 0.18 0.1 0.18]}
   :wedgeRectCallout {:poly [[[0 0] [1 0] [1 1] [0.4167 1] [0.2917 1.125] [0.1667 1] [0 1]]]
                      :pad 0.13}
   :wedgeRoundRectCallout {:d "M10 0H90Q100 0 100 10V90Q100 100 90 100H42L29 112L17 100H10Q0 100 0 90V10Q0 0 10 0Z"
                           :pad 0.13}
   :wedgeEllipseCallout {:d "M37.059 98.2963L29 112L23.504 92.4024A50 50 0 1 1 37.059 98.2963Z"
                         :text [0.146 0.146 0.146 0.146]
                         :pad 0.13}
   :cloudCallout {:d "M22 88C5 88 0 70 10 62C0 50 10 34 24 38C26 20 46 14 56 26C64 12 86 16 86 34C100 38 102 58 92 66C100 78 88 92 74 86C66 98 46 98 40 88C34 92 26 92 22 88ZM24 104A4 4 0 1 0 32 104A4 4 0 1 0 24 104ZM30 96A6 6 0 1 0 42 96A6 6 0 1 0 30 96Z"
                  :text [0.15 0.25 0.15 0.2]
                  :pad 0.13}
   :borderCallout1 {:poly [[[0 0] [1 0] [1 1] [0 1]] [[0 1.12 -0.0 0] [0.25 1]]] :pad 0.13}})

(defcomputed shapeGallery
  '[{:id lines :shapes [line lineArrow lineDoubleArrow lineDot]}
   {:id rectangles :shapes [rect roundRect snip1Rect snip2SameRect snip2DiagRect stadium]}
   {:id basic
    :shapes [ellipse
             triangle
             rtTriangle
             parallelogram
             trapezoid
             diamond
             pentagonReg
             hexagon
             heptagon
             octagon
             decagon
             dodecagon
             pie
             chord
             teardrop
             frame
             halfFrame
             corner
             diagStripe
             plus
             can
             cube
             bevel
             donut
             noSmoking
             blockArc
             foldedCorner
             smileyFace
             heart
             lightningBolt
             sun
             moon
             cloud
             arc
             bracketPair
             bracePair
             leftBracket
             rightBracket
             leftBrace
             rightBrace]}
   {:id arrows
    :shapes [rightArrow
             leftArrow
             upArrow
             downArrow
             leftRightArrow
             upDownArrow
             quadArrow
             notchedRightArrow
             stripedRightArrow
             homePlate
             chevron
             uturnArrow
             bentArrow
             circularArrow
             curvedRightArrow]}
   {:id equation :shapes [mathPlus mathMinus mathEqual mathMultiply mathDivide mathNotEqual]}
   {:id flowchart
    :shapes [flowChartProcess
             flowChartAlternateProcess
             flowChartDecision
             flowChartInputOutput
             flowChartPredefinedProcess
             flowChartInternalStorage
             flowChartDocument
             flowChartMultidocument
             flowChartTerminator
             flowChartPreparation
             flowChartManualInput
             flowChartManualOperation
             flowChartConnector
             flowChartOffpageConnector
             flowChartPunchedCard
             flowChartPunchedTape
             flowChartSummingJunction
             flowChartCollate
             flowChartSort
             flowChartExtract
             flowChartMerge
             flowChartOnlineStorage
             flowChartDelay
             flowChartMagneticDisk
             flowChartDisplay]}
   {:id stars
    :shapes [star4
             star5
             star6
             star7
             star8
             star10
             star12
             star16
             star24
             star32
             irregularSeal1
             wave
             doubleWave
             ribbon2
             verticalScroll
             horizontalScroll]}
   {:id callouts
    :shapes [wedgeRectCallout wedgeRoundRectCallout wedgeEllipseCallout cloudCallout borderCallout1]}])

(defcomponent shapePath "A shape of the gallery drawn in its box: its effect, its outline and its extra paths."
  {:o nil :name ""}
  `(node :id ~(str name "-shape")
     :color transparent
     :width ~(max o.w 0)
     :height ~(max o.h 0)
     :overflow visible
     :let {:g ~(or-else (get computed.shapes (or-else o.shape "rect")) computed.shapes.rect)
           :w ~(max (or-else o.w 100) 1)
           :h ~(max (or-else o.h 100) 1)
           :m ~(min w h)
           :lw ~(if (or (= o.line "none") (not (or-else o.line g.line))) 0 (or-else o.lineWidth 1))
           :ink ~(or-else (get computed.palette (or-else o.line "accent1.5")) o.line "#000000")
           :cap ~(if g.line (+ (* 3 lw) 4) 0)
           :e ~(+ (/ lw 2) cap (* (or-else g.pad 0) (max w h)) (if (= o.effect "glow") 8 0))
           :r ~(* (or-else g.round 0) m)
           :c ~(* r (- 1 0.5522848))
           :rx ~(/ w 2)
           :ry ~(/ h 2)
           :cx ~(* rx 0.5522848)
           :cy ~(* ry 0.5522848)
           :corners ~(* (or-else g.sides 0) (if g.inner 2 1))
           :a ~(if g.line (atan2 h w) 0)
           :x1 ~(if (and g.line g.tail) (- w (* cap (cos a))) w)
           :y1 ~(if (and g.line g.tail) (- h (* cap (sin a))) h)
           :x0 ~(if (and g.line g.head) (* cap (cos a)) 0)
           :y0 ~(if (and g.line g.head) (* cap (sin a)) 0)
           :ellipse ~(str "M0 "
                          ry
                          "C0 "
                          (- ry cy)
                          " "
                          (- rx cx)
                          " 0 "
                          rx
                          " 0C"
                          (+ rx cx)
                          " 0 "
                          w
                          " "
                          (- ry cy)
                          " "
                          w
                          " "
                          ry
                          "C"
                          w
                          " "
                          (+ ry cy)
                          " "
                          (+ rx cx)
                          " "
                          h
                          " "
                          rx
                          " "
                          h
                          "C"
                          (- rx cx)
                          " "
                          h
                          " 0 "
                          (+ ry cy)
                          " 0 "
                          ry
                          "Z")
           :pathData ~(cond g.d g.d
                            g.poly (+ (join ""
                                     (map (fn [it]
                                            (str "M"
                                                 (join "L"
                                                       (map (fn [it]
                                                              (str (fixed (+ (* it.0 w) (* (or-else it.2 0) m)) 2)
                                                                   " "
                                                                   (fixed (+ (* it.1 h) (* (or-else it.3 0) m)) 2)))
                                                            it))
                                                 "Z"))
                                          g.poly))
                               (if g.ellipse ellipse ""))
                            g.ellipse ellipse
                            g.line (str "M"
                                 x0
                                 " "
                                 y0
                                 "L"
                                 x1
                                 " "
                                 y1
                                 (cond (= g.tail "arrow") (str "M"
                                            w
                                            " "
                                            h
                                            "L"
                                            (- x1 (* (/ cap 2) (sin a)))
                                            " "
                                            (+ y1 (* (/ cap 2) (cos a)))
                                            "L"
                                            (+ x1 (* (/ cap 2) (sin a)))
                                            " "
                                            (- y1 (* (/ cap 2) (cos a)))
                                            "Z")
                                       (= g.tail "oval") (str "M"
                                            (- w (/ cap 2))
                                            " "
                                            h
                                            "A"
                                            (/ cap 2)
                                            " "
                                            (/ cap 2)
                                            " 0 1 1 "
                                            (+ w (/ cap 2))
                                            " "
                                            h
                                            "A"
                                            (/ cap 2)
                                            " "
                                            (/ cap 2)
                                            " 0 1 1 "
                                            (- w (/ cap 2))
                                            " "
                                            h
                                            "Z")
                                       :else "")
                                 (cond (= g.head "arrow") (str "M0 0L"
                                            (- x0 (* (/ cap 2) (sin a)))
                                            " "
                                            (+ y0 (* (/ cap 2) (cos a)))
                                            "L"
                                            (+ x0 (* (/ cap 2) (sin a)))
                                            " "
                                            (- y0 (* (/ cap 2) (cos a)))
                                            "Z")
                                       (= g.head "oval") (str "M"
                                            (/ (- cap) 2)
                                            " 0A"
                                            (/ cap 2)
                                            " "
                                            (/ cap 2)
                                            " 0 1 1 "
                                            (/ cap 2)
                                            " 0A"
                                            (/ cap 2)
                                            " "
                                            (/ cap 2)
                                            " 0 1 1 "
                                            (/ (- cap) 2)
                                            " 0Z")
                                       :else ""))
                            (> corners 0) (str "M"
                                 (join "L"
                                       (map (fn [it]
                                              (str (fixed (* rx
                                                             (+ 1
                                                                (* (if (and g.inner (= (mod it 2) 1)) g.inner 1)
                                                                   (cos (- (/ (* 360 it) corners) 90)))))
                                                          2)
                                                   " "
                                                   (fixed (* ry
                                                             (+ 1
                                                                (* (if (and g.inner (= (mod it 2) 1)) g.inner 1)
                                                                   (sin (- (/ (* 360 it) corners) 90)))))
                                                          2)))
                                            (range corners)))
                                 "Z")
                            (= r 0) (str "M0 0H" w "V" h "H0Z")
                            :else (str "M"
                                 r
                                 " 0H"
                                 (- w r)
                                 "C"
                                 (- w c)
                                 " 0 "
                                 w
                                 " "
                                 c
                                 " "
                                 w
                                 " "
                                 r
                                 "V"
                                 (- h r)
                                 "C"
                                 w
                                 " "
                                 (- h c)
                                 " "
                                 (- w c)
                                 " "
                                 h
                                 " "
                                 (- w r)
                                 " "
                                 h
                                 "H"
                                 r
                                 "C"
                                 c
                                 " "
                                 h
                                 " 0 "
                                 (- h c)
                                 " 0 "
                                 (- h r)
                                 "V"
                                 r
                                 "C0 "
                                 c
                                 " "
                                 c
                                 " 0 "
                                 r
                                 " 0Z"))
           :fill ~(or-else (get computed.palette (or-else o.fill "accent1")) o.fill)
           :halo ~(cond g.line ink
                        o.gradient (or-else (get computed.palette o.gradient.from) o.gradient.from)
                        (= o.fill "none") ink
                        :else fill)}
     ~(shape-effect o name)
     ~(shape-outline o name)
     ~(repeat [f (if (or g.light g.dark) [{:id "light" :faces (or-else g.light []) :color "#ffffff40"} {:id "dark" :faces (or-else g.dark []) :color "#00000033"}] []) :key f.id]
       (shape-feature o name f))))

;;; Views

(defview shape-effect "The shadow or the glow under shape `o`." [o name]
  `(path :id ~(str name "-effect")
     :when ~(or (= o.effect "shadow") (= o.effect "glow"))
     :x ~(+ (- e) (if (= o.effect "shadow") 4 0))
     :y ~(+ (- e) (if (= o.effect "shadow") 4 0))
     :width ~(+ w (* 2 e))
     :height ~(+ h (* 2 e))
     :fit fill
     :scale ~[(if o.flipH (- 1) 1) (if o.flipV (- 1) 1)]
     :viewBox ~(if g.d
                   (str (fixed (/ (* (- e) 100) w) 3)
                        " "
                        (fixed (/ (* (- e) 100) h) 3)
                        " "
                        (fixed (+ 100 (/ (* 200 e) w)) 3)
                        " "
                        (fixed (+ 100 (/ (* 200 e) h)) 3))
                   (str (- e) " " (- e) " " (+ w (* 2 e)) " " (+ h (* 2 e))))
     :d ~pathData
     :fillRule ~(if g.evenodd "evenodd" "nonzero")
     :fillColor ~(if (or (= o.effect "glow") g.open) "transparent" "#00000040")
     :stroke ~(cond (= o.effect "glow") (str (slice halo 0 7) "66") (or g.line g.open) "#00000040" :else "transparent")
     :strokeWidth ~(if (= o.effect "glow") (+ lw 12) lw)
     :strokeJoin round))

(defview shape-outline "Shape `o`'s path: its fill, its line, its dash." [o name]
  `(path :id ~(str name "-path")
     :x ~(- e)
     :y ~(- e)
     :width ~(+ w (* 2 e))
     :height ~(+ h (* 2 e))
     :fit fill
     :scale ~[(if o.flipH (- 1) 1) (if o.flipV (- 1) 1)]
     :viewBox ~(if g.d
                   (str (fixed (/ (* (- e) 100) w) 3)
                        " "
                        (fixed (/ (* (- e) 100) h) 3)
                        " "
                        (fixed (+ 100 (/ (* 200 e) w)) 3)
                        " "
                        (fixed (+ 100 (/ (* 200 e) h)) 3))
                   (str (- e) " " (- e) " " (+ w (* 2 e)) " " (+ h (* 2 e))))
     :d ~pathData
     :fillRule ~(if g.evenodd "evenodd" "nonzero")
     :fillColor ~(cond g.line ink
                       (or g.open (= o.fill "none") o.gradient) "transparent"
                       :else (or-else (get computed.palette (or-else o.fill "accent1")) o.fill))
     :fillGradient ~(if (and (not g.line) o.gradient)
                        {:kind "linear"
                         :angle (+ (or-else o.gradient.angle 90) 90)
                         :stops [{:at 0 :color (or-else (get computed.palette o.gradient.from) o.gradient.from)}
                                 {:at 1 :color (or-else (get computed.palette o.gradient.to) o.gradient.to)}]}
                        nil)
     :stroke ~(if (> lw 0) ink "transparent")
     :strokeWidth ~lw
     :strokeJoin ~(if g.line "round" "miter")
     :strokeDash ~(cond (= o.dash "dot") [lw lw]
                        (= o.dash "dash") [(* 4 lw) (* 3 lw)]
                        (= o.dash "dashDot") [(* 4 lw) (* 3 lw) lw (* 3 lw)]
                        (= o.dash "longDash") [(* 8 lw) (* 3 lw)]
                        :else [])))

(defview shape-feature "One of the extra paths a shape draws, `f`, such as an arrow's head." [o name f]
  `(path :id ~(str name "-" f.id)
     :x ~(- e)
     :y ~(- e)
     :width ~(+ w (* 2 e))
     :height ~(+ h (* 2 e))
     :fit fill
     :scale ~[(if o.flipH (- 1) 1) (if o.flipV (- 1) 1)]
     :viewBox ~(str (- e) " " (- e) " " (+ w (* 2 e)) " " (+ h (* 2 e)))
     :d ~(join ""
               (map (fn [it]
                      (str "M"
                           (join "L"
                                 (map (fn [it]
                                        (str (fixed (+ (* it.0 w) (* (or-else it.2 0) m)) 2)
                                             " "
                                             (fixed (+ (* it.1 h) (* (or-else it.3 0) m)) 2)))
                                      it))
                           "Z"))
                    f.faces))
     :fillColor ~f.color))
