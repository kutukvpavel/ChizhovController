#include "motor.h"

motor_t::motor_t(TIM_HandleTypeDef* tim, sr_io::out dir) : timer(tim), pin_dir(dir)
{
}

motor_t::~motor_t()
{
}

void motor_t::set_volume_rate(float v)
{
    
}

float motor_t::get_volume_rate()
{

}