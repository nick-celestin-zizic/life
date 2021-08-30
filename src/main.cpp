#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <cerrno>
#include <assert.h>
#include <climits>

#include <sys/mman.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "main.hpp"

void realloc_game_memory(GameMemory *gm,
                         usize screen_width_px, usize screen_height_px,
                         f64 scale) {
  gm->screen_width_px  = screen_width_px;
  gm->screen_height_px = screen_height_px;

  gm->gw_num_rows   = (usize) ((f64) screen_height_px / (f64) scale);
  gm->gw_num_cols   = (usize) ((f64) screen_width_px  / (f64) scale);
  gm->gw_size_bytes = gm->gw_num_cols  * gm->gw_num_rows * sizeof(CellState);
  
  const usize side = (screen_width_px > screen_height_px)   ?
    (usize) ((f64) screen_width_px / (f64) gm->gw_num_cols) :
    (usize) ((f64) screen_height_px / (f64) gm->gw_num_rows);
  
  gm->cell_side_px = side ? side : 1;

  gm->vb_stride_bytes       = screen_width_px * sizeof(Pixel);
  const usize vb_size_bytes = gm->vb_stride_bytes * screen_height_px;
  if (gm->memory) {
    const usize new_memory_size_bytes = 2 * gm->gw_size_bytes + vb_size_bytes;
    gm->memory = mremap(gm->memory,
                        gm->memory_size_bytes,
                        new_memory_size_bytes,
                        MREMAP_MAYMOVE);
    gm->memory_size_bytes = new_memory_size_bytes;
    memset(gm->memory, 0, gm->memory_size_bytes);
  } else {
    gm->memory_size_bytes = 2 * gm->gw_size_bytes + vb_size_bytes;
    gm->memory = mmap(NULL, gm->memory_size_bytes,
                      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                      -1, 0);
  }
  if (gm->memory == MAP_FAILED) {
    fprintf(stderr, "Could not allocate game memory: %s\n", strerror(errno));
    exit(1);
  }
  
  gm->game_world      = (CellState *) gm->memory;
  gm->game_world_back = (CellState *) (((byte *) gm->memory) +
                                       gm->gw_size_bytes);
  gm->video_buffer    = (Pixel *)     (((byte *) gm->memory) +
                                       (gm->gw_size_bytes * 2));
}

void free_game_memory(GameMemory *gm) {
  if (gm->memory) {
    munmap(gm->memory, gm->memory_size_bytes);
  }
  memset(gm, 0, sizeof(*gm));
}


// TODO this can be multithreaded
void render_world (GameMemory &gm, u8 current_palette) {
  byte *row = (byte *) gm.video_buffer;
  
  for (usize y = 0; y < gm.screen_height_px; y++) {
    auto pixel = (Pixel *) row;
    for (usize x = 0; x < gm.screen_width_px; x++, pixel++) {
      // render the cell
      u32 cell_x = (x / gm.cell_side_px);
      u32 cell_y = (y / gm.cell_side_px);
      
      u32 idx = cell_y * gm.gw_num_cols + cell_x;
      
      if (idx < gm.gw_num_rows * gm.gw_num_cols)
        *pixel = PALETTE[current_palette][gm.game_world[idx]];
    }
    row += gm.vb_stride_bytes;
  }
}

void init_world_random (GameMemory &gm, f64 density) {
  srand(time(0));

  for (usize y = 0; y < gm.gw_num_rows; y++) {
    for (usize x = 0; x < gm.gw_num_cols; x++) {
      usize idx = y * gm.gw_num_cols + x;
      
      const f64 r = (f64) rand()/(f64) (RAND_MAX - 1);
      //if (r <= 0.01) printf("%f\n", r);
      
      gm.game_world[idx] = (r > density) ? DEAD : ALIVE;
    }
  }
}

void init_world_blank (GameMemory &gm) {
  memset(gm.game_world, DEAD, gm.gw_size_bytes);
}

void update_game_world (GameMemory &gm) {
  for (u32 y = 0; y < gm.gw_num_rows; y++) {
    for (u32 x = 0; x < gm.gw_num_cols; x++) {
      const u32 idx = y * gm.gw_num_cols + x;
      
      const u32 up   = y ? (y - 1) : (gm.gw_num_rows - 1);
      const u32 left = x ? (x - 1) : (gm.gw_num_cols - 1);

      const u32 down  = (y == gm.gw_num_rows - 1) ? 0 : (y + 1);
      const u32 right = (x == gm.gw_num_cols - 1) ? 0 : (x + 1);

      const struct {u32 col; u32 row;} neighbors[8] = {
        {x,     up},
        {right, up},
        {right, y},
        {right, down},
        {x,     down},
        {left,  down},
        {left,  y},
        {left,  up}
      };
      
      u32 total_alive = 0;
      for (const auto current : neighbors) {
        const CellState neighbor =
          gm.game_world[current.row * gm.gw_num_cols + current.col];
        total_alive += (neighbor == ALIVE || neighbor == REVIVED) ? 1 : 0;
      }

      const auto cell = gm.game_world[idx];

      if ((cell == ALIVE || cell == REVIVED) && total_alive < 2)
        gm.game_world_back[idx] = DIED;
      else if ((cell == ALIVE || cell == REVIVED) && total_alive > 3)
        gm.game_world_back[idx] = DIED;
      else if (cell == DEAD && total_alive == 3)
        gm.game_world_back[idx] = ALIVE;
      else if (cell == DIED && total_alive == 3)
        gm.game_world_back[idx] = REVIVED;
      else
        gm.game_world_back[idx] = cell;
    }
  }
  
  CellState *tmp     = gm.game_world;
  gm.game_world      = gm.game_world_back;
  gm.game_world_back = tmp;
}

void generate_ppm (const char *output_path, usize width_px, usize height_px, usize cycles, f64 density) {
  GameMemory gm = {};
  realloc_game_memory(&gm, width_px, height_px, DEFAULT_SCALE);
  init_world_random(gm, density);
  
  for (usize c = 0; c < cycles; c++) {
    update_game_world(gm);
    printf("Running Simulation: %.2f%%\r", ((f64) c / (f64) cycles) * 100.0);
    fflush(stdout);
  }
  printf("Running Simulation: 100.00%%\n");
  
  printf("Rendering..\n");
  render_world(gm, 0);
  
  FILE *out = fopen(output_path, "wb");
  
  fprintf(out, "P3\n%lu %lu\n255\n", gm.screen_width_px, gm.screen_height_px);

  
  printf("Outputing to %s..\n", output_path);
  fflush(stdout);
  for (usize y = 0; y < gm.screen_height_px; y++) {
    for (usize x = 0; x < gm.screen_width_px; x++) {
      const Pixel pixel = gm.video_buffer[y * gm.screen_width_px + x];
      const u8 r = ((pixel >> (8 * 2)) & 0xFF);
      const u8 g = ((pixel >> (8 * 1)) & 0xFF);
      const u8 b = ((pixel >> (8 * 0)) & 0xFF);
      
      fprintf(out, "%d %d %d ", r, g, b);
    }
    fprintf(out, "\n");
  }
  
  printf("Done!\n");
  
  fclose(out);
  free_game_memory(&gm);
}

void render_live_x11 (void) {
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "ERROR: could not open display\n");
    exit(1);
  }

  Window window = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                                      0, 0,
                                      WIDTH_PX, HEIGHT_PX,
                                      0, 0, 0);
  XWindowAttributes attribs = {};
  XGetWindowAttributes(display, window, &attribs);
  
  GC gc = XCreateGC(display, window, 0, NULL);

  Atom X11_WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &X11_WM_DELETE_WINDOW, 1);

  XSelectInput(display, window,
               KeyPressMask        |
               ButtonPressMask     |
               ButtonReleaseMask   |
               StructureNotifyMask |
               PointerMotionMask);

  XMapWindow(display, window);
  
  XSynchronize(display, True);
  
  XSync(display, 0);

  f32 scale   = DEFAULT_SCALE;
  f32 density = DEFAULT_DENSITY;
  
  GameMemory gm = {};
  realloc_game_memory(&gm, WIDTH_PX, HEIGHT_PX, scale);
  init_world_random(gm, density);

  XImage *image = XCreateImage(display, attribs.visual, attribs.depth,
                               ZPixmap, 0,
                               (char *) gm.video_buffer,
                               gm.screen_width_px,
                               gm.screen_height_px,
                               32, gm.screen_width_px * sizeof(Pixel));

  bool should_quit = false;
  bool mouse_held  = false;
  bool should_update = false;
  u8 current_palette = 0;
  struct {int x; int y;} mouse_pos = {0, 0};
  CellCoord changed_cell = {-1, -1};
  while (!should_quit) {
    while (XPending(display) > 0) {
      XEvent event = {};
      XNextEvent(display, &event);
      switch (event.type) {
      case KeyPress: {
        switch (XLookupKeysym((XKeyEvent *) &event, 0)) {
        case 'q': {
          should_quit = true; } break;
          
        case 'r': {
          init_world_random(gm, density); } break;
          
        case 'c': {
          init_world_blank(gm); } break;
          
        case 's': {
          update_game_world(gm); } break;
          
        case 'p': {
          should_update = !should_update; } break;

        case 'i': {
          printf("%f\n", density); } break;
          
        case 65362: { // UP
          scale += 5;
          realloc_game_memory(&gm, gm.screen_width_px, gm.screen_height_px,
                              scale);
          image->data = (byte *) gm.video_buffer;
          init_world_random(gm, density); } break;
          
        case 65364: { // DOWN
          if (scale-5 > 0) scale -= 5;
          realloc_game_memory(&gm, gm.screen_width_px, gm.screen_height_px,
                              scale);
          image->data = (byte *) gm.video_buffer;
          init_world_random(gm, density); } break;
          
        case 65361: { // LEFT ARROW
          if (density > 0.0) density -= 0.01;
          init_world_random(gm, density); } break;

        case 65363: { // RIGHT ARROW
          if (density < 1.0) density += 0.01;
          init_world_random(gm, density); } break;

        case 32: { // SPACE
          if (current_palette >= NUM_PALETTES)
            current_palette = 0;
          else current_palette++;
        } break;
          
        default: {
          printf("%lu\n", XLookupKeysym((XKeyEvent *) &event, 0)); } break;
        } } break;
        
      case ButtonPress: {
        mouse_held = true;
        const auto btn = event.xbutton;
        mouse_pos = {btn.x, btn.y}; } break;
        
      case ButtonRelease: {
        mouse_held = false;
        changed_cell = { -1, -1 }; } break;
        
      case MotionNotify: {
        if (mouse_held) {
          const auto btn = event.xbutton;
          if (0 <= btn.x && (usize) btn.x < gm.screen_width_px &&
              0 <= btn.y && (usize) btn.y < gm.screen_height_px)
            mouse_pos = {btn.x, btn.y};
        } } break;
        
      case ConfigureNotify: {
        const usize
          &old_w = gm.screen_width_px,
          &old_h = gm.screen_width_px,
          &new_w = event.xconfigure.width,
          &new_h = event.xconfigure.height;
        
        if (new_w != old_w || new_h != old_h) {
          realloc_game_memory(&gm, new_w, new_h, scale);
          
          image->width          = new_w;
          image->height         = new_h;
          image->bytes_per_line = gm.vb_stride_bytes;
          image->data           = (byte *) gm.video_buffer;
          
          init_world_random(gm, density);
          should_update = false;
        } } break;
        
      case ClientMessage: {
        if ((Atom) event.xclient.data.l[0] == X11_WM_DELETE_WINDOW) {
          should_quit = True;
        } } break;
        
      }
    }

    if (should_update) update_game_world(gm);
    
    if (mouse_held) {
      CellCoord cell = {
        (s32) ((usize) mouse_pos.y / scale),
        (s32) ((usize) mouse_pos.x / scale)
      };
      
      if ((cell.row < (s64) gm.gw_num_rows &&
           cell.col < (s64) gm.gw_num_cols) &&
          !(changed_cell.row == cell.row &&
            changed_cell.col == cell.col)) {
        // change the cell
        changed_cell = cell;
        
        const auto idx = cell.row * gm.gw_num_cols + cell.col;

        switch (gm.game_world[idx]) {
        case ALIVE: case REVIVED: {
          gm.game_world[idx] = DEAD; } break;
          
          
        case DEAD: case DIED: {
          gm.game_world[idx] = ALIVE;
        }
        }
      }
    }
    
    render_world(gm, current_palette);
    
    XPutImage(display, window, gc, image,
              0, 0,
              0, 0,
              gm.screen_width_px,
              gm.screen_height_px);
  }

  free_game_memory(&gm);
  XCloseDisplay(display);
}

void usage (FILE *stream) {
  fprintf(stream, "Usage: ./life [-h] [-p PATH WIDTH HEIGHT CYCLES DENSITY]\n");
}

int main (int argc, char **argv) {
  if (argc == 1)
    render_live_x11();
  else if (argc > 1) {
    if (strcmp(argv[1], "-h") == 0) {
      usage(stdout);
      exit(0);
    } else if (strcmp(argv[1], "-p") == 0 && argc == 7) {
      const char *path   = argv[2];
      const usize width  = atoll(argv[3]);
      const usize height = atoll(argv[4]);
      const usize cycles = atoll(argv[5]);
      const f32 density  = atof(argv[6]);
      
      if (width == 0 || height == 0 || cycles == 0 || density == 0) {
        usage(stderr);
        exit(1);
      }
      generate_ppm(path, width, height, cycles, density);
    } else {
      usage(stderr);
      exit(1);
    }
  }
}
