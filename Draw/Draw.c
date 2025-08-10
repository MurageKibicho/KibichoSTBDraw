#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
// Wu line drawing (antialiased)
#define ipart(x) ((int)(x))
#define fpart(x) ((x) - floorf(x))
#define rfpart(x) (1.0f - fpart(x))
//clear && gcc Draw.c -lm -lgmp -o m.o && ./m.o

void ConvertToGridCoordinates(int *x, int *y, int height, int width)
{
	*x = (width/2)  + *x;
	*y = (height/2) - *y;
}

void SetPixelRGBA(int inputX, int inputY, int height, int width, uint8_t *image, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	if(a == 0){return;} // Fully transparent

	int x = width / 2 + inputX;
	int y = height / 2 - inputY;

	if (x < 0 || x >= width || y < 0 || y >= height) return;

	int offset = (y * width + x) * 4;

	// Destination color
	uint8_t dr = image[offset + 0];
	uint8_t dg = image[offset + 1];
	uint8_t db = image[offset + 2];
	uint8_t da = image[offset + 3];

	// Calculate output alpha
	uint16_t outA = a + ((da * (255 - a)) / 255);
	if (outA == 0) outA = 1;  // Avoid division by zero

	// Blend each channel
	uint8_t outR = (uint8_t)(((r * a + dr * da * (255 - a) / 255) / outA));
	uint8_t outG = (uint8_t)(((g * a + dg * da * (255 - a) / 255) / outA));
	uint8_t outB = (uint8_t)(((b * a + db * da * (255 - a) / 255) / outA));

	image[offset + 0] = outR;
	image[offset + 1] = outG;
	image[offset + 2] = outB;
	image[offset + 3] = (uint8_t)outA;
}

void SetGrayPixel(int inputX, int inputY, uint8_t brightness, uint8_t alpha, int height, int width, uint8_t *image)
{
	SetPixelRGBA(inputX, inputY, height, width, image,brightness, brightness, brightness, alpha);
}
void DrawWuLine(int x0, int y0, int x1, int y1, int height, int width, uint8_t *image)
{
	int steep = abs(y1 - y0) > abs(x1 - x0);
	if(steep){int tmp;tmp = x0; x0 = y0; y0 = tmp;tmp = x1; x1 = y1; y1 = tmp;}
	if(x0 > x1){int tmp;tmp = x0; x0 = x1; x1 = tmp;tmp = y0; y0 = y1; y1 = tmp;}

	float dx = x1 - x0;
	float dy = y1 - y0;
	float gradient = (dx == 0) ? 1 : dy / dx;

	// First endpoint
	float xEnd = roundf(x0);
	float yEnd = y0 + gradient * (xEnd - x0);
	float xGap = rfpart(x0 + 0.5f);
	int xPixel1 = (int)xEnd;
	int yPixel1 = ipart(yEnd);

	if (steep)
	{
		SetGrayPixel(yPixel1, xPixel1, 255, (uint8_t)(rfpart(yEnd) * xGap * 255), height, width, image);
		SetGrayPixel(yPixel1 + 1, xPixel1, 255, (uint8_t)(fpart(yEnd) * xGap * 255), height, width, image);
	}
	else
	{
		SetGrayPixel(xPixel1, yPixel1, 255, (uint8_t)(rfpart(yEnd) * xGap * 255), height, width, image);
		SetGrayPixel(xPixel1, yPixel1 + 1, 255, (uint8_t)(fpart(yEnd) * xGap * 255), height, width, image);
	}

	float intery = yEnd + gradient;

	// Second endpoint
	xEnd = roundf(x1);
	yEnd = y1 + gradient * (xEnd - x1);
	xGap = fpart(x1 + 0.5f);
	int xPixel2 = (int)xEnd;
	int yPixel2 = ipart(yEnd);

	if (steep)
	{
		SetGrayPixel(yPixel2, xPixel2, 255, (uint8_t)(rfpart(yEnd) * xGap * 255), height, width, image);
		SetGrayPixel(yPixel2 + 1, xPixel2, 255, (uint8_t)(fpart(yEnd) * xGap * 255), height, width, image);
	}
	else
	{
		SetGrayPixel(xPixel2, yPixel2, 255, (uint8_t)(rfpart(yEnd) * xGap * 255), height, width, image);
		SetGrayPixel(xPixel2, yPixel2 + 1, 255, (uint8_t)(fpart(yEnd) * xGap * 255), height, width, image);
	}

	// Main loop
	for (int x = xPixel1 + 1; x < xPixel2; x++)
	{
		int y = ipart(intery);
		if(steep)
		{
			SetGrayPixel(y, x, 255, (uint8_t)(rfpart(intery) * 255), height, width, image);
			SetGrayPixel(y + 1, x, 255, (uint8_t)(fpart(intery) * 255), height, width, image);
		}
		else
		{
			SetGrayPixel(x, y, 255, (uint8_t)(rfpart(intery) * 255), height, width, image);
			SetGrayPixel(x, y + 1, 255, (uint8_t)(fpart(intery) * 255), height, width, image);
		}
		intery += gradient;
	}
}

void DrawFilledCircle(int inputX, int inputY, int radius, int height, int width, uint8_t *image)
{
	for (int y = -radius; y <= radius; y++)
	{
		int xSpan = (int)sqrtf(radius * radius - y * y);
		for(int x = -xSpan; x <= xSpan; x++)
		{
			SetPixelRGBA(inputX + x, inputY + y, height, width, image, 255, 255, 255, 255);
		}
	}
}

void DrawAACircle(int inputX, int inputY, float radius, int height, int width, uint8_t *image)
{
	float step = 0.5f / radius; // angle step depends on radius

	for (float angle = 0; angle < 2 * M_PI; angle += step)
	{
		float x = inputX + radius * cosf(angle);
		float y = inputY + radius * sinf(angle);

		int ix = (int)floorf(x);
		int iy = (int)floorf(y);
		float fx = x - ix;
		float fy = y - iy;

		SetGrayPixel(ix, iy, 255, (uint8_t)((1 - fx) * (1 - fy) * 255), height, width, image);
		SetGrayPixel(ix + 1, iy, 255, (uint8_t)(fx * (1 - fy) * 255), height, width, image);
		SetGrayPixel(ix, iy + 1, 255, (uint8_t)((1 - fx) * fy * 255), height, width, image);
		SetGrayPixel(ix + 1, iy + 1, 255, (uint8_t)(fx * fy * 255), height, width, image);
	}
}

void FillBackground(uint8_t *image, int height, int width, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	int channels = 4;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int offset = (y * width + x) * channels;
			image[offset + 0] = r;
			image[offset + 1] = g;
			image[offset + 2] = b;
			image[offset + 3] = a;
		}
	}
}


int main()
{
	char *fileName = "Image/0.png";
	int height = 512;
	int width = 512;
	int channels = 4;
	uint8_t *image = calloc(height * width * channels, sizeof(uint8_t));
	//Background color
	FillBackground(image, height, width, 204, 204, 204, 255);
	 
	DrawFilledCircle(0, 0, 50, height, width, image);         
	DrawAACircle(0, 0, 60.0f, height, width, image);          
	DrawWuLine(-200, 200, 200, -200, height, width, image);

	stbi_write_png(fileName, width, height, channels, image, width * channels);
	free(image);
	return 0;
}
