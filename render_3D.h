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
