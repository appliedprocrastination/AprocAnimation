#include "Animation.h"
#include "FreeStack.h"
#include "csv_helpers.h"

File sdFile;
//Constructor
Frame::Frame(uint32_t *picture, bool alloced_picture, uint8_t *duty_cycle, bool alloced_duty, int cols, int rows)
{
    _cols = cols;
    _rows = rows;
    if (duty_cycle == nullptr)
    {
        int tolerance = 10000;
        if((FreeStack() < (_cols*_rows + tolerance))){
            Serial.printf("Failed to initialize frame with duty_cycle array of size: %d\n"
            "Available space in RAM: %d\n", _cols*_rows, FreeStack());
            return;
        }
        //Serial.printf("Initializing frame with duty_cycle array of size: %d\n"
        //   "Available space in RAM: %d\n", _cols*_rows, FreeStack());

        _malloced_duty = true;
        _duty_cycle = new uint8_t [cols*rows];
        for (int row = 0; row < rows; row++)
        {
            //_duty_cycle[col] = new uint8_t[rows];
            for (int col = 0; col < cols; col++)
            {
                _duty_cycle[row*cols + col] = (uint8_t)DUTY_CYCLE_RESOLUTION;
            }
            Serial.println();
        }
    }else
    {
        //Serial.printf("Starting to fill duty cycle from %s array at: %p\n", alloced_duty?"dynamically allocated":"static",duty_cycle);
        _malloced_duty = alloced_duty;
        _duty_cycle = duty_cycle;
        for (int y = 0; y < rows; y++)
        {
            for (int x = 0; x < cols; x++)
            {
                if (duty_cycle[y*cols + x] < DUTY_CYCLE_RESOLUTION)
                {
                    pwm_pixels_x.push_back(x);
                    pwm_pixels_y.push_back(y);
                }
            }
        }
    }

    if (picture == nullptr)
    {
        _picture = new uint32_t[cols];
        _malloced_picture = true;
    }
    else
    {
        _picture = picture;
        _malloced_picture = alloced_picture;
    }
}
Frame::~Frame()
{
    this->delete_frame();
}

// Public Methods
void Frame::delete_frame(void)
{
    pwm_pixels_x.clear();
    pwm_pixels_x.shrink_to_fit();
    pwm_pixels_y.clear();
    pwm_pixels_y.shrink_to_fit(); 
    //TODO: Is the above necessary?
    if(_malloced_duty)
        _delete_duty_cycle();
    if(_malloced_picture)
        _delete_picture();
}
uint32_t *Frame::get_picture()
{
    return _picture;
}
uint32_t Frame::get_picture_at(int x)
{
    return _picture[x];
}
uint8_t *Frame::get_duty_cycle()
{
    return _duty_cycle;
}
uint8_t Frame::get_duty_cycle_at(int x, int y)
{
    return _duty_cycle[y*_cols + x];
}

void Frame::overwrite_duty_cycle(uint8_t *duty_cycle,bool alloced_duty)
{
    _delete_duty_cycle();
    _duty_cycle = duty_cycle;
    _malloced_duty = alloced_duty;
}

void Frame::write_duty_cycle_at(int x, int y, uint8_t duty_cycle)
{
    if (duty_cycle > DUTY_CYCLE_RESOLUTION)
    {
        duty_cycle = DUTY_CYCLE_RESOLUTION;
    }

    if ((duty_cycle == DUTY_CYCLE_RESOLUTION) && (_duty_cycle[x + y*_cols] != DUTY_CYCLE_RESOLUTION))
    {
        //The coordinate is already stored in the pwm_pixels vectors,
        //but the new value is not a PWM value, so the coord. must be removed
        for (size_t i = 0; i < pwm_pixels_x.size(); i++)
        {
            if (pwm_pixels_x[i] == x && pwm_pixels_y[i] == y)
            {
                pwm_pixels_x.erase(pwm_pixels_x.begin() + i);
                pwm_pixels_y.erase(pwm_pixels_y.begin() + i);
                break;
            }
        }
    }
    else if ((duty_cycle < DUTY_CYCLE_RESOLUTION) && (duty_cycle > 0) && (_duty_cycle[x + y*_cols] == DUTY_CYCLE_RESOLUTION))
    {
        //The coordinates are not stored in the pwm_pixels vectors
        pwm_pixels_x.push_back(x);
        pwm_pixels_y.push_back(y);
    }
    else
    {
        //duty_cycle is either max or minimum (or see next line), no need to check for those values in the refresh screen routine.
        //the third possibility is that the coordinate is already stored in the pwm_pixels vectors, so no need to add them.
    }
    _duty_cycle[x + y*_cols] = duty_cycle;
}
void Frame::write_picture(uint32_t *picture, bool alloced_picture)
{
    _delete_picture();
    _picture = picture;
    _malloced_picture = alloced_picture;
}
void Frame::write_picture_at(int x, uint32_t picture)
{
    _picture[x] = picture;
}


void Frame::write_pixel(int x, int y, bool state)
{
    if (state)
    {
        set_pixel(x, y);
    }
    else
    {
        clear_pixel(x, y);
    }
}
void Frame::set_pixel(int x, int y)
{
    _picture[x] |= 1 << y;
}
void Frame::clear_pixel(int x, int y)
{
    _picture[x] &= ~(1 << y);
}

void Frame::merge_column_at(int x, int y_offset, uint32_t other_column){
    _picture[x] |= other_column << y_offset;
}

// Private Methods
inline void Frame::_delete_picture()
{
    if(_malloced_picture){
        delete _picture;
        _malloced_picture = false;
    }
}

inline void Frame::_delete_duty_cycle()
{
    if (_malloced_duty){
        delete _duty_cycle;
        _malloced_duty = false;
    }
}

//Constructor
Animation::Animation(Frame **frames, int num_frames, bool alloced_frames, bool alloced_frame_array, int cols, int rows)
{
    _num_frames = num_frames;
    _cols = cols;
    _rows = rows;
    
    if (frames == nullptr)
    {
        _frames = new Frame *[num_frames];
        for (int frame = 0; frame < num_frames; frame++)
        {
            _frames[frame] = new Frame();
        }
        _malloced_frames = true;
        _malloced_frame_array = true;
    }
    else
    {    
        _frames = frames;
        _malloced_frames = alloced_frames;
        _malloced_frame_array = alloced_frame_array;
    }
}

// Public Methods
void Animation::delete_anim(void)
{
    if(_malloced_frames){
        for (int frame = 0; frame < _num_frames; frame++)
        {
            _frames[frame]->delete_frame();
        }
    }
    if(_malloced_frame_array)
        delete _frames;
    if(_frame_buf != nullptr)
        delete _frame_buf;
    if (_duty_buf != nullptr)
        delete _duty_buf;
}

Frame *Animation::get_frame(int frame_num)
{
    return _frames[frame_num];
}
void Animation::write_frame(int frame_num, Frame *frame)
{
    _frames[frame_num] = frame;
}
void Animation::load_frames_from_array(uint32_t **animation, bool alloced_animation, uint8_t **duty_cycle, bool alloced_duty)
{
    //TODO: This method has not been properly adjusted to new variant of duty_cycle storage
    for (int frame = 0; frame < _num_frames; frame++)
    {
        _frames[frame]->write_picture(animation[frame],alloced_animation);
        if (duty_cycle != nullptr)
        {
            _frames[frame]->overwrite_duty_cycle(duty_cycle[frame],alloced_duty);
        }
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
        return &_blank_frame;
    }
    return _frames[_current_frame];
}
Frame *Animation::get_next_frame(){
    int idx = _get_next_frame_idx();
    if (idx == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
    }
    return _frames[idx];
}
Frame *Animation::get_prev_frame(){
    if (_prev_frame == -1 || _playback_state == IDLE)
    {
        return &_blank_frame;
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
    output[0] = _width;
    output[1] = _height;
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
int* Animation::get_absolute_location(int* output)
{
    output[0] = _location_x - _origin_x;
    output[1] = _location_y - _origin_y;
    return output;
}

int Animation::merge_with(Animation* other){
    // Before merging, verify that "other" is contained within the frame of "this"
    // If the entire "other" animation is outside the canvas of "this", then ignore it 
    int *abs_loc_other = new int[2];
    int *size_other = new int[2];

    int *origin_this = new int[2];
    int *size_this = new int[2];
    if (abs_loc_other != nullptr && size_other != nullptr && origin_this != nullptr && size_this != nullptr)
    {
        abs_loc_other = other->get_absolute_location(abs_loc_other);
        size_other = other->get_size(size_other); //width, height
        int other_x = abs_loc_other[0];
        int other_y = abs_loc_other[1];
        int other_width = size_other[0];
        int other_height = size_other[1];
        

        origin_this = this->get_origin(origin_this);
        size_this = this->get_size(size_this); //width, height
        int this_x = origin_this[0];
        int this_y = origin_this[1];
        int this_width = size_this[0];
        int this_height = size_this[1];

        int this_leftborder = this_x - this_width;
        int this_rightborder = this_width - this_x;
        int this_bottomborder = this_y - this_height;
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
        bool equal_num_frames = true;
        bool other_is_shortest = true; //only used if equal_num_frames goes false
        if (other->get_num_frames() < _num_frames)
        {
            // Copy last frame of other until it has the same length as this one
            // This is done below, because we don't want to alter the state of the other animation, just this one. 
            equal_num_frames = false;
            other_is_shortest = true; //unnecessary, but for clarity.
        }
        else if (other->get_num_frames() > _num_frames)
        {
            //Allocate new memory for this animation to expand until it has the same length as the other animation
            int difference = other->get_num_frames() - _num_frames;
            //Repeat the last frame of this animation the number of times determined by difference

            int tolerance = 10000;
            if (!(FreeStack() > (int)(_num_frames * _cols * sizeof(uint32_t) + _num_frames * _cols * _rows + tolerance)))
            {
                Serial.printf("Not enough memory to store animation of size: %d\n"
                                "Available space in RAM: %d",
                                _num_frames * _cols * 4 + _num_frames * _cols * _rows, FreeStack());
                return -2;
            }
            const int cols = _cols;
            const int rows = _rows;
            const int new_num_frames = _num_frames + difference;
            uint8_t* new_frame_buf8 = new uint8_t[new_num_frames * cols * sizeof(uint32_t)];
            if (new_frame_buf8 == 0)
            {
                Serial.printf("Could not allocate memory for frames of size: %d\n"
                                "Available space in RAM: %d",
                                new_num_frames * cols * sizeof(uint32_t), FreeStack());
            }

            uint32_t* new_frame_buf = (uint32_t *)new_frame_buf8;

            uint8_t* new_duty_buf = new uint8_t[new_num_frames * cols * rows * sizeof(uint8_t)];
            if (&new_duty_buf[0] == 0 || new_duty_buf == 0 || new_duty_buf == (uint8_t *)nullptr || &new_duty_buf[0] == (uint8_t *)nullptr || new_duty_buf == nullptr || &new_duty_buf[0] == nullptr || new_duty_buf == NULL || &new_duty_buf[0] == NULL || &new_duty_buf[0] == (uint8_t *)0 || new_duty_buf == (uint8_t *)0)
            {
                Serial.printf("Could not allocate memory for duty_cycle arr of size: %d\n"
                                "Available space in RAM: %d\n",
                                new_num_frames * cols * rows, FreeStack());

                Serial.printf("Pointer to _duty_buf: %8p,%d\n", _duty_buf, _duty_buf);
                Serial.printf("Pointer to _frame_buf: %p\n", _frame_buf);
                return -1;
            }

            //Copy content of old buffer into the new ones so the allocated memory of the old ones can be free'd 
            int f_old = 0; //Index of current frame in the old buffer (the last frame will be copied until the new buffer is filled)
            int f_index = 0; //frame index
            int d_index = 0; //duty index
            int startindex = 0; //startindex of the currently copied frame or duty buffer
            int endindex = 0; //endindex of the currently copied frame or duty buffer
            for (int f = 0; f < new_num_frames; f++)
            {
                if (f < _num_frames){
                    f_old = f; 
                }else{
                    f_old = _num_frames-1;
                }
                //copy frame:
                //new_frame_buf8 contains 252 1-bit values stored as 32-bit, represented as 8-bit...... Used as 12-bit. (Sorry)
                //TODO: The following is probably wrong, but I'm having trouble verifying it without access to hardware (because of what described in the line above)
                startindex =   f_old * cols * sizeof(uint32_t);
                endindex = (f_old + 1) * cols * sizeof(uint32_t); 
                for (int i = startindex; i < endindex; i++)
                {
                    new_frame_buf8[i] = _frame_buf8[f_index++];                
                }
                
                //new_duty_buf contains 252 8-bit values stored as 8-bit
                startindex = f_old * cols * rows * sizeof(uint8_t);
                endindex = (f_old + 1) * cols * rows * sizeof(uint8_t);
                for (int i = startindex; i < endindex; i++)
                {
                    new_duty_buf[i] = _duty_buf[d_index++];
                }
            }
            //Used as reference:
            //sdFile.read(new_frame_buf8, new_num_frames * cols * sizeof(uint32_t));
            //sdFile.read(new_duty_buf, new_num_frames * cols * rows);

            Frame** new_frames = new Frame *[new_num_frames];
            Serial.printf("frames:%d,cols:%d,rows:%d\n", new_num_frames, cols, rows);
            for (int frame = 0; frame < new_num_frames; frame++)
            {
                //The arrays sent to the frame constructor are marked as NOT dynamically allocated (even though they are)
                //This is because the "delete" will be handled by the animation object, and should therefore not be performed by the frame object.
                new_frames[frame] = new Frame(&new_frame_buf[frame * cols], false, &new_duty_buf[frame * cols * rows], false);
            }

            //Clear memory of old frames
            if (_malloced_frames)
            {
                for (int i = 0; i < _num_frames; i++)
                {
                    //Serial.printf("i:%d\n",i);
                    _frames[i]->delete_frame();
                }
            } // Otherwise: they are in the program memory and can't be deleted. We just discard the pointer and allow the space to be wasted.
            _num_frames = new_num_frames;
            if (_malloced_frame_array)
            {
                delete _frames;
            } // Otherwise: it's in the program memory and can't be deleted. We just discard the pointer and allow the space to be wasted.
            _frames = new_frames;
            if (_frame_buf8 != nullptr)
            {
                delete _frame_buf8;
            }
            _frame_buf8 = new_frame_buf8;
            _frame_buf = new_frame_buf;
            if (_duty_buf != nullptr)
            {
                delete _duty_buf;
            }
            _duty_buf = new_duty_buf;

            _malloced_frame_array = true;
            _malloced_frames = true;
            

            equal_num_frames = false;
            other_is_shortest = false;
        }


        // Frame values of the two animations need to be OR'ed together
        // Duty cycle values need to be ADDED together (I think) and clamped to min and max duty cycle
        Frame* this_frame_ptr;
        Frame* other_frame_ptr;
        /*if (equal_num_frames == false)
        {
            
            if (other_is_shortest)
            {
                //use the last frame several times
            }
            //otherwise: the problem has been solved above.
        }*/
        
        for (int f = 0; f < _num_frames; f++)
        {
            this_frame_ptr = this->get_frame(f);
            if(f < other->get_num_frames()){
                other_frame_ptr = other->get_frame(f);
            }else{
                Serial.printf("Merged %d frames from 'other' to 'this' (which has %d frames)\n",f,_num_frames);
                return 1;
            }
            Serial.printf("Other w: %d, Origin h:%d, this w: %d, this h: %d\n", other_width, other_height, this_width, this_height);
            for (int x = 0; x < other_width; x++)
            {
                Serial.printf("Other_x: %d, Origin_X:%d, Other_Y: %d, Origin_y: %d\n", other_x, _origin_x, other_y, _origin_y);
                this_frame_ptr->merge_column_at(other_x + _origin_x, other_y + _origin_y, other_frame_ptr->get_picture_at(x));
                /*for (size_t y = 0; y < other_height; y++)
                {
                    //Duty cycle stuff
                }
                */
            }
        }
    }
    delete abs_loc_other;
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
    uint32_t frame_buf[frames*cols];
    uint8_t duty_buf[frames*cols*rows];

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
        _start_idx);
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
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                duty_buf[(f*cols*rows)+(r*cols)+c] = curr_f->get_duty_cycle_at(c,r);
                if(r==0)
                    frame_buf[(f*cols)+c] = curr_f->get_picture_at(c);            
            }
        }
    }
    
    uint8_t* framebuf8 = (uint8_t*) frame_buf;
    sdFile.write(framebuf8,frames*cols*sizeof(uint32_t)); //*4 because the buffer is uint32_t
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
    if (_malloced_frames)
    {
        for (int i = 0; i < _num_frames; i++)
        {
            //Serial.printf("i:%d\n",i);
            _frames[i]->delete_frame();
        }
    }
    if (_malloced_frame_array)
    {
        delete _frames;
    }
    if (_frame_buf8 != nullptr){
        delete _frame_buf8;
    }
    if (_duty_buf != nullptr)
    {
        delete _duty_buf;
    }

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
    if(!(FreeStack() > (int)(_num_frames*_cols*sizeof(uint32_t) + _num_frames*_cols*_rows + tolerance))){
        Serial.printf("Not enough memory to store animation of size: %d\n"
        "Available space in RAM: %d", _num_frames*_cols*4 + _num_frames*_cols*_rows, FreeStack());
        return -2;
    }
    const int cols = _cols;
    const int rows = _rows;
    const int frames = _num_frames;
    _frame_buf8 = new uint8_t[frames * cols * sizeof(uint32_t)];
    if (_frame_buf8 == 0)
    {
        Serial.printf("Could not allocate memory for frames of size: %d\n"
        "Available space in RAM: %d",frames*cols*sizeof(uint32_t), FreeStack());
    }

    _frame_buf = (uint32_t *)_frame_buf8;

    _duty_buf = new uint8_t[frames*cols*rows*sizeof(uint8_t)];
    if (&_duty_buf[0] == 0 || _duty_buf == 0 || _duty_buf == (uint8_t*) nullptr || &_duty_buf[0] == (uint8_t*) nullptr || _duty_buf == nullptr || &_duty_buf[0] == nullptr || _duty_buf == NULL || &_duty_buf[0] == NULL ||&_duty_buf[0] ==(uint8_t *) 0 || _duty_buf == (uint8_t *)0 )
    {
        Serial.printf("Could not allocate memory for duty_cycle arr of size: %d\n"
        "Available space in RAM: %d\n",frames*cols*rows, FreeStack());

        Serial.printf("Pointer to _duty_buf: %8p,%d\n",_duty_buf,_duty_buf);
        Serial.printf("Pointer to _frame_buf: %p\n",_frame_buf);
        return-1;
    }else{
        //Serial.printf("Pointer to _duty_buf: %p,%d\n",_duty_buf,_duty_buf);
        //Serial.printf("Pointer to _frame_buf: %p\n",_frame_buf);
    }
    sprintf(full_filename,"A%u_D.bin",file_index);
    if (!sdFile.open(full_filename, O_RDONLY)) {
        Serial.printf("open file: '%s' failed\n",full_filename);
        sd.errorHalt("open failed");
        return -1;
    }

    sdFile.read(_frame_buf8, frames * cols * sizeof(uint32_t));
    sdFile.read(_duty_buf,frames*cols*rows);
    

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
            Serial.printf("%8lp, [frame:%2d,row:%2d]:",&_duty_buf[(f*cols*rows)+(r*cols)], f,r);
            for (int c = 0; c < cols; c++)
            {
                Serial.printf("0x%2x,", _duty_buf[(f*cols*rows)+(r*cols)+c]);
            }
            Serial.println();
        }
    }
    */

    _frames = new Frame *[frames];
    Serial.printf("frames:%d,cols:%d,rows:%d\n",frames,cols,rows);
    for (int frame = 0; frame < frames; frame++)
    {
        //The arrays sent to the frame constructor are marked as NOT dynamically allocated (even though they are)
        //This is because the "delete" will be handled by the animation object, and should therefore not be performed by the frame object.
        _frames[frame] = new Frame(&_frame_buf[frame * cols], false, &_duty_buf[frame * cols * rows], false);
    }
    _malloced_frame_array = true;
    _malloced_frames = true;
    
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
                Serial.printf("x:%d,y:%d,val:%d\n",c,r,_duty_buf[f*cols*rows + c*rows + r]);
                curr_f->write_duty_cycle_at(c,r,_duty_buf[f*cols*rows + c*rows + r]);
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
