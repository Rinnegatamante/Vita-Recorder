# Vita Recorder
Vita Recorder is a plugin that allows to record video clips during your play sessions.<br>
The code is based off VITA2PC at which has been applied improvements and the streaming feature has been replaced with file recording.

# Current features set
* Allows to record clips of unlimited duration (given enough free storage is available).
* Records clips in RAW mjpeg (can be opened on Windows Media Player and other popular video players as well as can be easily converted to more common formats on PC).
* Allows to downscale on CPU the output to 480x272 allowing for faster transcoding.
* Performs hw encoding in MJPEG thanks to sceJpegEncoder when possible. When resources are not enough, libjpeg-turbo is used instead, as fallback, for software encoding.
* Allows to perform both asynchronous and synchronous recording (The first won't affect game performances but you may end up having some artifacts or some missing frames, the latter will lower game performances but will produce frame perfect clips).
* Allows to apply frameskip on synchronous recording.

# How to install
* Put `VitaRecorder.suprx` in your `tai` folder.
* Add the plugin under a section for the game you want to use it for (eg `*GTAVCECTY`) in your `config.txt` file. (Alternatively you can place it under `*ALL` but some apps may crash with this due to the resources requirements)
* If you want to use this plugin on commercial games, you'll need to install [ioPlus](https://github.com/CelesteBlue-dev/PSVita-RE-tools/blob/master/ioPlus/ioPlus-0.1/release/ioplus.skprx?raw=true) as well by adding it in your `*KERNEL` section in your `config.txt`.

# Controls
* L + Select = Open the Config Menu
* L + Start = Start/Stop Recording (Shortcut)
* Triangle = Close Config Menu (when in Config Menu)

# Output Videos
The output videos can be found in `ux0:data` named as `vid_TITLEID_DATE_TIME.mjpg`.<br>
These files are raw mjpeg data and can be played with several video players such as `ffplay` or `WMP`.<br>
You can also use `ffmpeg` to convert them in more popular MP4 videos with a command like this (Note: This creates videos with fixed 25 fps):<br>
`ffmpeg -i vid_GTAVCECTY-17_04_2021-21_02_33.mjpg -pix_fmt yuv420p -b:v 4000k -c:v libx264 vid_GTAVCECTY-17_04_2021-21_02_33.mp4`

# Plans for the future
At the time of writing, the plugin is in an experimental stage. Current plan for the future is:<br>
* Add raw audio recording.
* Move to AVI container (it would allow to store RAW PCM data for the audio part and MJPEG for the video part.)
* Create a kernel plugin variant. (udcd_uvc can be used as base, the idea is to allow multiapp recordings)
* Whenever possible (mostly small homebrew apps due to the high resources requirements), stick to SceLibMp4Recorder for MP4 recording. (AVC+AAC)

# Known Issues
* Using Best or High video quality may result in empty videos being created (That's cause not enough resources are available for the encoder).

# Notes
<b>This plugin is an entry for the [KyûHEN PSVITA homebrew contest](https://kyuhen.customprotocol.com/en/).</b>

# Credits
Special thanks to the distinguished patroners for their awesome support:
- @Sarkies_Proxy
- Badmanwazzy37
- Colin VanBuren
- drd7of14
- Freddy Parra
- Max
- Tain Sueiras
- The Vita3K Project
- Titi Clash
