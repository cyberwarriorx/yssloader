Introduction
------------
This plugin was designed with Yabause debugging and Saturn game hacking in mind. If that describes you, you've come to the right utility.

The basic function is to load a Yabause Save State file(*.yss) and depending on which cpu is selected, load up the appropriate code and settings. By default It loads up LWRAM/HWRAM for SH2. 68000 is also supported. SCU DSP may be coming in the future.

Dependencies
------------
The code is based on the latest IDA Pro SDK and Visual Studio 2010. However you should be able to tweak it up to support other versions via the project settings. 

It also looks for several sig files if SBL/SGL code is detected. I would've included them but I question the legality of sharing them. You can generate them yourself fairly trivially so long as you can find the SBL/SGL SDK's. 

Build/Install Instructions
--------------------------
1. With the project loaded up in Visual Studio, go into yssloader's project properties. Select "Release" configuration.
2. Under "Configuration Properties"->"VC++ Directories", change the ida pro path for "Include Directories" and "Library Directories" to match your IDA Pro SDK directory.
3. Under "Configuration Properties"->"Build Events"->"Post-Build Events", change "Command Line" so it matches your IDA Pro directory.
4. Press OK to accept changes.
5. Make sure active configuration is set to "Release". Build Solution.

Usage Instruction
-----------------
1. Startup IDA Pro and select "New".
2. Select your Yabause save state file and press "Open".
3. Make sure "YSS File" is selected under file type list.
4. (Optional) Select the cpu from the list. If any unsupported cpu's are selected, it automatically chooses SH2.
5. Press OK. Wait for IDA Pro to finish loading.

Todo List
---------
There's a few things I really want to add at some point given the time:
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
