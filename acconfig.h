/*
 * Define this if your C library doesn't have usleep.
 */
#undef NEED_USLEEP

/*
 * If you don't have these types in <inttypes.h>, #define these to be
 * the types you do have.
 */
#undef int16_t
#undef int32_t
#undef int64_t
#undef int8_t
#undef uint16_t
#undef uint32_t
#undef uint8_t

/*
 * Debugging:
 * DEBUG: general debugging
 * DEBUG_MEM: debug memory allocation
 */
#undef DEBUG
#undef DEBUG_MEM

/*
 * #defines for operating system.
 * THESE WANT TO GO AWAY!
 * Any checks for a specific OS should be replaced by a check for
 * a feature that OS supports or doesn't support.
 */
#undef SunOS
#undef Solaris
#undef Linux
#undef HPUX

/* Audio device relate */
#undef HAVE_SPARC_AUDIO
#undef HAVE_SGI_AUDIO
#undef HAVE_PCA_AUDIO
#undef HAVE_LUIGI_AUDIO
#undef HAVE_OSS_AUDIO
#undef HAVE_HP_AUDIO
#undef HAVE_NETBSD_AUDIO
#undef HAVE_OSPREY_AUDIO
/* FreeBSD Speaker device */
#undef HAVE_MACHINE_PCAUDIOIO_H

@BOTTOM@

#ifndef WORDS_BIGENDIAN
#define WORDS_SMALLENDIAN
#endif
