there are lots of bugs :D



## General

* samples click and buzz like hell! apparently EMU8000's 3 (or 4?) point interpolation needs some more tweaking to make it work as needed
  * possible solutions - pad loop end with dummy samples manually, shift loop points for long non-chippy loops, etc...
* alright, seems you can't touch channels 30-31 at all, they indeed control the DRAM refresh?
  * hack - make ch 31 play through entire memory to keep it refreshed?
  * apparently DRAM refresh is not the issue here, must be interpolator overflow instead
* 16 bit samples are not implemented yet
* DRAM read/write position caching needs to be fixed. disabled for now
* volume ramping is buggy and effectively doesn't work
  * can be emulated more or less properly via EMU8000 envelope generator, but it will also require log pitch instead of delta (you can't turn pitch envelope off while keeping volume envelope running!)
* timers are basically detection check stubs (overflow the moment it's unmasked and started)
* IRQ/DMA is yet to be implemented
* "slow DRAM" mode (only ch28-29 are used for DRAM access) is broken now (ULTRINIT and ST3 can't detect card)



## Application-specific

* GUSPlay applications almost always work fine, with minor whining if samples are not looped properly
* ..same for MIDAS 0.40a, albeit didn't tested it much
* Impulse Tracker - one-shot samples are cut after first tick, volume bars are broken
* Scream Tracker 3.x, Second Reality, Unreal/Future Crew (version 1.1 with GUS support) - ~~corrupted samples at 2nd and further module load~~ fine except for minor ultraclick bugs (see below)
  * ~~ST3: load AWEGUS, run ST3, then load any module - plays fine; then, exit from the tracker, run ST3 again - can't detect GUS. running ULTRINIT then running ST3 again didn't help~~
  * upd: ST3 - managed to fix sample corruption but now some channels play wrong samples (some are at +1 octave). must have something with "ultraclick removal" feature?
* [So Be It/Xtacy](https://www.pouet.net/prod.php?which=1025) - uploads samples always on 0x220 but controls GF1 playback on user selected port. also crashes with reset on phong part:
  * from [pouet comment](https://www.pouet.net/prod.php?post=845293): *Crashes in DOSBox (all builds so far) at the "phong environment mapping" part due to corrupt MCB chain. Works all the way in PCem at 486/66, but without music (seems to want the GUS at 220/7/1, while PCem forces 240/5/3).*
* [Bugfixed/Acme](https://www.pouet.net/prod.php?which=1117) - ~~all samples are very short loops for some reason. also, uses 16bit samples and ping-pong loops~~ works for now! lots of whining but otherwise fine enough.
  * alright, I got it - player constantly rewrites channel control (reg 0x00), which in turn, triggers emu8k loop point update, which further triggers current position update. oops.
    also, seems to enable bidir loop without enabling regular loop as well
  * also needs a plenty of low RAM - one part crashed with "not enough memory" but music kept playing and eventually next part started fine; then, a bit later another out of memory crash occured, this time fatal.
* [Abraham/Plant](https://www.pouet.net/prod.php?which=1201) - ~~some channels play ROM crap alongside with the music~~ now it's much better, but bass sample is not looped (gets cut off) and there is lot of hifreq whining. perhaps bass uses volume ramping for slides
  * upd: ALRIGHT i broke it again :( most samples are cut off early, have absolutely no idea why
* [Psychic Flight/Spirit](https://www.pouet.net/prod.php?which=41739) - uses OUTSB with ES segment override to upload samples, crashes with GPF
  * upd: Jemm can't handle segment overrides on string I/O, oops
