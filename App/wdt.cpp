#include "wdt.h"

#include "../Core/Inc/iwdg.h"

#define MY_MAX_WDG_TASKS 16u

namespace wdt
{
   task_t tasks[MY_MAX_WDG_TASKS];
   size_t registered_tasks = 0;

   task_t* register_task(uint32_t interval_ms)
   {
      assert_param(registered_tasks < array_size(tasks));

      task_t* instance = &(tasks[registered_tasks++]);
      instance->deadline = pdMS_TO_TICKS(interval_ms);
      return instance;
   }

   void process(TickType_t now)
   {
      for(size_t i = 0; i < registered_tasks; i++){
         TickType_t diff = now - tasks[ i ].last_time;
         if( diff > tasks[i].deadline )
         {
            //Let hardware WDT reset us
            DBG("Task %u WDT!", i);
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskSuspendAll();
            taskDISABLE_INTERRUPTS();
            while( 1 );
         }
      }
   }
} // namespace name

_BEGIN_STD_C
void vApplicationIdleHook()
{
   const uint32_t min_interval_ms = 100;
   static TickType_t last_check = configINITIAL_TICK_COUNT;

   TickType_t now = xTaskGetTickCount();
   if ((now - last_check) > pdMS_TO_TICKS(min_interval_ms))
   {
      wdt::process(now);
      last_check = now;
   }
}
_END_STD_C