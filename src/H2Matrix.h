/*

Copyright (c) 2005-2016, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Aboria.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef H2_MATRIX_H_
#define H2_MATRIX_H_

#include "detail/FastMultipoleMethod.h"
#include <Eigen/SparseCore>

namespace Aboria {

template <typename Expansions, typename ColParticles,
         typename Query=typename ColParticles::query_type>
class H2Matrix {
    typedef typename Query::traits_type traits_type;
    typedef typename Query::reference reference;
    typedef typename Query::pointer pointer;
    static const unsigned int dimension = Query::dimension;
    typedef position_d<dimension> position;
    typedef typename Query::child_iterator child_iterator;
    typedef typename Query::all_iterator all_iterator;
    typedef typename traits_type::template vector_type<
        child_iterator
        >::type child_iterator_vector_type;
    typedef typename traits_type::template vector_type<
        child_iterator_vector_type
        >::type connectivity_type;
    
    typedef typename Eigen::SparseMatrix<double> sparse_matrix_type;
    typedef typename Eigen::Matrix<double,Eigen::Dynamic,1> column_vector_type;
    typedef typename Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> dense_matrix_type;
    typedef typename Expansions::p_vector_type p_vector_type;
    typedef typename Expansions::m_vector_type m_vector_type;
    typedef typename Expansions::l2p_matrix_type l2p_matrix_type;
    typedef typename Expansions::p2m_matrix_type p2m_matrix_type;
    typedef typename Expansions::p2p_matrix_type p2p_matrix_type;
    typedef typename Expansions::l2l_matrix_type l2l_matrix_type;
    typedef typename Expansions::m2l_matrix_type m2l_matrix_type;
    typedef typename traits_type::template vector_type<int>::type int_vector_type;
    typedef typename traits_type::template vector_type<p_vector_type>::type p_vectors_type;
    typedef typename traits_type::template vector_type<m_vector_type>::type m_vectors_type;
    typedef typename traits_type::template vector_type<l2p_matrix_type>::type l2p_matrices_type;
    typedef typename traits_type::template vector_type<p2m_matrix_type>::type p2m_matrices_type;
    typedef typename traits_type::template vector_type<p2p_matrix_type>::type p2p_matrices_type;
    typedef typename traits_type::template vector_type<p2p_matrices_type>::type vector_of_p2p_matrices_type;
    typedef typename traits_type::template vector_type<l2l_matrix_type>::type l2l_matrices_type;
    typedef typename traits_type::template vector_type<m2l_matrix_type>::type m2l_matrices_type;
    typedef typename traits_type::template vector_type<m2l_matrices_type>::type vector_of_m2l_matrices_type;
    typedef typename traits_type::template vector_type<size_t>::type indices_type;
    typedef typename traits_type::template vector_type<indices_type>::type vector_of_indices_type;

    typedef typename Query::particle_iterator particle_iterator;
    typedef typename particle_iterator::reference particle_reference;
    typedef detail::bbox<dimension> box_type;

    // vectors used to cache values
    mutable m_vectors_type m_W;
    mutable m_vectors_type m_g;
    mutable p_vectors_type m_source_vector;
    mutable p_vectors_type m_target_vector;

    l2p_matrices_type m_l2p_matrices;
    p2m_matrices_type m_p2m_matrices;
    l2l_matrices_type m_l2l_matrices;
    vector_of_p2p_matrices_type m_p2p_matrices;
    vector_of_m2l_matrices_type m_m2l_matrices;
    vector_of_indices_type m_row_indices;
    vector_of_indices_type m_col_indices;

    int_vector_type m_ext_indicies;

    connectivity_type m_strong_connectivity; 
    connectivity_type m_weak_connectivity; 

    Expansions m_expansions;

    const Query* m_query;
    const ColParticles* m_col_particles;
    const size_t m_row_size;

public:

    template <typename RowParticles>
    H2Matrix(const RowParticles &row_particles, const ColParticles &col_particles, const Expansions& expansions):
        m_query(&col_particles.get_query()),
        m_expansions(expansions),
        m_col_particles(&col_particles),
        m_row_size(row_particles.size())
    {
        //generate h2 matrix 
        const size_t n = m_query->number_of_buckets();
        LOG(2,"H2Matrix: creating matrix with "<<n<<" buckets, using "<<row_particles.size()<<" row particles and "<<col_particles.size()<<" column particles");
        const bool row_equals_col = static_cast<const void*>(&row_particles) 
                                        == static_cast<const void*>(&col_particles);
        m_W.resize(n);
        m_g.resize(n);
        m_l2l_matrices.resize(n);
        m_m2l_matrices.resize(n);
        m_row_indices.resize(n);
        m_col_indices.resize(n);
        m_ext_indicies.resize(n);
        m_source_vector.resize(n);
        m_target_vector.resize(n);
        m_l2p_matrices.resize(n);
        m_p2m_matrices.resize(n);
        m_p2p_matrices.resize(n);
        m_strong_connectivity.resize(n);
        m_weak_connectivity.resize(n);

        

        // setup row and column indices
        if (!row_equals_col) {
            LOG(2,"\trow particles are different to column particles");
            for (int i = 0; i < row_particles.size(); ++i) {
                //get target index
                pointer bucket;
                box_type box;
                m_query->get_bucket(get<position>(row_particles)[i],bucket,box);
                const size_t index = m_query->get_bucket_index(*bucket); 
                m_row_indices[index].push_back(i);
            }
        }
        for (auto& bucket: m_query->get_subtree()) {
            if (m_query->is_leaf_node(bucket)) { // leaf node
                const size_t index = m_query->get_bucket_index(bucket); 
                //get target_index
                for (auto& p: m_query->get_bucket_particles(bucket)) {
                    const size_t i = &get<position>(p)
                                   - &get<position>(col_particles)[0];
                    m_col_indices[index].push_back(i);
                    if (row_equals_col) {
                        m_row_indices[index].push_back(i);
                    }
                }

                m_source_vector[index].resize(m_col_indices[index].size());
                m_target_vector[index].resize(m_row_indices[index].size());
            }
        }

        // create a vector of column indicies for extended matrix/vector 
        detail::transform_exclusive_scan(m_source_vector.begin(),m_source_vector.end(),
                                        m_ext_indicies.begin(),
                                        [](const p_vector_type& i) { return i.size(); },
                                         0,std::plus<int>());


        // downward sweep of tree to generate matrices
        LOG(2,"\tgenerating matrices...");
        for (child_iterator ci = m_query->get_children(); ci != false; ++ci) {
            const box_type& target_box = m_query->get_bounds(ci);
            generate_matrices(child_iterator_vector_type(),box_type(),ci,row_particles,col_particles);
        }
        LOG(2,"\tdone");
    }

    H2Matrix(const H2Matrix& matrix) = default;

    H2Matrix(H2Matrix&& matrix) = default;

    ~H2Matrix() = default;

    // copy construct from another h2_matrix with different row_particles.
    template <typename RowParticles>
    H2Matrix(const H2Matrix<Expansions,ColParticles>& matrix, 
             const RowParticles &row_particles):
        m_W(matrix.m_W.size()),
        m_g(matrix.m_g.size()),
        m_source_vector(matrix.m_source_vector),
        m_ext_indicies(matrix.m_ext_indicies),
        //m_target_vector(matrix.m_target_vector), \\going to redo
        //m_l2p_matrices(matrix.m_l2p_matrices),     \\going to redo
        m_p2m_matrices(matrix.m_p2m_matrices),
        m_l2l_matrices(matrix.m_l2l_matrices),
        //m_p2p_matrices(matrix.m_p2p_matrices), \\going to redo these
        m_m2l_matrices(matrix.m_m2l_matrices),
        //m_row_indices(matrix.m_row_indices), \\going to redo these
        m_col_indices(matrix.m_col_indices),
        m_strong_connectivity(matrix.m_strong_connectivity),
        m_weak_connectivity(matrix.m_weak_connectivity),
        m_query(matrix.m_query),
        m_expansions(matrix.m_expansions),
        m_col_particles(matrix.m_col_particles),
        m_row_size(row_particles.size())
    {
        const size_t n = m_query->number_of_buckets();
        const bool row_equals_col = &row_particles == m_col_particles;
        m_target_vector.resize(n);
        m_row_indices.resize(n);
        m_p2p_matrices.resize(n);
        m_l2p_matrices.resize(n);

        // setup row and column indices
        if (row_equals_col) {
            std::copy(std::begin(m_col_indices),std::end(m_col_indices),
                      std::begin(m_row_indices));
        } else {
            for (int i = 0; i < row_particles.size(); ++i) {
                //get target index
                pointer bucket;
                box_type box;
                m_query->get_bucket(get<position>(row_particles)[i],bucket,box);
                const size_t index = m_query->get_bucket_index(*bucket); 
                m_row_indices[index].push_back(i);
            }
        }
        for (int i = 0; i < m_row_indices.size(); ++i) {
            m_target_vector[i].resize(m_row_indices[i].size());
        }

        for (child_iterator ci = m_query->get_children(); ci != false; ++ci) {
            const box_type& target_box = m_query->get_bounds(ci);
            generate_row_matrices(ci,row_particles,*m_col_particles);
        }
    }

    // target_vector += A*source_vector
    template <typename VectorTypeTarget, typename VectorTypeSource>
    void matrix_vector_multiply(VectorTypeTarget& target_vector, 
                          const VectorTypeSource& source_vector) const {

        // for all leaf nodes setup source vector
        for (auto& bucket: m_query->get_subtree()) {
            if (m_query->is_leaf_node(bucket)) { // leaf node
                const size_t index = m_query->get_bucket_index(bucket); 
                for (int i = 0; i < m_col_indices[index].size(); ++i) {
                    m_source_vector[index][i] = source_vector[m_col_indices[index][i]];
                }
            }
        }

        // upward sweep of tree
        for (child_iterator ci = m_query->get_children(); ci != false; ++ci) {
            mvm_upward_sweep(ci);
        }

        // downward sweep of tree. 
        for (child_iterator ci = m_query->get_children(); ci != false; ++ci) {
            m_vector_type g = m_vector_type::Zero();
            mvm_downward_sweep(g,ci);
        }

        // for all leaf nodes copy to target vector
        for (auto& bucket: m_query->get_subtree()) {
            if (m_query->is_leaf_node(bucket)) { // leaf node
                const size_t index = m_query->get_bucket_index(bucket); 
                for (int i = 0; i < m_row_indices[index].size(); ++i) {
                    target_vector[m_row_indices[index][i]] += m_target_vector[index][i];
                }
            }
        }
    }

    /// Convert H2 Matrix to an extended sparse matrix
    ///
    /// This creates an Eigen sparse matrix A that can be applied to  
    /// by the vector $[x W g]$ to produce $[y 0 0]$, i.e. $A*[x W g]' = [y 0 0]$, 
    /// where $x$ and $y$ are the input/output vector, i.e. $y = H2Matrix*x$, and $W$ 
    /// and $g$ are the multipole and local coefficients respectively.
    ///
    /// A is given by the block matrix representation:
    ///
    ///Sushnikova, D., & Oseledets, I. V. (2014). Preconditioners for hierarchical 
    ///matrices based on their extended sparse form. 
    ///Retrieved from http://arxiv.org/abs/1412.1253
    ///
    ///             |P2P  0     0      L2P| |x  |   |y|
    /// A*[x W g] = |0   M2L    L2L-I  -I |*|W  | = |0|
    ///             |0   M2M-I  0       0 | |g_n|   |0|
    ///             |P2M  -I    0       0 | |g_l|   |0|
    ///
    /// where $g_l$ is the $g$ vector for every leaf of the tree, and $g_n$ is the 
    /// remaining nodes. Note M2M = L2L'.
    ///
    sparse_matrix_type gen_extended_matrix() const {

        const size_t size_x = m_col_particles->size();
        ASSERT(m_vector_type::RowsAtCompileTime != Eigen::Dynamic,"not using compile time m size");
        const size_t vector_size = m_vector_type::RowsAtCompileTime;

        const size_t size_W = m_W.size()*vector_size;
        const size_t size_g = m_g.size()*vector_size;
        const size_t n = size_x + size_W + size_g;

        std::vector<int> reserve(n,0);

        // sizes for first x columns
        auto reserve0 = std::begin(reserve);
        for (int i = 0; i < m_source_vector.size(); ++i) {
            const size_t n = m_source_vector[i].size();
            // p2m
            std::transform(reserve0,reserve0+n,reserve0,
                    [](const int count){ return count+vector_size; });
            // p2p
            for (int j = 0; j < m_strong_connectivity[i].size(); ++j) {
                size_t index = m_query->get_bucket_index(*m_strong_connectivity[i][j]);
                const size_t target_size = m_target_vector[index].size();
                std::transform(reserve0,reserve0+n,reserve0,
                    [target_size](const int count){ return count+target_size; });
            }
            reserve0 += m_source_vector[i].size();
        }

        // sizes for W columns
        // m2l
        for (int i = 0; i < m_source_vector.size(); ++i) {
            for (int j = 0; j < m_weak_connectivity[i].size(); ++j) {
                std::transform(reserve0+i*vector_size,reserve0+(i+1)*vector_size,
                               reserve0+i*vector_size,
                    [](const int count){ return count+vector_size; });
            }
        }
        // m2m-I or -I
        std::transform(reserve0,reserve0+size_W,
                       reserve0,
                    [](const int count){ return count+1; });

        auto subtree_range = m_query->get_subtree();
        for (auto bucket_it = subtree_range.begin(); 
                bucket_it != subtree_range.end(); ++bucket_it) {
            const size_t i = m_query->get_bucket_index(*bucket_it); 
            // row is the index of the parent
            const size_t row_index = size_x + size_W + i*vector_size;
            // m2m
            if (!m_query->is_leaf_node(*bucket_it)) { // non leaf node
                for (child_iterator cj = 
                        m_query->get_children(bucket_it.get_child_iterator()); 
                     cj != false; ++cj) {
                    const size_t j = m_query->get_bucket_index(*cj); 
                    std::transform(reserve0+j*vector_size,reserve0+(j+1)*vector_size,
                               reserve0+j*vector_size,
                            [](const int count){ return count+vector_size; });

                }
            }
        }
        
        // sizes for g columns
        reserve0 += size_W;
        
        // looping over columns
        for (auto bucket_it = subtree_range.begin(); 
                bucket_it != subtree_range.end(); ++bucket_it) {
            const size_t i = m_query->get_bucket_index(*bucket_it); 
            // col is the index of the parent
            const size_t col_index = size_x + size_W + i*vector_size;
            
            // -I
            const size_t target_size = m_target_vector[i].size();
            std::transform(reserve0+i*vector_size,reserve0+(i+1)*vector_size,
                    reserve0+i*vector_size,
                    [&](const int count){ return count+target_size+1; });

            // L2L
            if (!m_query->is_leaf_node(*bucket_it)) { // non leaf node
                for (child_iterator cj = 
                        m_query->get_children(bucket_it.get_child_iterator()); 
                        cj != false; ++cj) {
                    std::transform(reserve0+i*vector_size,reserve0+(i+1)*vector_size,
                               reserve0+i*vector_size,
                        [&](const int count){ return count+vector_size; });
                }
            }
        }

        LOG(2,"creating "<<n<<"x"<<n<<" extended sparse matrix");
        LOG(3,"note: vector_size = "<<vector_size);
        LOG(3,"note: size_W is = "<<size_W);
        LOG(3,"note: size_g is = "<<size_g);
        for (int i = 0; i < n; ++i) {
            LOG(4,"for column "<<i<<", reserving "<<reserve[i]<<" rows");
        }

        // create matrix and reserve space
        sparse_matrix_type A(n,n);
        A.reserve(reserve);
            
        // fill in P2P
        size_t row_index = 0;
        // loop over rows, filling in columns as we go
        for (int i = 0; i < m_target_vector.size(); ++i) {
            for (int j = 0; j < m_strong_connectivity[i].size(); ++j) {
                size_t source_index = m_query->get_bucket_index(*m_strong_connectivity[i][j]);
                // loop over n particles (the n rows in this bucket)
                for (int p = 0; p < m_target_vector[i].size(); ++p) {
                    // p2p - loop over number of source particles (the columns)
                    for (int sp = 0; sp < m_source_vector[source_index].size(); ++sp) {
                        A.insert(row_index+p,m_ext_indicies[source_index]+sp) = m_p2p_matrices[i][j](p,sp);
                    }
                }
            }
            row_index += m_target_vector[i].size();
        }

        // fill in P2M
        // loop over cols this time
        for (int i = 0; i < m_source_vector.size(); ++i) {
            // loop over n particles (the n cols in this bucket)
            for (int p = 0; p < m_source_vector[i].size(); ++p) {
                // p2m - loop over size of multipoles (the rows)
                const size_t row_index = size_x + size_W + i*vector_size;
                for (int m = 0; m < vector_size; ++m) {
                    A.insert(row_index + m, m_ext_indicies[i]+p) = m_p2m_matrices[i](m,p);
                }
                
            }
        }

    
        // fill in W columns
        // m2l
        // loop over rows
        for (int i = 0; i < m_source_vector.size(); ++i) {
            for (int j = 0; j < m_weak_connectivity[i].size(); ++j) {
                const size_t row_index = size_x + i*vector_size;
                const size_t index = m_query->get_bucket_index(*m_weak_connectivity[i][j]);
                const size_t col_index = size_x + index*vector_size;
                for (int im = 0; im < vector_size; ++im) {
                    for (int jm = 0; jm < vector_size; ++jm) {
                        A.insert(row_index+im, col_index+jm) = m_m2l_matrices[i][j](im,jm);
                    }
                }
            }
        }

        // m2m - I or -I
        // looping over rows (parents)
        for (auto bucket_it = subtree_range.begin(); 
                bucket_it != subtree_range.end(); ++bucket_it) {
            const size_t i = m_query->get_bucket_index(*bucket_it); 
            // row is the index of the parent
            const size_t row_index = size_x + size_W + i*vector_size;

            // -I
            const size_t col_index = size_x + i*vector_size;
            for (int im = 0; im < vector_size; ++im) {
                A.insert(row_index+im, col_index+im) = -1;
            }

            // m2m
            if (!m_query->is_leaf_node(*bucket_it)) { // non leaf node
                for (child_iterator cj = 
                        m_query->get_children(bucket_it.get_child_iterator()); 
                     cj != false; ++cj) {
                    const size_t j = m_query->get_bucket_index(*cj); 
                    size_t col_index = size_x + j*vector_size;
                    //std::cout << "inserting at ("<<row_index<<","<<col_index<<")" << std::endl;
                    for (int im = 0; im < vector_size; ++im) {
                        for (int jm = 0; jm < vector_size; ++jm) {
                            A.insert(row_index+im, col_index+jm) = 
                                                        m_l2l_matrices[j](jm,im);
                        }
                    }
                }
            }
        }
         
        // fill in g columns
        // looping over rows
        // L2P
        row_index = 0;
        for (int i = 0; i < m_target_vector.size(); ++i) {
            if (m_source_vector[i].size() > 0) { // leaf node
                size_t col_index = size_x+size_W+i*vector_size;
                for (int p = 0; p < m_target_vector[i].size(); ++p) {
                    for (int m = 0; m < vector_size; ++m) {
                        A.insert(row_index+p, col_index+m) = m_l2p_matrices[i](p,m);
                    }
                }
                row_index += m_target_vector[i].size();
            }
        }
        
        // looping over columns
        for (auto bucket_it = subtree_range.begin(); 
                bucket_it != subtree_range.end(); ++bucket_it) {
            const size_t i = m_query->get_bucket_index(*bucket_it); 
            // col is the index of the parent
            const size_t col_index = size_x + size_W + i*vector_size;
            
            // -I
            const size_t row_index = size_x + i*vector_size;
            for (int im = 0; im < vector_size; ++im) {
                A.insert(row_index+im, col_index+im) = -1;
            }

            // L2L
            if (!m_query->is_leaf_node(*bucket_it)) { // leaf node
                for (child_iterator cj = 
                        m_query->get_children(bucket_it.get_child_iterator()); 
                        cj != false; ++cj) {
                    const size_t j = m_query->get_bucket_index(*cj); 
                    size_t row_index = size_x + j*vector_size;
                    //std::cout << "inserting at ("<<row_index<<","<<col_index<<")" << std::endl;
                    for (int im = 0; im < vector_size; ++im) {
                        for (int jm = 0; jm < vector_size; ++jm) {
                            A.insert(row_index+im, col_index+jm) = 
                                m_l2l_matrices[j](im,jm);
                        }
                    }
                }
            }
        }

        A.makeCompressed();

#ifndef NDEBUG
        for (int i = 0; i < n; ++i) {
            size_t count = 0;
            for (sparse_matrix_type::InnerIterator it(A,i); it ;++it) {
                count++;
            }
            ASSERT(count == reserve[i], "column "<<i<<" reserved size "<<reserve[i]<<" and final count "<<count<<" do not agree");
        }
        row_index = 0;
        size_t col_index = 0;
        /*
        for (int i = 0; i < m_target_vector.size(); ++i) {
            for (int pi = 0; pi < m_target_vector[i].size(); ++pi,++row_index) {
                double sum = 0;
                col_index = 0;
                for (int j = 0; j < m_source_vector.size(); ++j) {
                    for (int pj = 0; pj < m_source_vector[j].size(); ++pj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                //std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_source_vector[j][pj];
                            }
                        }
                    }
                }
                for (int j = 0; j < m_W.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                //std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_W[j][mj];
                            }
                        }

                    }
                }
                for (int j = 0; j < m_g.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                //std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_g[j][mj];
                            }
                        }

                    }
                }
                std::cout << "X: row "<<row_index<<" sum = "<<sum<<" should be "<<m_target_vector[i][pi] << std::endl;
            }
        }
        row_index = size_x;
        for (int i = 0; i < m_W.size(); ++i) {
            for (int mi = 0; mi < m_source_vector.size(); ++mi,++row_index) {
                double sum = 0;
                col_index = 0;
                for (int j = 0; j < m_source_vector.size(); ++j) {
                    for (int pj = 0; pj < m_source_vector[j].size(); ++pj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_source_vector[j][pj];
                            }
                        }
                    }
                }
                for (int j = 0; j < m_W.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_W[j][mj];
                            }
                        }

                    }
                }
                for (int j = 0; j < m_g.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_g[j][mj];
                            }
                        }

                    }
                }
                std::cout << "W: row "<<row_index<<" sum = "<<sum<<" should be "<<0 << std::endl;
            }
        }
        row_index = size_x+size_W;
        for (int i = 0; i < m_g.size(); ++i) {
            for (int mi = 0; mi < m_source_vector.size(); ++mi,++row_index) {
                double sum = 0;
                col_index = 0;
                for (int j = 0; j < m_source_vector.size(); ++j) {
                    for (int pj = 0; pj < m_source_vector[j].size(); ++pj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_source_vector[j][pj];
                            }
                        }
                    }
                }
                for (int j = 0; j < m_W.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_W[j][mj];
                            }
                        }

                    }
                }
                for (int j = 0; j < m_g.size(); ++j) {
                    for (int mj = 0; mj < vector_size; ++mj,++col_index) {
                        for (sparse_matrix_type::InnerIterator it(A,col_index); it ;++it) {
                            if (it.row() == row_index) {
                                std::cout << "("<<it.row()<<","<<it.col()<<") = "<<it.value() << std::endl;
                                sum += it.value()*m_g[j][mj];
                            }
                        }

                    }
                }
                std::cout << "g: row "<<row_index<<" sum = "<<sum<<" should be "<<0 << std::endl;
            }
        }
        */
        

#endif

        return A;

    }

    template <typename VectorTypeSource>
    column_vector_type gen_extended_vector(const VectorTypeSource& source_vector) const {
        const size_t size_x = source_vector.size();
        ASSERT(size_x == m_col_particles->size(),"source vector not same size as column particles");
        const size_t vector_size = m_vector_type::RowsAtCompileTime;
        const size_t size_W = m_W.size()*vector_size;
        const size_t size_g = m_g.size()*vector_size;
        const size_t n = size_x + size_W + size_g;

        column_vector_type extended_vector(n);

        // x
        for (auto& bucket: m_query->get_subtree()) {
            if (m_query->is_leaf_node(bucket)) { // leaf node
                const size_t index = m_query->get_bucket_index(bucket); 
                for (int i = 0; i < m_col_indices[index].size(); ++i) {
                    extended_vector[m_ext_indicies[index]+i] = 
                                                source_vector[m_col_indices[index][i]];
                }
            }
        }
        
        // zero remainder
        for (int i = size_x; i < n; ++i) {
            extended_vector[i] = 0;
            
        }

        return extended_vector;
    }

    column_vector_type get_internal_state() const {
        const size_t size_x = m_ext_indicies.back()+m_source_vector.back().size();
        ASSERT(size_x == m_col_particles->size(),"source vector not same size as column particles");
        const size_t vector_size = m_vector_type::RowsAtCompileTime;
        const size_t size_W = m_W.size()*vector_size;
        const size_t size_g = m_g.size()*vector_size;
        const size_t n = size_x + size_W + size_g;

        column_vector_type extended_vector(n);
        LOG(2,"get_internal_state:");

        // x
        LOG(4,"x");
        for (int i = 0; i < m_source_vector.size(); ++i) {
            for (int p = 0; p < m_source_vector[i].size(); ++p) {
                extended_vector[m_ext_indicies[i]+p] = m_source_vector[i][p]; 
                LOG(4,"row "<<m_ext_indicies[i]+p<<" value "<<extended_vector[m_ext_indicies[i]+p]);
            }
        }

        // W
        LOG(4,"W");
        for (int i = 0; i < m_W.size(); ++i) {
            for (int m = 0; m < vector_size; ++m) {
                extended_vector[size_x+i*vector_size + m] = m_W[i][m];
                LOG(4,"row "<<size_x+i*vector_size+m<<" value "<<extended_vector[size_x+i*vector_size + m]);
            }
            
        }

        // g
        LOG(4,"g");
        for (int i = 0; i < m_g.size(); ++i) {
            for (int m = 0; m < vector_size; ++m) {
                extended_vector[size_x + size_W + i*vector_size + m] = m_g[i][m];
                LOG(4,"row "<<size_x + size_W + i*vector_size + m<<" value "<<extended_vector[size_x + size_W + i*vector_size + m]);
            }
        }

        return extended_vector;
    }

    column_vector_type filter_extended_vector(
            const column_vector_type& extended_vector) const {
        const size_t size_x = m_col_particles->size();
        const size_t vector_size = m_vector_type::RowsAtCompileTime;
        const size_t size_W = m_W.size()*vector_size;
        const size_t size_g = m_g.size()*vector_size;
        const size_t n = size_x + size_W + size_g;

        column_vector_type filtered_vector(size_x);

        // x
        for (auto& bucket: m_query->get_subtree()) {
            if (m_query->is_leaf_node(bucket)) { // leaf node
                const size_t index = m_query->get_bucket_index(bucket); 
                for (int i = 0; i < m_row_indices[index].size(); ++i) {
                    filtered_vector[m_row_indices[index][i]] = 
                                            extended_vector[m_ext_indicies[index]+i];
                }
            }
        }

        return filtered_vector;
    }

        
private:
    template <typename RowParticles>
    void generate_matrices(
            const child_iterator_vector_type& parents_strong_connections,
            const box_type& box_parent, 
            const child_iterator& ci,
            const RowParticles &row_particles,
            const ColParticles &col_particles
            ) {

        const box_type& target_box = m_query->get_bounds(ci);
        size_t target_index = m_query->get_bucket_index(*ci);
        LOG(3,"generate_matrices with bucket "<<target_box);
        
        detail::theta_condition<dimension> theta(target_box.bmin,target_box.bmax);

        // add strongly connected buckets to current connectivity list
        if (parents_strong_connections.empty()) {
            for (child_iterator cj = m_query->get_children(); cj != false; ++cj) {
                const box_type& source_box = m_query->get_bounds(cj);
                if (theta.check(source_box.bmin,source_box.bmax)) {
                    // add strongly connected buckets to current connectivity list
                    m_strong_connectivity[target_index].push_back(cj);
                } else {
                    // from weakly connected buckets, 
                    // add connectivity and generate m2l matricies
                    size_t source_index = m_query->get_bucket_index(*cj);
                    m_m2l_matrices[target_index].emplace_back();
                    m_expansions.M2L_matrix(
                            *(m_m2l_matrices[target_index].end()-1)
                            ,target_box,source_box);
                    m_weak_connectivity[target_index].push_back(cj);

                }
            }
        } else {

            // transfer matrix with parent if not at start
            m_expansions.L2L_matrix(m_l2l_matrices[target_index],target_box,box_parent);

            for (const child_iterator& source: parents_strong_connections) {
                if (m_query->is_leaf_node(*source)) {
                    m_strong_connectivity[target_index].push_back(source);
                } else {
                    for (child_iterator cj = m_query->get_children(source); cj != false; ++cj) {
                        const box_type& source_box = m_query->get_bounds(cj);
                        if (theta.check(source_box.bmin,source_box.bmax)) {
                            m_strong_connectivity[target_index].push_back(cj);
                        } else {
                            size_t source_index = m_query->get_bucket_index(*cj);
                            m_m2l_matrices[target_index].emplace_back();
                            m_expansions.M2L_matrix(
                                    *(m_m2l_matrices[target_index].end()-1)
                                    ,target_box,source_box);
                            m_weak_connectivity[target_index].push_back(cj);

                        }
                    }
                }
            }
        }
        if (!m_query->is_leaf_node(*ci)) { // leaf node
            for (child_iterator cj = m_query->get_children(ci); cj != false; ++cj) {
                generate_matrices(m_strong_connectivity[target_index]
                                    ,target_box,cj,row_particles,col_particles);
            }
        } else {
            m_expansions.P2M_matrix(m_p2m_matrices[target_index], 
                    target_box,
                    m_col_indices[target_index],
                    col_particles);
            m_expansions.L2P_matrix(m_l2p_matrices[target_index], 
                    target_box,
                    m_row_indices[target_index],
                    row_particles);

            
            child_iterator_vector_type strong_copy = m_strong_connectivity[target_index];
            m_strong_connectivity[target_index].clear();
            for (child_iterator& source: strong_copy) {
                if (m_query->is_leaf_node(*source)) {
                    m_p2p_matrices[target_index].emplace_back();
                    m_strong_connectivity[target_index].push_back(source);
                    size_t source_index = m_query->get_bucket_index(*source);
                    m_expansions.P2P_matrix(
                            *(m_p2p_matrices[target_index].end()-1),
                            m_row_indices[target_index],m_col_indices[source_index],
                            row_particles,col_particles);
                } else {
                    auto range = m_query->get_subtree(source);
                    for (all_iterator i = range.begin(); i!=range.end(); ++i) {
                        if (m_query->is_leaf_node(*i)) {
                            m_strong_connectivity[target_index].push_back(i.get_child_iterator());
                            m_p2p_matrices[target_index].emplace_back();
                            size_t index = m_query->get_bucket_index(*i);
                            m_expansions.P2P_matrix(
                                *(m_p2p_matrices[target_index].end()-1),
                                m_row_indices[target_index],m_col_indices[index],
                                row_particles,col_particles);
                        }
                    }
                }
            }
        }
    }

    template <typename RowParticles>
    void generate_row_matrices(
            const child_iterator& ci,
            const RowParticles &row_particles,
            const ColParticles &col_particles
            ) {

        const box_type& target_box = m_query->get_bounds(ci);
        size_t target_index = m_query->get_bucket_index(*ci);
        LOG(3,"generate_row_matrices with bucket "<<target_box);
        
        if (!m_query->is_leaf_node(*ci)) { // leaf node
            for (child_iterator cj = m_query->get_children(ci); cj != false; ++cj) {
                generate_row_matrices(cj,row_particles,col_particles);
            }
        } else {
            m_expansions.L2P_matrix(m_l2p_matrices[target_index], 
                    target_box,
                    m_row_indices[target_index],
                    row_particles);

            for (child_iterator& source: m_strong_connectivity[target_index]) {
                ASSERT(m_query->is_leaf_node(*source),"should be leaf node");
                m_p2p_matrices[target_index].emplace_back();
                size_t source_index = m_query->get_bucket_index(*source);
                m_expansions.P2P_matrix(
                        *(m_p2p_matrices[target_index].end()-1),
                        m_row_indices[target_index],m_col_indices[source_index],
                        row_particles,col_particles);

            }
        }
    }

    m_vector_type& mvm_upward_sweep(const child_iterator& ci) const {
        const size_t my_index = m_query->get_bucket_index(*ci);
        const box_type& target_box = m_query->get_bounds(ci);
        LOG(3,"calculate_dive_P2M_and_M2M with bucket "<<target_box);
        m_vector_type& W = m_W[my_index];
        if (m_query->is_leaf_node(*ci)) { // leaf node
            W = m_p2m_matrices[my_index]*m_source_vector[my_index];
        } else { 
            // do M2M 
            W = m_vector_type::Zero();
            for (child_iterator cj = m_query->get_children(ci); cj != false; ++cj) {
                const size_t child_index = m_query->get_bucket_index(*cj);
                m_vector_type& child_W = mvm_upward_sweep(cj);
                W += m_l2l_matrices[child_index].transpose()*child_W;
            }
        }
        return W;
    }

    void mvm_downward_sweep(
            const m_vector_type& g_parent, 
            const child_iterator& ci) const {
        const box_type& target_box = m_query->get_bounds(ci);
        LOG(3,"calculate_dive_M2L_and_L2L with bucket "<<target_box);
        size_t target_index = m_query->get_bucket_index(*ci);
        m_vector_type& g = m_g[target_index];

        // L2L
        g = m_l2l_matrices[target_index]*g_parent;

        // M2L (weakly connected buckets)
        for (int i = 0; i < m_weak_connectivity[target_index].size(); ++i) {
            const child_iterator& source_ci = m_weak_connectivity[target_index][i];
            size_t source_index = m_query->get_bucket_index(*source_ci);
            g += m_m2l_matrices[target_index][i]*m_W[source_index];
        }

        if (!m_query->is_leaf_node(*ci)) { // dive down to next level
            for (child_iterator cj = m_query->get_children(ci); cj != false; ++cj) {
                mvm_downward_sweep(g,cj);
            }
        } else {
            m_target_vector[target_index] = m_l2p_matrices[target_index]*g;

            // direct evaluation (strongly connected buckets)
            for (int i = 0; i < m_strong_connectivity[target_index].size(); ++i) {
                const child_iterator& source_ci = m_strong_connectivity[target_index][i];
                size_t source_index = m_query->get_bucket_index(*source_ci);
                m_target_vector[target_index] += 
                        m_p2p_matrices[target_index][i]*m_source_vector[source_index];
            }
        }

    }


};


template <typename Expansions, typename RowParticlesType, typename ColParticlesType>
H2Matrix<Expansions,ColParticlesType>
make_h2_matrix(const RowParticlesType& row_particles, const ColParticlesType& col_particles, const Expansions& expansions) {
    return H2Matrix<Expansions,ColParticlesType>(row_particles,col_particles,expansions);
}

}

#endif

