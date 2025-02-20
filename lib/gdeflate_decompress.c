/*
 * gdeflate_decompress.c - a decompressor for GDEFLATE
 *
 * Copyright 2016 Eric Biggers
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2020, 2021, 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#define WITH_GDEFLATE
#define HIDE_INTERFACE

#define libdeflate_decompressor libdeflate_gdeflate_decompressor

#include "deflate_decompress.c"

/*
 * This is the main GDEFLATE decompression routine.  See libdeflate.h for the
 * documentation.
 *
 * Note that the real code is in gdeflate_decompress_template.h.  The part here
 * just handles calling the appropriate implementation depending on the CPU
 * features at runtime.
 */
LIBDEFLATEEXPORT enum libdeflate_result LIBDEFLATEAPI
libdeflate_gdeflate_decompress(struct libdeflate_decompressor * restrict d,
			       struct libdeflate_gdeflate_in_page *in_pages,
			       size_t in_npages, void * restrict out,
			       size_t out_nbytes_avail,
			       size_t *actual_out_nbytes_ret)
{
	u8 * restrict out_bytes = out;

	if (unlikely(in_pages == NULL || in_npages == 0))
		return LIBDEFLATE_BAD_DATA;

	for (size_t npage = 0; npage < in_npages; npage++) {
		size_t page_out_nbytes_ret, page_in_nbytes_ret;
		enum libdeflate_result res;

		res = decompress_impl(d, in_pages[npage].data,
				      in_pages[npage].nbytes, out_bytes,
				      out_nbytes_avail, &page_in_nbytes_ret,
				      &page_out_nbytes_ret);

		if (unlikely(res != LIBDEFLATE_SUCCESS))
			return res;

		out_bytes += page_out_nbytes_ret;
		out_nbytes_avail -= page_out_nbytes_ret;

		if (actual_out_nbytes_ret)
			*actual_out_nbytes_ret += page_out_nbytes_ret;
	}

	return LIBDEFLATE_SUCCESS;
}

LIBDEFLATEEXPORT enum libdeflate_result LIBDEFLATEAPI
libdeflate_gdeflate_decompress_ex(struct libdeflate_gdeflate_decompressor * restrict d,
				  struct libdeflate_gdeflate_in_page * restrict in_pages,
				  struct libdeflate_gdeflate_out_page * restrict out_pages, size_t npages)
{
	if (unlikely(in_pages == NULL || out_pages == NULL || npages == 0))
		return LIBDEFLATE_BAD_DATA;

	for (size_t npage = 0; npage < npages; npage++) {
		size_t page_out_nbytes_ret, page_in_nbytes_ret;
		enum libdeflate_result res;

		res = decompress_impl(d, in_pages[npage].data,
				      in_pages[npage].nbytes, out_pages[npage].data,
				      out_pages[npage].nbytes, &page_in_nbytes_ret,
				      &page_out_nbytes_ret);

		if (unlikely(res != LIBDEFLATE_SUCCESS))
			return res;
	}

	return LIBDEFLATE_SUCCESS;
}

LIBDEFLATEEXPORT struct libdeflate_decompressor * LIBDEFLATEAPI
libdeflate_alloc_gdeflate_decompressor(void)
{
	/*
	 * Note that only certain parts of the decompressor actually must be
	 * initialized here:
	 *
	 * - 'static_codes_loaded' must be initialized to false.
	 *
	 * - The first half of the main portion of each decode table must be
	 *   initialized to any value, to avoid reading from uninitialized
	 *   memory during table expansion in build_decode_table().  (Although,
	 *   this is really just to avoid warnings with dynamic tools like
	 *   valgrind, since build_decode_table() is guaranteed to initialize
	 *   all entries eventually anyway.)
	 *
	 * But for simplicity, we currently just zero the whole decompressor.
	 */
	struct libdeflate_decompressor *d = libdeflate_malloc(sizeof(*d));

	if (d == NULL)
		return NULL;
	memset(d, 0, sizeof(*d));
	return d;
}

LIBDEFLATEEXPORT void LIBDEFLATEAPI
libdeflate_free_gdeflate_decompressor(struct libdeflate_decompressor *d)
{
	libdeflate_free(d);
}
