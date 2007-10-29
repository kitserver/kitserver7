Kitserver 7 for PES2008 PC                               October 27, 2007
=============================================================================
Version 7.0.0d - the Philadelphia Release

FEATURES
---------------
- Enable up to 4 controllers (for demo)
- Select quality levels which PES doesn't want you to choose
- Any window resolution supported, hidden fullscreen resolutions unlocked
- Manual/Automatic aspect ratio correction.

INSTALLATION
---------------
As in Kitserver 6, you have to unpack the archive to your PES installation
folder and run the setup.exe. What is new is that you can now also select your
settings.exe. This is necessary because some changes are only possible in that
file. simply select these both files (they should already be preselected) and
click "Install". If you choose that you don't want to use Kitserver any longer,
run setup.exe again and click "Remove". You can also install/remove Kitserver
only for one exe by setting the other one to "no action".

HOW TO USE
---------------
After installing Kitserver, you are asked if you want to enable all quality
levels. In PES2008, Konami doesn't allow you to choose a high quality level if
the game thinks your PC isn't good enough for that. If you click on "Yes", that
check is removed and you can try if they are right with their assumption.
Apart from that you can now choose 4 controllers in the "Controller" tab. That
means you can configure your gamepads here and later play with them. Note that
by default the keyboard is controller #1. If you setup your gamepad as first
controller, you can't play against someone using the keyboard and your gamepad.
For that, you shouldn't setup a gamepad as first controller but always as #2, #3
or #4. However you can't use your gamepad in the main menu then, that works only
with the first controller.


LOD MIXER CONFIGURATION
-----------------------
This is the first release of LOD-Mixer for PES2008. Currently two features are
implemented: screen resolution , and aspect ratio correction. Both of these
can be configured in kitserver main configuration file (kitserver/config.txt).

1. Screen Resolution.
~~~~~~~~~~~~~~~~~~~~~
You can set any screen resolution you want, if you play in a Windowed mode. Even
crazy screens like 1567x532 will work, but you're likely to suffer from 
performance problems on such cases.
Hidden fullscreen resolutions are fully unlocked now as well. However, only those
that your video card really supports in full-screen mode, will work. If you 
accidently choose an unsupported fullscreen resolution, then PES should still 
be able to start in a window.

You specify the resolution in config.txt using "screen.width" and "screen.height"
settings. Here's an example config fragment for LOD mixer:

[lodmixer]
screen.width = 1920
screen.height = 1200

2. Aspect Ratio.
~~~~~~~~~~~~~~~~
PES2008 offers only two choices for aspect ratio - 4:3 and 16:9. However, lots of
LCD monitors don't exactly fit into either of these two. Often, a 16:10 ratio is
used, or even 16:9.6. This results in the picture being distored: players either
too fat or too skinny, and ball is not round.

With LOD Mixer, you can set the aspect ratio to whatever you want. Either let
LOD Mixer calcaulate it automatically - from width and height of the resolution
that you specify - or set the value manually. Automatic way would work quite
accurately, assuming the pixel is square. Sometimes, however, you would want to
set it manually. For example, i play on widescreen monitor, but using a 800x600 
resolution, because my video card is not powerful enough. The automatic 
calculation would give 4:3, but since the view is stretched to fill the entire 
screen, we need to account for that. Setting aspect ratio to 1.6 (which is a 
natural AR for my laptop) does the trick.

Here are some configuration examples:

Example 1 - we let the game choose resolution, but enforce aspect ratio:

[lodmixer]
screen.aspect-ratio = 1.66666

Example 2 - we set both the resolution and aspect ratio:

[lodmixer]
screen.width = 800
screen.height = 600
screen.aspect-ratio = 1.6

Example 3 - we set the resolution, and rely on automatic aspect ratio:

[lodmixer]
screen.width = 1650
screen.height = 1050



Have a lot of fun,
Robbie and Juce.
