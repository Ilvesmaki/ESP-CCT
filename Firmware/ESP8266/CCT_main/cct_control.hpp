#ifndef __CCT_CONTROL__H__
#define __CCT_CONTROL__H__

#include <math.h>

const float DEFAULT_WW_CW_RATIO = 0.5f;
const float CONTROL_MIN = 0.0f;
const float CONTROL_MAX = 1.0f;
const float DEFAULT_BRIGHTNESS = 0.5f;
const unsigned int DEFAULT_MAX_CONTROL_VALUE = 255;
const float MIN_BRIGHTNESS = 0.01f;

/* Determines the time when exiting from setting the ww_cw ratio if no button is
pressed during this time. */
const unsigned int SETTINGS_OFF_TIME = 5000;

/* Button presses under this period are consired as ON/OFF signals */
const int TOGGLE_LIMIT = 300; // ms

enum status_t
{
    OFF = 0,
    ON = 1
};

class cct_control
{
private:

    void buttonControl();

    /**
     * @brief Set the outputs to given brightness
     * 
     * @param brightness brightness value
     */
    void setOutputs(float brightness);

    void toggle();

    void updateAlert();

    /* data */
    const float CCT_RATIO_STEP = 0.0002f;
    const float BRIGHTNESS_STEP = 0.0002f;

    unsigned int max_control_value_;

    /* 0.0 means ww full and cw off.
       0.5 means both "full".
       1.0 means cw full on */
    float ww_cw_ratio_;

    /* global brightness from MIN_BRIGHTNESS to given 1.0. */
    float brightness_ = MIN_BRIGHTNESS;
    int direction_ = 1;

    /* Determines if we are currently in the setup mode */
    bool ww_cw_setup_ = false;
    /* Leave timer for setup. Starts when button is turned off. Resets when pressed*/
    unsigned int ww_cw_setup_timer_ = 0;

    /* Pointers to the control values outside of this class*/
    unsigned int *cw_value_ = nullptr;
    unsigned int *ww_value_ = nullptr;

    /* If outside reference values are not given, use these values*/
    unsigned int loc_cw_value_ = 0;
    unsigned int loc_ww_value_ = 0;

    /* Holds the current button status */
    status_t button_status_;
    unsigned int button_press_timer_ = 0;

    status_t output_status_ = OFF;

    status_t output_inverted_;

    unsigned int delayed_off_timer_ = 0;

    bool externally_controlled_ = false;

    void (*values_updated_cb_)() = nullptr;

public:
    /**
     * @brief Construct a new cct control object
     * 
     * @param cw_value Pointer to an cw_value where new pwm values are stored, 
     * use nullptr if not used.
     * @param ww_value Pointer to an ww_value where new pwm values are stored, 
     * use nullptr if not used.
     * @param button_status button status during initialization. If this cannot
     * be determined during init, use setButtonStatus() and then start() before
     * launching tick service
     * @param output_inverted Wheter output is inverted or not, default OFF
     * @param max_control_value Max control value for the PWM outputs, defaults to 255
     * @param brightness brightness on startup
     * @param ww_cw_ratio ww_cw_ratio on startup
     */
    cct_control(unsigned int *cw_value, 
                unsigned int *ww_value,
                status_t button_status,
                status_t output_inverted = OFF,
                unsigned int max_control_value = DEFAULT_MAX_CONTROL_VALUE, 
                float brightness = DEFAULT_BRIGHTNESS, 
                float ww_cw_ratio = DEFAULT_WW_CW_RATIO);

    ~cct_control();

    /**
     * @brief Starts the controller from beginning
     * 
     * If user cannot determine, for example, button status when constructing
     * static object, user can set the button status in setup function and then
     * call this function. This will only move the object to ww_cw setup if 
     * button is pressed when this is called. Note that there is no need to call
     * this if button status is already determined in the construction phase.
     * 
     */
    void start();

    /**
     * @brief Counts the internal timer and does the calculation if needed.
     * 
     * Call this function periodically in every 1ms in tick handler or directly 
     * from timer ISR. 
     * This adjusts the output pointer values thus it is not thread safe. 
     * 
     */
    void tick();

    /**
     * @brief Function will turn the outputs on for given time.
     * 
     * If button is pressed or output is explicitely turned off via other 
     * functions during this time interval, the outputs will turn off and timer
     * will be reseted. This is usually used with PIR sensor etc.
     * 
     * @param time On time in milliseconds. 
     */
    void delayedOff(unsigned int time);

    /**
     * @brief Set the new button status
     * 
     * This function is typically called from ISR. It starts counter to count
     * the on time of the button to determine the function.
     * 
     * @param new_status New status to be placed. Either ON or OFF.
     */
    void setButtonStatus(status_t new_status);

    /**
     * @brief Set the Output Off externally
     * 
     * If the button is currently pressed, it disables its current behaviour 
     * until unpressed.
     * 
     */
    void setOutputOff();

    /**
     * @brief Set the Output On externally
     * 
     * If the button is currently pressed, it disables its current behaviour 
     * until unpressed.
     * 
     */
    void setOutputOn();

    void setBrightness(float new_brightness);

    void setWWCWRatio(float new_ratio);

    void setUpdateCB(void (*cb_fun)());

    float getWWCWRatio();

    float getBrightness();

    unsigned int getWWValue();

    unsigned int getCWValue();

    status_t getOutputStatus();

};

#endif  //!__CCT_CONTROL__H__