;; sound.lisp: the beat, the sound of a song without one of its own: a beatbox loop made for Tempo
;; (audio/make_beat.py). Data, so a revision can add a song's own sound beside it (/audio/name).
{:audio {:beat {:file "../assets/audio/beatbox.wav"
                :loop true
                :volume 0.7
                :into state.beat
                :bands 32
                :updates 50
                :note "The sound of a song without a sound of its own: a beatbox loop made for Tempo (audio/make_beat.py)"}}}
