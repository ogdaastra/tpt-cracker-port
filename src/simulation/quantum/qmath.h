#ifndef QMATH_H
#define QMATH_H

#include <set>
#include "simulation/quantum/linalg.h"

namespace QMATH {
    void swap_state_vector(LINALG::vec &state, std::set<std::pair<int, int> > &to_swap, int d1, int d2);
}

#endif