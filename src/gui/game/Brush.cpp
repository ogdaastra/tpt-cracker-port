#include "Brush.h"
#include "client/Client.h"
#include "graphics/Renderer.h"

Brush::Brush(ui::Point size_):
	outline(NULL),
	bitmap(NULL),
	size(0, 0),
	radius(0, 0)
{
	SetRadius(size_);
};

Brush::~Brush()
{
	delete[] bitmap;
	delete[] outline;
}

void Brush::updateOutline()
{
	if(!bitmap)
		GenerateBitmap();
	if(!bitmap)
		return;
	delete[] outline;
	outline = new unsigned char[size.X*size.Y];
	for(int x = 0; x < size.X; x++)
	{
		for(int y = 0; y < size.Y; y++)
		{
			if(bitmap[y*size.X+x] && (!y || !x || x == size.X-1 || y == size.Y-1 || !bitmap[y*size.X+(x+1)] || !bitmap[y*size.X+(x-1)] || !bitmap[(y-1)*size.X+x] || !bitmap[(y+1)*size.X+x]))
			{
				outline[y*size.X+x] = 255;
			}
			else
				outline[y*size.X+x] = 0;
		}
	}
}

void Brush::SetRadius(ui::Point radius)
{
	this->radius = radius;
	this->size = radius+radius+ui::Point(1, 1);

	GenerateBitmap();
	updateOutline();
}

void Brush::GenerateBitmap()
{
	delete[] bitmap;
	bitmap = new unsigned char[size.X*size.Y];
	for(int x = 0; x < size.X; x++)
	{
		for(int y = 0; y < size.Y; y++)
		{
			bitmap[(y*size.X)+x] = 255;
		}
	}
}

unsigned char *Brush::GetBitmap()
{
	if(!bitmap)
		GenerateBitmap();
	return bitmap;
}

unsigned char *Brush::GetOutline()
{
	if(!outline)
		updateOutline();
	if(!outline)
		return NULL;
	return outline;
}

void Brush::RenderRect(Renderer * ren, ui::Point position1, ui::Point position2)
{
	int width, height;
	width = position2.X-position1.X;
	height = position2.Y-position1.Y;
	if(height<0)
	{
		position1.Y += height;
		height *= -1;
	}
	if(width<0)
	{
		position1.X += width;
		width *= -1;
	}

	ren->xor_line(position1.X, position1.Y, position1.X+width, position1.Y);
	if(height>0){
		ren->xor_line(position1.X, position1.Y+height, position1.X+width, position1.Y+height);
		if(height>1){
			ren->xor_line(position1.X+width, position1.Y+1, position1.X+width, position1.Y+height-1);
			if(width>0)
				ren->xor_line(position1.X, position1.Y+1, position1.X, position1.Y+height-1);
		}
	}
}

void Brush::RenderLine(Renderer * ren, ui::Point position1, ui::Point position2)
{
	ren->xor_line(position1.X, position1.Y, position2.X, position2.Y);
}

void Brush::RenderPoint(Renderer * ren, ui::Point position)
{
	if(!outline)
		updateOutline();
	if(!outline)
		return;
	ren->xor_bitmap(outline, position.X-radius.X, position.Y-radius.Y, size.X, size.Y);

	// Render crosshair if large enough
	if (drawCrosshair && radius.X > 15 && radius.Y > 15) {
		ren->xor_line(position.X, position.Y - 5, position.X, position.Y + 5);
		ren->xor_line(position.X - 5, position.Y, position.X + 5, position.Y);
	}

	// Hollow indicator
	if (isHollow) {
		bool shouldHideHUD = Client::Ref().GetPrefBool("autoHideHUD", false) && position.Y < 100;
		int alpha = shouldHideHUD ? position.Y * 2 : 255;

		ren->fillrect(12, 41, ren->g->textwidth("[HOLLOW BRUSH]") + 8, 15, 0, 0, 0, alpha * 0.5);
		ren->drawtext(16, 43, "[HOLLOW BRUSH]", 32, 216, 255, alpha * 0.75);
	}
}

void Brush::RenderFill(Renderer * ren, ui::Point position)
{
	ren->xor_line(position.X-5, position.Y, position.X-1, position.Y);
	ren->xor_line(position.X+5, position.Y, position.X+1, position.Y);
	ren->xor_line(position.X, position.Y-5, position.X, position.Y-1);
	ren->xor_line(position.X, position.Y+5, position.X, position.Y+1);
}
