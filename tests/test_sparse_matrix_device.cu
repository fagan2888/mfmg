/*************************************************************************
 * Copyright (c) 2018 by the mfmg authors                                *
 * All rights reserved.                                                  *
 *                                                                       *
 * This file is part of the mfmg libary. mfmg is distributed under a BSD *
 * 3-clause license. For the licensing terms see the LICENSE file in the *
 * top-level directory                                                   *
 *                                                                       *
 * SPDX-License-Identifier: BSD-3-Clause                                 *
 *************************************************************************/

#define BOOST_TEST_MODULE utils

#include "main.cc"

#include <mfmg/sparse_matrix_device.cuh>
#include <mfmg/utils.cuh>

#include <deal.II/lac/la_parallel_vector.h>

#include <set>

template <typename ScalarType>
std::vector<ScalarType> copy_to_host(ScalarType *val_dev,
                                     unsigned int n_elements)
{
  mfmg::ASSERT(n_elements > 0, "Cannot copy an empty array to the host");
  std::vector<ScalarType> val_host(n_elements);
  cudaError_t error_code =
      cudaMemcpy(&val_host[0], val_dev, n_elements * sizeof(ScalarType),
                 cudaMemcpyDeviceToHost);
  mfmg::ASSERT_CUDA(error_code);

  return val_host;
}

BOOST_AUTO_TEST_CASE(serial_mv)
{
  MPI_Comm comm = MPI_COMM_WORLD;
  unsigned int const comm_size = dealii::Utilities::MPI::n_mpi_processes(comm);
  if (comm_size == 1)
  {
    cusparseHandle_t cusparse_handle = nullptr;
    cusparseStatus_t cusparse_error_code;
    cusparse_error_code = cusparseCreate(&cusparse_handle);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);

    // Build the sparse matrix on the host
    unsigned int const size = 10;
    dealii::IndexSet parallel_partitioning(size);
    for (unsigned int i = 0; i < size; ++i)
      parallel_partitioning.add_index(i);
    parallel_partitioning.compress();
    dealii::TrilinosWrappers::SparseMatrix sparse_matrix(parallel_partitioning);

    unsigned int nnz = 0;
    for (unsigned int i = 0; i < size; ++i)
    {
      std::default_random_engine generator(i);
      std::uniform_int_distribution<int> distribution(0, size - 1);
      std::set<int> column_indices;
      for (unsigned int j = 0; j < 5; ++j)
      {
        int column_index = distribution(generator);
        sparse_matrix.set(i, column_index, static_cast<double>(i + j));
        column_indices.insert(column_index);
      }
      nnz += column_indices.size();
    }
    sparse_matrix.compress(dealii::VectorOperation::insert);

    // Move the sparse matrix to the device and change the format to a regular
    // CSR
    mfmg::SparseMatrixDevice<double> sparse_matrix_dev =
        mfmg::convert_matrix(sparse_matrix);
    cusparseMatDescr_t descr;
    cusparse_error_code = cusparseCreateMatDescr(&descr);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    cusparse_error_code =
        cusparseSetMatType(descr, CUSPARSE_MATRIX_TYPE_GENERAL);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    cusparse_error_code =
        cusparseSetMatIndexBase(descr, CUSPARSE_INDEX_BASE_ZERO);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    sparse_matrix_dev.descr = descr;
    sparse_matrix_dev.cusparse_handle = cusparse_handle;

    // Build a vector on the host
    dealii::LinearAlgebra::distributed::Vector<double> vector(
        parallel_partitioning, comm);
    unsigned int vector_local_size = vector.local_size();
    for (unsigned int i = 0; i < vector_local_size; ++i)
      vector[i] = i;

    // Move the vector to the device
    double *vector_val_dev;
    cudaError_t cuda_error_code =
        cudaMalloc(&vector_val_dev, vector_local_size * sizeof(double));
    mfmg::ASSERT_CUDA(cuda_error_code);
    mfmg::VectorDevice<double> vector_dev(vector_val_dev,
                                          vector.get_partitioner());
    cuda_error_code =
        cudaMemcpy(vector_dev.val_dev, vector.begin(),
                   vector_local_size * sizeof(double), cudaMemcpyHostToDevice);
    mfmg::ASSERT_CUDA(cuda_error_code);

    // Perform the matrix-vector multiplication on the host
    dealii::LinearAlgebra::distributed::Vector<double> result(vector);
    sparse_matrix.vmult(result, vector);

    // Perform the matrix-vector multiplication on the host
    double *result_val_dev;
    cuda_error_code =
        cudaMalloc(&result_val_dev, vector_local_size * sizeof(double));
    mfmg::ASSERT_CUDA(cuda_error_code);
    mfmg::VectorDevice<double> result_dev(result_val_dev,
                                          vector.get_partitioner());

    sparse_matrix_dev.vmult(result_dev, vector_dev);

    // Check the result
    std::vector<double> result_host =
        copy_to_host(result_dev.val_dev, vector_local_size);
    for (unsigned int i = 0; i < vector_local_size; ++i)
      BOOST_CHECK_CLOSE(result[i], result_host[i], 1e-14);

    // Destroy cusparse_handle
    cusparse_error_code = cusparseDestroy(cusparse_handle);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    cusparse_handle = nullptr;
  }
}

BOOST_AUTO_TEST_CASE(distributed_mv)
{
  // We assume that the user launched as many processes as there are gpus,
  // that each node as the same number of GPUS, and that each node has at  least
  // two GPUs. The reason for the last assumption is to make sure that the test
  // runs on the tester but not on desktop or laptop that have only one GPU.
  int n_devices = 0;
  cudaError_t cuda_error_code = cudaGetDeviceCount(&n_devices);
  mfmg::ASSERT_CUDA(cuda_error_code);

  cusparseHandle_t cusparse_handle = nullptr;
  cusparseStatus_t cusparse_error_code;
  cusparse_error_code = cusparseCreate(&cusparse_handle);
  mfmg::ASSERT_CUSPARSE(cusparse_error_code);

  if (n_devices > 1)
  {
    MPI_Comm comm = MPI_COMM_WORLD;
    unsigned int const comm_size =
        dealii::Utilities::MPI::n_mpi_processes(comm);
    unsigned int const rank = dealii::Utilities::MPI::this_mpi_process(comm);

    // Set the device for each process
    int device_id = rank % n_devices;
    cuda_error_code = cudaSetDevice(device_id);

    // Build the sparse matrix on the host
    unsigned int const n_local_rows = 10;
    unsigned int const row_offset = rank * n_local_rows;
    unsigned int const size = comm_size * n_local_rows;
    dealii::IndexSet parallel_partitioning(size);
    for (unsigned int i = 0; i < n_local_rows; ++i)
      parallel_partitioning.add_index(row_offset + i);
    parallel_partitioning.compress();
    dealii::TrilinosWrappers::SparseMatrix sparse_matrix(parallel_partitioning);

    unsigned int nnz = 0;
    for (unsigned int i = 0; i < n_local_rows; ++i)
    {
      std::default_random_engine generator(i);
      std::uniform_int_distribution<int> distribution(0, size - 1);
      std::set<int> column_indices;
      for (unsigned int j = 0; j < 5; ++j)
      {
        int column_index = distribution(generator);
        sparse_matrix.set(row_offset + i, column_index,
                          static_cast<double>(i + j));
        column_indices.insert(column_index);
      }
      nnz += column_indices.size();
    }
    sparse_matrix.compress(dealii::VectorOperation::insert);

    // Move the sparse matrix to the device and change the format to a regular
    // CSR
    mfmg::SparseMatrixDevice<double> sparse_matrix_dev =
        mfmg::convert_matrix(sparse_matrix);
    cusparseMatDescr_t descr;
    cusparse_error_code = cusparseCreateMatDescr(&descr);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    cusparse_error_code =
        cusparseSetMatType(descr, CUSPARSE_MATRIX_TYPE_GENERAL);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    cusparse_error_code =
        cusparseSetMatIndexBase(descr, CUSPARSE_INDEX_BASE_ZERO);
    mfmg::ASSERT_CUSPARSE(cusparse_error_code);
    sparse_matrix_dev.descr = descr;
    sparse_matrix_dev.cusparse_handle = cusparse_handle;

    // Build a vector on the host
    dealii::LinearAlgebra::distributed::Vector<double> vector(
        parallel_partitioning, comm);
    unsigned int vector_local_size = vector.local_size();
    for (unsigned int i = 0; i < vector_local_size; ++i)
      vector.local_element(i) = i;

    // Move the vector to the device
    double *vector_val_dev;
    cuda_error_code =
        cudaMalloc(&vector_val_dev, vector_local_size * sizeof(double));
    mfmg::ASSERT_CUDA(cuda_error_code);
    mfmg::VectorDevice<double> vector_dev(vector_val_dev,
                                          vector.get_partitioner());
    cuda_error_code =
        cudaMemcpy(vector_dev.val_dev, vector.begin(),
                   vector_local_size * sizeof(double), cudaMemcpyHostToDevice);
    mfmg::ASSERT_CUDA(cuda_error_code);

    // Perform the matrix-vector multiplication on the host
    dealii::LinearAlgebra::distributed::Vector<double> result(vector);
    sparse_matrix.vmult(result, vector);

    // Perform the matrix-vector multiplication on the host
    double *result_val_dev;
    cuda_error_code =
        cudaMalloc(&result_val_dev, vector_local_size * sizeof(double));
    mfmg::ASSERT_CUDA(cuda_error_code);
    mfmg::VectorDevice<double> result_dev(result_val_dev,
                                          vector.get_partitioner());

    sparse_matrix_dev.vmult(result_dev, vector_dev);

    // Check the result
    std::vector<double> result_host =
        copy_to_host(result_dev.val_dev, vector_local_size);
    for (unsigned int i = 0; i < vector_local_size; ++i)
      BOOST_CHECK_CLOSE(result.local_element(i), result_host[i], 1e-14);
  }

  // Destroy cusparse_handle
  cusparse_error_code = cusparseDestroy(cusparse_handle);
  mfmg::ASSERT_CUSPARSE(cusparse_error_code);
  cusparse_handle = nullptr;
}