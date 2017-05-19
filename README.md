 # VulkFM 

A simple FM synthesizer for your basic FM needs.


## Why?

I made this because I've been curious on how FM synthesis works, and what better way to learn than implement it for yourself. The 
main purpose of the code is to learn, and experimenting on how different parameters change the sound in what way.


## Inner workings

 * Instrument holds the static setting for the operators.
 * Voice owns the oscilator and envelope and references an Instrument.
 * Oscilator generates the waveform
 * Envelope generates the envelope.
 * VulkFM have a list of instruments an a pool of free voices. Responsible for collecting 
   sample data from the voices and mixing them to a final sample.


## External libraries

 * SDL2+OpenGL for video and sound playback
 * For UI I use [Dear ImGui](https://github.com/ocornut/imgui)

