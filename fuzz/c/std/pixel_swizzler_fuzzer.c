// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------

// Silence the nested slash-star warning for the next comment's command line.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"

/*
This fuzzer (the fuzz function) is typically run indirectly, by a framework
such as https://github.com/google/oss-fuzz calling LLVMFuzzerTestOneInput.

When working on the fuzz implementation, or as a coherence check, defining
WUFFS_CONFIG__FUZZLIB_MAIN will let you manually run fuzz over a set of files:

gcc -DWUFFS_CONFIG__FUZZLIB_MAIN pixel_swizzler_fuzzer.c
./a.out ../../../test/data
rm -f ./a.out

It should print "PASS", amongst other information, and exit(0).
*/

#pragma clang diagnostic pop

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../fuzzlib/fuzzlib.c"

const uint32_t pixfmts[] = {
    WUFFS_BASE__PIXEL_FORMAT__Y,
    WUFFS_BASE__PIXEL_FORMAT__Y_16BE,
    WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY,
    WUFFS_BASE__PIXEL_FORMAT__BGR_565,
    WUFFS_BASE__PIXEL_FORMAT__BGR,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL,
    WUFFS_BASE__PIXEL_FORMAT__BGRX,
    WUFFS_BASE__PIXEL_FORMAT__RGB,
    WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL,
};

const wuffs_base__pixel_blend blends[] = {
    WUFFS_BASE__PIXEL_BLEND__SRC,
    WUFFS_BASE__PIXEL_BLEND__SRC_OVER,
};

// fuzz tests that, regardless of the randomized inputs, calling
// wuffs_base__pixel_swizzler__swizzle_interleaved_from_slice will not crash
// the fuzzer (e.g. due to reads or write past buffer bounds).
const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash) {
  uint8_t dst_palette_array[1024];
  uint8_t src_palette_array[1024];
  if ((src->meta.wi - src->meta.ri) < 2048) {
    return "not enough data";
  }
  memcpy(dst_palette_array, src->data.ptr + src->meta.ri, 1024);
  src->meta.ri += 1024;
  memcpy(src_palette_array, src->data.ptr + src->meta.ri, 1024);
  src->meta.ri += 1024;
  wuffs_base__slice_u8 dst_palette =
      wuffs_base__make_slice_u8(&dst_palette_array[0], 1024);
  wuffs_base__slice_u8 src_palette =
      wuffs_base__make_slice_u8(&src_palette_array[0], 1024);

  size_t num_pixfmts = sizeof(pixfmts) / sizeof(pixfmts[0]);
  wuffs_base__pixel_format dst_pixfmt = wuffs_base__make_pixel_format(
      pixfmts[(0xFF & (hash >> 0)) % num_pixfmts]);
  wuffs_base__pixel_format src_pixfmt = wuffs_base__make_pixel_format(
      pixfmts[(0xFF & (hash >> 8)) % num_pixfmts]);

  size_t num_blends = sizeof(blends) / sizeof(blends[0]);
  wuffs_base__pixel_blend blend = blends[(0xFF & (hash >> 16)) % num_blends];

  size_t dst_len = 0xFF & (hash >> 24);
  size_t src_len = 0xFF & (hash >> 32);

  wuffs_base__pixel_swizzler swizzler;
  wuffs_base__status status = wuffs_base__pixel_swizzler__prepare(
      &swizzler, dst_pixfmt, dst_palette, src_pixfmt, src_palette, blend);
  if (status.repr) {
    return wuffs_base__status__message(&status);
  }

  const char* ret = NULL;
  wuffs_base__slice_u8 dst_slice = ((wuffs_base__slice_u8){});
  wuffs_base__slice_u8 src_slice = ((wuffs_base__slice_u8){});

  // Use a {} code block so that "goto exit" doesn't trigger "jump bypasses
  // variable initialization" warnings.
  {
    if ((src->meta.wi - src->meta.ri) < dst_len) {
      ret = "not enough data";
      goto exit;
    }
    dst_slice = wuffs_base__malloc_slice_u8(malloc, dst_len);
    if (!dst_slice.ptr) {
      ret = "out of memory";
      goto exit;
    }
    memcpy(dst_slice.ptr, src->data.ptr + src->meta.ri, dst_len);
    src->meta.ri += dst_len;

    if ((src->meta.wi - src->meta.ri) < src_len) {
      ret = "not enough data";
      goto exit;
    }
    src_slice = wuffs_base__malloc_slice_u8(malloc, src_len);
    if (!src_slice.ptr) {
      ret = "out of memory";
      goto exit;
    }
    memcpy(src_slice.ptr, src->data.ptr + src->meta.ri, src_len);
    src->meta.ri += src_len;

    wuffs_base__pixel_swizzler__swizzle_interleaved_from_slice(
        &swizzler, dst_slice, dst_palette, src_slice);
  }

exit:
  free(src_slice.ptr);
  free(dst_slice.ptr);
  return ret;
}
