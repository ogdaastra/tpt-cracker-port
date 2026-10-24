#include <complex>
#include <vector>
#include <iostream>

#include "linalg.h"

namespace LINALG {

/**
 * The tensor product is every element of a * b, merged. For example,
 * [a, b] (X) [c, d] = [ac, ad, bc, bd]
 */
vec tensor_product(const vec &a, const vec &b) {
    vec returned;
    for (unsigned int i = 0; i < a.size(); i++) {
        for (unsigned int j = 0; j < b.size(); j++)
            returned.push_back(a[i] * b[j]);
    }
    return returned;
}

/**
 * Like a tensor product, but with matrices. For example,
 * [a b]     [e f]  
 * [c d] (X) [g h] 
 * is
 * [ae af be bf]
 * [ag ah bg bh]
 * [ce cf de df]
 * [cg ch dh dh]
 */
matrix kronecker_product(const matrix &a, const matrix &b) {
    matrix returned;
    for (unsigned int row_a = 0; row_a < a.size(); row_a++) {
        for (unsigned int row_b = 0; row_b < b.size(); row_b++) {
            vec current_row;

            for (unsigned int col_a = 0; col_a < a[row_a].size(); col_a++) {
                for (unsigned int col_b = 0; col_b < b[row_b].size(); col_b++) {
                    current_row.push_back(a[row_a][col_a] * b[row_b][col_b]);
                }
            }
            returned.push_back(current_row);
        }
    }
    return returned;
}

/**
 * Simply multiples a matrix m by vector v, ie
 * the product mv. Each term of the product
 * is the dot product of each row of the matrix with the vector
 */
vec mult_matrix_vec(const matrix &m, const vec &v) {
    if (m.size() != v.size())
        throw "Matrix height must be same as vector size\n";
    
    vec returned;
    for (unsigned int row = 0; row < m.size(); row++) {
        std::complex<double> dot_product = 0;
        for (unsigned int i = 0; i < v.size(); i++)
            dot_product += v[i] * m[row][i];
        if (abs(dot_product) < 0.001)
            dot_product = std::complex<double>(0, 0); // Rounding error
        returned.push_back(dot_product);
    }
    return returned;
}

/**
 * Returns the identity matrix of size size x size
 */
matrix get_identity(int size) {
    matrix returned;
    for (unsigned int i = 0; i < size; i++) {
        vec row;
        for (unsigned int j = 0; j < size; j++)
            row.push_back(j == i ? 1 : 0);
        returned.push_back(row);
    }
    return returned;
}




// ------------ Testing --------------- //
// Add any tests you need to run here
// Used for debugging only

/**
 * Run tests on linear algebra functions
 */
void test() {
    // Test tensor product
    std::cout << "Testing [1, 2] (X) [3, 4] (Tensor product). Expected: [3, 4, 6, 8]\n";
    vec a, b, r;
    a.push_back(1); a.push_back(2);
    b.push_back(3); b.push_back(4);
    r = tensor_product(a, b);
    if (r.size() != 4.0 || r[0] != 3.0 || r[1] != 4.0 || r[2] != 6.0 || r[3] != 8.0)
        std::cout << " - Test failed! r is actually equal to:\n - r = ";
    else
        std::cout << " - Test passed!\n - r = ";
    for (unsigned int i = 0; i < r.size(); i++)
        std::cout << r[i] << " ";
    std::cout << "\n";

    // Test kronecker product
    std::cout << "\nTesting [1, 2 \\ 3, 4] (X) [5, 6 \\ 7, 8] (Kronecker product).\n";
    std::cout << "Expected: [5, 6, 10, 12 \\ 7, 8, 14, 16 \\ 15, 18, 20, 24 \\ 21, 24, 28, 32]\n";
    a.clear(); b.clear();
    a.push_back(1); a.push_back(2);
    b.push_back(3); b.push_back(4);

    matrix m1, m2;
    m1.push_back(a);
    m1.push_back(b);

    a.clear(); b.clear();
    a.push_back(5); a.push_back(6);
    b.push_back(7); b.push_back(8);
    m2.push_back(a); m2.push_back(b);

    matrix mr = kronecker_product(m1, m2);
    double expected[][4] = {
        {5.0, 6.0, 10.0, 12.0},
        {7.0, 8.0, 14.0, 16.0},
        {15.0, 18.0, 20.0, 24.0},
        {21.0, 24.0, 28.0, 32.0}
    };

    std::cout << " - This is result:\n";
    for (unsigned int i = 0; i < mr.size(); i++) {
        for (unsigned int j = 0; j < mr[i].size(); j++) {
            std::cout << mr[i][j] << " ";
            if (mr[i][j] != expected[i][j]) {
                std::cout << " - Failure: Item at " << i << ", " << j << " should be "
                          << expected[i][j] << ", got " << mr[i][j] << "\n";
            }
        }
        std::cout << "\n";
    }
    std::cout << " - If there are no failures above it passed\n";

    // Test matrix multiplication: success
    std::cout << "\nTesting [1, 2 \\ 3, 4] * [5, 6], should be [17, 39]\n";
    a.clear(); a.push_back(5); a.push_back(6);
    r = mult_matrix_vec(m1, a);

    if (r.size() != 2 || r[0] != 17.0 || r[1] != 39.0)
        std::cout << " - Failure, r is actually:\n";
    else
        std::cout << " - Success, r is:\n - ";
    for (unsigned int i = 0; i < r.size(); i++)
        std::cout << r[i] << " ";
    std::cout << "\n";

    // Test matrix with invalid size
    std::cout << "\nAttempting [1, 2 \\ 3, 4] * [1], should throw an error\n";
    a.clear(); a.push_back(1);
    try {
        mult_matrix_vec(m1, a);
        std::cout << " - Test failure, should have thrown an exception. r is wrongly evaluated to be:\n - ";
        for (unsigned int i = 0; i < r.size(); i++)
            std::cout << r[i] << " ";
        std::cout << "\n";
    } catch(char const *e) {
        std::cout << " - Success, thrown error: " << e;
    }

    // Test identity
    std::cout << "\nTesting identity matrix, diagonal top left to bottom right should be all 1s, rest 0\n";
    std::cout << " - This is result:\n";

    mr = get_identity(5);
    double exp;

    for (unsigned int i = 0; i < mr.size(); i++) {
        for (unsigned int j = 0; j < mr[i].size(); j++) {
            std::cout << mr[i][j] << " ";
            exp = i == j ? 1 : 0;
            if (mr[i][j] != exp) {
                std::cout << " - Failure: Item at " << i << ", " << j << " should be "
                          << exp << ", got " << mr[i][j] << "\n";
            }
        }
        std::cout << "\n";
    }
    std::cout << " - If there are no failures above it passed\n";
}

};