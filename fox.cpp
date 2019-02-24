/**
Framebuffer Overlaying dispmanX
Fernando R. Pardo (fernando.rpardo@gmail.com)

DISCLAIMERS & (C)
This code is based on 
https://github.com/raspberrypi/firmware/tree/master/opt/vc/src/hello_pi/hello_dispmanx
under Copyright (c) 2012, Broadcom Europe Ltd

You actually need to download https://github.com/raspberrypi/firmware into your system to solve headers and lib dependecies
Modify the Makefile to make SDKSTAGE point to the place where you have locally stored the 'firmware' directory

DESCRIPTION
This is not a final application but an example of how to create an overlay caption to applications using DispmanX such as OMXplayer.
If you are using Chromium browser for the graphic interface of your application, as I do, you may want to show some graphics an text 
overlaying the video being played with OMXplayer
One way could be do build a framebuffer device /dev/fb0 that is mapped to a DispmanX layer rather than the framebuffer so that you can put that layer 
on top of OMXplayer. 
The approach here is to run an appliction in the background that dumps at regular interval an area of the framebuffer into a dispmanx layer. 

The graphic interface either CLI or the ones having X11 underneath (graphic desktop, Chromium browser) do render on the framebuffer. 
Framebuffer is Dispmanx later -127 that means that anything rendering on any layer above -127 will hide your framebuffer 

USAGE:
(1) open a SSH session and play any video using OMXplayer e.g. 
omxplayer https://static.videezy.com/system/resources/previews/000/021/067/original/P1022200.mp4
(2) open another SSH session an type
fox
You should see at this point the video being played and the selected portion of the fb overlaying the video

LIMITATIONS:
Example works for dispmanx type VC_IMAGE_RGB565 (16-bits color) that is the one in hello_dispmanx. It is pending to extend this example
to 8-bits color types (VC_IMAGE_TF_RGBA32)

**/


#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <assert.h>
// frame buffer
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "bcm_host.h"

// DEFAULT VALUES
#define TIME_RESOLUTION 1000 	// 1000= time base is miliseconds
#define FOX_REFRESHRATE	50 		// miliseconds (refresh rate= FOX_REFRESHRATE x TIME_RESOLUTION microseconds)
#define WIDTH   		1920 	//1920
#define HEIGHT  		300 	//1080
#define FOX_ALPHA 		120 	// alpha
#define FOX_LAYER		200		// default dispmanx layer used by fox


typedef unsigned short DMX_PIXEL_W;	// VC_IMAGE_RGB565 (16-bits)
//typedef unsigned int DMX_PIXEL_W;	// VC_IMAGE_TF_RGBA32 (32-bits)


typedef struct
{
    DISPMANX_DISPLAY_HANDLE_T   display;
    DISPMANX_MODEINFO_T         info;
    void                       *image;
    DISPMANX_UPDATE_HANDLE_T    update;
    DISPMANX_RESOURCE_HANDLE_T  resource;
    DISPMANX_ELEMENT_HANDLE_T   element;
    uint32_t                    vc_image_ptr;
} RECT_VARS_T;

static RECT_VARS_T  gRectVars;

void fb_Info(void);

/* ----------------------------------------------------------------------------------------------------- */
/* -------------------------------------------  KEYBOARD  ---------------------------------------------- */
// restore values
struct termios term_flags;
int term_ctrl;
int termios_init()
{
	/* get the original state */
	tcgetattr(STDIN_FILENO, &term_flags);
	term_ctrl = fcntl(STDIN_FILENO, F_GETFL, 0);
	return 0;
}

int termios_restore()
{
	tcsetattr(STDIN_FILENO, TCSANOW, &term_flags);
	fcntl(STDIN_FILENO, F_SETFL, term_ctrl);
	return 0;
}

int kbhit(void)
{
	struct termios newtio, oldtio;
	int oldf;

    if (tcgetattr(STDIN_FILENO, &oldtio) < 0) /* get the original state */
        return -1;
    newtio = oldtio;
	/* echo off, canonical mode off */
    newtio.c_lflag &= ~(ECHO | ICANON );  
	tcsetattr(STDIN_FILENO, TCSANOW, &newtio);
 	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	int ch = getchar();
 	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}
 	tcsetattr(STDIN_FILENO, TCSANOW, &oldtio);
	fcntl(STDIN_FILENO, F_SETFL, oldf);
	return 0;
}

/* ----------------------------------------------------------------------------------------------------- */
/* -------------------------------------------  FOX functions ------------------------------------------ */
void fox_background(void *dmxptr, uint32_t dmx_width, uint32_t dmx_height, DMX_PIXEL_W background_color)
{
	DMX_PIXEL_W *ptr_dmx_row= (DMX_PIXEL_W *)dmxptr;
	DMX_PIXEL_W *ptr_dmx;
	for(uint32_t y= 0; y<dmx_height; y++)
	{
		ptr_dmx= ptr_dmx_row;
		for(uint32_t x=0; x<dmx_width; x++)
			*ptr_dmx++= background_color; // background color
		ptr_dmx_row += dmx_width;
	}
}

int fox_fb_copy(char *fbp, void *dmxptr, struct fb_var_screeninfo *vinfo, uint32_t fb_x0, uint32_t fb_y0, uint32_t dmx_width, uint32_t dmx_height)
{
	unsigned int FB_WIDTH= vinfo->xres;
	unsigned int FB_HEIGHT= vinfo->yres;
	unsigned int FB_BPP= vinfo->bits_per_pixel;

	unsigned char p[4];	
	p[3]= 0;
	uint32_t *ptr_fb_row = (uint32_t *) (fbp + (FB_BPP / 8) *  FB_WIDTH * (FB_HEIGHT - fb_y0 + dmx_height));
	uint32_t *ptr;	
	DMX_PIXEL_W *ptr_dmx_row = (DMX_PIXEL_W *)( (uint32_t)dmxptr + dmx_width * dmx_height * sizeof(DMX_PIXEL_W) );
	DMX_PIXEL_W *ptr_dmx;
	DMX_PIXEL_W b, g, r;
	for(uint32_t y= 0; y<dmx_height; y++)
	{
		ptr_fb_row -=  FB_WIDTH;
		ptr= ptr_fb_row + fb_x0;
		
		ptr_dmx_row -= dmx_width;
		ptr_dmx= ptr_dmx_row;			
		
		for(uint32_t x=0; x<dmx_width; x++)
		{
			// Framebuffer 00RRGGBB -> dispmanx R 5 | G 6 | B 5
			*(uint32_t*)&p= *ptr++;
			b= p[0] >> 3;
			g= p[1] >> 2;
			r= p[2] >> 3;
			*((uint32_t*) &p) &= 0x00FFFFFF;
			if(*((uint32_t*) &p) != 0x00000000) //  0x00000000 as transparent color
				*ptr_dmx= ((r <<11) & 0b1111100000000000) | ((g <<5) & 0b0000011111100000) | (b & 0b0000000000011111);
			ptr_dmx ++;
		}
	}
	return 0;
}

void fox_sync(RECT_VARS_T *vars, VC_RECT_T *dst_rect, VC_IMAGE_TYPE_T type,  uint32_t dmx_width, uint32_t dmx_height)
{
	// Set the entries in the rect structure
	vc_dispmanx_rect_set( dst_rect, 0, 0, dmx_width, dmx_height);
	// Write the bitmap data to VideoCore memory
	int	ret = vc_dispmanx_resource_write_data( vars->resource,  type,  dmx_width * sizeof(DMX_PIXEL_W),  vars->image, dst_rect );
	assert( ret == 0 );
	// Start a new update, DISPMANX_NO_HANDLE on error
	vars->update = vc_dispmanx_update_start( 10 );
	assert( vars->update );
	// Signal that a region of the bitmap has been modified
	ret = vc_dispmanx_element_modified( vars->update, vars->element, dst_rect );
	assert( ret == 0 );
	// End an update and wait for it to complete
	ret = vc_dispmanx_update_submit_sync( vars->update );
	assert( ret == 0 );	
}

/* ----------------------------------------------------------------------------------------------------- */
/* -----------------------------------------------  MAIN  ---------------------------------------------- */

void usage(char *program)
{
    fprintf(stdout, "Usage: %s ", program);
	fprintf(stdout, " [-i] [-r <refresh rate>] [-a <alpha>}");
	fprintf(stdout, " [-l <layer>}\n"); 
	fprintf(stdout, "    -a - alpha 0..255 255= opaque\n"); 
	fprintf(stdout, "    -l - layer. -127 is framebuffer\n");
	fprintf(stdout, "    -i - FB & DMX information\n"); 
}

int main(int argc, char *argv[])
{
    uint32_t        screen= 0;
    int             ret;
    VC_RECT_T       src_rect;
    VC_RECT_T       dst_rect;
    VC_IMAGE_TYPE_T type = 	VC_IMAGE_RGB565; //VC_IMAGE_TF_RGBA32; //VC_IMAGE_RGB565;
	uint32_t fox_alpha= 	FOX_ALPHA;
	int fox_refresh_rate= 	FOX_REFRESHRATE;
	int fox_layer= 			FOX_LAYER; // layer -128 is behind fb / -127 is fb and it is shown
	
	// KBD
	int ic=0;
	char c= 0;
	char prompt[]= "FOX> ";
	char command_line[200] = {0};
	
	unsigned short background_colors[]= {0xF800, 0x07E0, 0x000E};
	int ixbg= 0;
	
	// (1) Command line options
	for(int i=1; i<argc; i++)
	{
		if(argv[i][0]=='-') 
		{
			switch(argv[i][1])
			{
				case '?':
				case 'h':
						usage(argv[0]);
						exit(EXIT_FAILURE);
					break;
				case 'i':
						fb_Info();
						exit(EXIT_FAILURE);
					break;						
				case 'a': 
						if((i+1)<argc)
						{
							printf("\nalpha %d", atoi(argv[i+1]));
							fox_alpha= atoi(argv[i+1]);
							i+=1;
						}  
					break;	
				case 'r': 
						if((i+1)<argc)
						{
							printf("\nrefresh rate %d miliseconds", atoi(argv[i+1]));
							fox_refresh_rate= atoi(argv[i+1]);
							i+=1;
						}  
					break;
				case 'l': 
						if((i+1)<argc)
						{
							printf("\nlayer %d", atoi(argv[i+1]));
							fox_layer= atoi(argv[i+1]);
							i+=1;
						}  
					break;						
			}
		}
	}
	
	// (2) FRAMEBUFFER INIT
	long int screensize;	
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;	
	int fd = open("/dev/fb0", O_RDWR);
	if (fd == -1) {
		perror("ERROR: cannot open framebuffer device");
		exit(EXIT_FAILURE);
	}
	// Get fixed screen information
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("ERROR reading fixed information");
		exit(EXIT_FAILURE);
	}
	// Get variable screen information
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("ERROR reading variable information");
		exit(EXIT_FAILURE);
	}
	screensize = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
	printf("\nFramebuffer is %dx%d, %dbpp  %d bytes size", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, (int)screensize);
	// Map to memory
	char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ((int)fbp == -1) {
		perror("ERROR: failed to map framebuffer device to memory");
		exit(EXIT_FAILURE);
	}
	
	// (3) DISPMANX init
	VC_DISPMANX_ALPHA_T alpha = { (DISPMANX_FLAGS_ALPHA_T) (DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS),  fox_alpha, 0 };
	RECT_VARS_T *vars = &gRectVars;
	bcm_host_init();
	printf("\nOpen display[%i]...", screen );
	// Opens a display on device 0 (HDMI)
	vars->display = vc_dispmanx_display_open( screen );
	ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
	assert(ret == 0);
	printf( "\nDispmanX display is %dx%d", vars->info.width, vars->info.height );	

	// (4) set area to copy
    uint32_t dmx_width = WIDTH > vars->info.width? vars->info.width : WIDTH;
	uint32_t dmx_height = HEIGHT > vars->info.height? vars->info.height : HEIGHT;
	void *pdmx= vars->image = calloc( 1, dmx_width  * dmx_height * sizeof(DMX_PIXEL_W));
	assert(vars->image);
	// FRAMEBUFFER 0,0 is bottom-left corner
	uint32_t fb_x0= 0;
	uint32_t fb_y0= dmx_height;	
	// DMX 0,0 is top-left corner
	uint32_t dmx_x0= 0; // to the left
	uint32_t dmx_y0= (vars->info.height - dmx_height );		// show at the bottom
	printf( "\nCOPY AREA x,y  %d,%d   %dx%d\n", dmx_x0, dmx_y0, dmx_width, dmx_height);
	
	// (5) Init
	fox_background(vars->image, dmx_width, dmx_height, 0xf100);
	fox_fb_copy(fbp, vars->image, &vinfo, fb_x0, fb_y0, dmx_width, dmx_height);

	// (6) DispmanX stuff
	// Create a new resource
    vars->resource = vc_dispmanx_resource_create( type,  dmx_width, dmx_height, &vars->vc_image_ptr );
    assert( vars->resource );
	// Set the entries in the rect structure
    vc_dispmanx_rect_set( &dst_rect, 0, 0, dmx_width, dmx_height);
	// Write the bitmap data to VideoCore memory
    ret = vc_dispmanx_resource_write_data(  vars->resource,  type,  dmx_width * sizeof(DMX_PIXEL_W),  vars->image, &dst_rect );
    assert( ret == 0 );
	// Start a new update, DISPMANX_NO_HANDLE on error
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );
	// Set the entries in the rect structure
    vc_dispmanx_rect_set( &src_rect, 0, 0, dmx_width << 16, dmx_height << 16 );
	// Set the entries in the rect structure
	// set dmx position where allocated memory is dumped
    vc_dispmanx_rect_set( &dst_rect, dmx_x0,  dmx_y0, dmx_width,  dmx_height );
	// Add an elment to a display as part of an update
    vars->element = vc_dispmanx_element_add(    (DISPMANX_UPDATE_HANDLE_T )vars->update,
                                                (DISPMANX_DISPLAY_HANDLE_T)vars->display,
                                                (int32_t) fox_layer,               
                                                (const VC_RECT_T *) &dst_rect,
                                                (DISPMANX_RESOURCE_HANDLE_T)vars->resource,
                                                (const VC_RECT_T *)&src_rect,
                                                DISPMANX_PROTECTION_NONE,
                                                &alpha,
                                                (DISPMANX_CLAMP_T*) NULL, // clamp
                                                (DISPMANX_TRANSFORM_T ) VC_IMAGE_ROT0 );
	// End an update and wait for it to complete
    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );

	// (7) Enter loop until ESC is pressed	
	printf (prompt);
	termios_init();
	bool terminate= false;
	unsigned int loop_count= fox_refresh_rate;
	while (!terminate)
	{
		while(!kbhit()) 
		{
			usleep(TIME_RESOLUTION);
			if(--loop_count<=0)
			{
				loop_count= fox_refresh_rate;
				memset(pdmx, 0, dmx_width  * dmx_height * sizeof(DMX_PIXEL_W) );				
				fox_fb_copy(fbp, vars->image, &vinfo, fb_x0, fb_y0, dmx_width, dmx_height);
				fox_sync(vars, &dst_rect, type, dmx_width, dmx_height);
			}
		}
		
		switch(c = getchar()) 
		{
			case '\n': 
			{
				printf("%c", c); // Echo
				// DO SOMETHING with your command
				// You have a command in command_line
				if(strlen(command_line)) printf("Command line is '%s'\n", command_line);
				// (End of DO SOMETHING)
				ic=0;
				command_line[0]= '\0';						
				printf (prompt);
			}
			break;
			case '\b':
			case 127:
			{
				if(ic>0) 
				{
					printf("\b \b"); // Backspace
					command_line[--ic]= '\0';
				}
			}
			break;
			// ESCAPE
			case 27:
			{
				// "throw away" next two characters which specify escape sequence
				char c1=0;
				char c2=0;
				if(kbhit()) 
				{
					c1 = getchar();
					if(kbhit()) c2 = getchar();
				}
				switch(c2) 
				{
					// Simple ESC key -> terminate
					case 0:
						termios_restore();
						terminate= true;
					break;
					// UP-arrow key
					case 65:
					// DOWN-arrow key
					case 66:
					break;
					// RIGHT-arrow key
					case 67:
					{
						printf (prompt);
						printf("color =  0x%04X\n",  background_colors[ixbg]);
						fox_background(vars->image, dmx_width, dmx_height, background_colors[ixbg]);
						ixbg = (ixbg + 1) % (sizeof(background_colors)/sizeof(unsigned short));	
						fox_fb_copy(fbp, vars->image, &vinfo, fb_x0, fb_y0, dmx_width, dmx_height);
						fox_sync(vars, &dst_rect, type, dmx_width, dmx_height);
					}
					break;
					// LEFT-arrow key
					case 68:		 
					break;
					default: 
						printf("DEFAULT %d\n", (int)c1); 
						printf (prompt);
				}
			}
			break;			
			default: 
			{
				printf("%c", c); // Echo
				command_line[ic++]= c;
				command_line[ic]= '\0';
			}
		}
	} 	

	// (8) Terminate
    printf( "Program terminated...\n" );
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );
	// Remove a display element from its display
    ret = vc_dispmanx_element_remove( vars->update, vars->element );
    assert( ret == 0 );
    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );
    ret = vc_dispmanx_resource_delete( vars->resource );
    assert( ret == 0 );
    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );

	// (9) Free resources
	if(pdmx) free(pdmx);
	// frame buffer
	if(fbp) munmap(fbp, screensize);
	if(fd) close(fd);
    return 0;
} // main()





void fb_Info(void)
{
	// Frame buffer
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int fd = open("/dev/fb0", O_RDWR);
	if (fd == -1) {
		perror("ERROR: cannot open framebuffer device");
		exit(EXIT_FAILURE);
	}
	// Get fixed screen information
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("ERROR reading fixed information");
		exit(EXIT_FAILURE);
	}
	// Get variable screen information
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("ERROR reading variable information");
		exit(EXIT_FAILURE);
	}
	printf("\nFRAMEBUFFER %dx%d, %dbpp", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	printf("\n");
	close(fd);
	
	// DispmanX
	RECT_VARS_T *vars;
	vars = &gRectVars;
	bcm_host_init();
	uint32_t screen = 0;
	printf("DISPMANX display[%i]...\n", screen );
	vars->display = vc_dispmanx_display_open( screen );
	int ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
	assert(ret == 0);
	printf( "DISPMANX is %d x %d\n", vars->info.width, vars->info.height );	
    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );	
	printf("\n\n");
}

// END OF FILE
