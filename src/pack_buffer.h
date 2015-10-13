/*
 *   Serialization code for numeric data.
 *     ... drawn from Beej's guide.
 *     ... modified to prepend SOS_buffer_* to the function signatures.
 *                (2015, Chad Wood)
 *
 *   See: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 */


#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

unsigned long          SOS_buffer_pack754(long double f, unsigned bits, unsigned expbits);
double                 SOS_buffer_unpack754(unsigned long long int i, unsigned bits, unsigned expbits);
void                   SOS_buffer_packi32(unsigned char *buf, int i);
void                   SOS_buffer_packi64(unsigned char *buf, long i);
int                    SOS_buffer_unpacki32(unsigned char *buf);
long                   SOS_buffer_unpacki64(unsigned char *buf);
unsigned long          SOS_buffer_unpacku64(unsigned char *buf);
int                    SOS_buffer_pack(unsigned char *buf, char *format, ...);
int                    SOS_buffer_unpack(unsigned char *buf, char *format, ...);
