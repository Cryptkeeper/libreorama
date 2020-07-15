# libreorama
libreorama is a free, MIT licensed sequence player program written in C99. It is designed as a minimized, CLI alternative to the player software found in [Light-O-Rama's ShowTime Sequencing Suite](http://www1.lightorama.com/showtime-sequencing-suite/).

## Why?
Light-O-Rama's ShowTime Sequencing Suite is Windows only and requires a mouse/keyboard. As a result, users typically use a laptop or desktop computer to control Light-O-Rama hardware. This can be very inconvenient to deploy and manage remotely, especially for temporary installations. Alternatively, you can buy their [Director hardware](http://store.lightorama.com/g3mp3director.html), but it is expensive and prevents customization and remote management altogether.

The community has created several alternatives, such as [xLights](https://github.com/smeighan/xLights) and [fpp](https://github.com/FalconChristmas/fpp) (which is designed to run on [Raspberry Pis](https://www.raspberrypi.org/)). Unfortunately these alternatives are focused on other hardware offerings and offer little support for Light-O-Rama's unique features. Such as its sequence file format, hardware capabilities, and [custom protocol](https://github.com/Cryptkeeper/lightorama-protocol).

Furthermore in offering such a wide array of hardware support, [xLights](https://github.com/smeighan/xLights) and [fpp](https://github.com/FalconChristmas/fpp) have become very complex codebases, making it difficult to target specific behavior or feature sets.

## Design Goals
libreorama emphasizes the [Unix philosophy](https://en.wikipedia.org/wiki/Unix_philosophy) of "do one thing well".

* Support [Light-O-Rama hardware](http://store.lightorama.com/showtime-products.html), and Light-O-Rama hardware only.
* Out of the box support for Light-O-Rama's sequence file format and network protocol.
* No time scheduling features, no setup, no configuration, no custom file formats. Grab the binary and go.
* Portable code. Simple C code makes it easy to compile for different environments and OSes.
* Fast, lightweight and reliable. Keep memory allocations, disk activity and network usage minimal.
* Hackable and customizable. libreorama is ~2,000 lines of code and a simple project.

## Compatibility
libreorama has been tested with the following hardware:

| Unit Model | Firmware Version | Channel Count |
| --- | --- | --- |
| 1x `LOR1602Wg3` | 1.12 | 16 |
| 1x `CTB16PCg3` | 1.09 | 16 |
| 3x `CTB16PCg3` | 1.11 | 16 |

The hardware communication layer is based off my reverse engineered documentation of the [Light-O-Rama protocol](https://github.com/Cryptkeeper/lightorama-protocol). As such, you should consider not all hardware (especially [RGB devices](http://store.lightorama.com/rgbdevices.html)) compatible.

libreorama currently supports the following Light-O-Rama sequencing effects:

```
* Set Brightness (any brightness supported)
* Fade (any brightness & duration supported)
* Shimmer & Twinkle
* Channel On
* Unit Off (turns off all channels on a unit)
* Channel Masking and unit broadcasts (protocol behavior)
```

For Light-O-Rama networks focused on AC lighting control, libreorama is likely to work out of the box. You mileage _will_ vary. libreorama is provided for free and is unsupported :)

Feedback and compatibility testing by the community is welcomed. 

* libreorama uses my Light-O-Rama library, [liblightorama](https://github.com/Cryptkeeper/liblightorama), for hardware communication which offers an easy way for contributors to improve hardware support without needing to dive into libreorama directly. 
* My reverse engineered [Light-O-Rama network protocol documentation](https://github.com/Cryptkeeper/lightorama-protocol/) is available to familiarize yourself with the internals of Light-O-Rama hardware and software.

## Compiling
libreorama uses CMake for compiling and requires these dependencies. Please research how to obtain these (if needed) for your respective OS as I have only tested them on macOS. You may need to adjust header search & include paths.

* [liblightorama](https://github.com/Cryptkeeper/liblightorama)
* [libserialport](https://sigrok.org/wiki/Libserialport)
* libalut (I use [freealut](https://github.com/vancegroup/freealut) on macOS)
* [libxml2](http://www.xmlsoft.org/)
* OpenAL (you likely already have this installed)

Once installed, compile libreorama:

1. `git clone https://github.com/Cryptkeeper/libreorama`
2. `cd libreorama/`
3. `cmake .`
4. `make`

This will compile a `libreorama` binary, ready to use. `make install` is available to install libreorama to your user bin path.

## Usage
libreorama is designed as a CLI program that plays a sequence (or list of sequences) when ran. Once playback is complete, it will exit. Scheduling behavior can be done using libreorama in conjunction with other programs such as [cron](https://en.wikipedia.org/wiki/Cron).

```
Usage: libreorama [options] <serial port name>

Options:
	-b <serial port baud rate> (defaults to 19200)
	-f <show file path> (defaults to "show.txt")
	-p <pre-allocated frame buffer length> (defaults to 0 bytes)
	-c <time correction offset in milliseconds> (defaults to 0)
	-l loop show infinitely (defaults to false)
```

Light-O-Rama hardware communicates using serial ports, typically with a single connection point to the host system. Simply provide the serial port/device name to libreorama (and optionally, a custom baud rate).

"Shows" are newline separated text files containing the sequence files to play. libreorama will read each sequentially line and play the corresponding sequence. libreorama will not reload modified show files and must be restarted.

```
sequences/My First Sequence.lms
sequences/My Second Sequence.lms
...
```

There is no explicit limit to how many sequences are in a show (besides a minimum of one).

## Performance
On my hardware, libreorama playing a 16 channel sequence, with audio, has an average of `0.63%` CPU usage and `31.3MB` of RAM usage (with the vast majority being the audio file).

```
MacBook Pro (15-inch, 2017)
2.8 GHz Quad-Core Intel Core i7
16 GB 2133 MHz LPDDR3
```

As such, libreorama will comfortably run a complex Light-O-Rama network off a [Raspberry Pi](https://www.raspberrypi.org/).

### Protocol Encoding Optimizations
libreorama uses a runtime minimiser for optimizing the outgoing network protocol as it is encoded during playback. The minimiser ([`src/lorinterface/minify.c`](src/lorinterface/minify.c)) focuses on preventing duplicate frames, using Light-O-Rama protocol's [channel masking functionality](https://github.com/Cryptkeeper/lightorama-protocol/blob/master/PROTOCOL.md#channel-masking) and simplifying bulk resets.

Any sequence that duplicates effects across channels, or commonly controls several channels within a unit at a time, will see a ~30% improvement in network bandwidth usage.

### Frame Buffer
libreorama interprets the Light-O-Rama sequence file in frames. Each frame is simplified if possible, and encoded into the network protocol equivalent to be written to the serial port. As it is encoded, it is stored in the "frame buffer". For complex sequences, there may be a lot of network traffic and subsequently a larger frame buffer is necessary. libreorama will automatically expand (and shrink) the frame buffer to fit demand. When it expands the frame buffer, it will print a warning.

`reallocated frame buffer to 128 bytes (increase pre-allocation?)`

If this frequently happens (and your host environment has memory to spare), consider using the `-p` option to pre-allocate a larger frame buffer. This enables you to use more memory to reduce CPU time.

### Playback Timing
libreorama will automatically determine a step time (or FPS) for each sequence when loaded. Currently, it will select the highest resolution step time needed to faithfully playback the sequence without difference. 

Between playback steps, libreorama uses `nanosleep` to wait for the next frame. This avoids the CPU usage of spinlocks, but introduces a potential time offset issue (for example, request a sleep of 50 milliseconds but it sleeps for 55). To avoid this, libreorama will automatically offset its playback step time to adjust for over-sleep (see [`src/interval.c`](src/interval.c) for implementation details).

For sequences lagging behind their audio playback, the `-c` option allows you to provide a time correction offset (in milliseconds). This shifts sequence playback forward, effectively delaying audio playback.

## License
See [LICENSE](LICENSE).
