/*
 *  File access <-> AVIOContext
 *  Copyright (C) 2011 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fa_libav.h"

/**
 *
 */
static int
fa_libav_read(void *opaque, uint8_t *buf, int size)
{
  fa_handle_t *fh = opaque;
  return fa_read(fh, buf, size);
}


/**
 *
 */
static int64_t
fa_libav_seek(void *opaque, int64_t offset, int whence)
{
  fa_handle_t *fh = opaque;
  if(whence == AVSEEK_SIZE)
    return fa_fsize(fh);

  return fa_seek(fh, offset, whence & ~AVSEEK_FORCE);
}


/**
 *
 */
AVIOContext *
fa_libav_open(const char *url, int buf_size, char *errbuf, size_t errlen)
{
  fa_handle_t *fh;

  if((fh = fa_open(url, errbuf, errlen)) == NULL)
    return NULL;

  if(buf_size == 0)
    buf_size = 32768;
  void *buf = malloc(buf_size);
  return avio_alloc_context(buf, buf_size, 0, fh, fa_libav_read, NULL, 
			    fa_libav_seek);
}


/**
 *
 */
static AVFormatContext *
fa_libav_open_error(char *errbuf, size_t errlen, const char *hdr, int errcode)
{
  char libaverr[256];

  if(av_strerror(errcode, libaverr, sizeof(libaverr)))
    snprintf(libaverr, sizeof(libaverr), "libav error %d", errcode);
  
  snprintf(errbuf, errlen, "%s: %s", hdr, libaverr);
  return NULL;
}



/**
 *
 */
AVFormatContext *
fa_libav_open_format(AVIOContext *avio, const char *url, 
		     char *errbuf, size_t errlen)
{
  AVInputFormat *fmt = NULL;
  AVFormatContext *fctx;
  int err;

  avio_seek(avio, 0, SEEK_SET);

  if((err = av_probe_input_buffer(avio, &fmt, url, NULL, 0, 0)) != 0)
    return fa_libav_open_error(errbuf, errlen,
			       "Unable to probe file", err);

  if(fmt == NULL) {
    snprintf(errbuf, errlen, "Unknown file format");
    return NULL;
  }

  if((err = av_open_input_stream(&fctx, avio, url, fmt, NULL)) != 0)
    return fa_libav_open_error(errbuf, errlen,
			       "Unable to open file as input format", err);

  if(av_find_stream_info(fctx) < 0) {
    av_close_input_stream(fctx);
    return fa_libav_open_error(errbuf, errlen,
			       "Unable to handle file contents", err);
  }

  return fctx;
}


/**
 *
 */
void
fa_libav_close(AVIOContext *avio)
{
  fa_close(avio->opaque);
  free(avio->buffer);
  av_free(avio);
}


/**
 *
 */
void
fa_libav_close_format(AVFormatContext *fctx)
{
  AVIOContext *avio = fctx->pb;
  av_close_input_stream(fctx);
  fa_libav_close(avio);
}


/**
 *
 */
uint8_t *
fa_libav_load_and_close(AVIOContext *avio, size_t *sizep)
{
  size_t r, size = avio_size(avio);
  uint8_t *mem = malloc(size+1);

  avio_seek(avio, 0, SEEK_SET);
  r = avio_read(avio, mem, size);
  fa_libav_close(avio);

  if(r != size) {
    free(mem);
    return NULL;
  }

  if(sizep != NULL)
    *sizep = size;
  mem[size] = 0; 
  return mem;
}
