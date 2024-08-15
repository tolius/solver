/* dictzip.c -- 
 * Created: Tue Jul 16 12:45:41 1996 by faith@dict.org
 * Copyright 1996-1998, 2000, 2002 Rickard E. Faith (faith@dict.org)
 * Copyright 2002-2008 Aleksey Cheusov (vle@gmx.net)
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 1, or (at your option) any
 * later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "dictzip.h"
#include "data.h"

#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#ifndef max
#define max(a,b) \
       ({ typeof (a) _a = (a); \
           typeof (b) _b = (b); \
         _a > _b ? _a : _b; })
#endif
#ifndef min
#define min(a,b) \
       ({ typeof (a) _a = (a); \
           typeof (b) _b = (b); \
         _a < _b ? _a : _b; })
#endif

int fileno(FILE *stream);

static void xfwrite(
   const void *ptr, size_t size, size_t nmemb,
   FILE * stream)
{
   size_t ret = fwrite(ptr, size, nmemb, stream);
   if (ret < nmemb){
      perror("fwrite(3) failed");
      exit ( 1 );
   }
}

static void xfflush(FILE *stream)
{
   if (fflush(stream) != 0){
      perror("fflush(3) failed");
      exit ( 1 );
   }
}

static void xfclose(FILE *stream)
{
   if (fclose(stream) != 0){
      perror("fclose(3) failed");
      exit ( 1 );
   }
}

void dict_data_print_header(FILE *str, dictData *header)
{
	char        *date, *year;
	long        ratio, num, den;
	static int  first = 1;

	if (first) {
		fprintf(str,
			"type   crc        date    time chunks  size     compr."
			"  uncompr. ratio name\n");
		first = 0;
	}

	switch (header->type) {
	case DICT_TEXT:
		date = ctime(&header->mtime) + 4; /* no day of week */
		date[12] = date[20] = '\0'; /* no year or newline*/
		year = &date[16];
		fprintf(str, "text %08lx %s %11s ", header->crc, year, date);
		fprintf(str, "            ");
		fprintf(str, "          %9ld ", header->length);
		fprintf(str, "  0.0%% %s",
			header->origFilename ? header->origFilename : "");
		putc('\n', str);
		break;
	case DICT_GZIP:
	case DICT_DZIP:
		fprintf(str, "%s", header->type == DICT_DZIP ? "dzip " : "gzip ");
#if 0
		switch (header->method) {
		case 0:  fprintf(str, "store"); break;
		case 1:  fprintf(str, "compr"); break;
		case 2:  fprintf(str, "pack "); break;
		case 3:  fprintf(str, "lzh  "); break;
		case 8:  fprintf(str, "defla"); break;
		default: fprintf(str, "?    "); break;
		}
#endif
		date = ctime(&header->mtime) + 4; /* no day of week */
		date[12] = date[20] = '\0'; /* no year or newline*/
		year = &date[16];
		fprintf(str, "%08lx %s %11s ", header->crc, year, date);
		if (header->type == DICT_DZIP) {
			fprintf(str, "%5d %5d ", header->chunkCount, header->chunkLength);
		}
		else {
			fprintf(str, "            ");
		}
		fprintf(str, "%9ld %9ld ", header->compressedLength, header->length);
		/* Algorithm for calculating ratio from gzip-1.2.4,
		   util.c:display_ratio Copyright (C) 1992-1993 Jean-loup Gailly.
		   May be distributed under the terms of the GNU General Public
		   License. */
		num = header->length - (header->compressedLength - header->headerLength);
		den = header->length;
		if (!den)
			ratio = 0;
		else if (den < 2147483L)
			ratio = 1000L * num / den;
		else
			ratio = num / (den / 1000L);
		if (ratio < 0) {
			putc('-', str);
			ratio = -ratio;
		}
		else putc(' ', str);
		fprintf(str, "%2ld.%1ld%%", ratio / 10L, ratio % 10L);
		fprintf(str, " %s",
			header->origFilename ? header->origFilename : "");
		putc('\n', str);
		break;
	case DICT_UNKNOWN:
	default:
		break;
	}
}

int dict_data_zip(const char *inFilename, const char *outFilename, const char *preFilter, 
	const char *postFilter, const char *origFilename, int chunkLength, unsigned short formatVersion)
{
	char          inBuffer[IN_BUFFER_SIZE];
	char          outBuffer[OUT_BUFFER_SIZE];
	int           count;
	unsigned long inputCRC = crc32(0L, Z_NULL, 0);
	z_stream      zStream;
	FILE          *outStr;
	FILE          *inStr;
	int           len;
#ifdef _WIN32
	struct __stat64   st;
#else
	struct stat   st;
#endif
	char          *header;
	int           headerLength;
	int           dataLength;
	int           extraLength;
//	int           chunkLength;
#if HEADER_CRC
	int           headerCRC;
#endif
	unsigned long chunks;
	unsigned long chunk = 0;
	unsigned long total = 0;
	int           i;
	char          tail[8];
	char          *pt;
	int is_name_provided = (origFilename != NULL);


	/* Open files */
	if (!(inStr = fopen(inFilename, "rb")))
		err_fatal_errno(__func__,
			"Cannot open \"%s\" for read\n", inFilename);
	if (!(outStr = fopen(outFilename, "wb")))
		err_fatal_errno(__func__,
			"Cannot open \"%s\"for write\n", outFilename);

	if (!is_name_provided)
	{
		origFilename = xmalloc(strlen(inFilename) + 1);
		pt = max(strrchr(inFilename, '/'), strrchr(inFilename, '\\'));
		if (pt)
			strcpy(origFilename, pt + 1);
		else
			strcpy(origFilename, inFilename);
	}

	/* Initialize compression engine */
	zStream.zalloc = NULL;
	zStream.zfree = NULL;
	zStream.opaque = NULL;
	zStream.next_in = NULL;
	zStream.avail_in = 0;
	zStream.next_out = NULL;
	zStream.avail_out = 0;
	if (deflateInit2(&zStream,
		Z_BEST_COMPRESSION,
		Z_DEFLATED,
		-15,	/* Suppress zlib header */
		Z_BEST_COMPRESSION,
		Z_DEFAULT_STRATEGY) != Z_OK)
		err_internal(__func__,
			"Cannot initialize deflation engine: %s\n", zStream.msg);

	/* Write initial header information */
	if (chunkLength <= 0)
		chunkLength = (preFilter ? PREFILTER_IN_BUFFER_SIZE : IN_BUFFER_SIZE);
#ifdef _WIN32
	_fstat64(fileno(inStr), &st);
#else
	fstat(fileno(inStr), &st);
#endif
	chunks = st.st_size / chunkLength;
	if (st.st_size % chunkLength) ++chunks;
	PRINTF(DBG_VERBOSE, ("%lu chunks * %u per chunk = %lu (filesize = %lu)\n",
		chunks, chunkLength, chunks * chunkLength,
		(unsigned long)st.st_size));
	dataLength = chunks * 2;

	extraLength = 10 + dataLength;
	if (extraLength > 0xFFFF) {
		fprintf(stderr, "\nFile too long: %lu chunks needed, 32762 allowed\n", chunks);
		fclose(inStr);
		fclose(outStr);
		unlink(outFilename);
		return -1;
	}

	headerLength = GZ_FEXTRA_START
		+ extraLength		/* FEXTRA */
		+ strlen(origFilename) + 1	/* FNAME  */
		+ (HEADER_CRC ? 2 : 0);	/* FHCRC  */
	PRINTF(DBG_VERBOSE, ("(data = %d, extra = %d, header = %d)\n",
		dataLength, extraLength, headerLength));
	header = xmalloc(headerLength);
	for (i = 0; i < headerLength; i++) header[i] = 0;
	header[GZ_ID1] = GZ_MAGIC1;
	header[GZ_ID2] = GZ_MAGIC2;
	header[GZ_CM] = Z_DEFLATED;
	header[GZ_FLG] = GZ_FEXTRA | GZ_FNAME;
#if HEADER_CRC
	header[GZ_FLG] |= GZ_FHCRC;
#endif
	header[GZ_MTIME + 3] = (st.st_mtime & 0xff000000) >> 24;
	header[GZ_MTIME + 2] = (st.st_mtime & 0x00ff0000) >> 16;
	header[GZ_MTIME + 1] = (formatVersion == 0xFFFF) ? (st.st_mtime & 0x0000ff00) >> 8 : (formatVersion & 0xFF00) >> 8;
	header[GZ_MTIME + 0] = (formatVersion == 0xFFFF) ? (st.st_mtime & 0x000000ff) >> 0 : (formatVersion & 0xFF);
	header[GZ_XFL] = GZ_MAX;
	header[GZ_OS] = GZ_OS_UNIX;
	header[GZ_XLEN + 1] = (extraLength & 0xff00) >> 8;
	header[GZ_XLEN + 0] = (extraLength & 0x00ff) >> 0;
	header[GZ_SI1] = GZ_RND_S1;
	header[GZ_SI2] = GZ_RND_S2;
	header[GZ_SUBLEN + 1] = ((extraLength - 4) & 0xff00) >> 8;
	header[GZ_SUBLEN + 0] = ((extraLength - 4) & 0x00ff) >> 0;
	header[GZ_VERSION + 1] = 0;
	header[GZ_VERSION + 0] = 1;
	header[GZ_CHUNKLEN + 1] = (chunkLength & 0xff00) >> 8;
	header[GZ_CHUNKLEN + 0] = (chunkLength & 0x00ff) >> 0;
	header[GZ_CHUNKCNT + 1] = (chunks & 0xff00) >> 8;
	header[GZ_CHUNKCNT + 0] = (chunks & 0x00ff) >> 0;
	strcpy(&header[GZ_FEXTRA_START + extraLength], origFilename);
	xfwrite(header, 1, headerLength, outStr);

	/* Read, compress, write */
	while (!feof(inStr)) {
		if ((count = fread(inBuffer, 1, chunkLength, inStr))) {
			dict_data_filter(inBuffer, &count, IN_BUFFER_SIZE, preFilter);

			inputCRC = crc32(inputCRC, (const Bytef *)inBuffer, count);
			zStream.next_in = (Bytef *)inBuffer;
			zStream.avail_in = count;
			zStream.next_out = (Bytef *)outBuffer;
			zStream.avail_out = OUT_BUFFER_SIZE;
			if (deflate(&zStream, Z_FULL_FLUSH) != Z_OK)
				err_fatal(__func__, "deflate: %s\n", zStream.msg);
			assert(zStream.avail_in == 0);
			len = OUT_BUFFER_SIZE - zStream.avail_out;
			assert(len <= 0xffff);

			dict_data_filter(outBuffer, &len, OUT_BUFFER_SIZE, postFilter);

			assert(len <= 0xffff);
			header[GZ_RNDDATA + chunk * 2 + 1] = (len & 0xff00) >> 8;
			header[GZ_RNDDATA + chunk * 2 + 0] = (len & 0x00ff) >> 0;
			xfwrite(outBuffer, 1, len, outStr);

			++chunk;
			total += count;
#if defined(DEBUG) || defined(_DEBUG)
			printf("chunk %5lu: %lu of %lu total\r",
				chunk, total, (unsigned long)st.st_size);
			xfflush(stdout);
#endif
		}
	}
	PRINTF(DBG_VERBOSE, ("total: %lu chunks, %lu bytes\n", chunks, (unsigned long)st.st_size));

	/* Write last bit */
#if 0
	dmalloc_verify(0);
#endif
	zStream.next_in = (Bytef *)inBuffer;
	zStream.avail_in = 0;
	zStream.next_out = (Bytef *)outBuffer;
	zStream.avail_out = OUT_BUFFER_SIZE;
	if (deflate(&zStream, Z_FINISH) != Z_STREAM_END)
		err_fatal(__func__, "deflate: %s\n", zStream.msg);
	assert(zStream.avail_in == 0);
	len = OUT_BUFFER_SIZE - zStream.avail_out;
	xfwrite(outBuffer, 1, len, outStr);
	PRINTF(DBG_VERBOSE, ("(wrote %d bytes, final, crc = %lx)\n",
		len, inputCRC));

	/* Write CRC and length */
#if 0
	dmalloc_verify(0);
#endif
	tail[0 + 3] = (inputCRC & 0xff000000) >> 24;
	tail[0 + 2] = (inputCRC & 0x00ff0000) >> 16;
	tail[0 + 1] = (inputCRC & 0x0000ff00) >> 8;
	tail[0 + 0] = (inputCRC & 0x000000ff) >> 0;
	tail[4 + 3] = (st.st_size & 0xff000000) >> 24;
	tail[4 + 2] = (st.st_size & 0x00ff0000) >> 16;
	tail[4 + 1] = (st.st_size & 0x0000ff00) >> 8;
	tail[4 + 0] = (st.st_size & 0x000000ff) >> 0;
	xfwrite(tail, 1, 8, outStr);

	/* Write final header information */
#if 0
	dmalloc_verify(0);
#endif
	rewind(outStr);
#if HEADER_CRC
	headerCRC = crc32(0L, Z_NULL, 0);
	headerCRC = crc32(headerCRC, header, headerLength - 2);
	header[headerLength - 1] = (headerCRC & 0xff00) >> 8;
	header[headerLength - 2] = (headerCRC & 0x00ff) >> 0;
#endif
	xfwrite(header, 1, headerLength, outStr);

	/* Close files */
#if 0
	dmalloc_verify(0);
#endif
	xfclose(outStr);
	xfclose(inStr);

	/* Shut down compression */
	if (deflateEnd(&zStream) != Z_OK)
		err_fatal(__func__, "defalteEnd: %s\n", zStream.msg);

	if (!is_name_provided)
		xfree(origFilename);
	xfree(header);

	return 0;
}

int dict_databuf_zip(const char *data, unsigned long len_data, const char *outFilename, const char *preFilter, 
	const char *postFilter, const char *origFilename, int chunkLength, unsigned short formatVersion)
{
//	char          inBuffer[IN_BUFFER_SIZE];
	const char    *inBuffer;
	char          outBuffer[OUT_BUFFER_SIZE];
	int           count;
	unsigned long inputCRC = crc32(0L, Z_NULL, 0);
	z_stream      zStream;
	FILE          *outStr;
	int           len;
#ifdef _WIN32
	struct __stat64   st;
#else
	struct stat   st;
#endif
	char          *header;
	int           headerLength;
	int           dataLength;
	int           extraLength;
//	int           chunkLength;
#if HEADER_CRC
	int           headerCRC;
#endif
	unsigned long chunks;
	unsigned long chunk = 0;
	unsigned long total = 0;
	int           i;
	char          tail[8];
	char          *pt;
	int is_name_provided = (origFilename != NULL);


	/* Open files */
	if (len_data == 0)
		err_fatal_errno(__func__,
			"Data size for read is 0\n");
	if (!(outStr = fopen(outFilename, "wb")))
		err_fatal_errno(__func__,
			"Cannot open \"%s\"for write\n", outFilename);

	if (!is_name_provided)
	{
		origFilename = xmalloc(strlen(outFilename) + 1);
		pt = max(strrchr(outFilename, '/'), strrchr(outFilename, '\\'));
		if (pt)
			strcpy(origFilename, pt + 1);
		else
			strcpy(origFilename, outFilename);
	}

	/* Initialize compression engine */
	zStream.zalloc = NULL;
	zStream.zfree = NULL;
	zStream.opaque = NULL;
	zStream.next_in = NULL;
	zStream.avail_in = 0;
	zStream.next_out = NULL;
	zStream.avail_out = 0;
	if (deflateInit2(&zStream,
		Z_BEST_COMPRESSION,
		Z_DEFLATED,
		-15,	/* Suppress zlib header */
		Z_BEST_COMPRESSION,
		Z_DEFAULT_STRATEGY) != Z_OK)
		err_internal(__func__,
			"Cannot initialize deflation engine: %s\n", zStream.msg);

	/* Write initial header information */
	if (chunkLength <= 0)
		chunkLength = (preFilter ? PREFILTER_IN_BUFFER_SIZE : IN_BUFFER_SIZE);
#ifdef _WIN32
	_fstat64(fileno(outStr), &st);  // to get date/time of the out file
#else
	fstat(fileno(outStr), &st);  // to get date/time of the out file
#endif
	chunks = len_data / chunkLength;
	if (len_data % chunkLength) ++chunks;
	PRINTF(DBG_VERBOSE, ("%lu chunks * %u per chunk = %lu (filesize = %lu)\n",
		chunks, chunkLength, chunks * chunkLength,
		len_data));
	dataLength = chunks * 2;

	extraLength = 10 + dataLength;
	if (extraLength > 0xFFFF) {
		fprintf(stderr, "\nFile too long: %lu chunks needed, 32762 allowed\n", chunks);
		fclose(outStr);
		unlink(outFilename);
		return -1;
	}

	headerLength = GZ_FEXTRA_START
		+ extraLength		/* FEXTRA */
		+ strlen(origFilename) + 1	/* FNAME  */
		+ (HEADER_CRC ? 2 : 0);	/* FHCRC  */
	PRINTF(DBG_VERBOSE, ("(data = %d, extra = %d, header = %d)\n",
		dataLength, extraLength, headerLength));
	header = xmalloc(headerLength);
	for (i = 0; i < headerLength; i++) header[i] = 0;
	header[GZ_ID1] = GZ_MAGIC1;
	header[GZ_ID2] = GZ_MAGIC2;
	header[GZ_CM] = Z_DEFLATED;
	header[GZ_FLG] = GZ_FEXTRA | GZ_FNAME;
#if HEADER_CRC
	header[GZ_FLG] |= GZ_FHCRC;
#endif
	header[GZ_MTIME + 3] = (st.st_mtime & 0xff000000) >> 24;
	header[GZ_MTIME + 2] = (st.st_mtime & 0x00ff0000) >> 16;
	header[GZ_MTIME + 1] = (formatVersion == 0xFFFF) ? (st.st_mtime & 0x0000ff00) >> 8 : (formatVersion & 0xFF00) >> 8 ;
	header[GZ_MTIME + 0] = (formatVersion == 0xFFFF) ? (st.st_mtime & 0x000000ff) >> 0 : (formatVersion & 0xFF);
	header[GZ_XFL] = GZ_MAX;
	header[GZ_OS] = GZ_OS_UNIX;
	header[GZ_XLEN + 1] = (extraLength & 0xff00) >> 8;
	header[GZ_XLEN + 0] = (extraLength & 0x00ff) >> 0;
	header[GZ_SI1] = GZ_RND_S1;
	header[GZ_SI2] = GZ_RND_S2;
	header[GZ_SUBLEN + 1] = ((extraLength - 4) & 0xff00) >> 8;
	header[GZ_SUBLEN + 0] = ((extraLength - 4) & 0x00ff) >> 0;
	header[GZ_VERSION + 1] = 0;
	header[GZ_VERSION + 0] = 1;
	header[GZ_CHUNKLEN + 1] = (chunkLength & 0xff00) >> 8;
	header[GZ_CHUNKLEN + 0] = (chunkLength & 0x00ff) >> 0;
	header[GZ_CHUNKCNT + 1] = (chunks & 0xff00) >> 8;
	header[GZ_CHUNKCNT + 0] = (chunks & 0x00ff) >> 0;
	strcpy(&header[GZ_FEXTRA_START + extraLength], origFilename);
	xfwrite(header, 1, headerLength, outStr);

	/* Read, compress, write */
	inBuffer = data;
	for (i = 0; i < chunks; i++) {
		count = (i < chunks - 1 || len_data % chunkLength == 0) ? chunkLength : len_data % chunkLength;
//		memcpy(inBuffer, data, count); // TODO: use data directly and delete inBuffer
		dict_data_filter(inBuffer, &count, IN_BUFFER_SIZE, preFilter);
		inputCRC = crc32(inputCRC, (const Bytef *)inBuffer, count);
		zStream.next_in = (Bytef *)inBuffer;
		zStream.avail_in = count;
		zStream.next_out = (Bytef *)outBuffer;
		zStream.avail_out = OUT_BUFFER_SIZE;
		if (deflate(&zStream, Z_FULL_FLUSH) != Z_OK)
			err_fatal(__func__, "deflate: %s\n", zStream.msg);
		assert(zStream.avail_in == 0);
		len = OUT_BUFFER_SIZE - zStream.avail_out;
		assert(len <= 0xffff);

		dict_data_filter(outBuffer, &len, OUT_BUFFER_SIZE, postFilter);

		assert(len <= 0xffff);
		header[GZ_RNDDATA + chunk * 2 + 1] = (len & 0xff00) >> 8;
		header[GZ_RNDDATA + chunk * 2 + 0] = (len & 0x00ff) >> 0;
		xfwrite(outBuffer, 1, len, outStr);

		++chunk;
		total += count;
		inBuffer += count;
#if defined(DEBUG) || defined(_DEBUG)
		printf("chunk %5lu: %lu of %lu total\r",
			chunk, total, len_data);
		xfflush(stdout);
#endif
	}
	PRINTF(DBG_VERBOSE, ("total: %lu chunks, %lu bytes\n", chunks, len_data));

	/* Write last bit */
#if 0
	dmalloc_verify(0);
#endif
	zStream.next_in = (Bytef *)inBuffer;
	zStream.avail_in = 0;
	zStream.next_out = (Bytef *)outBuffer;
	zStream.avail_out = OUT_BUFFER_SIZE;
	if (deflate(&zStream, Z_FINISH) != Z_STREAM_END)
		err_fatal(__func__, "deflate: %s\n", zStream.msg);
	assert(zStream.avail_in == 0);
	len = OUT_BUFFER_SIZE - zStream.avail_out;
	xfwrite(outBuffer, 1, len, outStr);
	PRINTF(DBG_VERBOSE, ("(wrote %d bytes, final, crc = %lx)\n",
		len, inputCRC));

	/* Write CRC and length */
#if 0
	dmalloc_verify(0);
#endif
	tail[0 + 3] = (inputCRC & 0xff000000) >> 24;
	tail[0 + 2] = (inputCRC & 0x00ff0000) >> 16;
	tail[0 + 1] = (inputCRC & 0x0000ff00) >> 8;
	tail[0 + 0] = (inputCRC & 0x000000ff) >> 0;
	tail[4 + 3] = (len_data & 0xff000000) >> 24;
	tail[4 + 2] = (len_data & 0x00ff0000) >> 16;
	tail[4 + 1] = (len_data & 0x0000ff00) >> 8;
	tail[4 + 0] = (len_data & 0x000000ff) >> 0;
	xfwrite(tail, 1, 8, outStr);

	/* Write final header information */
#if 0
	dmalloc_verify(0);
#endif
	rewind(outStr);
#if HEADER_CRC
	headerCRC = crc32(0L, Z_NULL, 0);
	headerCRC = crc32(headerCRC, header, headerLength - 2);
	header[headerLength - 1] = (headerCRC & 0xff00) >> 8;
	header[headerLength - 2] = (headerCRC & 0x00ff) >> 0;
#endif
	xfwrite(header, 1, headerLength, outStr);

	/* Close files */
#if 0
	dmalloc_verify(0);
#endif
	xfclose(outStr);

	/* Shut down compression */
	if (deflateEnd(&zStream) != Z_OK)
		err_fatal(__func__, "defalteEnd: %s\n", zStream.msg);

	if (!is_name_provided)
		xfree(origFilename);
	xfree(header);

	return 0;
}
