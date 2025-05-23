/*
* FW-Dyson-BMS	-	(unofficial) Firmware Upgrade for Dyson BMS - V6/V7 Vacuums
* Copyright (C) 2022 tinfever
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
* The author can be contacted at tinfever6@(insert-everyone's-favorite-google-email-domain).com
* 
* NOTE: As an addendum to the GNU General Public License, any hardware using code or information from this project must also make publicly available complete electrical schematics and a bill of materials for such hardware.
*/

#include "main.h"
#include "LED.h"
#include "mcc_generated_files/epwm1.h"
#include "config.h"
#include "isl94208.h"

// Example: ledBlinkpattern (3, 0b110, 250, 250, 750, 500, 32);
// This would give 3 yellow (0b110 RGB) blinks, with 250ms ontime and 250ms off time
// There would be a 750ms blank time before and 500ms blank time after the three blinks
// There is a PWM fade slope of positive 32, so each blink will fade in at a rate of +32 to the PWM level per loop iteration
void ledBlinkpattern (uint8_t num_blinks, uint8_t led_color_rgb,
                    uint16_t blink_on_time_ms, uint16_t blink_off_time_ms,
                    uint16_t starting_blank_time_ms, uint16_t ending_blank_time_ms,
                    int8_t pwm_fade_slope){       
    uint16_t timer_ms = nonblocking_wait_counter.value*32;
    
    static uint8_t max_steps = 0; 
    static uint8_t step = 0;
    static uint16_t next_step_time = 0;
    
    /*Example step schedule for num_blinks = 3, blink_interval_ms = 500, starting_blank_time_ms = 1000, ending_blank_time_ms = 1000
    *Step #     End time    LED ON
    * 0          1000,      0     //starting blank interval
    * 1          1500,      1     //blink one
    * 2          2000,      0
    * 3          2500,      1     //blink two
    * 4          3000,      0
    * 5          3500,      1     //blink three
    * 6          4000,      0
    * 7          5000,      0     //ending blank interval
     */
    
    uint16_t starting_pwm_val = 1023;
    if (pwm_fade_slope > 0){            //Means we need to fade in
        starting_pwm_val = 0;
    }
    
    //Initialize
    if (!nonblocking_wait_counter.enable){
        Set_LED_RGB(0b000, starting_pwm_val);     //Turn off LED
        nonblocking_wait_counter.value = 0;
        nonblocking_wait_counter.enable = 1;
        max_steps = (2*num_blinks+2)-1;     //Subtract one so it is zero indexed
        step = 0;
        next_step_time = starting_blank_time_ms;
        if (LED_code_cycle_counter.enable){
            LED_code_cycle_counter.value++;
        }
    }
    
    if (step == 0 && timer_ms > next_step_time){                        //starting blank time
        step++;
        if (num_blinks != 0){   // Leave LED off if num_blinks is zero. Next step will be cycle complete.
            Set_LED_RGB(led_color_rgb, starting_pwm_val);     //Turn on LED
        }
        next_step_time += blink_on_time_ms;
    }
    else if (step == max_steps-1 && timer_ms > next_step_time){         //ending blank time
        step++;
        Set_LED_RGB(0b000, starting_pwm_val);     //Turn off LED
        next_step_time += ending_blank_time_ms;
    }
    else if (step == max_steps && timer_ms > next_step_time){           //cycle complete
        Set_LED_RGB(0b000, starting_pwm_val);     //Turn off LED
        nonblocking_wait_counter.enable = 0;
        nonblocking_wait_counter.value = 0;
    }
    else if (step % 2 != 0 && timer_ms > next_step_time){        //Step number is odd
        step++;
        Set_LED_RGB(0b000, starting_pwm_val);     //Turn off LED
        next_step_time += blink_off_time_ms;
    }
    else if (step % 2 == 0 && timer_ms > next_step_time){       //Step number is even
        step++;
        Set_LED_RGB(led_color_rgb, starting_pwm_val);     //Turn on LED
        next_step_time += blink_on_time_ms;
    }
    
    int16_t current_pwm = (int16_t) EPWM1_ReadDutyValue();
    
    if (pwm_fade_slope < 0){                                    //Fade out
        if (current_pwm > -pwm_fade_slope){                     //Make sure we won't go below zero
            EPWM1_LoadDutyValue((uint16_t) (current_pwm +  pwm_fade_slope) );
        }
        else if (current_pwm <= -pwm_fade_slope){
            EPWM1_LoadDutyValue(0);
            nonblocking_wait_counter.value = next_step_time/32; //Shortcut to next LED step
                        if (num_blinks == 1){                           //If there is only one blink in the cycle, and the PWM fade is done, do all the clean up procedures immediately to avoid brief LED blank time
                nonblocking_wait_counter.enable = 0;
                nonblocking_wait_counter.value = 0;
                if (LED_code_cycle_counter.enable){
                    LED_code_cycle_counter.value++;
                }
            }
        }
    }
    else if (pwm_fade_slope > 0){                               //Fade in
        if (current_pwm + pwm_fade_slope < 1023){                     //make sure we don't overflow 1023 max pwm
            EPWM1_LoadDutyValue((uint16_t) (current_pwm + pwm_fade_slope));
        }
        else if (current_pwm + pwm_fade_slope >= 1023){
            EPWM1_LoadDutyValue(1023);
            nonblocking_wait_counter.value = (next_step_time/32)+1; //Shortcut to next LED step
            if (num_blinks == 1){                           //If there is only one blink in the cycle, and the PWM fade is done, do all the clean up procedures immediately to avoid brief LED blank time
                nonblocking_wait_counter.enable = 0;
                nonblocking_wait_counter.value = 0;
                if (LED_code_cycle_counter.enable){
                    LED_code_cycle_counter.value++;
                }
            }
        }

    }
    
}

void resetLEDBlinkPattern (void){
    Set_LED_RGB(0b000, 1023);     //Turn off LED
    nonblocking_wait_counter.enable = false;
    nonblocking_wait_counter.value = 0;
    LED_code_cycle_counter.enable = false;
    LED_code_cycle_counter.value = 0;
    
}
// Accepts binary input 0b000. Bit 2 = Red Enable. Bit 1 = Green Enable. Bit 0 = Red Enable. R.G.B.
// PWM_val sets PWM brightness level 0-1023
void Set_LED_RGB(uint8_t RGB_en, uint16_t PWM_val){  
    
    EPWM1_LoadDutyValue(PWM_val);
    
    if (RGB_en & 0b001){
        blueLED = 1;
    }
    else{
        blueLED = 0;
    }
    
    if (RGB_en & 0b010){
        greenLED = 1;
    }
    else{
        greenLED = 0;
    }
    
    if (RGB_en & 0b100){
        redLED = 1;
    }
    else{
        redLED = 0;
    }
}

bool cellDeltaLEDIndicator (void){
    uint8_t num_yellow_blinks = (uint8_t) ( (cellstats.packdelta_mV+25) / 50 );      //One blink per 50mV min-max cell delta. Adding 25 to do traditional rounding
    LED_code_cycle_counter.enable = true;
    ledBlinkpattern (num_yellow_blinks, 0b110, 250, 250, 750, 500, 0);
    if (LED_code_cycle_counter.value > 1){
        resetLEDBlinkPattern();
        return true;
    }
    else {
        return false;
    }
}

bool cellVoltageLEDIndicator (void){
    static bool loaded_num_green_blinks = 0;
    static uint8_t wait_count = 0;
    static uint8_t num_green_blinks = 0;
    
    if (wait_count < 5){
        wait_count++;
        return false;   //Let three main loop iterations pass to give voltage measurements a chance to stabilize after a high current load which would cause lower than normal battery voltage readings.
    }
    else if (!loaded_num_green_blinks){
        num_green_blinks = (uint8_t) ( (cellstats.mincell_mV-3000) / 200 + 1);      // Min cell < 3.2V = 1 blinks, mincell < 3.4 = 2 blink, min cell < 3.6V = 3 blinks, min cell < 3.8V = 4 blinks, min cell < 4V = 5 blinks, mincell < 4.2V = 6 blinks.
        loaded_num_green_blinks = 1;    //This reads and holds the number of blinks to be presented so the blink count won't change even if cell voltages do.
    }
    
    
    
    LED_code_cycle_counter.enable = true;
    ledBlinkpattern (num_green_blinks, 0b010, 250, 250, 500, 500, 0);
    if (LED_code_cycle_counter.value > 1){
        resetLEDBlinkPattern();
        wait_count = 0;
        loaded_num_green_blinks = 0;
        return true;
    }
    else {
        return false;
    }
}