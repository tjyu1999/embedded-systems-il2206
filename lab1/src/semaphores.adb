-- Package: Semaphores
--
-- ==> Complete the code at the indicated places

package body Semaphores is
   protected body CountingSemaphore is
      -- To be completed
      entry Wait When Count > 0 is
         begin
         Count := Count - 1; -- decrement
      end wait;
      
      -- To be completed
      entry Signal When Count < MaxCount is
         begin
         Count := Count + 1; -- increment
      end Signal;
   end CountingSemaphore;
end Semaphores;
