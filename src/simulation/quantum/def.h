#ifndef QDEF
#define QDEF

#include "common/String.h"
#include "simulation/quantum/linalg.h"

using LINALG::matrix;
using LINALG::vec;

// Ctype enum for type of quantum gate
// ID: ctype value
namespace QGATE_NAME {
    const int X = 1;
    const int SWAP = 2;
    const int CNOT = 3;
    const int CCNOT = 4;
    const int CSWAP = 5;
    const int DETECT = 6;
    const int H = 7;
    const int Y = 8;
    const int Z = 9;
    const int S = 10;
    const int SINV = 11;
    const int T = 12;
    const int TINV = 13;
    const int SQRTNOT = 14;
    const int RESET = 15;

    // Remember when adding gates update does_nothing, get_name,
    // is_multi_gate and the matrix returns (if applicable)

    const int COOLDOWN = 5; // Number of frames QLOG has to cooldown before firing again, if <= 4 then SPRK might SPRK it twice

    extern String get_name(int gate_id);
    extern bool does_nothing(int gate_id);
    extern bool is_multi_gate(int gate_id);
};

namespace QGATE {
    extern const matrix X;
    extern const matrix SWAP;
    extern const matrix CNOT;
    extern const matrix CCNOT;
    extern const matrix CSWAP;
    extern const matrix H;
    extern const matrix Y;
    extern const matrix Z;
    extern const matrix S;
    extern const matrix SINV;
    extern const matrix T;
    extern const matrix TINV;
    extern const matrix SQRTNOT;

    const matrix get_matrix_by_id(int id);
};

#endif