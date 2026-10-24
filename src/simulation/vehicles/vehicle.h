#ifndef VEHICLE_H
#define VEHICLE_H

/**
 * Note that vehicles update TWICE per frame
 * 
 * Explainations for vehicle properties:
 * - width:            width in px
 * - height:           height in px
 * - acceleration:     acceleration when moving on ground
 * - fly_acceleration: acceleration when flying
 * - max_speed:        vx and vy must be smaller in magnitude than this
 * - collision_speed:  min speed for collision damage to vehicle
 * - runover_speed:    min speed to runover STKM
 * - rotation_speed:   radians to rotate per update 
 */

#include "simulation/ElementCommon.h"
#include "simulation/transform/transform.h"
#include <vector>

#define PI (3.141592f)

// Config for all vehicles
#define MIN_STKM_DISTANCE_TO_ENTER 10 // STKM <= to center pixel will "enter" a vehicle

inline bool is_stkm(int tmp2) { return tmp2 == 1 || tmp2 == 2; }
inline bool is_figh(int tmp2) { return tmp2 >= 2; }
enum STKM_COMMANDS { NOCMD = 0, UP = 1, DOWN = 2, LEFT = 3, RIGHT = 4, LEFT_AND_RIGHT = 5 };

// Use VehicleBuilder, do not construct directly!
class Vehicle {
public:
    Vehicle(int w, int h, float accel, float fly_accel, float max_speed,
        float collision_speed, float runover_speed, float rotation_speed):
        width(w), height(h), acceleration(accel), fly_acceleration(fly_accel), max_speed(max_speed),
        collision_speed(collision_speed), runover_speed(runover_speed), rotation_speed(rotation_speed) {}
    const int width, height;
    const float acceleration, fly_acceleration, max_speed,
                collision_speed, runover_speed, rotation_speed;
};

// Usage: Vehicle v = VehicleBuilder().SetSize(...).SetFlyAccel(...)... .Build();
class VehicleBuilder {
public:
    VehicleBuilder() {};
    VehicleBuilder& SetSize(int w, int h) {
        width = w, height = h;
        return *this;
    }
    VehicleBuilder& SetGroundAccel(float s)    { accel = s;           return *this; }
    VehicleBuilder& SetFlyAccel(float s)       { fly_accel = s;       return *this; }
    VehicleBuilder& SetMaxSpeed(float s)       { max_speed = s;       return *this; }
    VehicleBuilder& SetCollisionSpeed(float s) { collision_speed = s; return *this; }
    VehicleBuilder& SetRunoverSpeed(float s)   { runover_speed = s;   return *this; }
    VehicleBuilder& SetRotationSpeed(float s)  { rotation_speed = s;  return *this; }
    Vehicle& Build();
private:
    Vehicle * vehicle = nullptr;
    int width, height;
    float accel, fly_accel, max_speed,
          collision_speed, runover_speed,
          rotation_speed;
};

// Stores graphics information for drawing images of vehicles
class VehiclePixel {
public:
    VehiclePixel(int x_, int y_, int r_, int g_, int b_):
        x(x_), y(y_), r(r_), g(g_), b(b_) {}
    const int x, y, r, g, b;
};

// draw_px calls draw_px_raw, filling in center and flip with variables from the cpart
void draw_px(const std::vector<VehiclePixel> &img, Renderer *ren, Particle *cpart, float rotation);
void draw_px_raw(const std::vector<VehiclePixel> &img, Renderer *ren, Particle *cpart, int cx, int cy, bool flip, float rotation);

#endif
