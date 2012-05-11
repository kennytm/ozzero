/*
 *  Authors:
 *    Kostja Popow (popow@ps.uni-sb.de)
 *    Michael Mehl (mehl@dfki.de)
 *
 *  Contributors:
 *    Christian Schulte <schulte@ps.uni-sb.de>
 *
 *  Copyright:
 *    Kostja Popow, 1997-1999
 *    Michael Mehl, 1997-1999
 *    Christian Schulte, 1997-1999
 *
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 *
 *  This file is part of Mozart, an implementation
 *  of Oz 3:
 *     http://www.mozart-oz.org
 *
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

#ifndef __AMH
#define __AMH

#include <stdint.h>

typedef enum
{
    TimerInterrupt = 1 << 1, // reflects the arrival of an alarm from OS;
    IOReady	 = 1 << 2, // IO handler has signaled IO ready
    UserAlarm	 = 1 << 3, // some Oz delays are to be processed;
    StartGC	 = 1 << 4, // need a GC
    TasksReady     = 1 << 5, //
    SigPending	 = 1 << 6  //
} StatusBit;

class Board;
class Thread;
typedef int Bool;
typedef uint32_t TaggedRef;

class AM
{
    Board* _currentBoard;
    Board* _rootBoard;
    Thread* _currentThread;
    Bool _currentBoardIsRoot;
    TaggedRef debugPort;
    Bool debugMode;
    Bool propLocation;
    int statusReg;
    // There are other members after this, but we don't need to care anymore.

public:
    int isSetSFlag(StatusBit flag)
    {
	//printf("statusReg = %#x\n", statusReg);
	return statusReg & flag;
    }
};

extern AM am;

#endif

