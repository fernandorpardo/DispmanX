## fox

Framebuffer overlaying DispmanX
A simple example for Raspberry Pi

### DISCLAIMERS & (C)

This code is based on 
https://github.com/raspberrypi/firmware/tree/master/opt/vc/src/hello_pi/hello_dispmanx
under Copyright (c) 2012, Broadcom Europe Ltd

You actually need to download https://github.com/raspberrypi/firmware into your system to solve headers and lib dependecies
Modify the Makefile to make SDKSTAGE point to the place where you have locally stored the 'firmware' directory


### DESCRIPTION

This is not a final application but an example for Raspberry Pi of how to create an overlay caption to applications using DispmanX such as OMXplayer.
If you are using Chromium browser for the graphic interface of your application, as I do, you may want to show some graphics an text 
overlaying the video being played with OMXplayer
One way could be do build a framebuffer device /dev/fb0 that is mapped to a DispmanX layer rather than the framebuffer so that you can put that layer 
on top of OMXplayer. 
The approach here is to run an appliction in the background that dumps at regular interval an area of the framebuffer into a dispmanx layer. 

The graphic interface either CLI or the ones having X11 underneath (graphic desktop, Chromium browser) do render on the framebuffer. 
Framebuffer is Dispmanx later -127 that means that anything rendering on any layer above -127 will hide your framebuffer 


### USAGE

(1) open a SSH session and play any video using OMXplayer e.g. 
omxplayer https://static.videezy.com/system/resources/previews/000/021/067/original/P1022200.mp4
(2) open another SSH session an type
fox
You should see at this point the video being played and the selected portion of the fb overlaying the video


### LIMITATIONS

Example works for dispmanx type VC_IMAGE_RGB565 (16-bits color) that is the one in hello_dispmanx. It is pending to extend this example
to 8-bits color types (VC_IMAGE_TF_RGBA32)


