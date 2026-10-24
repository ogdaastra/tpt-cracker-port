#include "simulation/quantum/def.h"
#include "simulation/quantum/linalg.h"
#include "common/String.h"
#include <string>

using LINALG::matrix;
using LINALG::vec;

namespace QGATE_NAME {
    String get_name(int gate_id) {
        switch(gate_id) {
            case X: return String("X");
            case SWAP: return String("SWAP");
            case CNOT: return String("CNOT");
            case CCNOT: return String("CCNOT");
            case CSWAP: return String("CSWAP");
            case DETECT: return String("OBSERVE");
            case H: return String("H");
            case Y: return String("Y");
            case Z: return String("Z");
            case S: return String("S");
            case SINV: return String("SINV");
            case T: return String("T");
            case TINV: return String("TINV");
            case SQRTNOT: return String("SQRTNOT");
            case RESET: return String("RESET");
            default: return String("Nothing");
        }
    }

    // Does the gate do nothing?
    bool does_nothing(int gate_id) {
        return gate_id < 1 || gate_id > 15;
    }

    // Is the gate a multi gate (> 1 input)
    bool is_multi_gate(int gate_id) {
        if (gate_id == X || gate_id == H || gate_id == Y || gate_id == Z ||
            gate_id == S || gate_id == SINV || gate_id == T ||
            gate_id == TINV || gate_id == DETECT) return false;
        return true;
    }
};

// Gate definitions
namespace QGATE {
    const std::complex<double> i(0, 1); // Define "i"

    const matrix X({
        LINALG::vec({0, 1}),
        LINALG::vec({1, 0})
    });
    const matrix SWAP({
        LINALG::vec({1, 0, 0, 0}),
        LINALG::vec({0, 0, 1, 0}),
        LINALG::vec({0, 1, 0, 0}),
        LINALG::vec({0, 0, 0, 1})
    });
    const matrix CNOT({
        LINALG::vec({1, 0, 0, 0}),
        LINALG::vec({0, 1, 0, 0}),
        LINALG::vec({0, 0, 0, 1}),
        LINALG::vec({0, 0, 1, 0})
    });
    const matrix CCNOT({
        LINALG::vec({1, 0, 0, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 1, 0, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 1, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 1, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 1, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 0, 1, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 0, 0, 0, 1}),
        LINALG::vec({0, 0, 0, 0, 0, 0, 1, 0})
    });
    const matrix CSWAP({
        LINALG::vec({1, 0, 0, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 1, 0, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 1, 0, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 1, 0, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 1, 0, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 0, 0, 1, 0}),
        LINALG::vec({0, 0, 0, 0, 0, 1, 0, 0}),
        LINALG::vec({0, 0, 0, 0, 0, 0, 0, 1})
    });
    const matrix H({
        LINALG::vec({0.7071067811865475, 0.7071067811865475}),
        LINALG::vec({0.7071067811865475, -0.7071067811865475})
    });
    const matrix Y({
        LINALG::vec({0, -i}),
        LINALG::vec({i, 0})
    });
    const matrix Z({
        LINALG::vec({1, 0}),
        LINALG::vec({0, -1})
    });
    const matrix S({
        LINALG::vec({1, 0}),
        LINALG::vec({0, i})
    });
    const matrix SINV({
        LINALG::vec({1, 0}),
        LINALG::vec({0, -i})
    });
    const matrix T({
        LINALG::vec({1, 0}),
        LINALG::vec({0, 0.7071067811865475 + 0.7071067811865475 * i})
    });
    const matrix TINV({
        LINALG::vec({1, 0}),
        LINALG::vec({0, 0.7071067811865475 - 0.7071067811865475 * i})
    });
    const matrix SQRTNOT({
        LINALG::vec({0.5 + 0.5 * i, 0.5 - 0.5 * i}),
        LINALG::vec({0.5 - 0.5 * i, 0.5 + 0.5 * i})
    });

    const matrix get_matrix_by_id(int gate_id) {
        switch(gate_id) {
            case QGATE_NAME::X: return X;
            case QGATE_NAME::SWAP: return SWAP;
            case QGATE_NAME::CNOT: return CNOT;
            case QGATE_NAME::CCNOT: return CCNOT;
            case QGATE_NAME::CSWAP: return CSWAP;
            case QGATE_NAME::H: return H;
            case QGATE_NAME::Y: return Y;
            case QGATE_NAME::Z: return Z;
            case QGATE_NAME::S: return S;
            case QGATE_NAME::SINV: return SINV;
            case QGATE_NAME::T: return T;
            case QGATE_NAME::TINV: return TINV;
            case QGATE_NAME::SQRTNOT: return SQRTNOT;
            default: return matrix();
        }
    }
};