#include "motor.h"

#include "nvs.h"

#include <math.h>

//private
#define SPEED_RANGE_NUM 3
#define MIN_TIMER_ARR (UINT16_MAX / 1000)
#define SET_STATUS_BIT(b) reg->status |= (1u << (b))
#define RESET_STATUS_BIT(b) reg->status &= ~(1u << (b))
#define CHECK_STATUS_BIT(b) ((reg->status & (1u << (b))) > 0)

struct speed_range_t
{
    float min_hz;
    float max_hz;
    float hyst_hz;
    float arr_clock;
    uint16_t psc;
};
static speed_range_t ranges[SPEED_RANGE_NUM] = {
    {
        .min_hz = 0,
        .max_hz = 70,
        .hyst_hz = 10,
        .arr_clock = 82031.25f,
        .psc = 1024 - 1
    },
    {
        .min_hz = 60,
        .max_hz = 2500,
        .hyst_hz = 10,
        .arr_clock = 2.625E6,
        .psc = 32 - 1
    },
    {
        .min_hz = 1500,
        .max_hz = 84000,
        .hyst_hz = 100,
        .arr_clock = 8.4E7,
        .psc = 0
    }
};

static size_t calculate_range(float pulse_hz, size_t current)
{
    assert_param(current < SPEED_RANGE_NUM);
    assert_param(pulse_hz >= 0);
    
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

motor_t::motor_t(TIM_HandleTypeDef* tim, uint32_t channel, sr_io::out dir, const motor_params_t* p, motor_reg_t* r) 
    : timer(tim), timer_channel(channel), pin_dir(dir), params(*p), reg(r)
{
    static_assert(sizeof(motor_params_t) % 2 == 0);
    static_assert(sizeof(motor_reg_t) % 2 == 0);

    params_mutex = xSemaphoreCreateMutex();
    assert_param(params_mutex);
    assert_param(tim);
    assert_param(p);
    assert_param(r);

    HAL_TIM_Base_Start(timer);
    set_volume_rate(reg->volume_rate);
    DBG("Created a pump: TIM @ %p, DIR @ %u, params @ %p, reg @ %p, preset = %f", 
        timer->Instance, dir, p, r, reg->volume_rate);
}

motor_t::~motor_t()
{
    HAL_TIM_PWM_Stop(timer, timer_channel);
    HAL_TIM_Base_Stop(timer);
    vSemaphoreDelete(params_mutex);
}

void motor_t::reload_params()
{
    while (xSemaphoreTake(params_mutex, portMAX_DELAY));
    params = *nvs::get_motor_params();
    xSemaphoreGive(params_mutex);
}
void motor_t::print_debug_info()
{
    printf(
        "\tVol.rate = %f\n"
        "\tTimer range = %u\n"
        "\tARR = %lu\n"
        "\tPSC = %lu\n"
        "\tCR1 = %04lX\n",
        last_volume_rate,
        current_range,
        timer->Instance->ARR,
        timer->Instance->PSC,
        timer->Instance->CR1
    );
}

HAL_StatusTypeDef motor_t::set_volume_rate(float v)
{
    if (v == last_volume_rate) return HAL_OK;
    if (v <= __FLT_EPSILON__)
    {
        HAL_TIM_PWM_Stop(timer, timer_channel);
        last_volume_rate = 0;
        reg->volume_rate = 0;
        reg->rps = 0;
        return HAL_OK;
    }

    if (xSemaphoreTake(params_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return HAL_BUSY;
    float rps = v * params.volume_rate_to_rps;
    if (rps > params.max_rate_rps) rps = params.max_rate_rps;
    reg->rps = rps;
    reg->volume_rate = rps / params.volume_rate_to_rps;
    float pulse_hz = rps * params.microsteps * params.teeth;
    xSemaphoreGive(params_mutex);

    current_range = calculate_range(pulse_hz, current_range);
    uint16_t psc = ranges[current_range].psc;
    assert_param(pulse_hz > 0);
    float farr = roundf(ranges[current_range].arr_clock / pulse_hz);
    if (farr > UINT16_MAX) farr = UINT16_MAX;
    else if (farr < MIN_TIMER_ARR) farr = MIN_TIMER_ARR;
    uint16_t arr = static_cast<uint16_t>(farr);
    
    taskENTER_CRITICAL();
    timer->Instance->PSC = psc;
    timer->Instance->ARR = arr;
    taskEXIT_CRITICAL();

    if (last_volume_rate <= 0)
    {
        HAL_TIM_PWM_Start(timer, timer_channel);
    }
    last_volume_rate = v;

    return HAL_OK;
}

float motor_t::get_volume_rate_limit()
{
    return params.max_rate_rps / params.volume_rate_to_rps;
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
        HAL_TIM_PWM_Stop(timer, timer_channel);
        SET_STATUS_BIT(status_bits::paused);
    }
    else {
        HAL_TIM_PWM_Start(timer, timer_channel);
        RESET_STATUS_BIT(status_bits::paused);
    }
}
bool motor_t::get_paused()
{
    return CHECK_STATUS_BIT(status_bits::paused);
}