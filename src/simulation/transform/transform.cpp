#include "transform.h"

/* Rotate around origin */
void rotate(int &x, int &y, float rotation) {
    if (fabs(rotation) < 0.01) return;
    int x2 = round(x * cos(rotation)) - round(y * sin(rotation));
    int y2 = round(y * cos(rotation)) + round(x * sin(rotation));
    x = x2; y = y2;
}

void rotate(float &x, float &y, float rotation) {
    if (fabs(rotation) < 0.01) return;
    float x2 = x * cos(rotation) - y * sin(rotation);
    float y2 = y * cos(rotation) + x * sin(rotation);
    x = x2; y = y2;
}

/* Rotate around a point */
void rotate_around(int &x, int &y, int cx, int cy, float rotation) {
    x -= cx, y -= cy;
    rotate(x, y, rotation);
    x += cx, y += cy;
}

void rotate_around(float &x, float &y, float cx, float cy, float rotation) {
    x -= cx, y -= cy;
    rotate(x, y, rotation);
    x += cx, y += cy;
}