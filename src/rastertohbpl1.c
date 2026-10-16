/*
 * rastertohbpl1 - CUPS raster to HBPLv1 filter for the Dell C1760nw and
 * other HBPLv1 printers (Dell 1250c/C1660w, Xerox Phaser 6010,
 * Fuji Xerox DocuPrint CP105b/CP205, Epson AcuLaser C1700).
 *
 * The HBPLv1 encoder below (putbits ... encode_page) is taken unchanged
 * from foo2hbpl1.c, written by Dave Coffin in March 2014, as distributed
 * in https://github.com/mikerr/foo2zjs. Only the input side differs: this
 * program reads CUPS raster (as produced by macOS's own PDF renderer)
 * instead of Ghostscript PBM/PGM/PPM/PAM output.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/utsname.h>
#include <cups/cups.h>
#include <cups/raster.h>

int	MediaCode = 0;
char	*Username = NULL;
char	*Filename = NULL;
int	Clip[] = { 8,8,8,8 };

void
error(int fatal, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (fatal) exit(fatal);
}

struct stream
{
    unsigned char *buf;
    int size, off, bits;
};

void
putbits(struct stream *s, unsigned val, int nbits)
{
    if (s->off + 16 > s->size &&
	!(s->buf = realloc(s->buf, s->size += 0x100000)))
	    error (1, "Out of memory\n");
    if (s->bits)
    {
	s->off--;
	val |= s->buf[s->off] >> (8-s->bits) << nbits;
	nbits += s->bits;
    }
    s->bits = nbits & 7;
    while ((nbits -= 8) > 0)
	s->buf[s->off++] = val >> nbits;
    s->buf[s->off++] = val << -nbits;
}

/*
   Runlengths are integers between 1 and 17057 encoded as follows:

	1	00
	2	01 0
	3	01 1
	4	100 0
	5	100 1
	6	101 00
	7	101 01
	8	101 10
	9	101 11
	10	110 0000
	11	110 0001
	12	110 0010
	   ...
	25	110 1111
	26	111 000 000
	27	111 000 001
	28	111 000 010
	29	111 000 011
	   ...
	33	111 000 111
	34	111 001 000
	   ...
	41	111 001 111
	42	111 010 000
	50	111 011 0000
	66	111 100 00000
	98	111 101 000000
	162	111 110 000000000
	674	111 111 00000000000000
	17057	111 111 11111111111111
*/
void
put_len(struct stream *s, unsigned val)
{
    unsigned code[] =
    {
	  1, 0, 2,
	  2, 2, 3,
	  4, 8, 4,
	  6, 0x14, 5,
	 10, 0x60, 7,
	 26, 0x1c0, 9,
	 50, 0x3b0, 10,
	 66, 0x780, 11,
	 98, 0xf40, 12,
	162, 0x7c00, 15,
	674, 0xfc000, 20,
	17058
    };
    int c = 0;

    if (val < 1 || val > 17057) return;
    while (val >= code[c+3]) c += 3;
    putbits(s, val-code[c] + code[c+1], code[c+2]);
}

/*
   CMYK byte differences are encoded as follows:

	 0	000
	+1	001
	-1	010
	 2	011s0	s = 0 for +, 1 for -
	 3	011s1
	 4	100s00
	 5	100s01
	 6	100s10
	 7	100s11
	 8	101s000
	 9	101s001
	    ...
	 14	101s110
	 15	101s111
	 16	110s00000
	 17	110s00001
	 18	110s00010
	    ...
	 46	110s11110
	 47	110s11111
	 48	1110s00000
	 49	1110s00001
	    ...
	 78	1110s11110
	 79	1110s11111
	 80	1111s000000
	 81	1111s000001
	    ...
	 126	1111s101110
	 127	1111s101111
	 128	11111110000
*/
void
put_diff(struct stream *s, signed char val)
{
    static unsigned short code[] =
    {
	 2,  3, 3, 1,
	 4,  4, 3, 2,
	 8,  5, 3, 3,
	16,  6, 3, 5,
	48, 14, 4, 5,
	80, 15, 4, 6,
	129
    };
    int sign, abs, c = 0;

    switch (val)
    {
    case  0:  putbits(s, 0, 3);  return;
    case  1:  putbits(s, 1, 3);  return;
    case -1:  putbits(s, 2, 3);  return;
    }
    abs = ((sign = val < 0)) ? -val:val;
    while (abs >= code[c+4]) c += 4;
    putbits(s, code[c+1], code[c+2]);
    putbits(s, sign, 1);
    putbits(s, abs-code[c], code[c+3]);
}

void
setle(unsigned char *c, int s, int i)
{
    while (s--)
    {
	*c++ = i;
	i >>= 8;
    }
}

void
start_doc(int color)
{
    char reca[] = { 0x41,0x81,0xa1,0x00,0x82,0xa2,0x07,0x00,0x83,0xa2,0x01,0x00 };
    time_t t;
    struct tm *tmp;
    char datestr[16], timestr[16];
    char cname[128] = "My Computer";
    char *mname[] =
    {	"",
	"NORMAL",
	"THICK",
	"HIGHQUALITY",
	"COAT2",
	"LABEL",
	"ENVELOPE",
	"RECYCLED",
	"NORMALREV",
	"THICKSIDE2",
	"HIGHQUALITYREV",
	"COATEDPAPER2REV",
	"RECYCLEREV",
    };

    t = time(NULL);
    tmp = localtime(&t);
    strftime(datestr, sizeof datestr, "%m/%d/%Y", tmp);
    strftime(timestr, sizeof timestr, "%H:%M:%S", tmp);

    #if 1
    {
	struct utsname u;

	uname(&u);
	strncpy(cname, u.nodename, 128);
	cname[127] = 0;
    }
    #endif

/* Lines end with \n, not \r\n */

    printf(
	"\033%%-12345X@PJL SET STRINGCODESET=UTF8\n"
	"@PJL COMMENT DATE=%s\n"
	"@PJL COMMENT TIME=%s\n"
	"@PJL COMMENT DNAME=%s\n"
	"@PJL JOB MODE=PRINTER\n"
	"@PJL SET JOBATTR=\"@LUNA=%s\"\n"
	"@PJL SET JOBATTR=\"@TRCH=OFF\"\n"
	"@PJL SET DUPLEX=OFF\n"
	"@PJL SET BINDING=LONGEDGE\n"
	"@PJL SET IWAMANUALDUP=OFF\n"
	"@PJL SET JOBATTR=\"@MSIP=%s\"\n"
	"@PJL SET RENDERMODE=%s\n"
	"@PJL SET ECONOMODE=OFF\n"
	"@PJL SET RET=ON\n"
	"@PJL SET JOBATTR=\"@IREC=OFF\"\n"
	"@PJL SET JOBATTR=\"@TRAP=ON\"\n"
	"@PJL SET JOBATTR=\"@JOAU=%s\"\n"
	"@PJL SET JOBATTR=\"@CNAM=%s\"\n"
	"@PJL SET COPIES=1\n"
	"@PJL SET QTY=1\n"
	"@PJL SET PAPERDIRECTION=SEF\n"
	"@PJL SET RESOLUTION=600\n"
	"@PJL SET BITSPERPIXEL=8\n"
	"@PJL SET JOBATTR=\"@DRDM=XRC\"\n"
	"@PJL SET JOBATTR=\"@TSCR=11\"\n"
	"@PJL SET JOBATTR=\"@GSCR=11\"\n"
	"@PJL SET JOBATTR=\"@ISCR=12\"\n"
	"@PJL SET JOBATTR=\"@TTRC=11\"\n"
	"@PJL SET JOBATTR=\"@GTRC=11\"\n"
	"@PJL SET JOBATTR=\"@ITRC=12\"\n"
	"@PJL SET JOBATTR=\"@TCPR=11\"\n"
	"@PJL SET JOBATTR=\"@GCPR=11\"\n"
	"@PJL SET JOBATTR=\"@ICPR=12\"\n"
	"@PJL SET JOBATTR=\"@TUCR=11\"\n"
	"@PJL SET JOBATTR=\"@GUCR=11\"\n"
	"@PJL SET JOBATTR=\"@IUCR=12\"\n"
	"@PJL SET JOBATTR=\"@BSPM=OFF\"\n"
	"@PJL SET JOBATTR=\"@TDFT=0\"\n"
	"@PJL SET JOBATTR=\"@GDFT=0\"\n"
	"@PJL SET JOBATTR=\"@IDFT=0\"\n"
	"@PJL ENTER LANGUAGE=HBPL\n"
	, datestr, timestr
	, Filename ? Filename : ""
	, Username ? Username : ""
	, mname[MediaCode]
	, color ? "COLOR" : "GRAYSCALE"
	, Username ? Username : ""
	, cname);
    fwrite (reca, 1, sizeof reca, stdout);
}

#define IP (((int *)image) + off)
#define CP (((char *)image) + off)
#define DP (((char *)image) + off*deep)
#define BP(x) ((blank[(off+x) >> 3] << ((off+x) & 7)) & 128)
#define put_token(s,x) putbits(s, huff[hsel][x] >> 4, huff[hsel][x] & 15)

void
encode_page(int color, int width, int height, char *image)
{
    unsigned char head[90] =
    {
	0x43,0x91,0xa1,0x00,0x92,0xa1,0x01,0x93,0xa1,0x01,0x94,0xa1,
	0x00,0x95,0xc2,0x00,0x00,0x00,0x00,0x96,0xa1,0x00,0x97,0xc3,
	0x00,0x00,0x00,0x00,0x98,0xa1,0x00,0x99,0xa4,0x01,0x00,0x00,
	0x00,0x9a,0xc4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x9b,
	0xa1,0x00,0x9c,0xa1,0x01,0x9d,0xa1,0x00,0x9e,0xa1,0x02,0x9f,
	0xa1,0x05,0xa0,0xa1,0x08,0xa1,0xa1,0x00,0xa2,0xc4,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x51,0x52,0xa3,0xa1,0x00,0xa4,
	0xb1,0xa4
    };
    unsigned char body[52] =
    {
	0x20,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x10,0x32,0x04,0x00,
	0xa1,0x42,0x00,0x00,0x00,0x00,0xff
    };
    static short papers[] =
    {	// Official sizes to nearest 1/600 inch
	// will accept +-1.5mm (35/600 inch) tolerance
	  0, 5100, 6600,	// Letter
	  2, 5100, 8400,	// Legal
	  4, 4961, 7016,	// A4
	  6, 4350, 6300,	// Executive
	 13, 2475, 5700,	// #10 envelope
	 15, 2325, 4500,	// Monarch envelope
	 17, 3827, 5409,	// C5 envelope
	 19, 2599, 5197,	// DL envelope
//	 ??, 4158, 5906,	// B5 ISO
	 22, 4299, 6071,	// B5 JIS
	 30, 3496, 4961,	// A5
	410, 5100, 7800,	// Folio
    };
    static const unsigned short huff[2][8] =
    {
	{ 0x01,0x63,0x1c5,0x1d5,0x1e5,0x22,0x3e6 }, // for text & graphics
	{ 0x22,0x63,0x1c5,0x1d5,0x1e5,0x01,0x3e6 }, // for images
    };
    unsigned char *blank;
    static int pagenum = 0;
    struct stream stream[5] = { { 0 } };
    int dirs[] = { -1,0,-1,1,2 }, rotor[] = { 0,1,2,3,4 };
    int i, j, row, col, deep, dir, run, try, bdir, brun, total;
    int paper = 510, hsel = 0, off = 0, bit = 0, stat = 0;
    int margin = width-96;

    for (i = 0; i < sizeof papers / sizeof *papers; i += 3)
	if (abs(width-papers[i+1]) < 36 && abs(height-papers[i+2]) < 36)
	    paper = papers[i];
    if (!MediaCode)
	MediaCode = paper & 1 ? 6 : 1;
    if (!pagenum)
	start_doc(color);
    head[12] = paper >> 1;
    if (paper == 510)
    {
	setle (head+15, 2,  (width*254+300)/600);  // units of 0.1mm
	setle (head+17, 2, (height*254+300)/600);
	head[21] = 2;
    }
    width = -(-width & -8);
    setle (head+33, 4, ++pagenum);
    setle (head+39, 4, width);
    setle (head+43, 4, height);
    setle (head+70, 4, width);
    setle (head+74, 4, height);
    head[55] = 9 + color*130;
    if (color)	body[6] = 1;
    else	body[4] = 8;

    deep = 1 + color*3;
    for (i=1; i < 5; i++)
	dirs[i] -= width;
    if (!color) dirs[4] = -8;

    blank = calloc(height+2, width/8);
    memset (blank++, -color, width/8+1);
    for (row = 1; row <= height; row++)
    {
	for (col = deep; col < deep*2; col++)
	    image[row*width*deep + col] = -1;
	for (col = 8; col < width*deep; col += 4)
	    if (*(int *)(image + row*width*deep + col))
	    {
		for (col = 12; col < margin/8; col++)
		    blank[row*(width/8)+col] = -1;
		blank[row*(width/8)+col] = -2 << (~margin & 7);
		break;
	    }
    }
    memset (image, -color, (width+1)*deep);
    image += (width+1)*deep;
    blank += width/8;

    while (off < width * height)
    {
	for (bdir = brun = dir = 0; dir < 5; dir++)
	{
	    try = dirs[rotor[dir]];
	    for (run = 0; run < 17057; run++, try++)
	    {
		if (color)
		{
		    if (IP[run] != IP[try]) break;
		}
		else
		    if (CP[run] != CP[try]) break;

		if (BP(run) != BP(try)) break;
	    }
	    if (run > brun)
	    {
		bdir = dir;
		brun = run;
	    }
	}
	if (brun == 0)
	{
	    put_token(stream, 5);
	    for (i = 0; i < deep; i++)
		put_diff(stream+1+i, DP[i] - DP[i-deep]);
	    bit = 0;
	    off++;
	    stat--;
	    continue;
	}
	if (brun > width * height - off)
	    brun = width * height - off;
	if (bdir)
	{
	    j = rotor[bdir];
	    for (i = bdir; i; i--)
		rotor[i] = rotor[i-1];
	    rotor[0] = j;
	}
	if ((off-1+brun)/width != (off-1)/width)
	{
	    if (abs(stat) > 8 && ((stat >> 31) & 1) != hsel)
	    {
		hsel ^= 1;
		put_token(stream, 6);
	    }
	    stat = 0;
	}
	stat += bdir == bit;
	put_token(stream, bdir - bit);
	put_len(stream, brun);
	bit = brun < 17057;
	off += brun;
    }

    putbits(stream, 0xff, 8);
    for (total = 48, i = 0; i <= deep; i++)
    {
	putbits(stream+i, 0xff, 8);
	stream[i].off--;
	setle (body+32 + i*4, 4, stream[i].off);
	total += stream[i].off;
    }
    head[85] = 0xa2 + (total > 0xffff)*2;
    setle (head+86, 4, total);
    fwrite(head, 1, 88+(total > 0xffff)*2, stdout);
    fwrite(body, 1, 48, stdout);
    for (i = 0; i <= deep; i++)
    {
	fwrite(stream[i].buf, 1, stream[i].off, stdout);
	free(stream[i].buf);
    }
    free(blank-width/8-1);
    printf("SD");
}
#undef IP
#undef CP
#undef DP
#undef BP
#undef put_token

/*
 * CUPS raster input. Each page is converted into the same buffer layout
 * foo2hbpl1's do_file() builds from PBM/PGM/PPM/PAM input, then handed
 * to encode_page() unchanged.
 */

static int
media_code(const char *type)
{
    static const char *names[] =
    {	"", "Plain", "Bond", "LWCard", "LWGCard", "Labels",
	"Envelope", "Recycled", "Plain2", "Bond2", "LWCard2",
	"LWGCard2", "Recycled2"
    };
    int i;

    for (i = 1; i < (int) (sizeof names / sizeof *names); i++)
	if (!strcasecmp(type, names[i]))
	    return i;
    return 0;
}

static int
do_page(cups_raster_t *ras, cups_page_header2_t *h)
{
    int rwide = h->cupsWidth, rhigh = h->cupsHeight;
    int iwide, ihigh, xoff = 0, yoff = 0;
    int ideep, deep, wide, byte, row, col, i, k;
    unsigned char *image, *line, *sp, *dp;
    double pw, ph;

    if (h->cupsBitsPerColor != 8 || h->cupsColorOrder != CUPS_ORDER_CHUNKED)
    {
	fprintf(stderr, "ERROR: Unsupported raster format (%u bits, order %d)\n",
		h->cupsBitsPerColor, h->cupsColorOrder);
	return 0;
    }
    switch (h->cupsColorSpace)
    {
    case CUPS_CSPACE_W:
    case CUPS_CSPACE_SW:
    case CUPS_CSPACE_K:
	ideep = 1;
	break;
    case CUPS_CSPACE_RGB:
    case CUPS_CSPACE_SRGB:
    case CUPS_CSPACE_ADOBERGB:
	ideep = 3;
	break;
    case CUPS_CSPACE_CMYK:
	ideep = 4;
	break;
    default:
	fprintf(stderr, "ERROR: Unsupported color space %d\n", h->cupsColorSpace);
	return 0;
    }
    if (rwide <= 0 || rhigh <= 0 || h->cupsBytesPerLine < (unsigned) (rwide * ideep))
	return 0;
    if (h->HWResolution[0] != 600 || h->HWResolution[1] != 600)
	fprintf(stderr, "WARNING: Resolution is %ux%u, printer expects 600x600\n",
		h->HWResolution[0], h->HWResolution[1]);

    /*
     * The encoder needs the whole sheet, but the raster may only cover the
     * imageable area. Place it on a full page using the page geometry.
     */
    pw = h->cupsPageSize[0] > 0 ? h->cupsPageSize[0] : h->PageSize[0];
    ph = h->cupsPageSize[1] > 0 ? h->cupsPageSize[1] : h->PageSize[1];
    iwide = (int) (pw * h->HWResolution[0] / 72 + 0.5);
    ihigh = (int) (ph * h->HWResolution[1] / 72 + 0.5);
    if (h->ImagingBoundingBox[2] > h->ImagingBoundingBox[0] &&
	h->ImagingBoundingBox[3] > h->ImagingBoundingBox[1])
    {
	xoff = (int) (h->ImagingBoundingBox[0] * h->HWResolution[0] / 72.0 + 0.5);
	yoff = (int) ((ph - h->ImagingBoundingBox[3]) * h->HWResolution[1] / 72 + 0.5);
	if (yoff < 0) yoff = 0;
    }
    if (iwide < xoff + rwide) iwide = xoff + rwide;
    if (ihigh < yoff + rhigh) ihigh = yoff + rhigh;

    deep = ideep == 1 ? 1 : 4;
    wide = -(-iwide & -8);
    byte = wide * deep;
    image = calloc(2*ihigh+2, byte);
    line = malloc(h->cupsBytesPerLine);
    if (!image || !line)
	error(1, "ERROR: Out of memory\n");

    for (row = 1; row <= ihigh; row++)
    {
	if (row <= yoff || row > yoff + rhigh)
	    goto clip;		/* outside the raster: leave blank */
	if (cupsRasterReadPixels(ras, line, h->cupsBytesPerLine) == 0)
	    memset(line, h->cupsColorSpace == CUPS_CSPACE_K
			 || ideep == 4 ? 0 : 255, h->cupsBytesPerLine);
	sp = line;
	dp = image + row*byte + xoff*deep;
	for (col = 0; col < rwide; col++)
	{
	    dp += deep;
	    switch (ideep)
	    {
	    case 1:
		*dp = h->cupsColorSpace == CUPS_CSPACE_K ? *sp : ~*sp;
		break;
	    case 3:
		for (k = sp[2], i = 0; i < 2; i++)
		    if (k < sp[i]) k = sp[i];
		*dp = ~k;
		for (i = 0; i < 3; i++)
		    dp[i+1] = k ? (k - sp[i]) * 255 / k : 255;
		break;
	    case 4:
		for (i=0; i < 4; i++)
		    dp[i] = sp[((i-1) & 3)];
		break;
	    }
	    sp += ideep;
	}
clip:
	for (i = 0; i < deep*Clip[0]; i++)
	    image[row*byte + deep+i] = 0;
	for (i = deep*(iwide-Clip[2]); i < byte; i++)
	    image[row*byte + deep+i] = 0;
    }
    memset(image+deep, 0, byte*(Clip[1]+1));
    memset(image+deep + byte*(ihigh-Clip[3]+1), 0, byte*Clip[3]);

    if (!MediaCode && h->MediaType[0])
	MediaCode = media_code(h->MediaType);
    encode_page(deep > 1, iwide, ihigh, (char *) image);
    free(image);
    free(line);
    return 1;
}

int
main(int argc, char *argv[])
{
    cups_raster_t *ras;
    cups_page_header2_t h;
    int fd = 0, pages = 0;

    if (argc < 6 || argc > 7)
    {
	fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
		argv[0]);
	return 1;
    }
    if (argv[2][0]) Username = argv[2];
    if (argv[3][0]) Filename = argv[3];
    if (argc == 7 && (fd = open(argv[6], O_RDONLY)) < 0)
    {
	perror("ERROR: Unable to open raster file");
	return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);
    while (cupsRasterReadHeader2(ras, &h))
    {
	fprintf(stderr, "INFO: Printing page %d\n", pages + 1);
	if (!do_page(ras, &h))
	    break;
	fprintf(stderr, "PAGE: %d 1\n", ++pages);
    }
    cupsRasterClose(ras);
    if (fd) close(fd);

    if (pages)
	puts("B\033%-12345X@PJL EOJ");
    fflush(stdout);
    fprintf(stderr, pages ? "INFO: Ready to print.\n" : "ERROR: No pages found!\n");
    return pages ? 0 : 1;
}
