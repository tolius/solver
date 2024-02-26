#include "dz.h"
#include "dictzip.h"
#include "data.h"


extern int dict_data_read(
	dictData *h, unsigned long start, unsigned long size,
	const char *preFilter, const char *postFilter, char* buffer);

static void xfwrite(
	const void *ptr, size_t size, size_t nmemb,
	FILE * stream)
{
	size_t ret = fwrite(ptr, size, nmemb, stream);
	if (ret < nmemb) {
		perror("fwrite(3) failed");
		exit(1);
	}
}

static void xfflush(FILE *stream)
{
	if (fflush(stream) != 0) {
		perror("fflush(3) failed");
		exit(1);
	}
}

static void xfclose(FILE *stream)
{
	if (fclose(stream) != 0) {
		perror("fclose(3) failed");
		exit(1);
	}
}


int dz_pack_file(const char* inFilename, const char* outFilename, 
	int chunkLength, unsigned short flags)
{
	int ret_val;
	
	ret_val = dict_data_zip(inFilename, outFilename, NULL, NULL, NULL, chunkLength, flags);
	if (ret_val)
		err_fatal(__func__, "Compression failed\n");
	return ret_val;
}

int dz_pack(const char* data, unsigned long len_data, const char* outFilename, 
	const char* origFilename, int chunkLength, unsigned short flags)
{
	int ret_val;
	
	ret_val = dict_databuf_zip(data, len_data, outFilename, NULL, NULL, origFilename, chunkLength, flags);
	if (ret_val)
		err_fatal(__func__, "Compression failed\n");
	return ret_val;
}

int dz_unpack(const char* inFilename, const char* outFilename, unsigned long size)
{
	dictData *header;
	FILE *str;
	int len;
	unsigned long j;
	char *buf;

	header = dict_data_open(inFilename, 0);

	if (!(str = fopen(outFilename, "wb")))
	{
		err_fatal_errno(__func__, "Cannot open %s for write\n", outFilename);
		return -1;
	}

	if (!size)
		size = header->length;
	len = header->chunkLength;
	for (j = 0; j < size; j += len)
	{
		if (j + len >= size)
			len = size - j;
		buf = dict_data_read_(header, j, len, NULL, NULL);
		xfwrite(buf, len, 1, str);
		xfflush(str);
		xfree(buf);
	}
	dict_data_close(header);
	return 0;
}

void* dz_open(const char* inFilename)
{
	dictData *header;

	header = dict_data_open(inFilename, 0);
	return header;
}

void dz_close(void* header)
{
	dict_data_close(header);
}

int dz_read(void* header, unsigned long start, unsigned long size, char* buffer)
{
	int errcode = dict_data_read(header, start, size, NULL, NULL, buffer);
	return errcode;
}

void dz_read_all(const char* inFilename, char* buffer)
{
	void* header = dz_open(inFilename);
	unsigned long size = dz_get_orig_length(header);
	dz_read(header, 0, size, buffer);
	dz_close(header);
}

int dz_is_open(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->headerLength;
}

unsigned long dz_get_orig_length(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->length;
}

unsigned long dz_get_compr_length(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->compressedLength;
}

const char* dz_get_orig_filename(void* header)
{
	if (header == NULL)
		return NULL;
	return ((dictData*)header)->origFilename;
}

const char* dz_get_compr_filename(void* header)
{
	if (header == NULL)
		return NULL;
	return ((dictData*)header)->filename;
}

time_t dz_get_orig_time(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->mtime;
}

const char* dz_get_comment(void* header)
{
	if (header == NULL)
		return NULL;
	return ((dictData*)header)->comment;
}

unsigned long dz_get_orig_crc(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->crc;
}

unsigned short dz_get_flags(void* header)
{
	if (header == NULL)
		return 0;
	return ((dictData*)header)->mtime & 0xFFFF;
}

unsigned long dz_calc_crc(const char* data, unsigned long len_data)
{
	unsigned long inputCRC = crc32(0L, Z_NULL, 0);
	inputCRC = crc32(inputCRC, (const Bytef*)data, len_data);
	return inputCRC;
}
