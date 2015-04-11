/*
 * Copyright (c) 2007 - 2015 Joseph Gaeddert
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include "autotest/autotest.h"
#include "liquid.h"

// autotest helper function
//  _sequence_len   :   sequence length
void qdetector_cccf_runtest(unsigned int _sequence_len);

//
// AUTOTESTS
//

void autotest_qdetector_cccf_n64()   { qdetector_cccf_runtest(  64); }
void autotest_qdetector_cccf_n83()   { qdetector_cccf_runtest(  83); }

void autotest_qdetector_cccf_n128()  { qdetector_cccf_runtest( 128); }
void autotest_qdetector_cccf_n167()  { qdetector_cccf_runtest( 167); }

void autotest_qdetector_cccf_n256()  { qdetector_cccf_runtest( 256); }
void autotest_qdetector_cccf_n335()  { qdetector_cccf_runtest( 335); }

void autotest_qdetector_cccf_n512()  { qdetector_cccf_runtest( 512); }
void autotest_qdetector_cccf_n671()  { qdetector_cccf_runtest( 671); }

void autotest_qdetector_cccf_n1024() { qdetector_cccf_runtest(1024); }
void autotest_qdetector_cccf_n1341() { qdetector_cccf_runtest(1341); }

// autotest helper function
//  _sequence_len   :   sequence length
void qdetector_cccf_runtest(unsigned int _sequence_len)
{
    unsigned int k     =     2;     // samples per symbol
    unsigned int m     =     7;     // filter delay [symbols]
    float        beta  =  0.3f;     // excess bandwidth factor
    int          ftype = LIQUID_FIRFILT_ARKAISER; // filter type
    float        gamma =  1.0f;     // channel gain
    float        tau   = -0.3f;     // fractional sample timing offset
    float        dphi  = -0.000f;   // carrier frequency offset (zero for now)
    float        phi   =  0.5f;     // carrier phase offset

    unsigned int i;

    // derived values
    unsigned int num_symbols = 8*_sequence_len + 2*m;
    unsigned int num_samples = k * num_symbols;

    // arrays
    float complex x[num_samples];   // transmitted signal
    float complex y[num_samples];   // received signal

    // generate synchronization sequence (QPSK symbols)
    float complex sequence[_sequence_len];
    for (i=0; i<_sequence_len; i++) {
        sequence[i] = (rand() % 2 ? 1.0f : -1.0f) * M_SQRT1_2 +
                      (rand() % 2 ? 1.0f : -1.0f) * M_SQRT1_2 * _Complex_I;
    }

    // generate transmitted signal
    firinterp_crcf interp = firinterp_crcf_create_rnyquist(ftype, k, m, beta, -tau);
    unsigned int n = 0;
    for (i=0; i<num_symbols; i++) {
        // original sequence, then random symbols
        float complex sym = i < _sequence_len ? sequence[i] : sequence[rand()%_sequence_len];

        // interpolate
        firinterp_crcf_execute(interp, sym, &x[n]);
        n += k;
    }
    firinterp_crcf_destroy(interp);

    // add channel impairments
    for (i=0; i<num_samples; i++) {
        y[i] = x[i];

        // channel gain
        y[i] *= gamma;

        // carrier offset
        y[i] *= cexpf(_Complex_I*(dphi*i + phi));
    }

    // estimates
    float tau_hat   = 0.0f;
    float gamma_hat = 0.0f;
    float dphi_hat  = 0.0f;
    float phi_hat   = 0.0f;
    int   frame_detected = 0;
    int   false_positive = 0;

    // create detector
    qdetector_cccf q = qdetector_cccf_create(sequence, _sequence_len, ftype, k, m, beta);
    if (liquid_autotest_verbose)
        qdetector_cccf_print(q);

    unsigned int buf_len = qdetector_cccf_get_buf_len(q);

    // try to detect frame
    float complex * v = NULL;
    for (i=0; i<num_samples; i++) {
        if (frame_detected)
            break;

        v = qdetector_cccf_execute(q,y[i]);

        if (v != NULL) {
            frame_detected = 1;

            // get statistics
            tau_hat   = qdetector_cccf_get_tau(q);
            gamma_hat = qdetector_cccf_get_gamma(q);
            dphi_hat  = qdetector_cccf_get_dphi(q);
            phi_hat   = qdetector_cccf_get_phi(q);
            break;
        }
    }
    unsigned int sample_index = i;

    // destroy objects
    qdetector_cccf_destroy(q);

    if (liquid_autotest_verbose) {
        // print results
        printf("\n");
        printf("frame detected  :   %s\n", frame_detected ? "yes" : "no");
        printf("  sample index  : %8u, actual=%8u (error=%8d)\n", sample_index, buf_len, (int)sample_index - (int)buf_len);
        printf("  gamma hat     : %8.3f, actual=%8.3f (error=%8.3f)\n",            gamma_hat, gamma, gamma_hat - gamma);
        printf("  tau hat       : %8.3f, actual=%8.3f (error=%8.3f) samples\n",    tau_hat,   tau,   tau_hat   - tau  );
        printf("  dphi hat      : %8.5f, actual=%8.5f (error=%8.5f) rad/sample\n", dphi_hat,  dphi,  dphi_hat  - dphi );
        printf("  phi hat       : %8.5f, actual=%8.5f (error=%8.5f) radians\n",    phi_hat,   phi,   phi_hat   - phi  );
        printf("\n");
    }

    if (false_positive)
        AUTOTEST_FAIL("false positive detected");
    else if (!frame_detected)
        AUTOTEST_FAIL("frame not detected");
    else {
        // check signal level estimate
        CONTEND_DELTA( gamma_hat, gamma, 0.05f );

        // check timing offset estimate
        CONTEND_DELTA( tau_hat, tau, 0.05f );

        // check carrier frequency offset estimate
        CONTEND_DELTA( dphi_hat, dphi, 0.01f );

        // check carrier phase offset estimate
        CONTEND_DELTA( phi_hat, phi, 0.1f );
    }
}


