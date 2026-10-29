/*
 * pnet_options.h - the public p-net build options (upstream: pnet_options.h.in).
 */
#ifndef PNET_OPTIONS_H
#define PNET_OPTIONS_H

#ifndef PNET_MAX_PHYSICAL_PORTS
#define PNET_MAX_PHYSICAL_PORTS 1   // one RJ45
#endif

#ifndef PNET_MAX_DIRECTORYPATH_SIZE
#define PNET_MAX_DIRECTORYPATH_SIZE 32   // no directories: the "files" are flash records (files_rp2.c)
#endif

#endif /* PNET_OPTIONS_H */
