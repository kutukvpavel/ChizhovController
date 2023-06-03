#include "wdt.h"

#include "../Core/Inc/iwdg.h"
#include "../Core/Inc/gpio.h"
#include "task_handles.h"

#define MY_MAX_WDG_TASKS 16u

namespace wdt
{
   task_t tasks[MY_MAX_WDG_TASKS];
   size_t registered_tasks = 0;

   task_t* register_task(uint32_t interval_ms, const char* name)
   {
      assert_param(registered_tasks < array_size(tasks));

      task_t* instance = &(tasks[registered_tasks++]);
      instance->deadline = pdMS_TO_TICKS(interval_ms);
      instance->last_time = xTaskGetTickCount();
      instance->name = name;

      DBG("Registered task WDT: %s = #%u", name, registered_tasks);
      return instance;
   }

   void process(TickType_t now)
   {
      for(size_t i = 0; i < registered_tasks; i++){
         TickType_t diff = now - tasks[ i ].last_time;
         if( diff > tasks[i].deadline )
         {
            //Let hardware WDT reset us
            if (tasks[i].name) DBG("Task %s WDT!", tasks[i].name);
            vTaskDelay(pdMS_TO_TICKS(1000));
            vTaskSuspendAll();
            taskDISABLE_INTERRUPTS();
            LL_GPIO_SetOutputPin(Onboard_LED_GPIO_Port, Onboard_LED_Pin);
            while( 1 );
         }
      }
      HAL_IWDG_Refresh(&hiwdg);
   }
} // namespace name

_BEGIN_STD_C
STATIC_TASK_BODY(MY_WDT)
{
   const uint32_t delay = 100;
   static TickType_t last_wake;

   last_wake = xTaskGetTickCount();
   for (;;)
   {
      wdt::process(last_wake);
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
   }
}
_END_STD_C