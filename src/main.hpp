#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include <cstddef>

typedef char byte;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef size_t usize;

typedef float  f32;
typedef double f64;

typedef u32 Pixel;

#define OUTPUT_PATH "./build/life.ppm"
#define CYCLES (30 * 60)
#define WIDTH_PX  1080
#define HEIGHT_PX 1080

#define DEFAULT_SCALE 1
#define DEFAULT_DENSITY 0.08
#define DENSITY_STEP 0.01

#define DEAD_CLR    
#define ALIVE_CLR   
#define DIED_CLR    
#define REVIVED_CLR 

typedef enum: u32 {
  DEAD = 0,
  ALIVE,
  DIED,
  REVIVED,
} CellState;

#define NUM_PALETTES 8
u32 PALETTE[NUM_PALETTES][4] = {
  // FUZZYFOUR
  {
    0x302387,
    0xfffdaf,
    0xff3796,
    0x00faac,
  },
  // HALLOWPUMPKIN
  {
    0x300030,
    0xf8f088,
    0x602878,
    0xf89020,
  },
  // SPACEHAZE
  {
    0x0b0630,
    0xf8e3c4,
    0x6b1fb1,
    0xcc3495,
  },
  // SODA-CAP
  {
    0x2176cc,
    0xfca6ac,
    0xff7d6e,
    0xe8e7cb,
  },
  // MOONLIGHT GB
  {
    0x0f052d,
    0x36868f,
    0x5fc75d,
    0x203671,
  },
  // 2BIT DEMICHROME
  {
    0x000000,
    0xe9efec,
    0xa0a08b,
    0x555568,
  },
  // AYY4 PALETTE
  {
    0x00303b,
    0xf1f2da,
    0xffce96,
    0xff7777,
  },
  // PORPLE
  {
    0x000000,
    0xD61B4C,
    0x5500D0,
    0xFFD166,
  },
};

typedef struct {
  usize cell_side_px;
  
  void *memory;
  usize memory_size_bytes;

  CellState *game_world;
  CellState *game_world_back;
  usize gw_size_bytes;
  usize gw_num_cols, gw_num_rows;
  
  Pixel *video_buffer;
  usize vb_stride_bytes;
  usize screen_width_px, screen_height_px;
} GameMemory;

typedef struct {
  s32 row;
  s32 col;
} CellCoord;

#endif // MAIN_H_
