#ifndef _DZ_H_
#define _DZ_H_

#include <time.h>


int dz_pack_file(const char* inFilename, const char* outFilename, int chunkLength, unsigned short flags);
int dz_pack(const char* data, unsigned long len_data, const char* outFilename, 
	const char* origFilename, int chunkLength, unsigned short flags);
int dz_unpack(const char* inFilename, const char* outFilename, unsigned long size); // if size == 0, use length from header
void* dz_open(const char* inFilename);
void dz_close(void* header);
int dz_read(void* header, unsigned long start, unsigned long size, char* buffer);
void dz_read_all(const char* inFilename, char* buffer);
int dz_is_open(void* header);
unsigned long dz_get_orig_length(void* header);
unsigned long dz_get_compr_length(void* header);
const char* dz_get_orig_filename(void* header);
const char* dz_get_compr_filename(void* header);
time_t dz_get_orig_time(void* header);
const char* dz_get_comment(void* header);
unsigned long dz_get_orig_crc(void* header);
unsigned short dz_get_flags(void* header);
unsigned long dz_calc_crc(const char* data, unsigned long len_data);

#endif