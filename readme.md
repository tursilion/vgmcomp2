20220703

This is a tool suite for converting and editting music for the SN76489 and compatible sound chips. It does NOT include a tracker, but rather seeks to be able to adapt music from other trackers for use.

Currently, imports are available for VGM (SN PSG, AY-8910 PSG, Gameboy DMG, MegaDrive/Genesis YM2612, NES APU, and Pokey), MOD files (many formats), and SID files (with varying degrees of success). The output of the importer is a set of text files containing frequency and volume data, which is designed to be easily manipulated. Almost 40 tools for manipulating the data is provided.

Limited support for playing back the data on the AY-3-8910 and the Commodore SID is also available.

Finally, a compression tool and playback library is provided for the final audio data. A PC reference library is provided, as well as optimized libraries for the TI-99/4A and ColecoVision.

Full source code for all tools is provided. Where not prevented by GPL, all other code is Public Domain and free for your own use, completely unencumbered. See [PublicDomainMostly.txt](https://github.com/tursilion/vgmcomp2/raw/master/dist/PublicDomainMostly.txt). The code is written for Windows but is mostly command-line and may compile elsewhere.

Download the latest release zip here:
[https://github.com/tursilion/vgmcomp2/raw/master/dist/VGMComp2.zip](https://github.com/tursilion/vgmcomp2/raw/master/dist/VGMComp2.zip)

You can also view the [Documentation](https://github.com/tursilion/vgmcomp2/raw/master/dist/VGMComp2.pdf)

Here are some example videos:
- NES source: [https://lbry.tv/@tursilion:1/2020-05-31-03-45-19:9](https://lbry.tv/@tursilion:1/2020-05-31-03-45-19:9)
- MOD source: [https://lbry.tv/@tursilion:1/2020-07-21-02-06-47:e](https://lbry.tv/@tursilion:1/2020-07-21-02-06-47:e)
- Genesis Source: [https://lbry.tv/@tursilion:1/VgmComp2---Sega-Genesis-to-SN-Example:f](https://lbry.tv/@tursilion:1/VgmComp2---Sega-Genesis-to-SN-Example:f)
