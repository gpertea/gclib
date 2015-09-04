#include <iostream>
#include <list>
#include "GThreads.h"


using namespace std;
//using namespace tthread;

// Thread local storage variable (not available on Mac OSX ?!)
#ifndef GTHREADS_NO_TLS
thread_local int gLocalVar;
#endif

// Mutex + global count variable
GMutex gMutex;
GFastMutex gFastMutex;
int gCount;

// Condition variable
GConditionVar gCond;

// Thread function: Thread ID
void ThreadIDs(GThreadData& my)
{
    {
      GLockGuard<GMutex> lock(gMutex);
      
      //cout << " My thread id is " << thisThread->get_id() << "." << endl << flush;
      cout << " My thread id is " << my.thread->get_id() << "." << endl << flush;
    }
  this_thread::sleep_for(chrono::seconds(4));
}

#ifndef GTHREADS_NO_TLS
// Thread function: Thread-local storage
void ThreadTLS(void * aArg)
{
  gLocalVar = 2;
  cout << " My gLocalVar is " << gLocalVar << "." << endl;
}
#endif

// Thread function: Mutex locking
void ThreadLock(void * aArg)
{
  for(int i = 0; i < 10000; ++ i)
  {
    GLockGuard<GMutex> lock(gMutex);
    ++ gCount;
  }
}

// Thread function: Mutex locking
void ThreadLock2(void * aArg)
{
  for(int i = 0; i < 10000; ++ i)
  {
    GLockGuard<GFastMutex> lock(gFastMutex);
    ++ gCount;
  }
}

// Thread function: Condition notifier
void ThreadCondition1(void * aArg)
{
  GLockGuard<GMutex> lock(gMutex);
  -- gCount;
  gCond.notify_all();
}

// Thread function: Condition waiter
void ThreadCondition2(void * aArg)
{
  cout << " Wating:" << flush;
  GLockGuard<GMutex> lock(gMutex);
  while(gCount > 0)
  {
    cout << "*" << gCount << flush;
    gCond.wait(gMutex);
    cout << "received notification at " << gCount << endl << flush;
  }
  cout << "-reached 0!" << endl;
}

// Thread function: Yield
void ThreadYield(void * aArg)
{
  // Yield...
  this_thread::yield();
}


// This is the main program (i.e. the main thread)
int main()
{
  // Test 1: Show number of CPU cores in the system
  cout << "PART I: Info" << endl;
  cout << " Number of processor cores: " << GThread::hardware_concurrency() << endl;

  // Test 2: thread IDs
  /*
  cout << endl << "PART II: Thread IDs" << endl;
  {
    // Show the main thread ID
    //cout << " Main thread id is " << this_thread::get_id() << "." << endl;

    // Start a bunch of child threads - only run a single thread at a time
    GThread t1(ThreadIDs, 0);
    this_thread::sleep_for(chrono::milliseconds(400));
    GThread t2(ThreadIDs, 0);
    this_thread::sleep_for(chrono::milliseconds(400));
    GThread t3(ThreadIDs, 0);
    this_thread::sleep_for(chrono::milliseconds(400));
    GThread t4(ThreadIDs, 0);
    //t1.join();
    //t2.join();
    //t3.join();
    //t4.join();
    this_thread::sleep_for(chrono::milliseconds(200));
	cout << "Waiting for all threads to finish.." << flush;
    int num_threads;
    while ((num_threads=GThread::num_running())>0) {
      cout << "." << num_threads << "." << flush;
      this_thread::sleep_for(chrono::milliseconds(300));
      }
    cout << ".. Done. " << endl;
  }

  // Test 3: thread local storage
  cout << endl << "PART III: Thread local storage" << endl;
#ifndef GTHREADS_NO_TLS
  {
    // Clear the TLS variable (it should keep this value after all threads are
    // finished).
    gLocalVar = 1;
    cout << " Main gLocalVar is " << gLocalVar << "." << endl;

    // Start a child thread that modifies gLocalVar
    GThread t1(ThreadTLS, 0);
    t1.join();

    // Check if the TLS variable has changed
    if(gLocalVar == 1)
      cout << " Main gLocalVar was not changed by the child thread - OK!" << endl;
    else
      cout << " Main gLocalVar was changed by the child thread - FAIL!" << endl;
  }
#else
  cout << " TLS is not supported on this platform..." << endl;
#endif

  // Test 4: mutex locking
  cout << endl << "PART IV: Mutex locking (100 threads x 10000 iterations)" << endl;
  {
    // Clear the global counter.
    gCount = 0;

    // Start a bunch of child threads
    list<GThread *> threadList;
    for(int i = 0; i < 100; ++ i)
      threadList.push_back(new GThread(ThreadLock, 0));

    // Wait for the threads to finish
    list<GThread *>::iterator it;
    for(it = threadList.begin(); it != threadList.end(); ++ it)
    {
      GThread * t = *it;
      t->join();
      delete t;
    }

    // Check the global count
    cout << " gCount = " << gCount << endl;
  }

  // Test 5: fast_mutex locking
  cout << endl << "PART V: Fast mutex locking (100 threads x 10000 iterations)" << endl;
  {
    // Clear the global counter.
    gCount = 0;

    // Start a bunch of child threads
    list<GThread *> threadList;
    for(int i = 0; i < 100; ++ i)
      threadList.push_back(new GThread(ThreadLock2, 0));

    // Wait for the threads to finish
    list<GThread *>::iterator it;
    for(it = threadList.begin(); it != threadList.end(); ++ it)
    {
      GThread * t = *it;
      t->join();
      delete t;
    }

    // Check the global count
    cout << " gCount = " << gCount << endl;
  }
*/
  // Test 6: condition variable
  cout << endl << "PART VI: Condition variable (40 + 1 threads)" << endl;
  {
    // Set the global counter to the number of threads to run.
    {
     GLockGuard<GMutex> lock(gMutex);
     gCount = 6;
    }

    // Start the waiting thread (it will wait for gCount to reach zero).
    GThread t1(ThreadCondition2, 0);
    
    this_thread::sleep_for(chrono::milliseconds(400));
      {
       
       for (int c=5;c>=0;c--) {
           {
           GLockGuard<GMutex> lock(gMutex);
           --gCount;
           gCond.notify_one();
           }
           cout << "Notify_" << c << "|" << flush;
           //this_thread::sleep_for(chrono::milliseconds(200));
           cout << "Notify_sent" << c << "|" << flush;
           }
      }

    // Start a bunch of child threads (these will decrease gCount by 1 when they
    // finish)
    /*
    list<GThread *> threadList;
    for(int i = 0; i < 40; ++ i)
      threadList.push_back(new GThread(ThreadCondition1, 0));
    */
    // Wait for the waiting thread to finish
    t1.join();

    // Wait for the other threads to finish
    /*
    list<GThread *>::iterator it;
    for(it = threadList.begin(); it != threadList.end(); ++ it)
    {
      GThread * t = *it;
      t->join();
      delete t;
    }
    */
  }
/*
  // Test 7: yield
  cout << endl << "PART VII: Yield (40 + 1 threads)" << endl;
  {
    // Start a bunch of child threads
    list<GThread *> threadList;
    for(int i = 0; i < 40; ++ i)
      threadList.push_back(new GThread(ThreadYield, 0));

    // Yield...
    this_thread::yield();

    // Wait for the threads to finish
    list<GThread *>::iterator it;
    for(it = threadList.begin(); it != threadList.end(); ++ it)
    {
      GThread * t = *it;
      t->join();
      delete t;
    }
  }

  // Test 8: sleep
  cout << endl << "PART VIII: Sleep (10 x 100 ms)" << endl;
  {
    // Sleep...
    cout << " Sleeping" << flush;
    for(int i = 0; i < 10; ++ i)
    {
      this_thread::sleep_for(chrono::milliseconds(100));
      cout << "." << flush;
    }
    cout << endl;
  }
  */
}
