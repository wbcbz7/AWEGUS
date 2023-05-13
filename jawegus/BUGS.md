there are lots of bugs :D



## General

* samples click and buzz like hell! apparently EMU8000's 3 (or 4?) point interpolation needs some more tweaking to make it work as needed
  * possible solutions - pad loop end with dummy samples manually, shift loop points for long non-chippy loops, etc...
* alright, seems you can't touch channels 30-31 at all, they indeed control the DRAM refresh?
  * hack - make ch 31 play through entire memory to keep it refreshed?
  * apparently DRAM refresh is not the issue here, must be ~~interpolator overflow~~ DRAM access idiosyncrasies instead
* DRAM read/write position caching needs to be fixed. disabled for now
* volume ramping is buggy and effectively doesn't work
  * can be emulated more or less properly via EMU8000 envelope generator, but it will also require log pitch instead of delta (you can't turn pitch envelope off while keeping volume envelope running!)
* timer IRQ is kind of...working, but not always :) COM1/COM2 works fine, SB16 bombs out with GPF
* DMA emulation has either off-by-one or write buffer flush issues (sample loop end buzzes sometimes), DMA TC IRQ is to be implemented.



## Application-specific

* GUSPlay applications almost always work fine, with minor whining if samples are not looped properly
* ..same for MIDAS 0.40a, albeit didn't tested it much
* Impulse Tracker - one-shot samples are cut after first tick, volume bars are broken (only 1st sample is shown correctly). on the other hand, GUS timer works - you need to explicitly provide IRQ in command line to enable GUS timer mode
  * upd: ITGUSLO.DRV works (broken pitch in EG version!), volume bars are still broken
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
* CapaMod: no timer + no DMA mode works awesome, DMA + no timer ~~hangs on sample upload~~ fixed. timer mode still doesn't work
  * upd: uses very high timer frequency (~1250 Hz). ~~UART timer goes out of sync :(~~ fixed
  * upd: writes IRQ/DMA settings from ULTRASND variable via 2xB, also uses Register Select at 2xF (which is present on Rev 3.4+) to clear all IRQs before start.
  * upd: DMA transfer hangs because the player polls i8237 status register for TC bit set. again the byproduct of missing Jemm's DMA virtualization :)
  * upd: DMA fixed by doing dummy verify+block transfer solely for setting TC flag in i8237 status reg
  * timer upd: timer does work, but IRQ status is not acknowledged for unknown reason. at exit, does not stops the timer, only shuts timer IRQs off. *facepalm.jpg*
    * ignoring 2x6 fixes the issue, but it's not a solution.
* DOSLIB GUS test.exe: ~~"Timer Readback fail (irqstatus=0x00)" if IRQ emulation is on, passes timer check if emulation is off~~ fixed
  * yeah [polling loop](https://github.com/joncampbell123/doslib/blob/master/hw/ultrasnd/ultrasnd.c#L473) assumes that timer IRQ bit in 2x6 is always set before IRQ is triggered, checks 2x6 in main loop. actually it may fail on real hardware as well!
  * upd: nope! it doesn't ack timer IRQ at all so 2x6 should reflect IRQ status! so either Jemm really does not reflect V86 IF to real one, or I messed up with timer IRQ code again :D
  * upd2: the latter.
* Star Control 2: hangs on start.
  * uses DMA terminal count IRQ. also probably requires 16-bit DMA channel to work
    * implemented both, still hangs
* [x14/Orange](https://www.pouet.net/prod.php?which=1301) - bombs out to DOS prompt at start, with keyboard locked out. runs more or less fine without emulator but with Jemm loaded
  * runs in ring0 protected mode. okay :) as it works with Jemm, it supports at least VCPI, and possibly DPMI also.
  * runs under WinXP as well! (not with GUSemu32 though). so it supports DPMI, and I guess it uses Tran's PMODE.
