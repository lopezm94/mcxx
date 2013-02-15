/*--------------------------------------------------------------------
  (C) Copyright 2006-2012 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  See AUTHORS file in the top level directory for information 
  regarding developers and contributors.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.
  
  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/

/*
<testinfo>
test_generator=config/mercurium-serial-simd
</testinfo>
*/

#include <stdio.h>

#define PI 3.141592653589793238462643f

#define FLOAT_TYPE float

/* Array declaration. */

void __attribute__((noinline)) h264(
        FLOAT_TYPE (* X)[28], 
        FLOAT_TYPE (* H)[28], 
        FLOAT_TYPE (* K)[16],
        FLOAT_TYPE (* Y)[16], 
        FLOAT_TYPE (* output)[16])
{
    int i, j;

    for (i = 0; i <= 24; i++)
    {
#pragma omp simd
       for (j = 0; j <= 24; j++)
        {
            X[i][j] = PI * j;
        }
    }
    for (i = 0; i <= 14; i++)
    {
#pragma omp simd
        for (j = 0; j <= 24; j++)
        {
            H[i][j] =
                (X[i][j] + 2.0f * X[i + 1][j] + 4.0f * X[i + 2][j] + 4.0f * X[i + 3][j] +
                 2.0f * X[i + 4][j] + X[i + 5][j]) / 14.0f;
        }
    }
    for (i = 0; i <= 14; i++)
    {
        //NOT VECTORIZABLE YET!
        for (j = 0; j <= 14; j++)
        {
            K[i][j] =
                (H[i][j] + 2.0f * H[i][j + 1] + 4.0f * H[i][j + 2] + 4.0f * H[i][j + 3] +
                 2.0f * H[i][j + 4] + H[i][j + 5]) / 14.0f;
        }
    }
    for (i = 0; i <= 14; i++)
    {
#pragma omp simd 
        for (j = 0; j <= 14; j++)
        {
            Y[i][j] = K[i][j] + H[i][j];
        }
    }
    for (i = 0; i <= 14; i++)
    {
#pragma omp simd
        for (j = 0; j <= 14; j++)
        {
            output[i][j] = Y[i][j];
        }
    }
}

void __attribute__((noinline)) h264_sc(
        FLOAT_TYPE (* X)[28], 
        FLOAT_TYPE (* H)[28], 
        FLOAT_TYPE (* K)[16],
        FLOAT_TYPE (* Y)[16], 
        FLOAT_TYPE (* output)[16])
{
    int i, j;

    for (i = 0; i <= 24; i++)
    {
       for (j = 0; j <= 24; j++)
        {
            X[i][j] = PI * j;
        }
    }
    for (i = 0; i <= 14; i++)
    {
        for (j = 0; j <= 24; j++)
        {
            H[i][j] =
                (X[i][j] + 2.0f * X[i + 1][j] + 4.0f * X[i + 2][j] + 4.0f * X[i + 3][j] +
                 2.0f * X[i + 4][j] + X[i + 5][j]) / 14.0f;
        }
    }
    for (i = 0; i <= 14; i++)
    {
        for (j = 0; j <= 14; j++)
        {
            K[i][j] =
                (H[i][j] + 2.0f * H[i][j + 1] + 4.0f * H[i][j + 2] + 4.0f * H[i][j + 3] +
                 2.0f * H[i][j + 4] + H[i][j + 5]) / 14.0f;
        }
    }
    for (i = 0; i <= 14; i++)
    {
        for (j = 0; j <= 14; j++)
        {
            Y[i][j] = K[i][j] + H[i][j];
        }
    }
    for (i = 0; i <= 14; i++)
    {
        for (j = 0; j <= 14; j++)
        {
            output[i][j] = Y[i][j];
        }
    }
}   

int main ()
{
    FLOAT_TYPE __attribute__((aligned(16))) X[26][28];
    FLOAT_TYPE __attribute__((aligned(16))) H[16][28];
    FLOAT_TYPE __attribute__((aligned(16))) K[16][16];
    FLOAT_TYPE __attribute__((aligned(16))) Y[16][16];
    FLOAT_TYPE __attribute__((aligned(16))) output[16][16];

    FLOAT_TYPE __attribute__((aligned(16))) X_sc[26][28];
    FLOAT_TYPE __attribute__((aligned(16))) H_sc[16][28];
    FLOAT_TYPE __attribute__((aligned(16))) K_sc[16][16];
    FLOAT_TYPE __attribute__((aligned(16))) Y_sc[16][16];
    FLOAT_TYPE __attribute__((aligned(16))) output_sc[16][16];

    int i, j, k;

    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < 16; j++)
        {
            output[i][j] = 0.0f;
            output_sc[i][j] = 0.0f;
        }
    }

    h264(X, H, K, Y, output);
    h264_sc(X_sc, H_sc, K_sc, Y_sc, output_sc);


    for (i=0; i<16*16; i++)
    {
        if(X[i] != X_sc[i])
        {
            printf("ERROR\n");
            exit(1);
        }
        if(H[i] != H_sc[i])
        {
            printf("ERROR\n");
            exit(1);
        }
        if(K[i] != K_sc[i])
        {
            printf("ERROR\n");
            exit(1);
        }
        if(Y[i] != Y_sc[i])
        {
            printf("ERROR\n");
            exit(1);
        }
        if(output[i] != output_sc[i])
        {
            printf("ERROR\n");
            exit(1);
        }
    }

    return 0;
}

