#include "simulation/vehicles/vehicle.h"
#include "simulation/vehicles/horse.h"

#include "simulation/FixedUtils.h"

const int NECK_RADIUS = 9;
const int NOSE_RADIUS = 4;
const int TAIL_RADIUS = 4;
const int STKM_HEAD_DY = -6;
const float GALLOP_PHASE_FREQ = 5.0f;

Vehicle Horse = VehicleBuilder()
    .SetSize(25, 15)
    .SetGroundAccel(1.5f)
    .SetFlyAccel(1.5f)
    .SetMaxSpeed(2.5f)
    .SetCollisionSpeed(2.0f)
    .SetRunoverSpeed(1.5f)
    .SetRotationSpeed(0.1f)
    .Build();

void draw_horse(Renderer *ren, Particle *cpart, float vx, float vy) {
    /**
     * Our horse will look something like this:
     *            __
     * LEFT  ____/    RIGHT
     *      /|   |
     * (Rotated and flipped depending on variables)
     */

	float bodyAngle = FROM_FIXED(cpart->tmp3, ren->sim->elements[PT_HRSE].FixedMultiplier);
	float neckAngle = FROM_FIXED(cpart->tmp4, ren->sim->elements[PT_HRSE].FixedMultiplier);

    // Main body (a stick). Right part is smaller to account for neck
    int leftx = -Horse.width / 2,
        rightx = Horse.width / 2 - NECK_RADIUS,
        lefty = 0, righty = 0;
    if (cpart->tmp & 2) { // Going other direction
        std::swap(leftx, rightx);
        std::swap(lefty, righty);
    }
    int leftx_rot = leftx, lefty_rot = lefty, rightx_rot = rightx, righty_rot = righty;
    rotate(leftx_rot, lefty_rot, bodyAngle);
    rotate(rightx_rot, righty_rot, bodyAngle);
    ren->draw_line(cpart->x + leftx_rot, cpart->y + lefty_rot, cpart->x + rightx_rot, cpart->y + righty_rot, 255, 255, 255, 255);

    // Neck, goes from right upwards
    int flip = cpart->tmp & 2 ? -1 : 1;
    int neckx = rightx + flip * NECK_RADIUS * cos(neckAngle);
    int necky = righty + NECK_RADIUS * sin(neckAngle);
    int neckx_rot = neckx, necky_rot = necky;
    rotate(neckx_rot, necky_rot, bodyAngle);
    ren->draw_line(cpart->x + rightx_rot, cpart->y + righty_rot, cpart->x + neckx_rot, cpart->y + necky_rot, 255, 255, 255, 255);

    // The nose (snout? face?) of the horse, 90 deg offset from neck
    int nosex = neckx + flip * NOSE_RADIUS * cos(neckAngle + PI / 2);
    int nosey = necky + NOSE_RADIUS * sin(neckAngle + PI / 2);
    rotate(nosex, nosey, bodyAngle);
    ren->draw_line(cpart->x + nosex, cpart->y + nosey, cpart->x + neckx_rot, cpart->y + necky_rot, 255, 255, 255, 255);

    // Tail, gets straighter the faster the horse goes
    float speed = sqrt(vx * vx + vy * vy);
    float tail_theta = -PI / 4 + std::min(10.0f, speed) / 10.0f * PI / 2;
    int tailx = leftx - flip * TAIL_RADIUS * cos(tail_theta),
        taily = lefty - TAIL_RADIUS * sin(tail_theta);
    rotate(tailx, taily, bodyAngle);
    ren->draw_line(cpart->x + leftx_rot, cpart->y + lefty_rot, cpart->x + tailx, cpart->y + taily, 255, 255, 255, 255);

    // Legs depend on speed
    if (fabs(cpart->vx) < 0.4f && fabs(cpart->vy) < 0.4f) { // Idle animation
        int leg1x = -Horse.width / 2,
            leg1y = Horse.height / 2,
            leg2x = Horse.width / 2 - NECK_RADIUS,
            leg2y = Horse.height / 2;

        if (cpart->tmp & 2) { // Going other direction
            std::swap(leg1x, leg2x);
            std::swap(leg1y, leg2y);
        }
        rotate(leg1x, leg1y, bodyAngle);
        rotate(leg2x, leg2y, bodyAngle);
        ren->draw_line(cpart->x + leg1x, cpart->y + leg1y, cpart->x + leftx_rot, cpart->y + lefty_rot, 255, 255, 255, 255);
        ren->draw_line(cpart->x + leg2x, cpart->y + leg2y, cpart->x + rightx_rot, cpart->y + righty_rot, 255, 255, 255, 255);

        // Neigh
        if (ren->sim->timer % 1000 < 200)
            ren->drawtext(cpart->x - 20, cpart->y - 10, "*neigh*", 255, 255, 255, 200);
    }
    else { // Advanced galloping :D
        /**
         * Leg is 2 jointed: the joint connected to the body rotates about pi / 6 left and right
         * The knee joint rotates at most 0 to the right and 90 to the left
         */
        int gallop_phase, joint1x, joint1y, joint1x_rot, joint1y_rot;
        for (size_t leg = 0; leg < 4; leg++) {
            gallop_phase = ren->sim->timer + leg * GALLOP_PHASE_FREQ;

            // First 2 legs are left
            joint1x = leg < 2 ? leftx : rightx;
            joint1y = leg < 2 ? lefty : righty;
            joint1x_rot = leg < 2 ? leftx_rot : rightx_rot;
            joint1y_rot = leg < 2 ? lefty_rot : righty_rot;

            // Rotate knee (from 3/4 pi to 1/4 pi)
            float knee_rotation = sin(gallop_phase / GALLOP_PHASE_FREQ) * (PI / 6) + (PI / 2);
            int kneex = joint1x + Horse.height / 3 * cos(knee_rotation) * flip;
            int kneey = joint1y + Horse.height / 3 * sin(knee_rotation);
            int kneex_rot = kneex, kneey_rot = kneey;
            rotate(kneex_rot, kneey_rot, bodyAngle);
            ren->draw_line(cpart->x + joint1x_rot, cpart->y + joint1y_rot, cpart->x + kneex_rot, cpart->y + kneey_rot, 255, 255, 255, 255);

            // Foot
            float foot_rotation = sin(gallop_phase / GALLOP_PHASE_FREQ) * (PI / 4) + (PI / 4);
            int footx = kneex + Horse.height / 3 * cos(knee_rotation + foot_rotation) * flip;
            int footy = kneey + Horse.height / 3 * sin(knee_rotation + foot_rotation);
            rotate(footx, footy, bodyAngle);
            ren->draw_line(cpart->x + footx, cpart->y + footy, cpart->x + kneex_rot, cpart->y + kneey_rot, 255, 255, 255, 255);
        }
    }

    // Something is riding
    if (cpart->tmp2 > 0) {
        int color = ren->sim->elements[(cpart->ctype > 0 && cpart->ctype < PT_NUM) ? cpart->ctype : PT_DUST].Colour;
        int r = PIXR(color), g = PIXG(color), b = PIXB(color);
        int head_dx = 0, head_dy = STKM_HEAD_DY;
        int neck_dx = 0, neck_dy = STKM_HEAD_DY + 2;
        int foot1_dx = -1, foot1_dy = -STKM_HEAD_DY;
        int foot2_dx =  1, foot2_dy = -STKM_HEAD_DY; 

        rotate(head_dx, head_dy, bodyAngle);
        rotate(neck_dx, neck_dy, bodyAngle);
        rotate(foot1_dx, foot1_dy, bodyAngle);
        rotate(foot2_dy, foot2_dy, bodyAngle);

        // Draw legs
        ren->draw_line(cpart->x + neck_dx, cpart->y + neck_dy, cpart->x + foot1_dx, cpart->y + foot1_dy, 255, 255, 255, 255);
        ren->draw_line(cpart->x + neck_dx, cpart->y + neck_dy, cpart->x + foot2_dx, cpart->y + foot2_dy, 255, 255, 255, 255);

        // STKM is riding
        if (cpart->tmp2 == 1 || cpart->tmp2 == 2) {
            ren->draw_line(cpart->x + head_dx - 2, cpart->y + head_dy - 2, cpart->x + head_dx + 2, cpart->y + head_dy - 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx - 2, cpart->y + head_dy + 2, cpart->x + head_dx + 2, cpart->y + head_dy + 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx - 2, cpart->y + head_dy + 2, cpart->x + head_dx - 2, cpart->y + head_dy - 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx + 2, cpart->y + head_dy + 2, cpart->x + head_dx + 2, cpart->y + head_dy - 2, r, g, b, 255);
        }
        // FIGH is riding
        else if (cpart->tmp2 > 2) {
            ren->draw_line(cpart->x + head_dx - 2, cpart->y + head_dy, cpart->x + head_dx, cpart->y + head_dy + 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx - 2, cpart->y + head_dy, cpart->x + head_dx, cpart->y + head_dy - 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx + 2, cpart->y + head_dy, cpart->x + head_dx, cpart->y + head_dy + 2, r, g, b, 255);
            ren->draw_line(cpart->x + head_dx + 2, cpart->y + head_dy, cpart->x + head_dx, cpart->y + head_dy - 2, r, g, b, 255);
        }
    }
    return;
}
