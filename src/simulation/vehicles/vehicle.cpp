#include "simulation/vehicles/vehicle.h"
#include "graphics/Pixel.h"
#include "graphics/Renderer.h"
#include "simulation/FixedUtils.h"

Vehicle& VehicleBuilder::Build() {
    if (vehicle) delete vehicle;
    vehicle = new Vehicle(width, height, accel, fly_accel, max_speed,
        collision_speed, runover_speed, rotation_speed);
    return *vehicle;
}

void draw_px_raw(const std::vector<VehiclePixel> &img, Renderer *ren, Particle *cpart, int cx, int cy, bool flip, float rotation) {
    int x, y;
    for (auto px = img.begin(); px != img.end(); ++px) {
        x = px->x; y = px->y;

        // Flip horizontally if car is going other way
        if (flip) x = -x;
        rotate(x, y, rotation);

        // Heat display
        if (ren->colour_mode & COLOUR_HEAT)
        {
            auto color = Renderer::heatTableAt(int((cpart->temp - MIN_TEMP) / (MAX_TEMP - MIN_TEMP) * 1024));
            ren->drawrect(cx + x, cy + y, 1, 1, PIXR(color),  PIXG(color),  PIXB(color), 255);
        }
        // Normal display
        else {
            ren->drawrect(cx + x, cy + y, 1, 1, px->r, px->g, px->b, 255);

            // Blend if occupied
            if (cpart->tmp2 == 1) // Player 1
                ren->blendpixel(cx + x, cy + y, 255, 224, 160, 30);
            else if (cpart->tmp2 == 2) // Player 2
                ren->blendpixel(cx + x, cy + y, 100, 100, 255, 30);
            else if (cpart->tmp2 > 2) // FIGH
                ren->blendpixel(cx + x, cy + y, 255, 50, 0, 30);
        }
    }
}

void draw_px(const std::vector<VehiclePixel> &img, Renderer *ren, Particle *cpart, float rotation) {
    draw_px_raw(img, ren, cpart, cpart->x, cpart->y, FROM_FIXED(cpart->tmp4, ren->sim->elements[cpart->type].FixedMultiplier), rotation);
}
