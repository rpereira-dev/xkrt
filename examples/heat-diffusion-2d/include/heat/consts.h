/*
** Copyright 2024 INRIA
**
** Contributors :
**
** Romain PEREIRA, romain.pereira@inria.fr
** Romain PEREIRA, rpereira@anl.gov
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

# ifndef __HEAT_CONSTS_H__
#  define __HEAT_CONSTS_H__

////////////////////
// YOU CAN CHANGE //
////////////////////

/* type to use for the temperature */
#  define TYPE float

/* Number of timesteps */
#  define N_STEP 1000

/* Number of vtk images to generate */
#  define N_VTK MIN(10, N_STEP)

/* Thermal diffusivity */
#  define ALPHA (1.11e-4f)

/* Boundary conditions (°C) */
#  define TEMPERATURE_BOUNDARY 100

//////////////////////////////////////////////////
// YOU CAN CHANGE BUT BE CAUTIOUS FOR STABILITY //
//////////////////////////////////////////////////

// TIME STEP FORMULAE IS
//
//  D(i,j) = S(i,j) + ALPHA * DT / (DX * DY) * (
//          (S(i+1,  j) - 2 * S(i,j) + S(i-1,  j)) / (DX * DX) +
//          (S(  i,j+1) - 2 * S(i,j) + S(  i,j-1)) / (DY * DY)
//      );

/* Number of points per dimension in the grid */
#  define NX (32)
//#  define NX (32768+8192)
//#  define NX (4096)
#  define NY NX

/* halo, only (1x1) supported yet */
#  define HX 1
#  define HY 1
static_assert(HX == 1);
static_assert(HY == 1);

/* Size of a cell (m) */
#  define DX (1.0f)
#  define DY (DX)

/* Duration of the simulation (in s.) */
#  define DT (0.5f*(DX*DX * DY*DY) / (2.0f*ALPHA*(DX*DX + DY*DY)))

//////////////////////////////////////////
//  INFERED FROM CONSTS (do not modify) //
//////////////////////////////////////////

#  define DURATION (DT * (TYPE) N_STEP)

# define GRID(G, I, J, L) (G[(J)*L+(I)])
# define LD NX

# endif /* __HEAT_CONSTS_H__ */
