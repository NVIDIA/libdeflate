/*
 * gdeflate_compress.c - a compressor for GDEFLATE
 *
 * Originally public domain; changes after 2016-09-07 are copyrighted.
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

#define libdeflate_compressor libdeflate_gdeflate_compressor

#include "deflate_compress.c"

LIBDEFLATEEXPORT struct libdeflate_compressor * LIBDEFLATEAPI
libdeflate_alloc_gdeflate_compressor(int compression_level)
{
	struct libdeflate_compressor *c;
	size_t size = offsetof(struct libdeflate_compressor, p);

	if (compression_level < 0 || compression_level > 12)
		return NULL;

#if SUPPORT_NEAR_OPTIMAL_PARSING
	if (compression_level >= 8)
		size += sizeof(c->p.n);
	else if (compression_level >= 1)
		size += sizeof(c->p.g);
#else
	if (compression_level >= 1)
		size += sizeof(c->p.g);
#endif

	c = libdeflate_aligned_malloc(MATCHFINDER_MEM_ALIGNMENT, size);
	if (!c)
		return NULL;

	c->compression_level = compression_level;

	/*
	 * The higher the compression level, the more we should bother trying to
	 * compress very small inputs.
	 */
	c->min_size_to_compress = 56 - (compression_level * 4);

	switch (compression_level) {
	case 0:
		c->impl = deflate_compress_none;
		break;
	case 1:
		c->impl = deflate_compress_greedy;
		c->max_search_depth = 2;
		c->nice_match_length = 8;
		break;
	case 2:
		c->impl = deflate_compress_greedy;
		c->max_search_depth = 6;
		c->nice_match_length = 10;
		break;
	case 3:
		c->impl = deflate_compress_greedy;
		c->max_search_depth = 12;
		c->nice_match_length = 14;
		break;
	case 4:
		c->impl = deflate_compress_greedy;
		c->max_search_depth = 24;
		c->nice_match_length = 24;
		break;
	case 5:
		c->impl = deflate_compress_lazy;
		c->max_search_depth = 20;
		c->nice_match_length = 30;
		break;
	case 6:
		c->impl = deflate_compress_lazy;
		c->max_search_depth = 40;
		c->nice_match_length = 65;
		break;
	case 7:
		c->impl = deflate_compress_lazy;
		c->max_search_depth = 100;
		c->nice_match_length = 130;
		break;
#if SUPPORT_NEAR_OPTIMAL_PARSING
	case 8:
		c->impl = deflate_compress_near_optimal;
		c->max_search_depth = 12;
		c->nice_match_length = 20;
		c->p.n.num_optim_passes = 1;
		break;
	case 9:
		c->impl = deflate_compress_near_optimal;
		c->max_search_depth = 16;
		c->nice_match_length = 26;
		c->p.n.num_optim_passes = 2;
		break;
	case 10:
		c->impl = deflate_compress_near_optimal;
		c->max_search_depth = 30;
		c->nice_match_length = 50;
		c->p.n.num_optim_passes = 2;
		break;
	case 11:
		c->impl = deflate_compress_near_optimal;
		c->max_search_depth = 60;
		c->nice_match_length = 80;
		c->p.n.num_optim_passes = 3;
		break;
	default:
		c->impl = deflate_compress_near_optimal;
		c->max_search_depth = 100;
		c->nice_match_length = 133;
		c->p.n.num_optim_passes = 4;
		break;
#else
	case 8:
		c->impl = deflate_compress_lazy;
		c->max_search_depth = 150;
		c->nice_match_length = 200;
		break;
	default:
		c->impl = deflate_compress_lazy;
		c->max_search_depth = 200;
		c->nice_match_length = DEFLATE_MAX_MATCH_LEN;
		break;
#endif
	}

	deflate_init_offset_slot_fast(c);
	deflate_init_static_codes(c);
	deflate_init_length_slot();

	return c;
}

LIBDEFLATEEXPORT size_t LIBDEFLATEAPI
libdeflate_gdeflate_compress(struct libdeflate_compressor *c,
			     const void *in, size_t in_nbytes,
			     struct libdeflate_gdeflate_out_page* out_pages,
			     size_t out_npages)
{
	const u8 * in_bytes = in;
	size_t out_nbytes = 0;
	size_t npages;
	size_t upper_bound = libdeflate_gdeflate_compress_bound(c,
						in_nbytes, &npages);
	size_t page_upper_bound = upper_bound / npages;

	if (unlikely(out_pages == NULL || out_npages != npages))
		return 0;

	for (size_t page = 0; page < npages; page++) {
		size_t comp_page_nbytes;
		const size_t page_nbytes = in_nbytes > GDEFLATE_PAGE_SIZE ?
			GDEFLATE_PAGE_SIZE : in_nbytes;

		if (unlikely(out_pages[page].nbytes < page_upper_bound)) {
			return 0;
		}

		comp_page_nbytes = (*c->impl)(c, in_bytes, page_nbytes,
			out_pages[page].data, page_upper_bound);

		out_pages[page].nbytes = comp_page_nbytes;

		/* Page did not fit - bail out. */
		if (unlikely(comp_page_nbytes == 0))
			return 0;

		in_bytes += page_nbytes;
		in_nbytes -= page_nbytes;

		out_nbytes += comp_page_nbytes;
	}

	return out_nbytes;
}

LIBDEFLATEEXPORT size_t LIBDEFLATEAPI
libdeflate_gdeflate_compress_ex(struct libdeflate_compressor *c,
			        const struct libdeflate_gdeflate_in_page* in_pages,
			        struct libdeflate_gdeflate_out_page* out_pages, size_t npages)
{
	if (unlikely(out_pages == NULL || in_pages == NULL))
		return 0;

	for (size_t page = 0; page < npages; page++) {
		size_t comp_page_nbytes;

		comp_page_nbytes = (*c->impl)(c, in_pages[page].data, in_pages[page].nbytes,
					      out_pages[page].data, out_pages[page].nbytes);

		out_pages[page].nbytes = comp_page_nbytes;

		/* Page did not fit - bail out. */
		if (unlikely(comp_page_nbytes == 0))
			return page;
	}

	return npages;
}

LIBDEFLATEEXPORT void LIBDEFLATEAPI
libdeflate_free_gdeflate_compressor(struct libdeflate_compressor *c)
{
	libdeflate_aligned_free(c);
}

LIBDEFLATEEXPORT size_t LIBDEFLATEAPI
libdeflate_gdeflate_compress_bound(struct libdeflate_compressor *c,
				   size_t in_nbytes, size_t *out_npages)
{
	const size_t page_bound = libdeflate_deflate_compress_bound(c, GDEFLATE_PAGE_SIZE);
	const size_t npages = DIV_ROUND_UP(in_nbytes, GDEFLATE_PAGE_SIZE);

	if (out_npages)
		*out_npages = npages;

	return (page_bound + (NUM_STREAMS * BITS_PER_PACKET) / 8) * npages;
}
