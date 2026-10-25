#ifndef BRUSH_H_
#define BRUSH_H_
#include "Config.h"

#include "gui/interface/Point.h"

class Renderer;
class Brush
{
protected:
	unsigned char * outline;
	unsigned char * bitmap;
	ui::Point size;
	ui::Point radius;
	bool drawCrosshair = false;
	bool isHollow = false;
	void updateOutline();
public:
	Brush(ui::Point size);

	//Radius of the brush 0x0 - infxinf (Radius of 0x0 would be 1x1, radius of 1x1 would be 3x3)
	ui::Point GetRadius()
	{
		return radius;
	}

	//Size of the brush bitmap mask, 1x1 - infxinf
	ui::Point GetSize()
	{
		return size;
	}
	virtual void SetRadius(ui::Point radius);
	virtual ~Brush();
	virtual void RenderRect(Renderer * ren, ui::Point position1, ui::Point position2);
	virtual void RenderLine(Renderer * ren, ui::Point position1, ui::Point position2);
	virtual void RenderPoint(Renderer * ren, ui::Point position);
	virtual void RenderFill(Renderer * ren, ui::Point position);
	virtual void GenerateBitmap();
	//Get a bitmap for drawing particles
	unsigned char * GetBitmap();
	unsigned char * GetOutline();

	void SetDrawCrosshair(bool t) { drawCrosshair = t; }
	void SetHollow(bool t) { isHollow = t; }
};

class HollowBrush: public Brush {
public:
	HollowBrush(ui::Point size_):
		Brush(size_) {
		SetRadius(size_);
	};
	void GenerateBitmap() override {
		delete[] bitmap;
		bitmap = new unsigned char[size.X*size.Y];
		for(int x = 0; x < size.X; x++)
			for(int y = 0; y < size.Y; y++)
				bitmap[(y*size.X)+x] = 255;
		for(int x = 1; x < size.X - 1; x++)
			for(int y = 1; y < size.Y - 1; y++)
				bitmap[(y*size.X)+x] = 0;
	}
};

#endif /* BRUSH_H_ */
