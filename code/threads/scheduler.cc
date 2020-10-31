// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Compare function
//----------------------------------------------------------------------
//本來就在code裡面的比較函式
int PriorityCompare(Thread *a, Thread *b) {
    if(a->getPriority() == b->getPriority())
        return 0;
int res =a->getPriority() > b->getPriority() ? 1 : -1;
	cout<<a->getName()<< " "<< a->getPriority()<<" ";
	cout<<b->getName()<<" "<<b->getPriority()<<" \n";	
	
    return res;
}

//使用與Priority相同的架構去改寫的比較函式 只是換成比較誰的burst time比較小
int JobTimeCompare(Thread *a,Thread *b){
	if(a->getBurstTime() == b->getBurstTime())
		return 0;
	return a->getBurstTime() > b->getBurstTime()?1:-1;
}
//這邊回傳1的原因是要去分析/lib/list.cc中的 Insert function
//可以看到當compare()固定回傳1的話thread會在readylist尾端添加,與FCFS的效果一樣
//也就是執行this->last->next = element   this->last = element
int FCFSCompare(Thread *a, Thread *b){
	return 1;
}
//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{
	Scheduler(RR);
}

Scheduler::Scheduler(SchedulerType type)
{
	schedulerType = type;
	switch(schedulerType) {
    	case RR:
        	readyList = new List<Thread *>;
       	 	cout<<"Now using RR\n";
		break;
    	case SJF:
		//因為要傳我們的比較函數進去,所以參照Priority使用排序過的List作為我們的readylist
		readyList = new SortedList<Thread *>(JobTimeCompare);
		cout<<"Now using SJF\n";
			
        	break;
    	case Priority:
		//本來就有的code
		readyList = new SortedList<Thread *>(PriorityCompare);
        	cout<<"Now using PRIO\n";

		break;
		
    	case FIFO:
		//因為要傳我們的比較函數進去,所以參照Priority使用排序過的List作為我們的readylist
		readyList = new SortedList<Thread *>(FCFSCompare);
        	cout<<"Now using FIFO\n";
		break;
	case SRTF:
		//因為SRTF是可以搶佔的,所以不用傳比較函數進去 使用預設的list即可
		readyList = new List<Thread *>;
		cout<<"Now using SRTF\n";
		break;
   	}
	toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    //這邊我們假設當thread進入到ready的時候代表他剛執行完run
    //因為我們用lastTime紀錄上一次被設成run的時間,所以在這邊我們要加的時間
    //就便成是目前kernel的時間 減去 lastTime
    kernel->currentThread->realBurstTime += kernel->stats->totalTicks- kernel->currentThread->lastTime;

    //紀錄目前kernel的時間 為了等等計算等待時間做使用
   kernel->currentThread->lastTime = kernel->stats->totalTicks;
    thread->setStatus(READY);
    
    readyList->Append(thread);
   
    Print();

	cout<<endl;
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
	//readyList 空的表示已經沒有thread要執行 所以可以計算平均等待時間並輸出
	float res = 0.0;
	for(int i =0 ;i < waitTimeArr.size(); i++)
	{
		cout<< waitTimeArr[i]<<" ";
		res+=waitTimeArr[i];
	}
	cout<<endl;
	//waitTimeArr.size()-1 是因為要扣掉main函式 單純只計算使用者的程式
	cout<<"Average Waiting Time : "<< res/(waitTimeArr.size()-1)<<endl;
	return NULL;
    } else {
    	return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
 
//	cout << "Current Thread" <<oldThread->getName() << "    Next Thread"<<nextThread->getName()<<endl;
   
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    //這邊我們假設當thread進入到run的時候代表他剛才在ready
    //因為我們用lastTime紀錄上一次被設成ready的時間,所以在這邊我們要加的時間
    //就便成是目前kernel的時間 減去 lastTime
   kernel->currentThread->waitTime += kernel->stats->totalTicks- kernel->currentThread->lastTime;
    //紀錄目前kernel的時間 為了等等計算Burst time 做使用
   kernel->currentThread->lastTime = kernel->stats->totalTicks;  
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
#ifdef USER_PROGRAM
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

//---------------------------------------------------------
//模擬FindNextToRun所做的函式,特別為了SRTF
//因為我們想要進入到readyList->iter裡面作排程 而不是只取list中最前面的那個
Thread *
Scheduler::FindSRTFNextToRun (int remainTime)
{

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
	//此段與FindNextToRun相同 請參照
	float res = 0.0;
	for(int i =0 ;i < waitTimeArr.size(); i++)
	{
		cout<< waitTimeArr[i]<<" ";
		res+=waitTimeArr[i];
	}
	cout<<endl;
	cout<<"Average Waiting Time : "<< res/(waitTimeArr.size()-1)<<endl;
	return NULL;
    } else {
	//回傳適當的thread
	return	readyList->iter(remainTime);
    }
}

