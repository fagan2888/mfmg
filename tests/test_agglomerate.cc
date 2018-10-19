/*************************************************************************
 * Copyright (c) 2017-2018 by the mfmg authors                           *
 * All rights reserved.                                                  *
 *                                                                       *
 * This file is part of the mfmg libary. mfmg is distributed under a BSD *
 * 3-clause license. For the licensing terms see the LICENSE file in the *
 * top-level directory                                                   *
 *                                                                       *
 * SPDX-License-Identifier: BSD-3-Clause                                 *
 *************************************************************************/

#define BOOST_TEST_MODULE agglomerate

#include <mfmg/dealii/amge_host.hpp>
#include <mfmg/dealii/dealii_mesh_evaluator.hpp>

#include <deal.II/distributed/tria.h>
#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/lac/trilinos_vector.h>
#include <deal.II/numerics/data_out.h>

#include <array>

#include "main.cc"

template <int dim>
std::vector<unsigned int> test(MPI_Comm const &world)
{
  dealii::parallel::distributed::Triangulation<dim> triangulation(world);
  dealii::FE_Q<dim> fe(1);
  dealii::DoFHandler<dim> dof_handler(triangulation);

  dealii::GridGenerator::hyper_cube(triangulation);
  triangulation.refine_global(3);
  dof_handler.distribute_dofs(fe);

  using Vector = dealii::TrilinosWrappers::MPI::Vector;
  using DummyMeshEvaluator = mfmg::DealIIMeshEvaluator<dim>;

  mfmg::AMGe_host<dim, DummyMeshEvaluator, Vector> amge(world, dof_handler);

  boost::property_tree::ptree partitioner_params;
  partitioner_params.put("partitioner", "block");
  partitioner_params.put("nx", 2);
  partitioner_params.put("ny", 3);
  partitioner_params.put("nz", 4);

  amge.build_agglomerates(partitioner_params);

  std::vector<unsigned int> agglomerates;
  agglomerates.reserve(dof_handler.get_triangulation().n_active_cells());
  for (auto cell : dof_handler.active_cell_iterators())
    agglomerates.push_back(cell->user_index());

  return agglomerates;
}

BOOST_AUTO_TEST_CASE(simple_agglomerate_2d)
{
  std::vector<unsigned int> agglomerates = test<2>(MPI_COMM_WORLD);

  unsigned int world_size =
      dealii::Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);
  unsigned int world_rank =
      dealii::Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
  std::vector<unsigned int> ref_agglomerates;
  if (world_size == 1)
    ref_agglomerates = {{1, 1, 1, 1, 2,  2,  2,  2,  1,  1,  3,  3, 2,
                         2, 4, 4, 5, 5,  5,  5,  6,  6,  6,  6,  5, 5,
                         7, 7, 6, 6, 8,  8,  3,  3,  3,  3,  4,  4, 4,
                         4, 9, 9, 9, 9,  10, 10, 10, 10, 7,  7,  7, 7,
                         8, 8, 8, 8, 11, 11, 11, 11, 12, 12, 12, 12}};
  else if (world_size == 2)
  {
    if (world_rank == 0)
      ref_agglomerates = {{0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 3, 3, 2, 2,
                           4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 5, 5, 7, 7, 6, 6, 8, 8,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    else
      ref_agglomerates = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 3, 3, 2, 2, 4, 4,
                           5, 5, 5, 5, 6, 6, 6, 6, 5, 5, 7, 7, 6, 6, 8, 8}};
  }
  else if (world_size == 4)
  {
    if (world_rank == 0)
      ref_agglomerates = {{0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                           1, 1, 3, 3, 2, 2, 4, 4, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    if (world_rank == 1)
      ref_agglomerates = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 3, 3, 2, 2, 4,
                           4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    if (world_rank == 2)
      ref_agglomerates = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 3,
                           3, 2, 2, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0}};

    if (world_rank == 3)
      ref_agglomerates = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                           1, 2, 2, 2, 2, 1, 1, 3, 3, 2, 2, 4, 4}};
  }

  BOOST_TEST(agglomerates == ref_agglomerates);
}

BOOST_AUTO_TEST_CASE(simple_agglomerate_3d)
{
  unsigned int world_size =
      dealii::Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);
  unsigned int world_rank =
      dealii::Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
  std::vector<unsigned int> agglomerates = test<3>(MPI_COMM_WORLD);
  std::vector<unsigned int> ref_agglomerates;
  if (world_size == 1)
  {
    ref_agglomerates = {
        {1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,
         3,  3,  1,  1,  3,  3,  2,  2,  4,  4,  2,  2,  4,  4,  1,  1,  1,  1,
         1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  3,  3,  1,  1,
         3,  3,  2,  2,  4,  4,  2,  2,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,
         6,  6,  6,  6,  6,  6,  6,  6,  5,  5,  7,  7,  5,  5,  7,  7,  6,  6,
         8,  8,  6,  6,  8,  8,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,
         6,  6,  6,  6,  5,  5,  7,  7,  5,  5,  7,  7,  6,  6,  8,  8,  6,  6,
         8,  8,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,
         9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 3,  3,
         3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  9,  9,  9,  9,
         9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 7,  7,  7,  7,  7,  7,
         7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  11, 11, 11, 11, 11, 11, 11, 11,
         12, 12, 12, 12, 12, 12, 12, 12, 7,  7,  7,  7,  7,  7,  7,  7,  8,  8,
         8,  8,  8,  8,  8,  8,  11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12,
         12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
         14, 14, 13, 13, 15, 15, 13, 13, 15, 15, 14, 14, 16, 16, 14, 14, 16, 16,
         13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 13, 13,
         15, 15, 13, 13, 15, 15, 14, 14, 16, 16, 14, 14, 16, 16, 17, 17, 17, 17,
         17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 17, 17, 19, 19, 17, 17,
         19, 19, 18, 18, 20, 20, 18, 18, 20, 20, 17, 17, 17, 17, 17, 17, 17, 17,
         18, 18, 18, 18, 18, 18, 18, 18, 17, 17, 19, 19, 17, 17, 19, 19, 18, 18,
         20, 20, 18, 18, 20, 20, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16,
         16, 16, 16, 16, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22,
         22, 22, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
         21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 19, 19,
         19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 23, 23, 23, 23,
         23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 19, 19, 19, 19, 19, 19,
         19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 23, 23, 23, 23, 23, 23, 23, 23,
         24, 24, 24, 24, 24, 24, 24, 24}};
  }
  else if (world_size == 2)
  {
    if (world_rank == 0)
      ref_agglomerates = {
          {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
           1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,
           3,  3,  1,  1,  3,  3,  2,  2,  4,  4,  2,  2,  4,  4,  1,  1,  1,
           1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  3,  3,
           1,  1,  3,  3,  2,  2,  4,  4,  2,  2,  4,  4,  5,  5,  5,  5,  5,
           5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  5,  5,  7,  7,  5,  5,
           7,  7,  6,  6,  8,  8,  6,  6,  8,  8,  5,  5,  5,  5,  5,  5,  5,
           5,  6,  6,  6,  6,  6,  6,  6,  6,  5,  5,  7,  7,  5,  5,  7,  7,
           6,  6,  8,  8,  6,  6,  8,  8,  3,  3,  3,  3,  3,  3,  3,  3,  4,
           4,  4,  4,  4,  4,  4,  4,  9,  9,  9,  9,  9,  9,  9,  9,  10, 10,
           10, 10, 10, 10, 10, 10, 3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
           4,  4,  4,  4,  4,  9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10,
           10, 10, 10, 10, 7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,
           8,  8,  8,  11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
           12, 12, 7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,
           8,  11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0}};
    else
      ref_agglomerates = {
          {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
           0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
           2,  2,  2,  2,  2,  2,  2,  1,  1,  3,  3,  1,  1,  3,  3,  2,  2,
           4,  4,  2,  2,  4,  4,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
           2,  2,  2,  2,  2,  1,  1,  3,  3,  1,  1,  3,  3,  2,  2,  4,  4,
           2,  2,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,
           6,  6,  6,  5,  5,  7,  7,  5,  5,  7,  7,  6,  6,  8,  8,  6,  6,
           8,  8,  5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,
           6,  5,  5,  7,  7,  5,  5,  7,  7,  6,  6,  8,  8,  6,  6,  8,  8,
           3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  9,
           9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 3,  3,
           3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  9,  9,  9,
           9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 7,  7,  7,  7,
           7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  11, 11, 11, 11, 11,
           11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 7,  7,  7,  7,  7,  7,
           7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  11, 11, 11, 11, 11, 11, 11,
           11, 12, 12, 12, 12, 12, 12, 12, 12}};
  }
  else if (world_size == 4)
  {
    if (world_rank == 0)
      ref_agglomerates = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
           3, 3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2,
           2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4,
           5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 7, 7, 5, 5, 7,
           7, 6, 6, 8, 8, 6, 6, 8, 8, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6,
           6, 6, 5, 5, 7, 7, 5, 5, 7, 7, 6, 6, 8, 8, 6, 6, 8, 8, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    if (world_rank == 1)
      ref_agglomerates = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 3, 1, 1, 3,
           3, 2, 2, 4, 4, 2, 2, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
           2, 2, 1, 1, 3, 3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4, 5, 5, 5, 5, 5,
           5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 7, 7, 5, 5, 7, 7, 6, 6, 8, 8,
           6, 6, 8, 8, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 7,
           7, 5, 5, 7, 7, 6, 6, 8, 8, 6, 6, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    if (world_rank == 2)
      ref_agglomerates = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
           2, 2, 1, 1, 3, 3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4, 1, 1, 1, 1, 1,
           1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 3, 1, 1, 3, 3, 2, 2, 4, 4,
           2, 2, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 7,
           7, 5, 5, 7, 7, 6, 6, 8, 8, 6, 6, 8, 8, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6,
           6, 6, 6, 6, 6, 6, 5, 5, 7, 7, 5, 5, 7, 7, 6, 6, 8, 8, 6, 6, 8, 8, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    if (world_rank == 3)
      ref_agglomerates = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
           0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3,
           3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
           2, 2, 2, 2, 2, 2, 1, 1, 3, 3, 1, 1, 3, 3, 2, 2, 4, 4, 2, 2, 4, 4, 5,
           5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 7, 7, 5, 5, 7, 7,
           6, 6, 8, 8, 6, 6, 8, 8, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6,
           6, 5, 5, 7, 7, 5, 5, 7, 7, 6, 6, 8, 8, 6, 6, 8, 8}};
  }

  BOOST_TEST(agglomerates == ref_agglomerates);
}

BOOST_AUTO_TEST_CASE(zoltan_agglomerate_2d)
{
  int constexpr dim = 2;
  unsigned int world_size =
      dealii::Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);
  unsigned int world_rank =
      dealii::Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
  dealii::parallel::distributed::Triangulation<dim> triangulation(
      MPI_COMM_WORLD);
  dealii::FE_Q<dim> fe(1);
  dealii::DoFHandler<dim> dof_handler(triangulation);

  dealii::GridGenerator::hyper_cube(triangulation);
  triangulation.refine_global(3);
  dof_handler.distribute_dofs(fe);

  using Vector = dealii::TrilinosWrappers::MPI::Vector;
  using DummyMeshEvaluator = mfmg::DealIIMeshEvaluator<dim>;

  mfmg::AMGe_host<dim, DummyMeshEvaluator, Vector> amge(MPI_COMM_WORLD,
                                                        dof_handler);

  boost::property_tree::ptree partitioner_params;
  partitioner_params.put("partitioner", "zoltan");
  partitioner_params.put("n_agglomerates", 3);
  amge.build_agglomerates(partitioner_params);

  std::vector<unsigned int> agglomerates;
  agglomerates.reserve(dof_handler.get_triangulation().n_active_cells());
  for (auto cell : dof_handler.active_cell_iterators())
    agglomerates.push_back(cell->user_index());

  std::vector<unsigned int> ref_agglomerates;
  if (world_size == 1)
  {
    ref_agglomerates = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                        1, 1, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
                        2, 2, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};
  }
  else if (world_size == 2)
  {
    if (world_rank == 0)
      ref_agglomerates = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2,
                          1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    else
      ref_agglomerates = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 2,
                          2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
  }
  else if (world_size == 4)
  {
    if (world_rank == 0)
      ref_agglomerates = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                          1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (world_rank == 1)
      ref_agglomerates = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                          2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (world_rank == 2)
      ref_agglomerates = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                          1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0};
    if (world_rank == 3)
      ref_agglomerates = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
  }

  BOOST_TEST(agglomerates == ref_agglomerates);
}
