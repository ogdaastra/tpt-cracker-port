/* Linear algebra yay
 * 
 * We'll store particle states in vectors, which will be
 * linear algebra vectors. Matrices are vectors of vectors.
 * 
 * If you want to replace this with something meaningful feel free to,
 * but all I needed was something to multiply a matrix and a vector :|
 */
#ifndef LINALG_H
#define LINALG_H

#include <vector>
#include <complex>

namespace LINALG {
    typedef std::vector<std::complex<double> > vec;
    typedef std::vector<vec> matrix;

    vec tensor_product(const vec &a, const vec &b);
    matrix kronecker_product(const matrix &a, const matrix &b);
    vec mult_matrix_vec(const matrix &m, const vec &v);
    matrix get_identity(int size);

    // Testing only, tests linear algebra functions
    void test();
};


#endif