/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


/*
 *  NetworkEditor.cc: The network editor...
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Dataflow/Network/Scheduler.h>
#include <Dataflow/Network/Connection.h>
#include <Dataflow/Network/Network.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/Port.h>
#include <Core/Malloc/Allocator.h>
#include <iostream>
#include <queue>

using namespace SCIRun;
using namespace std;


Scheduler::Scheduler(Network* net)
  : net(net),
    first_schedule(true),
    schedule(true),
    serial_id(1),
    mailbox("NetworkEditor request FIFO", 100)
{
  net->attach(this);
}


Scheduler::~Scheduler()
{
}


bool
Scheduler::toggleOnOffScheduling()
{
  schedule = !schedule;
  return schedule;  
}


void
Scheduler::run()
{
  // Go into Main loop.
  do_scheduling_real(0);
  main_loop();
}


void
Scheduler::main_loop()
{
  // Dispatch events.
  int done=0;
  while(!done){
    MessageBase* msg=mailbox.receive();
    // Dispatch message..
    switch (msg->type) {
    case MessageTypes::MultiSend:
      {
	//cerr << "Got multisend\n";
	Module_Scheduler_Message* mmsg=(Module_Scheduler_Message*)msg;
	multisend_real(mmsg->p1);
	// Do not re-execute sender

	// do_scheduling on the module instance bound to
	// the output port p1 (the first arg in Multisend() call)
	do_scheduling_real(mmsg->p1->get_module()); 
      }
      break;

    case MessageTypes::ReSchedule:
      do_scheduling_real(0);
      break;

    case MessageTypes::SchedulerInternalExecuteDone:
      {
        Module_Scheduler_Message *ms_msg = (Module_Scheduler_Message *)msg;
        report_execution_finished_real(ms_msg->serial);
      }
      break;

    default:
      cerr << "Unknown message type: " << msg->type << std::endl;
      break;
    };

    delete msg;
  };
}


void
Scheduler::request_multisend(OPort* p1)
{
  mailbox.send(new Module_Scheduler_Message(p1));
}


void
Scheduler::multisend_real(OPort* oport)
{
  int nc=oport->nconnections();
  for (int c=0;c<nc;c++)
  {
    Connection* conn=oport->connection(c);
    IPort* iport=conn->iport;
    Module* m=iport->get_module();
    if (!m->need_execute)
    {
      m->need_execute = true;
    }
  }
}


void
Scheduler::do_scheduling()
{
  mailbox.send(new Module_Scheduler_Message());
}


void
Scheduler::do_scheduling_real(Module* exclude)
{
  if (!schedule)
    return;

  int nmodules=net->nmodules();
  queue<Module *> needexecute;		

  // build queue of module ptrs to execute
  int i;			    
  for(i=0;i<nmodules;i++)
  {
    Module* module=net->module(i);
    if (module->need_execute)
    {
      needexecute.push(module);
    }
  }
  if (needexecute.empty())
  {
    return;
  }

  // For all of the modules that need executing, execute the
  // downstream modules and arrange for the data to be sent to them
  // mm - this doesn't really execute them. It just adds modules to
  // the queue of those to execute based on dataflow dependencies.

  vector<Connection*> to_trigger;
  while (!needexecute.empty())
  {
    Module* module = needexecute.front();
    needexecute.pop();
    // Add oports
    int no=module->numOPorts();
    int i;
    for (i=0;i<no;i++)
    {
      OPort* oport=module->getOPort(i);
      int nc=oport->nconnections();
      for (int c=0;c<nc;c++)
      {
	Connection* conn=oport->connection(c);
	IPort* iport=conn->iport;
	Module* m=iport->get_module();
	if (m != exclude && !m->need_execute)
        {
	  m->need_execute = true;
	  needexecute.push(m);
	}
      }
    }

    // Now, look upstream.
    int ni=module->numIPorts();
    for (i=0;i<ni;i++)
    {
      IPort* iport=module->getIPort(i);
      if (iport->nconnections())
      {
	Connection* conn=iport->connection(0);
	OPort* oport=conn->oport;
	Module* m=oport->get_module();
	if (!m->need_execute)
        {
	  if (m != exclude)
          {
	    if (module->sched_class != Module::ViewerSpecial)
            {
	      // If this oport already has the data, add it
	      // to the to_trigger list.
	      if (oport->have_data())
              {
		to_trigger.push_back(conn);
	      }
              else
              {
		m->need_execute = true;
		needexecute.push(m);
	      }
	    }
	  }
	}
      }
    }
  }

  // Trigger the ports in the trigger list.
  for (i=0; i<(int)(to_trigger.size()); i++)
  {
    Connection* conn=to_trigger[i];
    OPort* oport=conn->oport;
    Module* module=oport->get_module();

    // Only tricker the non-executing modules.
    if (!module->need_execute)
    {
      module->mailbox.send(scinew Scheduler_Module_Message(conn));
    }
  }

  // Create our SerialSet so that we can track when execution is finished.
  unsigned int serial_base = 0;
  if (nmodules)
  {
    if (serial_id > 0x0FFFFFFF) serial_id = 1;
    serial_base = serial_id;
    serial_id += nmodules;
    serial_set.push_back(SerialSet(serial_base, nmodules));
  }

  // Execute all the modules.
  for(i=0;i<nmodules;i++)
  {
    Module* module=net->module(i);
    if (module->need_execute)
    {
      module->mailbox.send(scinew Scheduler_Module_Message(serial_base + i));
      module->need_execute = false;
    }
    else
    {
      // Already done, report it.
      report_execution_finished_real(serial_base + i);
    }
  }
}


void
Scheduler::report_execution_finished(const MessageBase *msg)
{
  ASSERT(msg->type == MessageTypes::ExecuteModule);
  Scheduler_Module_Message *sm_msg = (Scheduler_Module_Message *)msg;
  mailbox.send(scinew Module_Scheduler_Message(sm_msg->serial));
}


void
Scheduler::report_execution_finished_real(unsigned int serial)
{
  list<SerialSet>::iterator itr = serial_set.begin();
  while (itr != serial_set.end())
  {
    if (serial >= itr->base && serial < itr->base + itr->size)
    {
      itr->callback_count++;
      if (itr->callback_count == itr->size)
      {
        //cout << "EXECUTION DONE\n";
        serial_set.erase(itr);
        break;
      }
    }
    ++itr;
  }
}


Scheduler_Module_Message::Scheduler_Module_Message(unsigned int s)
  : MessageBase(MessageTypes::ExecuteModule), conn(NULL), serial(s)
{
}

Scheduler_Module_Message::Scheduler_Module_Message(Connection* conn)
  : MessageBase(MessageTypes::TriggerPort), conn(conn), serial(0)
{
}

Scheduler_Module_Message::~Scheduler_Module_Message()
{
}

Module_Scheduler_Message::Module_Scheduler_Message()
: MessageBase(MessageTypes::ReSchedule)
{
}

Module_Scheduler_Message::Module_Scheduler_Message(OPort* p1)
: MessageBase(MessageTypes::MultiSend), p1(p1)
{
}

Module_Scheduler_Message::Module_Scheduler_Message(unsigned int s)
  : MessageBase(MessageTypes::SchedulerInternalExecuteDone), serial(s)
{
}

Module_Scheduler_Message::~Module_Scheduler_Message()
{
}

