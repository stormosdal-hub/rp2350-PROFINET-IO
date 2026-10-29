/*
 * pnet_export.h - symbol export macros (upstream: CMake generate_export_header). A static
 * library exports nothing special.
 */
#ifndef PNET_EXPORT_H
#define PNET_EXPORT_H

#define PNET_EXPORT
#define PNET_NO_EXPORT
#define PNET_DEPRECATED        __attribute__((__deprecated__))
#define PNET_DEPRECATED_EXPORT PNET_EXPORT PNET_DEPRECATED
#define PNET_DEPRECATED_NO_EXPORT PNET_NO_EXPORT PNET_DEPRECATED

#endif /* PNET_EXPORT_H */
