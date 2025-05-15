#ifndef SRC_ENDIAN_H
#define SRC_ENDIAN_H

#ifdef linux
#include <endian.h>
#else

#include <machine/endian.h>

#define le16toh(x) 

#endif


#endif