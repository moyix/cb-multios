/*
 * Copyright (c) 2014 Kaprica Security, Inc.
 *
 * Permission is hereby granted, cgc_free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#ifndef GLD_H_
#define GLD_H_

#define MAX_HEIGHT 11
#define QUAD_PIXELS_WIDTH 2
#define MAX_LEVELS (MAX_HEIGHT + QUAD_PIXELS_WIDTH)
#define PIXELS_PER_TRIE (1<<(QUAD_PIXELS_WIDTH * 2))
#define MAX_BOARD_WIDTH (1<<MAX_LEVELS)
#define FALSE 0
#define TRUE 1

typedef enum {ALL, LIVING_CELL, DEAD_CELL, BOMB} cgc_cell_filter_e;

typedef struct coord {
    int x;
    int y;
} cgc_coord_t;

typedef struct region {
    cgc_coord_t start;
    cgc_coord_t end;
} cgc_region_t;

typedef struct conway_pixel {
    cgc_coord_t point;
    int is_alive;
    int bomb_set;
} cgc_conway_pixel_t;

typedef struct coord_list cgc_coord_list_t;
struct coord_list {
    cgc_coord_t coord;

    cgc_coord_list_t *prev;
    cgc_coord_list_t *next;
};

typedef struct pixel_list cgc_pixel_list_t;
struct pixel_list {
    cgc_conway_pixel_t px;

    cgc_pixel_list_t *prev;
    cgc_pixel_list_t *next;
};

#define UNSET_COORD(_coord)      \
    do {                         \
        _coord.x = -1;           \
        _coord.y = -1;           \
    } while(0)

#define SET_COORD(_coord, _x, _y)     \
    do {                              \
        _coord.x = _x;                \
        _coord.y = _y;                \
    } while(0)

#define COORD_EQUALS(_c1, _c2) (_c1.x == _c2.x && _c1.y == _c2.y)

#define SET_REGION(_region, _x1, _y1, _x2, _y2) \
    do {                                        \
        _region.start.x = _x1;                  \
        _region.start.y = _y1;                  \
        _region.end.x = _x2;                    \
        _region.end.y = _y2;                    \
    } while(0);

#define APPEND_LIST(_list_type, _head, _l2)     \
    do {                                        \
        if (_head == NULL) {                    \
            _head = _l2;                        \
        } else if (_l2 != NULL) {               \
            _list_type *l = _head;              \
            while (l->next != NULL)             \
                l = l->next;                    \
            l->next = _l2;                      \
            _l2->prev = l;                      \
        }                                       \
    } while(0)


#define INSERT_ITEM(_head, _item)               \
    do {                                        \
        if (_head == NULL) {                    \
            _head = _item;                      \
        } else if (_item != NULL) {             \
            _item->next = _head;                \
            _item->prev = NULL;                 \
            _head->prev = _item;                \
            _head = _item;                      \
        }                                       \
    } while(0)

#define INSERT_PIXEL_IN_ORDER(_head, _pixel)                                \
    do {                                                                    \
        if (_head == NULL) {                                                \
            _head = _pixel;                                                 \
        } else if (_pixel != NULL) {                                        \
            cgc_pixel_list_t *iter = _head;                                     \
            while (iter != NULL) {                                          \
                if(iter->px.point.x < _pixel->px.point.x) {                 \
                    if(!iter->next) {                                       \
                        iter->next = _pixel;                                \
                        _pixel->prev = iter;                                \
                        break;                                              \
                    }                                                       \
                    iter = iter->next;                                      \
                } else if(iter->px.point.x == _pixel->px.point.x) {         \
                    if(iter->px.point.y < _pixel->px.point.y) {             \
                        if(!iter->next) {                                   \
                            iter->next = _pixel;                            \
                            _pixel->prev = iter;                            \
                            break;                                          \
                        }                                                   \
                        iter = iter->next;                                  \
                    } else {                                                \
                        if (iter->prev != NULL)                             \
                            iter->prev->next = _pixel;                      \
                        _pixel->next = iter;                                \
                        _pixel->prev = iter->prev;                          \
                        iter->prev = _pixel;                                \
                        if (_pixel->prev == NULL)                           \
                            _head = _pixel;                                 \
                        iter = NULL;                                        \
                        break;                                              \
                    }                                                       \
                } else {                                                    \
                    if (iter->prev != NULL)                                 \
                        iter->prev->next = _pixel;                          \
                    _pixel->next = iter;                                    \
                    _pixel->prev = iter->prev;                              \
                    iter->prev = _pixel;                                    \
                    if (_pixel->prev == NULL)                               \
                        _head = _pixel;                                     \
                    iter = NULL;                                            \
                    break;                                                  \
                }                                                           \
            }                                                               \
        }                                                                   \
    } while(0)


#define FREE_ITEM(_head, _item)                 \
    do {                                        \
        if (_item->next != NULL)                \
            _item->next->prev = _item->prev;    \
        if (_item->prev != NULL)                \
            _item->prev->next = _item->next;    \
        else                                    \
            _head = _item->next;                \
        _item->prev = NULL;                     \
        _item->next = NULL;                     \
        cgc_free(_item);                            \
    } while(0)


#define FREE_LIST(type, _head)                  \
    do {                                        \
        type *l = _head;                        \
        while (_head != NULL) {                 \
            l = _head;                          \
            _head = _head->next;                \
            cgc_free(l);                            \
        }                                       \
    } while(0)

// quadtree functions
typedef struct qtree cgc_qtree_t;
typedef int (*cgc_qtree_insert)(cgc_qtree_t *qt, cgc_conway_pixel_t px);
typedef cgc_conway_pixel_t* (*cgc_qtree_get_pixel)(cgc_qtree_t *qt, cgc_coord_t point);

// game functions
typedef int (*cgc_conway_generation)(int num_steps);
typedef int (*cgc_gunshot)(cgc_coord_t point);
typedef int (*cgc_bomber)(cgc_coord_t point);

struct qtree
{
    size_t max_levels;
    size_t max_pixels;
    size_t num_pixels;
    cgc_pixel_list_t *pixels;

    cgc_region_t valid_region;
    int is_subdivided;

    // Functions
    cgc_qtree_insert insert;
    cgc_qtree_get_pixel get_pixel;

    cgc_conway_generation step;
    cgc_gunshot shoot_pixel;
    cgc_bomber set_bomb;

    // Subnodes
    cgc_qtree_t *nw;
    cgc_qtree_t *ne;
    cgc_qtree_t *sw;
    cgc_qtree_t *se;

};

cgc_qtree_t *cgc_gld_init_game();
void cgc_gld_clear_board();
void cgc_gld_print_board(char *str);

void cgc_qt_debug_print_tree(cgc_qtree_t *qt, char *str);
#endif