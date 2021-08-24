#include "Animation.h"
#include "FreeStack.h"
#include "csv_helpers.h"
#include <string.h>

File sdFile;
//Constructor
Frame::Frame(uint16_t *duty_cycle, int cols, int rows)
{
    _cols = cols;
    _rows = rows;
    if (duty_cycle == nullptr)
    {
        int tolerance = 10000;
        if ((FreeStack() < (_cols * _rows * sizeof(uint16_t) + tolerance)))
        {
            Serial.printf("Failed to initialize frame with duty_cycle array of size: %d\n"
                          "Available space in RAM: %d\n",
                          _cols * _rows * sizeof(uint16_t), FreeStack());
            return;
        }
        //Serial.printf("Initializing frame with duty_cycle array of size: %d\n"
        //   "Available space in RAM: %d\n", _cols*_rows, FreeStack());

        _duty_cycle = new uint16_t [cols*rows];
        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < cols; col++)
            {
                _duty_cycle[row*cols + col] = 0;
            }
            Serial.println();
        }
    }else
    {
        //Serial.printf("Starting to fill duty cycle from %s array at: %p\n", alloced_duty?"dynamically allocated":"static",duty_cycle);
        _duty_cycle = duty_cycle;
    }
}
Frame::~Frame()
{
    this->delete_frame();
}
Frame* Frame::get_copy_of_frame()
{
    int tolerance = 10000;
    if ((FreeStack() < (_cols * _rows * sizeof(uint16_t) + tolerance)))
    {
        Serial.printf("Failed to initialize frame with duty_cycle array of size: %d\n"
                        "Available space in RAM: %d\n",
                        _cols * _rows, FreeStack());
        return;
    }

    uint16_t* new_duty_cycle = new uint16_t[_cols * _rows];
    for (int y = 0; y < _rows; y++)
    {
        for (int x = 0; x < _cols; x++)
        {
            new_duty_cycle[y * _cols + x] = this->get_pixel_intensity_at(x, y);
        }
    }
    return new Frame(new_duty_cycle,_cols,_rows);
}
// Public Methods
void Frame::delete_frame(void)
{
    _delete_duty_cycle();
}
uint16_t *Frame::get_pixel_intensities()
{
    return _duty_cycle;
}
uint16_t Frame::get_pixel_intensity_at(int x, int y)
{
    if (x < _cols && x >= 0 && y < _rows && y >= 0)
    {
        return _duty_cycle[y*_cols + x];
    }
    return 0; //This means that any Frame is technically infinitely large (surrounded by zeros). Used when merging together frames.
}

void Frame::overwrite_pixel_intensities(uint16_t *duty_cycle)
{
    _delete_duty_cycle();
    _duty_cycle = duty_cycle;
}

void Frame::write_pixel_intensity_at(int x, int y, uint16_t duty_cycle)
{
    if (x < _cols && x >= 0 && y < _rows && y >= 0)
    {
        if (duty_cycle > DUTY_CYCLE_RESOLUTION)
        {
            duty_cycle = DUTY_CYCLE_RESOLUTION;
        }else
        if(duty_cycle < 0){
            duty_cycle = 0;
        }
        _duty_cycle[x + y * _cols] = duty_cycle;
    }// if the coordinates are outside of the frame, ignore them. TODO: return an error message if necessary.
}

void Frame::merge_pixel_intensity_at(int x, int y, uint16_t other_pixel_intensity){
    //Merge is done by adding together the two duty cycle values for now.
    uint16_t new_pixel_intensity = this->get_pixel_intensity_at(x,y) + other_pixel_intensity;
    //this->write_pixel_intensity_at(x,y,new_pixel_intensity); //We don't want to max out the pixel intensity at 4096 because going past that value means we can "unmerge" frames by subracting them from each other.
    if (new_pixel_intensity < 0)
    {
        new_pixel_intensity = 0;
    }
    _duty_cycle[x + y * _cols] = new_pixel_intensity;
}

void Frame::merge_with_frame(int other_bottom_left_x, int other_bottom_left_y, Frame *other){
    uint16_t other_pixel_intensity;
    //Iterate through the other frame and place it inside this one with the offset given by "other_bottom_left" coordinates.
    for (int x = 0; x < other->get_width(); x++)
    {
        for (int y = 0; y < other->get_height(); y++)
        {
            other_pixel_intensity = other->get_pixel_intensity_at(x, y);
            if (other_pixel_intensity > 0)
            {
                // This will cause a change, therefore merge in the pixel.
                this->merge_pixel_intensity_at(x + other_bottom_left_x, y + other_bottom_left_y, other_pixel_intensity);
            }//else: no change. Ignore this pixel
        }
    }
}

void Frame::unmerge_pixel_intensity_at(int x, int y, uint16_t other_pixel_intensity)
{
    //Merge is done by adding together the two duty cycle values for now.
    uint16_t new_pixel_intensity = this->get_pixel_intensity_at(x, y) - other_pixel_intensity;
    //this->write_pixel_intensity_at(x,y,new_pixel_intensity); //We don't want to max out the pixel intensity at 4096 because going past that value means we can "unmerge" frames by subracting them from each other.
    if (new_pixel_intensity < 0)
    {
        new_pixel_intensity = 0;
    }
    _duty_cycle[x + y * _cols] = new_pixel_intensity;
}

void Frame::unmerge_frame(int other_bottom_left_x, int other_bottom_left_y, Frame *other)
{
    uint16_t other_pixel_intensity;
    //Iterate through the other frame and place it inside this one with the offset given by "other_bottom_left" coordinates.
    for (int x = 0; x < other->get_width(); x++)
    {
        for (int y = 0; y < other->get_height(); y++)
        {
            other_pixel_intensity = other->get_pixel_intensity_at(x, y);
            if (other_pixel_intensity > 0)
            {
                // This will cause a change, therefore merge in the pixel.
                this->unmerge_pixel_intensity_at(x + other_bottom_left_x, y + other_bottom_left_y, other_pixel_intensity);
            } //else: no change. Ignore this pixel
        }
    }
}

int Frame::get_width()
{
    return _cols;
}
int Frame::get_height()
{
    return _rows;
}

void Frame::print_to_terminal(int pretty){
    if(Serial)
    {
        Serial.println("Frame:");
        if(pretty){
            char row_string[_cols+1];
            uint16_t value;

            //print top border;
            for (int x = 0; x < _cols+1; x++)
            {
                Serial.print("_");
            }
            Serial.println("_");
            for (int y = _rows; y >= 0; y--) //draw from top to bottom
            {
                //Print left side of border
                Serial.print("|");
                //Print out * or whitespace for values
                int x = 0;
                for (x; x < _cols; x++)
                {
                    value = get_pixel_intensity_at(x,y);
                    row_string[x] =  value > 0 ? '*' : ' ';
                }
                row_string[x] = 0;
                Serial.print(row_string);
                //Print right side of border
                Serial.println("|");
            }
            //Print bottom border
            for (int x = 0; x < _cols+1; x++)
            {
                Serial.print("-");
            }
            Serial.println("-");
        }else{
            char val_fmt_str[] = "%5u "; //Will return a 5 character long string + 1 string terminator (6 characters in total)
            char val_str[6 + 1];
            uint16_t value;
            //Print top border
            for (int x = 0; x < (6 * _cols) + 1; x++)
            {
                Serial.print("_");
            }
            Serial.println("_");
            for (int y = _rows; y >= 0; y--) //draw from top to bottom
            {
                //Print left border
                Serial.print("|");
                //Print out values
                int x = 0;
                for (x; x < _cols; x++)
                {
                    value = get_pixel_intensity_at(x, y);
                    sprintf(val_str, val_fmt_str, value);
                    Serial.print(val_str);
                }
                //Print right border
                Serial.println("|");
            }
            //Print bottom border
            for (int x = 0; x < (6 * _cols) + 1; x++)
            {
                Serial.print("-");
            }
            Serial.println("-");
        }
    }
}
// Private Methods

inline void Frame::_delete_duty_cycle()
{
    delete _duty_cycle;
    _duty_cycle = nullptr;
}

//Constructor
/*
\brief Constructor

\param frames An array of pointers to Frame objects. Default = nullptr
\param num_frames The length of the "frames" array. Default = 2 (two empty Frame objects are generated if "frames"=nullptr)
\param cols The number of columns in the animation. Default = COLS (defined in Animation.h)
\param rows The number of rows in the animation. Default = ROWS (defined in Animation.h)
\param origin_x The x coordinate of the origin point of this animation. The coordinate is local, meaning that it is relative to the bottom left corner of this animation itself. Default = 0
\param origin_y The y coordinate of the origin point of this animation. The coordinate is local, meaning that it is relative to the bottom left corner of this animation itself. Default = 0
\param location_x The x coordinate of the location of this animation. The coordinate is global, meaning that it is relative to the bottom left corner of another animation/canvas. Only used when merging this animation into another animation. Default = 0
\param location_y The y coordinate of the location of this animation. The coordinate is global, meaning that it is relative to the bottom left corner of another animation/canvas. Only used when merging this animation into another animation. Default = 0
*/
Animation::Animation(Frame **frames, int num_frames, int cols, int rows, int origin_x, int origin_y, int location_x, int location_y)
{
    _num_frames = num_frames;
    _cols = cols;
    _rows = rows;
    _origin_x = origin_x;
    _origin_y = origin_y;
    _location_x = location_x;
    _location_y = location_y;
    
    if (frames == nullptr)
    {
        _frames = new Frame *[num_frames];
        for (int frame = 0; frame < num_frames; frame++)
        {
            _frames[frame] = new Frame();
        }
    }
    else
    {    
        _frames = frames;
    }
}

// Public Methods
void Animation::delete_anim(void)
{
    
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->delete_frame();
    }
    if (_frames != nullptr){
        delete _frames;
        _frames = nullptr;
    }
    if(_blank_frame != nullptr){
        delete _blank_frame;
        _blank_frame = nullptr;
    }
}

Frame *Animation::get_frame(int frame_num)
{
    return _frames[frame_num];
}
void Animation::write_frame(int frame_num, Frame *frame)
{
    _frames[frame_num] = frame;
}
void Animation::load_frames_from_array(uint16_t **duty_cycle)
{
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->overwrite_pixel_intensities(duty_cycle[frame]);
    }
}

int Animation::get_current_frame_num()
{
    return _current_frame;
}

int Animation::get_num_frames(){
    return _num_frames;
}

void Animation::goto_next_frame()
{
    if (_current_frame_is_on_edge())
    {
        //Current frame IS on the edge
        switch (_playback_type)
        {
        case LOOP:
            if (_current_frame == _start_idx)
            {
                _loop_iteration++;
            }
            break;
        case ONCE:
            break;
        case BOUNCE:
            _loop_iteration++; //not checked for current_frame vs start_idx because we want to bounce on both ends of the array
            if (_loop_iteration != 1)
            {
                //First iteration we do not want to toggle
                _dir_fwd = !_dir_fwd;
            }
            break;
        case LOOP_N_TIMES:
            if (_current_frame == _start_idx)
            {
                _loop_iteration++;
            }

            break;
        default:
            _playback_state = ERROR;
            break;
        }
    }

    _prev_frame = _current_frame;
    _current_frame = _get_next_frame_idx();

    if(_playback_state == ERROR){
        //TODO: Handle this error. Letting it slip to IDLE for now.
        _current_frame = -1;
    }
    if(_current_frame == -1){
        _playback_state = IDLE;
    }
}
void Animation::goto_prev_frame()
{
    //This whole function may be unnecessary, since going back and forth is
    //handled in the goto_next_frame and get_next_frame_idx methods
    int tmp_prev_frame = _current_frame;
    _current_frame = _prev_frame; //TODO: use private method _get_prev_frame_idx()?
    _prev_frame = tmp_prev_frame;
    
}
Frame *Animation::get_current_frame(){
    if (_current_frame == -1 || _playback_state == IDLE)
    {
        return _blank_frame;
    }
    return _frames[_current_frame];
}
Frame *Animation::get_next_frame(){
    int idx = _get_next_frame_idx();
    if (idx == -1 || _playback_state == IDLE)
    {
        return _blank_frame;
    }
    return _frames[idx];
}
Frame *Animation::get_prev_frame(){
    if (_prev_frame == -1 || _playback_state == IDLE)
    {
        return _blank_frame;
    }
    
    return _frames[_prev_frame];
}

// Start new functionality added with Fetch V2.0
void Animation::set_origin(int new_origin_x, int new_origin_y)
{
    _origin_x = new_origin_x;
    _origin_y = new_origin_y;
}
/* 
*  Returns the location of the origin point relative to the 
*  canvas of this animation itself. The origin will normally 
*  be in the lower left corner, but other locations can be useful 
*  in certain scenarios (e.g. center)
*/
int *Animation::get_origin(int *output)
{
    output[0] = _origin_x;
    output[1] = _origin_y;
    return output;
}
void Animation::set_location(int new_loc_x, int new_loc_y)
{
    _location_x = new_loc_x;
    _location_y = new_loc_y;
}
/* 
*  Returns the location of the origin point relative to (0,0) of an external 
*  canvas. Used when this animation is passed as the argument to an other 
*  animations merge_with() function. Meaning that this animation is merged into another. 
*/
int *Animation::get_location(int *output)
{
    output[0] = _location_x;
    output[1] = _location_y;
    return output;
}
/*
*   Returns the width and height of this animation.
*/
int *Animation::get_size(int *output)
{
    output[0] = _cols;
    output[1] = _rows;
    return output;
}
/* 
*  Returns the location of the origin point relative to (0,0) of an external 
*  canvas, offset for the origin of this animation. That means the coordinates
*  returned by this function will be relative to the lower left corner of this
*  animation, regardless of where the origin is placed. Used when this animation
*  is passed as the argument to an other animations merge_with() function. 
*  Meaning that this animation is merged into another. 
*/
int* Animation::get_bottom_left_location(int* output)
{
    output[0] = _location_x - _origin_x;
    output[1] = _location_y - _origin_y;
    return output;
}

int Animation::merge_with(Animation* other){
    // Before merging, verify that "other" is contained within the frame of "this"
    // If the entire "other" animation is outside the canvas of "this", then ignore it.
    // If the "other" animation is partially outside, only pixels that are inside "this" canvas
    // will be considered.
    int *bottom_left_loc_other = new int[2];
    int *size_other = new int[2];

    int *origin_this = new int[2];
    int *size_this = new int[2];
    if (bottom_left_loc_other != nullptr && size_other != nullptr && origin_this != nullptr && size_this != nullptr)
    {
        bottom_left_loc_other = other->get_bottom_left_location(bottom_left_loc_other);
        size_other = other->get_size(size_other); //width, height
        int other_x = bottom_left_loc_other[0];
        int other_y = bottom_left_loc_other[1];
        int other_width = size_other[0];
        int other_height = size_other[1];
        

        origin_this = this->get_origin(origin_this);
        size_this = this->get_size(size_this); //width, height
        int this_x = origin_this[0];
        int this_y = origin_this[1];
        int this_width = size_this[0];
        int this_height = size_this[1];

        int this_leftborder = -this_x;
        int this_rightborder = this_width - this_x;
        int this_bottomborder = -this_y;
        int this_topborder = this_height - this_y;

        //check if outside left border of canvas
        if (other_x + other_width < this_leftborder)
            return -1;
        //check if outside bottom border of canvas
        if (other_y + other_height < this_bottomborder)
            return -1;
        //check if outside right border of canvas
        if (other_x > this_rightborder)
            return -1;
        //check if outside top border of canvas
        if (other_y > this_topborder)
            return -1;

        
        // Determine the number of frames necessary to complete both animations.
        if (other->get_num_frames() > _num_frames)
        {
            //Allocate new memory for this animation to expand until it has the same length as the other animation
            int difference = other->get_num_frames() - _num_frames;
            //Add blank frames at the end of this animation

            const int cols = _cols;
            const int rows = _rows;
            const int new_num_frames = _num_frames + difference;

            Frame** new_frames = new Frame *[new_num_frames];
            Serial.printf("frames:%d,cols:%d,rows:%d\n", new_num_frames, cols, rows);
            
            for (int f = 0; f < new_num_frames; f++)
            {
                if (f < _num_frames){
                    new_frames[f] = _frames[f];
                }else{
                    new_frames[f] = new Frame();
                }
            }
            
            _num_frames = new_num_frames;
            delete _frames; //Delete the old _frames array, 
            _frames = new_frames;

        }

        // Duty cycle values need to be ADDED together (I think) and clamped to min and max duty cycle
        Frame* this_frame_ptr;
        Frame* other_frame_ptr;
        int iterations = (other->get_num_frames() < _num_frames) ? other->get_num_frames() : _num_frames; //iterate through the lowest possible number of frames
        for (int f = 0; f < iterations; f++)
        {
            this_frame_ptr = this->get_frame(f);
            other_frame_ptr = other->get_frame(f);
            
            //Serial.printf("Other w: %d, Origin h:%d, this w: %d, this h: %d\n", other_width, other_height, this_width, this_height);
            for (int x = 0; x < other_width; x++)
            {
                //Serial.printf("Other_x: %d, Origin_X:%d, Other_Y: %d, Origin_y: %d\n", other_x, _origin_x, other_y, _origin_y);
                this_frame_ptr->merge_with_frame(other_x,other_y,other_frame_ptr);
            }
        }
    }
    delete bottom_left_loc_other;
    delete size_other;
    delete origin_this;
    delete size_this;
}
// End new functionality added with Fetch V2.0

void Animation::start_animation_at(int start_frame){
    // Starts animation on the frame selected by the user.
    if(start_frame == -1){
        start_frame = _num_frames - 1;
    }
    _playback_state = RUNNING;
    _start_idx = start_frame;
    _current_frame = start_frame;
    _loop_iteration = 0;
}
void Animation::start_animation(){
    // Starts animation according to the configured playback direction.
    this->get_playback_dir() ? this->start_animation_at(0) : this->start_animation_at(-1);
}
void Animation::write_playback_dir(bool forward){
    _dir_fwd = forward;
}
void Animation::write_max_loop_count(int n){
    _max_iterations = n;
}

bool Animation::get_playback_dir(){
    return _dir_fwd;
}

bool Animation::anim_done(){
    if(_playback_state == IDLE){
        return true;
    }
    return false;
}
void Animation::write_playback_type(PlaybackType type){
    _playback_type = type;
}
PlaybackType Animation::get_playback_type(){
    return _playback_type;
}

/*\brief Saves two corresponding files to the SD card.
    One that provides info about the animation settings, the other who contains the actual data.
    filename format: "A000_C.txt" for config files
    filename format: "A000_D.bin" for data files
    
    \param[in] file_index is a maximum 5 digit number that will be added to the filename. 
        If the number is not unique, the previous file (with the same index) will be overwritten.
 */
int Animation::save_to_SD_card(SdFatSdioEX sd, uint16_t file_index)
{
    if(!sd.begin()){
        Serial.println("SD initialitization failed. Save unsucessful.");
        return -1;
    }
    //Write ASCII config file:
    //(Using ASCII here because we as users are more likely to 
    // change these parameters manually than the animation itself.
    // The animation will mainly be updated through a GUI program)
    //filename format: "A000_C.txt" for config files
    //filename format: "A000_D.bin" for data files
    const int cfg_len = 150;
    const int cols = _cols;
    const int rows = _rows;
    const int frames = _num_frames;
    static char config_str[cfg_len]; //The data that will be written to the .txt file
    uint16_t duty_buf[frames*cols*rows];

    char full_filename[11]; //Max length of filename is 8 chars +".ext"
    sprintf(full_filename,"A%u_C.txt",file_index);
    if (!sdFile.open(full_filename, O_RDWR | O_CREAT)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }
    sprintf(config_str,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        _cols,
        _rows,
        _num_frames,
        _playback_type,
        _playback_state,
        _dir_fwd ? 1:0,
        _current_frame,
        _prev_frame,
        _loop_iteration,
        _max_iterations,
        _start_idx);//TODO: Add origin, location, height/width etc.
    sdFile.write(config_str);
    //sdFile.write(transport_animation, transport_anim_count*COLS*4);
    sdFile.flush();
    sdFile.close();
    Serial.printf("Config-file saved to SD card as: '%s'.\n",full_filename);

    //Write binary datafile:
    sprintf(full_filename,"A%u_D.bin",file_index);

    if (!sdFile.open(full_filename, O_RDWR | O_CREAT)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }
    //Fill buffers:
    for (int f = 0; f < frames; f++)
    {
        Frame *curr_f = get_frame(f);
        for (int y = 0; y < rows; y++)
        {
            for (int x = 0; x < cols; x++)
            {
                duty_buf[(f*cols*rows)+(y*cols)+x] = curr_f->get_pixel_intensity_at(x,y);           
            }
        }
    }
    
    sdFile.write(duty_buf,frames*cols*rows);
    sdFile.flush();

    /*
    Serial.printf("Duty buffer before save:\n");
    for(int f = 0; f<frames;f++){
        for (int r = 0; r < rows; r++)
        {
            Serial.printf("%8lp, [frame:%2d,row:%2d]:",&duty_buf[(f*cols*rows)+(r*cols)], f,r);
            for (int c = 0; c < cols; c++)
            {
                Serial.printf("0x%2x,", duty_buf[(f*cols*rows)+(r*cols)+c]);
            }
            Serial.println();
        }
    }
    */

    sdFile.close();
    Serial.printf("Data-file saved to SD card as: '%s'.\n",full_filename);
    
    Serial.println("Save sucessful.");
    return 1;
}

int Animation::read_from_SD_card(SdFatSdioEX sd, uint16_t file_index){
    /*if(!sd.begin()){
        Serial.println("SD initialitization failed. Read unsucessful.");
        return -1;
    }*/
    //Serial.printf("numframes:%d\n",_num_frames);
    char full_filename[11]; //Max length of filename is 8 chars +".ext"
    sprintf(full_filename, "A%u_C.txt", file_index);
    
    /*
    Serial.println("Test");
    if(!sdFile.exists(full_filename)){
        Serial.printf("File: '%s' does not exist\n",full_filename);
        return -1;
    }
    Serial.println("Test2");
    */
   
    //Clear memory of old frames
    //TODO: is it necessary to check for null-pointers?
    for (int i = 0; i < _num_frames; i++)
    {
        //Serial.printf("i:%d\n",i);
        _frames[i]->delete_frame();
    }
    delete _frames;
    _frames = nullptr;

    //Read ASCII config file:
    //(Using ASCII here because we as users are more likely to
    // change these parameters manually than the animation itself.
    // The animation will mainly be updated through a GUI program)
    //filename format: "A000_C.txt" for config files
    //filename format: "A000_D.bin" for data files

    if (!sdFile.open(full_filename, O_RDONLY))
    {
        Serial.printf("open file: '%s' failed\n", full_filename);
        sd.errorHalt("open failed");
        return -1;
    }

    
    char delim = ',';
    csvReadInt(&sdFile,&_cols,delim);
    csvReadInt(&sdFile,&_rows,delim);
    csvReadInt(&sdFile,&_num_frames,delim);
    csvReadPBType(&sdFile,&_playback_type,delim);
    csvReadPBState(&sdFile,&_playback_state,delim);
    csvReadBool(&sdFile,&_dir_fwd,delim);
    csvReadInt(&sdFile,&_current_frame,delim);
    csvReadInt(&sdFile,&_prev_frame,delim);
    csvReadInt(&sdFile,&_loop_iteration,delim);
    csvReadInt(&sdFile,&_max_iterations,delim);
    csvReadInt(&sdFile,&_start_idx,delim);
    
    sdFile.close();
    Serial.printf("Config-file read from SD card: '%s'.\n",full_filename);

    //Read binary datafile:
    int tolerance = 10000;
    if(!(FreeStack() > (int)(_num_frames*_cols*_rows*sizeof(uint16_t) + tolerance))){
        Serial.printf("Not enough memory to store animation of size: %d\n"
        "Available space in RAM: %d", _num_frames*_cols*4 + _num_frames*_cols*_rows, FreeStack());
        return -2;
    }
    const int cols = _cols;
    const int rows = _rows;
    const int frames = _num_frames;

    uint16_t* duty_buf = new uint16_t[frames*cols*rows*sizeof(uint16_t)];
    if (&duty_buf[0] == 0 || duty_buf == 0 || duty_buf == (uint16_t*) nullptr || &duty_buf[0] == (uint16_t*) nullptr || duty_buf == nullptr || &duty_buf[0] == nullptr || duty_buf == NULL || &duty_buf[0] == NULL ||&duty_buf[0] ==(uint16_t *) 0 || duty_buf == (uint16_t *)0 )
    {
        Serial.printf("Could not allocate memory for duty_cycle arr of size: %d\n"
        "Available space in RAM: %d\n",frames*cols*rows, FreeStack());

        Serial.printf("Pointer to duty_buf: %8p,%d\n",duty_buf,duty_buf);
        return-1;
    }else{
        //Serial.printf("Pointer to duty_buf: %p,%d\n",duty_buf,duty_buf);
        //Serial.printf("Pointer to _frame_buf: %p\n",_frame_buf);
    }
    sprintf(full_filename,"A%u_D.bin",file_index);
    if (!sdFile.open(full_filename, O_RDONLY)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }

    sdFile.read(duty_buf,frames*cols*rows*sizeof(uint16_t));
    

    /*Serial.printf("Frame buffer after read:\n");
    for(int f = 0; f<frames;f++){
        for (int c = 0; c < cols; c++)
        {
            Serial.printf("[frame:%2d,col:%2d]:0x%8lx\n",f, c, _frame_buf[(f*cols)+c]);
        }
        
    }*/
    /*
    Serial.printf("Duty buffer after read:\n");
    for(int f = 0; f<frames;f++){
        for (int r = 0; r < rows; r++){
            Serial.printf("%8lp, [frame:%2d,row:%2d]:",&duty_buf[(f*cols*rows)+(r*cols)], f,r);
            for (int c = 0; c < cols; c++)
            {
                Serial.printf("0x%2x,", duty_buf[(f*cols*rows)+(r*cols)+c]);
            }
            Serial.println();
        }
    }
    */

    _frames = new Frame *[frames];
    int i = 0;
    Serial.printf("frames:%d,cols:%d,rows:%d\n",frames,cols,rows);
    for (int frame = 0; frame < frames; frame++)
    {
        uint16_t *new_duty_array = new uint16_t[cols * rows];
        //copy content from buffer to new array (safer than using slices of the buffer as we did before 
        //because using slices means that attempting to free that region of memory (from the Frame object) will cause a hard fault)
        //Old, unsafe, solution: _frames[frame] = new Frame(&duty_buf[frame * cols * rows]);
        for (int y = 0; y < _rows; y++)
        {
            for (int x = 0; x < _cols; x++)
            {
                new_duty_array[y * _cols + x] = duty_buf[i++];
            }
        }
        _frames[frame] = new Frame(new_duty_array,cols,rows);
    }
    
    /* The below was needed when duty_cycle was stored differently in this method and in the frame object.
    Therefore it should now be irrelevant because we have changed how the Frame object stores duty cycles
    //Fill buffers:
    for (int f = 0; f < frames; f++)
    {
        Frame *curr_f = get_frame(f);
        for (int c = 0; c < cols; c++)
        {
            for (int r = 0; r < rows; r++)
            {
                //TODO: Write a method for Frame that can reuse this buffer instead of copying all values.
                Serial.printf("x:%d,y:%d,val:%d\n",c,r,duty_buf[f*cols*rows + c*rows + r]);
                curr_f->write_duty_cycle_at(c,r,duty_buf[f*cols*rows + c*rows + r]);
            }          
        }
    }*/

    sdFile.flush();
    sdFile.close();
    Serial.printf("Data-file: '%s' read from SD card.\n",full_filename);

    Serial.println("Read sucessful.");
    return 1;
}

// Private Methods

int Animation::_get_next_frame_idx()
{
    //NOTE TO SELF: This function should not modify any variables!
    if (!_current_frame_is_on_edge())
    {
        //Current frame is NOT on the edge
        if(_dir_fwd){
            return _current_frame + 1;
        }else{
            return _current_frame - 1;
        }

        //TODO: Add case for "ONCE" where the animation does not start at 0 or num_frames - 1
        //When that is done, the case of how to handle "edges" in the frame array for the "ONCE" case 
        //also needs to be changed.
    }
    else
    {
        //Current frame IS on the edge
        switch (_playback_type)
        {
        case LOOP:
            if (_dir_fwd)
            {
                if(_current_frame != 0){
                    return 0;
                }
                return _current_frame + 1;
            }
            if(_current_frame == 0){
                return _num_frames - 1;
            }
            return _current_frame - 1;
            
        case ONCE:
            if(_current_frame != _start_idx){
                return -1; //signals that an "empty frame" should be returned from the function calling this
            }
            if(_dir_fwd){
                if(_current_frame == 0){
                    return _current_frame + 1;
                }
                return 0; //the start condition was the last frame but direction forward
            }
            if(_current_frame != 0){
                return _current_frame - 1;
            }
            return _num_frames - 1;//The start condition was 0 but direction backward
        case BOUNCE:
            if (_dir_fwd)
            {
                return _current_frame + 1;
            }
            return _current_frame - 1;
            
            
        case LOOP_N_TIMES:
            if (_loop_iteration >= _max_iterations)
            {
                //using >= in case _max_iterations has illegal value (negative value)
                return -1; //signals that an "empty frame" should be returned from the function calling this
            }
            if (_dir_fwd)
            {
                if (_current_frame != 0)
                {
                    return 0;
                }
                return _current_frame + 1;
            }
            if (_current_frame == 0)
            {
                return _num_frames - 1;
            }
            return _current_frame - 1;

        default:
            _playback_state = ERROR;
            return -1;
        }
    }
}

int Animation::_get_prev_frame_idx()
{
   return _prev_frame;
}

bool Animation::_current_frame_is_on_edge(){
    if ((_current_frame > 0) && (_current_frame < (_num_frames - 1)) ){
        return false;
    }
    return true;
}
