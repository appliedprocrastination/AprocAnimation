/*
  Animation.h - library for storing Animations for Fetch
  Copyright (c) 2019 Simen E. Sørensen. 
*/

// ensure this library description is only included once
#ifndef Animation_h
#define Animation_h

#include <Arduino.h>
#include "SdFat.h"

#define DUTY_CYCLE_RESOLUTION 4096
//holders for infromation you're going to pass to shifting function
const int ALL_ROWS = 12;   //The total number of rows in the actual hardware
const int ALL_COLS = 21;   //The total number of columns in the actual hardware
const int ROWS = 10; //12; //The number of rows that are in use in the current program (different from ALL_ROWS in order to scale down the number of bits shifted out)
const int COLS = 19; //21; //The number of cols that are in use in the current program (different from ALL_COLS in order to scale down the number of bits shifted out)

//const int REGISTERS = ROWS;       // no of register series (indicating no of magnet-driver-PCBs connected to the Arduino)
//const int BYTES_PER_REGISTER = 4; // no of 8-bit shift registers in series per line (4 = 32 bits(/magnets))

enum PlaybackType
{
    ONCE,
    LOOP,
    BOUNCE,
    LOOP_N_TIMES //Add "STATIC_IMAGE" here?
};

enum PlaybackState
{
    IDLE, //could be called "DONE", but until a "MagnetMatrix" class controls playbacks (and can change from "done" to "not started" when ending an animation), the name may be confusing.
    RUNNING,
    ERROR
};

enum PlaybackProcess
{
    NONE,
    FADE_IN_FADE_OUT //Fade between frames. Artsy effect that should be used with caution.
};

class Frame
{
public:
    Frame(uint16_t *duty_cycle = nullptr, int cols = COLS, int rows = ROWS);
    ~Frame();
    Frame      *get_copy_of_frame();
    void        delete_frame(void);
    uint16_t   *get_pixel_intensities();
    uint16_t    get_pixel_intensity_at(int x, int y);
    void        overwrite_pixel_intensities(uint16_t *duty_cycle);
    void        write_pixel_intensity_at(int x, int y, uint16_t duty_cycle);

    void        merge_pixel_intensity_at(int x, int y, uint16_t other_pixel_intensity);
    void        merge_with_frame(int other_bottom_left_x, int other_bottom_left_y, Frame *other);

    void        unmerge_pixel_intensity_at(int x, int y, uint16_t other_pixel_intensity);
    void        unmerge_frame(int other_bottom_left_x, int other_bottom_left_y, Frame *other);

    int         get_width();
    int         get_height();

    void        print_to_terminal(int pretty=true);
private : 
    int         _cols;
    int         _rows;

    uint16_t  *  _duty_cycle;

    void        _delete_duty_cycle();
};

class Animation
{
public:
    Animation(Frame **frames = nullptr, int num_frames = 2, int cols = COLS, int rows = ROWS, int origin_x = 0, int origin_y = 0, int location_x = 0, int location_y = 0);
    void    delete_anim(void);
    Frame*  get_frame(int frame_num);
    void    write_frame(int frame_num, Frame* frame);
    void    load_frames_from_array(uint16_t** duty_cycle);
    int     get_current_frame_num();
    int     get_num_frames();
    void    goto_next_frame();
    void    goto_prev_frame();
    Frame*  get_current_frame();
    Frame*  get_next_frame();
    Frame*  get_prev_frame();
    // Start new functionality added with Fetch V2.0
    void    set_origin(int new_origin_x, int new_origin_y);
    int*    get_origin(int *output);
    void    set_location(int new_loc_x, int new_loc_y);
    int*    get_location(int *output);
    int*    get_size(int* output);
    int*    get_bottom_left_location(int *output);
    int     merge_with(Animation *other);
    // End new functionality added with Fetch V2.0

    void    start_animation_at(int start_frame = 0);
    void    start_animation();
    void    write_playback_dir(bool forward);
    void    write_max_loop_count(int n);
    bool    get_playback_dir();
    bool    anim_done();
    void    write_playback_type(PlaybackType type);
    PlaybackType get_playback_type();

    int     save_to_SD_card(SdFatSdioEX sd, uint16_t file_index);
    int     read_from_SD_card(SdFatSdioEX sd, uint16_t file_index);

private:
    int             _cols;
    int             _rows;
    int             _num_frames; 

    PlaybackType    _playback_type  = LOOP;
    PlaybackState   _playback_state = IDLE;
    PlaybackProcess _playback_process = NONE;

    bool            _dir_fwd = true;
    int             _current_frame = 0;
    int             _prev_frame = -1;
    int             _loop_iteration = 0;
    int             _max_iterations = -1;
    int             _start_idx = 0; //Where the animation started (not necessarily first index in array)
    // Start new functionality added with Fetch V2.0
    int             _origin_x = 0;
    int             _origin_y = 0;
    int             _location_x = 0;
    int             _location_y = 0;
    // End new functionality added with Fetch V2.0


    Frame         **_frames;
    Frame          *_blank_frame = new Frame(nullptr); //used as return statement when playback_state is DONE
    int             _get_next_frame_idx();
    int             _get_prev_frame_idx();
    bool            _current_frame_is_on_edge();
};

#endif