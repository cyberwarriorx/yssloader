Introduction
------------
This plugin was designed with Yabause debugging and saturn game hacking in mind. If that describes you, you've come to the right utility.

The basic function is to load a Yabause Save State file(*.yss) and depending on which cpu is selected, load up the appropriate code and settings. By default It loads up LWRAM/HWRAM for SH2. 68000 is also in the works. SCU DSP may be planned in the future.

Dependencies
------------
The code is based on the latest IDA Pro SDK and Visual Studio 2010. However you should be able to tweak it up to support other versions via the project settings. 

It also looks for several sig files if SBL/SGL code is detected. I would've included them but I question the legality of sharing them. You can generate them yourself fairly trivially so long as you can find the SBL/SGL SDK's. 

Missing or ideas
----------------
There's a few things I really want to add at some point given the time or that people may find helpful:
-68000 support
-SCU DSP support
-The ability to select which ram/register areas to include
-Register naming option
-Helpful comments or notes for known functions
-Support for more library sigs
-Tighter IP.BIN data/code detection. May be useful for those wanting to make region free games.
-Better SGL 2.1, 3.00, 3.02j, and later detection. SBL > 6.01

Special Thanks
--------------
Thanks to my buds on #yabause and rhdn
-Amon
-Bacon
-BlueCrab
-esperknight
-Guill
-pinchy
-SamIAm
-SaturnAR
-tehcloud
-WhiteSnake
