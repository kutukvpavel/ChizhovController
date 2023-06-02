#include "motor.h"

#include "nvs.h"

#include <math.h>

//private
#define SPEED_RANGE_NUM 3
#define MIN_TIMER_ARR (UINT16_MAX / 1000)
#define SET_STATUS_BIT(b) reg->status |= (1u << (b))
#define RESET_STATUS_BIT(b) reg->status &= ~(1u << (b))
#define CHECK_STATUS_BIT(b) ((reg->status & (1u << status_bits::missing)) > 0)

struct speed_range_t
{
    float min_hz;
    float max_hz;
    float hyst_hz;
    float hz_to_arr;
    uint16_t psc;
};
static speed_range_t ranges[SPEED_RANGE_NUM] = {
    {

    },
    {

    },
    {

    }
};

static size_t calculate_range(float pulse_hz, size_t current)
{
    auto& cr = ranges[current];
    if ((pulse_hz < (cr.max_hz + cr.hyst_hz)) && (pulse_hz > (cr.min_hz - cr.hyst_hz))) return current;

    float last_fit = __FLT_MAX__;
    size_t last_range = SPEED_RANGE_NUM - 1;
    float fit;
    for (size_t i = 0; i < array_size(ranges); i++)
    {
        auto& item = ranges[i];
        fit = abs((pulse_hz - item.min_hz) / (item.max_hz - item.min_hz) - 0.5f);
        if (fit < last_fit)
        {
            last_range = i;
            last_fit = fit;
        }
    }

    assert_param((last_fit < 1) && (last_fit > 0));
    assert_param(last_range < SPEED_RANGE_NUM);
    return last_range;
}

//motor_t

motor_t::motor_t(TIM_HandleTypeDef* tim, sr_io::out dir, const motor_params_t* p, motor_reg_t* r) 
    : timer(tim), pin_dir(dir), params(*p), reg(r)
{
    static_assert(sizeof(motor_params_t) % 2 == 0);
    static_assert(sizeof(motor_reg_t) % 2 == 0);

    params_mutex = xSemaphoreCreateMutex();
    assert_param(params_mutex);

    if (tim->Instance == TIM3)
    {
        MX_TIM3_Init();
    }
    else if (tim->Instance == TIM4)
    {
        MX_TIM4_Init();
    }
    else if (tim->Instance == TIM10)
    {
        MX_TIM10_Init();
    }
    else if (tim->Instance == TIM11)
    {
        MX_TIM11_Init();
    }
    else
    {
        DBG("Unknown pump timer!");
    }
}

motor_t::~motor_t()
{
    vSemaphoreDelete(params_mutex);
}

void motor_t::reload_params()
{
    while (xSemaphoreTake(params_mutex, portMAX_DELAY));
    params = *nvs::get_motor_params();
    xSemaphoreGive(params_mutex);
}

void motor_t::set_volume_rate(float v)
{
    if (v == last_volume_rate) return;
    if (v <= __FLT_EPSILON__)
    {
        HAL_TIM_Base_Stop(timer);
        last_volume_rate = 0;
        reg->volume_rate = 0;
        reg->rps = 0;
        return;
    }

    while (xSemaphoreTake(params_mutex, portMAX_DELAY));
    float rps = v * params.volume_rate_to_rps;
    if (rps > params.max_rate_rps) rps = params.max_rate_rps;
    reg->rps = rps;
    reg->volume_rate = rps / params.volume_rate_to_rps;
    float pulse_hz = rps * params.microsteps * params.teeth;
    xSemaphoreGive(params_mutex);

    current_range = calculate_range(pulse_hz, current_range);
    uint16_t psc = ranges[current_range].psc;
    float farr = roundf(ranges[current_range].hz_to_arr / pulse_hz);
    if (farr > UINT16_MAX) farr = UINT16_MAX;
    else if (farr < MIN_TIMER_ARR) farr = MIN_TIMER_ARR;
    uint16_t arr = static_cast<uint16_t>(farr);
    
    taskENTER_CRITICAL();
    timer->Instance->PSC = psc;
    timer->Instance->ARR = arr;
    taskEXIT_CRITICAL();

    if (last_volume_rate <= 0)
    {
        HAL_TIM_Base_Start(timer);
    }
    last_volume_rate = v;
}

float motor_t::get_volume_rate()
{
    return last_volume_rate;
}

float motor_t::get_speed_fraction()
{
    return reg->rps / params.max_rate_rps;
}

void motor_t::set_load_err(float v)
{
    reg->err = v;
    if (v > params.max_load_err) SET_STATUS_BIT(status_bits::overload);
    else RESET_STATUS_BIT(status_bits::overload);
}

float motor_t::get_load_fraction()
{
    float res = abs(reg->err / params.max_load_err);
    if (res > 1) res = 1;
    return res;
}

bool motor_t::get_overload()
{
    return CHECK_STATUS_BIT(status_bits::overload);
}
void motor_t::set_missing(bool v)
{
    if (v) SET_STATUS_BIT(status_bits::missing);
    else RESET_STATUS_BIT(status_bits::missing);
}
bool motor_t::get_missing()
{
    return CHECK_STATUS_BIT(status_bits::missing);
}
void motor_t::set_paused(bool v)
{
    if (get_paused() == v) return;
    if (v) {
        HAL_TIM_Base_Stop(timer);
        SET_STATUS_BIT(status_bits::paused);
    }
    else {
        HAL_TIM_Base_Start(timer);
        RESET_STATUS_BIT(status_bits::paused);
    }
}
bool motor_t::get_paused()
{
    return CHECK_STATUS_BIT(status_bits::paused);
}