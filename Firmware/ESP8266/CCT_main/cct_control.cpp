#include "cct_control.hpp"

cct_control::cct_control(unsigned int *cw_value, 
                unsigned int *ww_value,
                status_t button_status,
                status_t output_inverted, 
                unsigned int max_control_value, 
                float brightness, 
                float ww_cw_ratio): cw_value_(cw_value), ww_value_(ww_value),
                    button_status_(button_status), output_inverted_(output_inverted),
                    max_control_value_(max_control_value), brightness_(brightness), 
                    ww_cw_ratio_(ww_cw_ratio)
{
    if(cw_value == nullptr)
    {
        cw_value = &loc_cw_value_;
    }
    if(ww_value == nullptr)
    {
        ww_value = &loc_ww_value_;
    }
    start();
    
}

cct_control::~cct_control()
{

}

void cct_control::start()
{
    if(button_status_ == ON)
    {
        ww_cw_setup_ = true;
        /* Set outputs full on during setup */
        setOutputs(CONTROL_MAX);
    }
    else
    {
        setOutputs(0);
    }
}

void cct_control::tick()
{
    buttonControl();

    if(delayed_off_timer_ != 0)
    {
        delayed_off_timer_--;
        if(delayed_off_timer_ == 0 && button_status_ == OFF)
        {
            setOutputOff();
        }
    }
}

void cct_control::delayedOff(unsigned int time)
{
    if(delayed_off_timer_ != 0 || output_status_ == OFF)
    {
        delayed_off_timer_ = time;
        setOutputs(brightness_);
    }   
}

void cct_control::setButtonStatus(status_t new_status)
{
    button_status_ = new_status;
}

void cct_control::setOutputOff()
{
    if(button_status_ == ON)
    {
        externally_controlled_ = true;
    }
    setOutputs(0);
}

void cct_control::setOutputOn()
{
    if(button_status_ == ON)
    {
        externally_controlled_ = true;
    }
    setOutputs(brightness_);
}

void cct_control::setBrightness(float new_brightness)
{
    if(new_brightness > max_control_value_)
    {
        return;
    }
    
    if(button_status_ == ON)
    {
        externally_controlled_ = true;
    }
    brightness_ = new_brightness;
}

void cct_control::setWWCWRatio(float new_ratio)
{
    if(new_ratio < 0.0 || new_ratio > 1.0)
    {
        return;
    }

    if(button_status_ == ON)
    {
        externally_controlled_ = true;
    }
    ww_cw_ratio_ = new_ratio;
}

void cct_control::setUpdateCB(void (*cb_fun)())
{
    values_updated_cb_ = cb_fun;
}

float cct_control::getWWCWRatio()
{
    return ww_cw_ratio_;
}

float cct_control::getBrightness()
{
    return brightness_;
}

unsigned int cct_control::getWWValue()
{
    return *ww_value_;
}

unsigned int cct_control::getCWValue()
{
    return *cw_value_;
}

status_t cct_control::getOutputStatus()
{
    return output_status_;
}

/* PRIVATE MEMBER FUNCTIONS */

void cct_control::buttonControl()
{
    if(button_status_ == ON)
    {
        if(externally_controlled_)
        {
            button_press_timer_ = 0;
            return;
        }
        button_press_timer_++;

        if(button_press_timer_ > TOGGLE_LIMIT)
        {
            if(ww_cw_setup_)
            {
                if((ww_cw_ratio_ > CONTROL_MIN && direction_ == -1) || 
                    (ww_cw_ratio_ < CONTROL_MAX && direction_ == 1))
                {
                    ww_cw_ratio_ += direction_*CCT_RATIO_STEP;
                    ww_cw_ratio_ = fmax(CONTROL_MIN, fmin(CONTROL_MAX, ww_cw_ratio_));
                }
                ww_cw_setup_timer_ = SETTINGS_OFF_TIME;
                setOutputs(max_control_value_);
            }
            else
            {
                if((brightness_ > MIN_BRIGHTNESS && direction_ == -1) || 
                    (brightness_ < max_control_value_ && direction_ == 1))
                {
                    brightness_ += direction_*BRIGHTNESS_STEP;
                    brightness_ = fmax(CONTROL_MIN, fmin(CONTROL_MAX, brightness_));
                }
                setOutputs(brightness_);
            }
        }
    }
    else
    {
        if(externally_controlled_)
        {
            externally_controlled_ = false;
        }
        else if(ww_cw_setup_)
        {
            if(ww_cw_setup_timer_ == 0)
            {
                setOutputs(brightness_);
                ww_cw_setup_ = false;
                updateAlert();
            }
            else
            {
                ww_cw_setup_timer_--;
            }
        }
        else if(button_press_timer_ != 0 && button_press_timer_ <= TOGGLE_LIMIT)
        {
            toggle();
            updateAlert();
        }
        
        if(button_press_timer_ > TOGGLE_LIMIT)
        {
            direction_ *= -1;
            updateAlert();
        }
        button_press_timer_ = 0;      
    }
}

void cct_control::setOutputs(float brightness)
{
    if(brightness == 0.0f)
    {
        output_status_ = OFF;
        delayed_off_timer_ = 0;
    }
    else
    {
        output_status_ = ON;
    }

    float new_ww_value_f = (CONTROL_MAX - ww_cw_ratio_) * brightness * max_control_value_;
    float new_cw_value_f = ww_cw_ratio_ * brightness * max_control_value_;
    unsigned int new_ww_value = (unsigned int)fmax(0.0f, new_ww_value_f);
    unsigned int new_cw_value = (unsigned int)fmax(0.0f, new_cw_value_f);
    new_ww_value = new_ww_value <= max_control_value_ ? new_ww_value : max_control_value_;
    new_cw_value = new_cw_value <= max_control_value_ ? new_cw_value : max_control_value_;

    if(output_inverted_ == ON)
    {
        new_ww_value = max_control_value_ - new_ww_value;
        new_cw_value = max_control_value_ - new_cw_value;
    }
    
    *ww_value_ = new_ww_value;
    *cw_value_ = new_cw_value;
}

void cct_control::toggle()
{   
    if(output_status_ == ON)
    {
        setOutputs(0);
    }
    else
    {
        setOutputs(brightness_);
    }
}

void cct_control::updateAlert()
{
    if(values_updated_cb_)
    {
        (*values_updated_cb_)();
    }
}
