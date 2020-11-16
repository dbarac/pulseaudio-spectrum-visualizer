# PulseAudio spectrum visualizer

## Demo

![Visualizer demo](./demo.gif)

## Requirements

PulseAudio (tested with version 13.0)

## Compile

```bash
$ make
```

## Usage

To use the default PulseAudio device (may not work):

```bash
$ ./spectrum_visualizer
```

To use any other PulseAudio device:

```bash
$ ./spectrum_visualizer pulseaudio-device-name
```

Device names can be found with the pactl command.
One of the output devices with the class "monitor" should work.

```bash
$ pactl list | grep monitor
```