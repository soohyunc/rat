#include "config_win32.h"
#include "config_unix.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "repair.h"
#include "sndfile_types.h"
#include "sndfile.h"

#include <stdlib.h>
#include <stdio.h>

/* 
 * test_repair: this test takes an audio file, codes it, and simulates
 * loss on the coded audio, and writes a file of what would be decoded
 * when error concealment is applied.  
 */


static void
usage(void)
{
        fprintf(stderr, "test_repair [options] -c <codec> -r <repair> -d <rate> -s <seed> <src_file> <dst_file>
where options are -codecs and -repairs to list available\n");
        exit(-1);
} 

static void
list_repairs(void)
{
        /* Repair interface is not consistent with other interfaces. 
         * Does not have get_format get_details type fn, just get_name
         * (grumble, grumble, time, grumble) */

        u_int16 i, cnt;

        cnt = repair_get_count();
        for(i = 0; i < cnt; i++) {
                printf("%s\n", repair_get_name(i));
        }

        return;
}

static int
file_and_codec_compatible(const sndfile_fmt_t *sff, codec_id_t cid)
{
        const codec_format_t *cf;
        cf = codec_get_format(cid);
        
        if (cf == NULL) {
                return FALSE;
        }

        if (sff->sample_rate == (u_int32)cf->format.sample_rate &&
            sff->channels    == (u_int32)cf->format.channels) {
                return TRUE;
        } 

        return FALSE;
}

static void
list_codecs(void)
{
        u_int32 i, cnt;
        codec_id_t cid;
        const codec_format_t *cf;

        cnt = codec_get_number_of_codecs();

        for(i = 0; i < cnt; i++) {
                cid = codec_get_codec_number(i);
                cf  = codec_get_format(cid);
                printf("%s\n", cf->long_name);
        }

        return;
}

int 
main(int argc, char *argv[])
{
        const char *codec_name, *repair_name;
        codec_id_t cid;
        int repair_type = -1;
        struct s_sndfile *sf_in, *sf_out;
        sndfile_fmt_t     sff;
        double drop = 0.0;
        int ac, did_query = FALSE;
        long seed;

        codec_init();

        ac = 1;
        while(ac < argc && argv[ac][0] == '-') {
                if (strlen(argv[ac]) > 2) {
                        /* should be -codecs or -repairs */
                        switch(argv[ac][1]) {
                        case 'c':
                                list_codecs();
                                break;
                        case 'r':
                                list_repairs(); 
                                break;
                        default:
                                usage();
                        }
                        did_query = TRUE;
                } else {
                        /* All single arguments take parameters */
                        if (argc - ac < 1) {
                                usage();
                        } 

                        switch(argv[ac][1]) {
                        case 's':
                                seed = atoi(argv[++ac]);
                                break;
                        case 'd':
                                drop = strtod(argv[++ac], NULL);
                                break;
                        case 'c':
                                cid  = codec_get_by_name(argv[++ac]);
                                codec_name = argv[ac];
                                break;
                        case 'r':
                                repair_type = repair_get_by_name(argv[++ac]);
                                repair_name = argv[ac];
                                break;
                        default:
                                usage();
                        }
                }
                ac++;
        }
        
        if (did_query == TRUE) {
                /* Not sensible to be running query and executing test */
                exit(-1);
        }

        if (argc - ac != 2) {
                usage();
        }

        ac++;
        if (snd_read_open(&sf_in, argv[ac], NULL) == FALSE) {
                fprintf(stderr, "Could not open %s\n", argv[ac]);
                exit(-1);
        }

        if (snd_get_format(sf_in, &sff) == FALSE) {
                fprintf(stderr, "Failed to get format of %s\n", argv[ac]);
                exit(-1);
        }

        ac++;
        if (snd_write_open(&sf_out, argv[ac], "au", &sff) == FALSE) {
                fprintf(stderr, "Could not open %s\n", argv[ac]);
                exit(-1);
        }

        if (file_and_codec_compatible(&sff, cid) == FALSE) {
                fprintf(stderr, "Codec and file type are not compatible\n");
                exit(-1);
        }

        if (strcasecmp(repair_name, repair_get_name(repair_type))) {
                fprintf(stderr, "Repair %s not recognized\n", repair_name);
                exit(-1);
        }

        printf("# Parameters
#\tseed: %ld
#\tdrop: %.2f
#\tcodec:  %s
#\trepair: %s
#\tsource file: %s
#\tdestination file %s\n",
seed, drop, codec_name, repair_name, argv[argc - 2], argv[argc - 1]);

        snd_read_close(&sf_in);
        snd_write_close(&sf_in);

        codec_exit();

        return 0;
}
