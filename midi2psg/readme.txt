This is not finalized yet, but it works.

- Instruments are not parsed - everything has a gentle fade off of about 1 second. This works pretty well on most MIDIs. 
- The drum channel is recognized and broken out as noise
- A maximum of 3 polyphony notes per channel are permitted
- set a "speed_scale" as a float to change the playback speed (run with no arguments to see)
- set a "decay_scale" to change the rate of the volume decay
- requires MIDO library and Python 3

This is not 100% ready yet - I'll do some work to integrate it better with the other tools. I also want to make the polyphony configurable.

Script was written by Claude.ai
