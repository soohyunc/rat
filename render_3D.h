/*
 * FILE:    render_3D.h
 * PROGRAM: RAT
 * AUTHORS: Marcus Iken
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <math.h>

struct s_render_3D_dbentry;
int render_3D_filter_get_count(void);
char *render_3D_filter_get_name(int id);
int render_3D_filter_get_by_name(char *name);
int render_3D_filter_get_lengths_count(void);
int render_3D_filter_get_length(int idx);
int render_3D_filter_get_lower_azimuth(void);
int render_3D_filter_get_upper_azimuth(void);

struct s_render_3D_dbentry *render_3D_init(session_struct *sp);
void render_3D_free(struct s_render_3D_dbentry **data);
void render_3D(rx_queue_element_struct *el, int no_channels);
void convolve(short *signal, short *answer, double *overlap, double *response, int response_length, int signal_length);
void render_3D_set_parameters(struct s_render_3D_dbentry *p_3D_data, int sampling_rate, int azimuth, int filter_number, int length);
void render_3D_get_parameters(struct s_render_3D_dbentry *p_3D_data, int *azimuth, int *filter_type, int *filter_length);


