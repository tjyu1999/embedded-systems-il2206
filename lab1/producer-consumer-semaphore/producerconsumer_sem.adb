with Ada.Text_IO;
use Ada.Text_IO;

with Ada.Real_Time;
use Ada.Real_Time;

with Ada.Numerics.Discrete_Random;

with Semaphores;
use Semaphores;

procedure ProducerConsumer_Sem is
	
   N : constant Integer := 10; -- Number of produced and consumed tokens per task
   X : constant Integer := 3; -- Number of producers and consumer
   	
   -- Buffer Definition
   Size: constant Integer := 4;
   type Index is mod Size;
   type Item_Array is array(Index) of Integer;
   B : Item_Array;
   In_Ptr, Out_Ptr, Count : Index := 0;

   -- Random Delays
   subtype Delay_Interval is Integer range 50..250;
   package Random_Delay is new Ada.Numerics.Discrete_Random (Delay_Interval);
   use Random_Delay;
   G : Generator;

   -- => Complete code: Declation of Semaphores
	--    1. Semaphore 'NotFull' to indicate that buffer is not full
	--    2. Semaphore 'NotEmpty' to indicate that buffer is not empty
	--    3. Semaphore 'AtomicAccess' to ensure an atomic access to the buffer
   
   Not_Full : Semaphores.CountingSemaphore(Size, Size);
   Not_Empty : Semaphores.CountingSemaphore(Size, 0);
   Atomic_Access : Semaphores.CountingSemaphore(1, 1);
   	
   task type Producer;

   task type Consumer;

   task body Producer is
      Next : Time;
      Val : Integer;
   begin
      Next := Clock;      
      for I in 1..N loop
         Not_Full.Wait;
         Atomic_Access.Wait;
         
         -- => Complete Code: Write to Buffer
         B(In_Ptr) := I;
         Val := I;
         In_Ptr := In_Ptr + 1;
         
         Atomic_Access.Signal;
         Not_Empty.Signal;
         Put("Produced: ");
         Put_Line(Integer'Image(Val));

         -- Next 'Release' in 50..250ms
         Next := Next + Milliseconds(Random(G));
         delay until Next;
      end loop;
   end;

   task body Consumer is
      Next : Time;
      Val : Integer;
   begin
      Next := Clock;
      for I in 1..N loop
         Not_Empty.Wait;
         Atomic_Access.Wait;
         
         -- => Complete Code: Read from Buffer
         Val := B(Out_Ptr);
         Out_Ptr := Out_Ptr + 1;

         Atomic_Access.Signal;
         Not_Full.Signal;
         Put("Consumed: ");
         Put_Line(Integer'Image(Val));
         
         -- Next 'Release' in 50..250ms
         Next := Next + Milliseconds(Random(G));
         delay until Next;
      end loop;
   end;
	
   P: array (Integer range 1..X) of Producer;
   C: array (Integer range 1..X) of Consumer;
	
begin -- main task
   null;
end ProducerConsumer_Sem;
