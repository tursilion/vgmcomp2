20231219

NOTE: Apologies if you have trouble pulling - I reverted and force pushed to undo a previous change, and this may cause issues if you pulled while that commit was up. Nothing important changed in the last 60 days, so just clone again.

This is a tool suite for converting and editting music for the SN76489 and compatible sound chips. It does NOT include a tracker, but rather seeks to be able to adapt music from other trackers for use.

Currently, imports are available for VGM (SN PSG, AY-8910 PSG, Gameboy DMG, MegaDrive/Genesis YM2612, NES APU, and Pokey), MOD files (many formats), and SID files (with varying degrees of success). The output of the importer is a set of text files containing frequency and volume data, which is designed to be easily manipulated. Almost 40 tools for manipulating the data is provided.

Limited support for playing back the data on the AY-3-8910 and the Commodore SID is also available.

Finally, a compression tool and playback library is provided for the final audio data. A PC reference library is provided, as well as optimized libraries for the TI-99/4A and ColecoVision.

NOTE: for the TI, a new GCC patch was released December 13, 2023, which moves the stack register. The main branch tracks this update - you can use the old_ti_gcc branch if you want the previous behaviour (or just search for the R10/R15 code... right now all it does is copy the register from R15 to R10 and back. In the future I will patch the code to remove the extra instructions.)

Full source code for all tools is provided. Where not prevented by GPL, all other code is Public Domain and free for your own use, completely unencumbered. See [PublicDomainMostly.txt](https://github.com/tursilion/vgmcomp2/raw/main/dist/PublicDomainMostly.txt). The code is written for Windows but is mostly command-line and may compile elsewhere.

You can also view the [Documentation](https://github.com/tursilion/vgmcomp2/raw/main/dist/VGMComp2.pdf)

Here are some example videos:
- NES source: [https://lbry.tv/@tursilion:1/2020-05-31-03-45-19:9](https://lbry.tv/@tursilion:1/2020-05-31-03-45-19:9)
- MOD source: [https://lbry.tv/@tursilion:1/2020-07-21-02-06-47:e](https://lbry.tv/@tursilion:1/2020-07-21-02-06-47:e)
- Genesis Source: [https://lbry.tv/@tursilion:1/VgmComp2---Sega-Genesis-to-SN-Example:f](https://lbry.tv/@tursilion:1/VgmComp2---Sega-Genesis-to-SN-Example:f)
