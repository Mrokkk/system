#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>

#define DEFAULT_FB_PATH "/dev/fb0"

[[noreturn]] void die(const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", program_invocation_short_name);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    putc('\n', stderr);

    exit(EXIT_FAILURE);
}

static const char* fbvisual_string(int visual)
{
    switch (visual)
    {
        case FB_VISUAL_MONO01:             return "Mono 01";
        case FB_VISUAL_MONO10:             return "Mono 10";
        case FB_VISUAL_TRUECOLOR:          return "True color";
        case FB_VISUAL_PSEUDOCOLOR:        return "Pseudo color";
        case FB_VISUAL_DIRECTCOLOR:        return "Direct color";
        case FB_VISUAL_STATIC_PSEUDOCOLOR: return "Pseudo color readonly";
        default:                           return "Unknown";
    }
}

static const char* fbtype_string(int type)
{
    switch (type)
    {
        case FB_TYPE_PACKED_PIXELS:      return "packed pixels";
        case FB_TYPE_TEXT:               return "text";
        case FB_TYPE_INTERLEAVED_PLANES: return "interleaved planes";
        case FB_TYPE_PLANES:             return "planes";
        case FB_TYPE_VGA_PLANES:         return "VGA planes";
        default:                         return "unknown";
    }
}

[[noreturn]] static void usage()
{
    fprintf(stderr, "Usage: %s [-f FBDEV] [-h] [RESX RESY BPP]\n", program_invocation_short_name);
    exit(0);
}

int main(int argc, char** argv)
{
    int c;
    int params[3] = {};
    size_t id = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    const char* fbdev = DEFAULT_FB_PATH;

    while ((c = getopt(argc, argv, "-f:h")) != -1)
    {
        switch (c)
        {
            case 1:
                if (id == 3)
                {
                    die("unexpected parameter -- %s", optarg);
                }
                params[id++] = strtoul(optarg, NULL, 0);
                break;
            case 'f':
                fbdev = optarg;
                break;
            case '?':
                exit(EXIT_FAILURE);
            case -1:
                break;
            case 'h':
                usage();
        }
    }

    int fb_fd = open(fbdev, O_RDWR);

    if (fb_fd == -1)
    {
        die("%s: cannot open: %s", fbdev, strerror(errno));
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo))
    {
        die("%s: cannot read framebuffer info: %s", fbdev, strerror(errno));
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        die("%s: cannot read current mode: %s", fbdev, strerror(errno));
    }

    if (id == 0)
    {
        goto show;
    }

    if (id < 3)
    {
        die("missing parameters");
    }

    vinfo.xres = params[0];
    vinfo.yres = params[1];
    vinfo.bits_per_pixel = params[2];

    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo))
    {
        die("%s: cannot set mode %ux%ux%u: %s", fbdev, params[0], params[1], params[2], strerror(errno));
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo))
    {
        die("%s: cannot read framebuffer info: %s", fbdev, strerror(errno));
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        die("%s: cannot read current mode: %s", fbdev, strerror(errno));
    }

show:
    printf("device: %s\n  id: %s\n  mode: %zux%zux%zu\n  type: %s %s\n  accel: %s\n",
        fbdev,
        finfo.id,
        vinfo.xres,
        vinfo.yres,
        vinfo.bits_per_pixel,
        fbvisual_string(finfo.visual),
        fbtype_string(finfo.type),
        finfo.accel ? "true" : "false");

    return 0;
}
