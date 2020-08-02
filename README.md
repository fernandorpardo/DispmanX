# fox

Framebuffer overlaying DispmanX.

A simple example for Raspberry Pi

### DISCLAIMERS & (C)

This code is based on [hello_dispmanx](https://github.com/raspberrypi/firmware/tree/master/opt/vc/src/hello_pi/hello_dispmanx)
under Copyright (c) 2012, Broadcom Europe Ltd

You actually need to download https://github.com/raspberrypi/firmware into your system to solve headers and lib dependencies.

Modify the Makefile to make SDKSTAGE point to the place where you have locally stored the 'firmware' directory.


### DESCRIPTION

This is not a final application but an example for Raspberry Pi of how to create an overlay caption to applications using DispmanX such as OMXplayer.

If you are using the Chromium browser for the graphic interface of your application, as I do, you may want to use it to generate some graphics and text overlaying the video being played with OMXplayer.

The graphic interface, either CLI or the ones having X11 underneath (graphic desktop, Chromium browser), do render on the framebuffer. Framebuffer is DispmanX layer -127, this means that anything rendering on any layer above -127 will hide your application. 

One way to keep showing your application while the video is being played could be to build a framebuffer device /dev/fb0 mapped to a DispmanX layer above the video rather than to the framebuffer memory.

The approach here is to run an process in the background that dumps at regular intervals an area of the framebuffer into a DispmanX layer above the one used by OMXplayer. 


### USAGE

From three different SSH sessions do the following:

SSH#1- dump this caption.png image into the fb using fbi (Linux framebuffer imageviewer)
```console
$ sudo fbi -a -noverbose -norandom -T 1 -t 8 caption.png
```
Now you see a black screen with the caption

![caption](https://user-images.githubusercontent.com/38065602/74604623-0b322500-50c0-11ea-8a21-fce1e08672e4.png)

SSH#2- play a video in loop, e.g.,
```console
$ omxplayer –loop https://static.videezy.com/system/resources/previews/000/021/067/original/P1022200.mp4
```
Now you see the video but not the caption. The caption is still there but hidden by layer 0 where OMXplayer dumps the video frames (remember the caption.png image is in layer -127)

SSH#3 - run fox
```console
$ fox
```

At this point, you should see the video with the caption (the selected portion of the fb overlaying the video)
![c](https://user-images.githubusercontent.com/38065602/74604634-2a30b700-50c0-11ea-9684-326d28c97b0c.png)


### LIMITATIONS

This example works for DispmanX type VC_IMAGE_RGB565 (16-bits color) that is the one in hello_dispmanx. A TODO task is to extend this example to 32-bits color types (VC_IMAGE_TF_RGBA32)


