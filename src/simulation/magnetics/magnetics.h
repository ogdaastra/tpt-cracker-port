#ifndef MAGNET_H
#define MAGNET_H

#include "Config.h"
#include <map>
#include <set>
#include <vector>

#define EMCELL 4
#define EM_FIELD_SIZE (YRES / EMCELL) * (XRES / EMCELL)
#define FASTXY(x, y) (x)+(y)*(XRES / EMCELL)

class Simulation;

class EMField {
public:
      Simulation &sim;
      bool isEnabled = false;

      // Cache for magnetic sources: index -> strength
      std::map<int, float> sources, added_sources;
      std::set<int> removed_sources;

      // Arrays for the simulation
      // Packed 2D arrays, magnetic = 2D grid of magnetic strengths (scalar)
      // Magnetic fields are scalar when dimensionally reduced to 2D

      // Magnetic field: scalar
      // Electric fields: x, y, px, py (previous x and y), stored as vectors <x ,y>
      // Use XY to convert x, y
      // EM_FIELD_SIZE items each (the world's size is chosen at startup)
      std::vector<float> mx, my, magnetic, pmagnetic, change_magnetic;
      std::vector<float> ex, ey, pex, pey, electric, pelectric;

      int XY(int x, int y) { return FASTXY(x, y); };
      int distance_2(int index1, int index2);
      void Clear();
      void Update();
      void PreUpdate();

      void ApplyElectromagneticForces(int i);
      void AddField(int x, int y, float power);
      void RemoveField(int x, int y);
      void EditField(int x, int y, float newpower);

      EMField(Simulation &sim);

      bool changed_this_frame = false;

private:
      void UpdateMagnetic();
      void UpdateElectric();
};

#endif