#include "simulation/quantum/qmath.h"
#include "simulation/quantum/linalg.h"

#include <bitset>
#include <set>
#include <algorithm>

/* Helper functions */
// ---------------------------- //

// Function to swap bits are position p and q in integer n
int swapbit(int n, int p, int q) {
    // If bits are different at position p and q swap
    if (((n & (1 << p)) >> p) ^ ((n & (1 << q)) >> q)) {
        n ^= (1 << p);
        n ^= (1 << q);
    }
    return n;
}


/* The QMATH namespace */
// ---------------------------- //
namespace QMATH {

// Given a quantum state swap the qbits at positions d1 and d2 (0 indexed)
// Swaps values of the state in place and returns a set of vector indices that were swapped
void swap_state_vector(LINALG::vec &state, std::set<std::pair<int, int> > &to_swap, int d1, int d2) {
    int b1 = 1 << d1; // Bit mask for pos1
    int b2 = 1 << d2; // Bit mask for pos2

    for (unsigned int i = 0; i < state.size(); i++) {
        bool x1 = (b1 & i);
        bool x2 = (b2 & i);

        if (x1 ^ x2) { // The 2 qbits are opposite (bit1 XOR bit2)
            int swapped = swapbit(i, d1, d2), index = i;
            if (swapped >= state.size())
                continue;
            if (index > swapped) // Make sure the swap pair is in order from lowest to highest
                std::swap(index, swapped);
            std::pair<int, int> swap_pair = std::make_pair(index, swapped);

            // If we haven't already swapped swap and insert into set
            if (to_swap.find(swap_pair) == to_swap.end()) {
                to_swap.insert(swap_pair);
                std::iter_swap(state.begin() + index, state.begin() + swapped);
            }
        }
    }
}

};

