;; sound.lisp: the beat, the sound of a song without one of its own: a beatbox loop made for Tempo
;; (audio/make_beat.py), on two decks so a crossfade overlaps the song ending and the next. Data, so a revision can
;; add a song's own sound beside them (/audio/name).
{:audio {:beat {:file "../assets/audio/beatbox.wav"
                :loop true
                :volume 0.7
                :into state.beat
                :bands 32
                :updates 50
                :note "The sound of a song without a sound of its own: a beatbox loop made for Tempo (audio/make_beat.py)"}
         :beat2 {:file "../assets/audio/beatbox.wav"
                 :loop true
                 :volume 0.7
                 :into state.beat2
                 :bands 32
                 :updates 50
                 :note "The beat's second deck: the next song fades in on it while the song ending fades out"}}}
