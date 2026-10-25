// whole render_fire (draw + in-place blur): old vs new (clipped blend + row skip), many frames of sparse fire
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
typedef unsigned int pixel;
#define PIXRGB(r,g,b) (((r)<<16)|((g)<<8)|(b))
#define PIXR(x) (((x)>>16)&0xFF)
#define PIXG(x) (((x)>>8)&0xFF)
#define PIXB(x) ((x)&0xFF)
const int CELL = 4, XRES = 612, YRES = 384, VIDXRES = 646, VIDYRES = 424;
struct St { pixel vid[VIDXRES*VIDYRES]; unsigned char fire_r[YRES/CELL][XRES/CELL], fire_g[YRES/CELL][XRES/CELL], fire_b[YRES/CELL][XRES/CELL]; };
static St A, B;
static unsigned int fire_alpha[CELL*3][CELL*3];
static bool findingElement;
static void addpixel(pixel *vid, int x, int y, int r, int g, int b, int a)
{
	if (x < 0 || y < 0 || x >= VIDXRES || y >= VIDYRES) return;
	pixel t = vid[y*VIDXRES+x];
	r = (a*r + 255*PIXR(t)) >> 8; g = (a*g + 255*PIXG(t)) >> 8; b = (a*b + 255*PIXB(t)) >> 8;
	if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	vid[y*VIDXRES+x] = PIXRGB(r, g, b);
}
#define BLUR \
			r *= 8; g *= 8; b *= 8; \
			for (y=-1; y<2; y++) for (x=-1; x<2; x++) \
				if ((x || y) && i+x>=0 && j+y>=0 && i+x<XRES/CELL && j+y<YRES/CELL) \
				{ r += s.fire_r[j+y][i+x]; g += s.fire_g[j+y][i+x]; b += s.fire_b[j+y][i+x]; } \
			r /= 16; g /= 16; b /= 16; \
			s.fire_r[j][i] = r>4 ? r-4 : 0; s.fire_g[j][i] = g>4 ? g-4 : 0; s.fire_b[j][i] = b>4 ? b-4 : 0;
static void oldFire(St &s)
{
	int i,j,x,y,r,g,b,a;
	for (j=0; j<YRES/CELL; j++)
		for (i=0; i<XRES/CELL; i++)
		{
			r = s.fire_r[j][i]; g = s.fire_g[j][i]; b = s.fire_b[j][i];
			if (r || g || b)
				for (y=-CELL; y<2*CELL; y++) for (x=-CELL; x<2*CELL; x++)
				{
					a = fire_alpha[y+CELL][x+CELL];
					if (findingElement) a /= 2;
					addpixel(s.vid, i*CELL+x, j*CELL+y, r, g, b, a);
				}
			BLUR
		}
}
static void newFire(St &s)
{
	pixel *vid = s.vid;
	int i,j,x,y,r,g,b;
	int alpha[CELL*3][CELL*3];
	for (y=0; y<CELL*3; y++) for (x=0; x<CELL*3; x++)
		alpha[y][x] = findingElement ? int(fire_alpha[y][x]) / 2 : int(fire_alpha[y][x]);
	bool rowFire[YRES/CELL];
	for (j=0; j<YRES/CELL; j++)
	{
		rowFire[j] = false;
		for (i=0; i<XRES/CELL && !rowFire[j]; i++)
			rowFire[j] = s.fire_r[j][i] || s.fire_g[j][i] || s.fire_b[j][i];
	}
	for (j=0; j<YRES/CELL; j++)
	{
		if (!rowFire[j] && (j == 0 || !rowFire[j-1]) && (j+1 >= YRES/CELL || !rowFire[j+1]))
			continue;
		bool anyFire = false;
		for (i=0; i<XRES/CELL; i++)
		{
			r = s.fire_r[j][i]; g = s.fire_g[j][i]; b = s.fire_b[j][i];
			if (r || g || b)
			{
				int x0 = std::max(-CELL, -i*CELL), x1 = std::min(2*CELL, VIDXRES - i*CELL);
				int y0 = std::max(-CELL, -j*CELL), y1 = std::min(2*CELL, VIDYRES - j*CELL);
				for (y=y0; y<y1; y++)
				{
					pixel *row = vid + (j*CELL+y)*VIDXRES + i*CELL;
					const int *arow = alpha[y+CELL] + CELL;
					for (x=x0; x<x1; x++)
					{
						int a = arow[x]; pixel t = row[x];
						int nr = (a*r + 255*PIXR(t)) >> 8, ng = (a*g + 255*PIXG(t)) >> 8, nb = (a*b + 255*PIXB(t)) >> 8;
						row[x] = PIXRGB(nr > 255 ? 255 : nr, ng > 255 ? 255 : ng, nb > 255 ? 255 : nb);
					}
				}
			}
			BLUR
			anyFire = anyFire || s.fire_r[j][i] || s.fire_g[j][i] || s.fire_b[j][i];
		}
		rowFire[j] = anyFire;
	}
}
int main()
{
	srand(11);
	long frames = 0;
	for (int round = 0; round < 12; round++)
	{
		findingElement = round % 2;
		memset(&A, 0, sizeof(A));
		for (int y = 0; y < CELL*3; y++) for (int x = 0; x < CELL*3; x++) fire_alpha[y][x] = rand() % 300;
		int density = 1 + round % 4; // how many fire sources are added per frame
		for (int f = 0; f < 60; f++, frames++)
		{
			for (int k = 0; k < VIDXRES*VIDYRES; k += 1 + rand() % 50) A.vid[k] = PIXRGB(rand()%256, rand()%256, rand()%256);
			for (int n = 0; n < density * (f < 30 ? 5 : 0); n++)
			{
				// sources anywhere, edges included
				int j = rand() % 4 == 0 ? (rand() % 2 ? 0 : YRES/CELL-1) : rand() % (YRES/CELL);
				int i = rand() % (XRES/CELL);
				A.fire_r[j][i] = rand()%256; A.fire_g[j][i] = rand()%256; A.fire_b[j][i] = rand()%256;
			}
			if (f == 0) B = A; else { memcpy(B.vid, A.vid, sizeof(A.vid)); memcpy(B.fire_r, A.fire_r, sizeof(A.fire_r)); memcpy(B.fire_g, A.fire_g, sizeof(A.fire_g)); memcpy(B.fire_b, A.fire_b, sizeof(A.fire_b)); }
			// B gets the same input state as A; run both
			oldFire(A);
			newFire(B);
			if (memcmp(&A, &B, sizeof(A))) { printf("MISMATCH round %d frame %d\n", round, f); return 1; }
		}
	}
	printf("identical over %ld frames (pixels and fire arrays)\n", frames);
	return 0;
}
