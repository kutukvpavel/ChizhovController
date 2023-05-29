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

   void process()
   {
      TickType_t now = xTaskGetTickCount();
      size_t i;
      for(i = 0; i < registered_tasks; ++i ){
         TickType_t diff = now - tasks[ i ].last_time;
         if(  diff > tasks[ i ].deadline ) break;
      }
      if( i == registered_tasks ) HAL_IWDG_Refresh(&hiwdg);
      else 
      {
         vTaskSuspendAll();
         taskDISABLE_INTERRUPTS();
         while( 1 );
      }
   }
} // namespace name

_BEGIN_STD_C
void vApplicationIdleHook()
{
   wdt::process();
}
_END_STD_C