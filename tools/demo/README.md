# Demo clips

`apexamp_demo` (built with `-DAPEX_BUILD_SNAPSHOTS=ON`) renders the ApexAmp demo
clips through the real plugin processor:

    ./apexamp_demo out-folder [--set paramId=value ...]

The guitar is synthetic (`GuitarDI.h`): a waveguide string per note with a
triangle pluck at the pick position, palm-mute damping, a fret-hand release and
a velocity-sensing bridge humbucker. Two humanised takes of a 135 bpm thall riff
on an 8-string in F# standard are calibrated with Auto Input, as a user would,
and panned hard left and right. The Legion's kick and bass sit in the middle.

    01_di.wav             the dry DI
    02_rhythm.wav         Thallbyssal, double-tracked
    03_legion.wav         with The Legion
    03b_legion_alone.wav  the Legion on its own
    04_drop.wav           the riff in E standard, dropped to F# by the Drop pedal

`--probe [--di file.wav] [--trim dB] [--scale x]` prints input and output levels
per 100 ms instead, for checking the gate and noise between notes.

Master for listening with ffmpeg, e.g.
`ffmpeg -i 03_legion.wav -af loudnorm=I=-14:TP=-1 -b:a 224k 03_legion.mp3`.
