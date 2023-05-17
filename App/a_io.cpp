#include "a_io.h"

#include "average/average.h"

#define PACKED_FOR_DMA __aligned(sizeof(uint32_t))
#define CHANNEL_REPETITION 4
#define MAP(i) { &buffer[i].ch_8, &buffer[i].ch_9, &buffer[i].vbat, &buffer[i].vref }

namespace a_io
{
    struct PACKED_FOR_DMA a_buffer_t
    {
        uint16_t ch_8;
        uint16_t ch_9;
        uint16_t vref;
        uint16_t vbat;
    };
    PACKED_FOR_DMA a_buffer_t buffer[CHANNEL_REPETITION] = { };
    uint16_t* mappings[in::LEN][CHANNEL_REPETITION] = { MAP(0), MAP(1), MAP(2), MAP(3) };
    struct channel
    {
        average* a;
        float v;
    };
    channel ch[in::LEN];

    void init()
    {
        for (size_t i = 0; i < in::LEN; i++)
        {
            ch[i].a = new average(10);
        }
    }

    void process_data()
    {
        for (size_t j = 0; j < CHANNEL_REPETITION; j++)
        {
            for (size_t i = 0; i < in::LEN; i++)
            {
                auto& c = ch[i];
                c.a->enqueue(*(mappings[j][i]) ); //TODO
            }
        }
    }

    float get_input(in i)
    {
        assert_param(i < in::LEN);

        return ch[i].v;
    }
} // namespace a_io

_BEGIN_STD_C

void StartAdcTask(void *argument)
{
    a_io::init();

    for (;;)
    {
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20))) continue;
        a_io::process_data();
    }
}

_END_STD_C