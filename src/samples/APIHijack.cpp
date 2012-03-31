#include "APIHijack.h"
#include <syscall.h>
#include <string.h>
#include <stdlib.h>

#define AUDIT_FILE "/home/vivek/projects/linux-dlp/src/samples/audit.log"

using namespace std;

static int tlsIdx;

DR_EXPORT void dr_init(client_id_t id)
{
  APIHijack& oHijack = APIHijack::GetInstance();
  oHijack.Initialize();
}

void APIHijack::Initialize() 
{
  drmgr_init();
  dr_register_filter_syscall_event(APIHijack::event_filter_syscall);
  drmgr_register_pre_syscall_event(APIHijack::event_pre_syscall);
  dr_register_post_syscall_event(APIHijack::event_post_syscall);
  dr_register_exit_event(APIHijack::event_exit);

  tlsIdx = drmgr_register_cls_field(APIHijack::event_thread_context_init,
				    APIHijack::event_thread_context_exit);
  //DR_ASSERT(m_tlsIdx != -1);
}

void APIHijack::Uninitialize() 
{
  
}

void APIHijack::event_thread_context_init(void *drcontext, bool new_depth)
{
  if (new_depth) {
    Logger* logger = (Logger*) dr_thread_alloc(drcontext, sizeof(Logger));
    logger->Initialize(AUDIT_FILE);
    drmgr_set_cls_field(drcontext, tlsIdx, logger);
  }
}

void APIHijack::event_thread_context_exit(void *drcontext, bool process_exit)
{
  if (process_exit) {
    Logger *logger = (Logger *) drmgr_get_cls_field(drcontext, tlsIdx);
    dr_thread_free(drcontext, logger, sizeof(Logger));
  }
}

void APIHijack::event_exit(void) 
{
  drmgr_unregister_cls_field(APIHijack::event_thread_context_init,
  			     APIHijack::event_thread_context_exit,
  			     tlsIdx);
  drmgr_exit();
}

bool APIHijack::event_filter_syscall(void *drcontext, int sysnum)
{
  return (sysnum == SYS_open);
}

bool APIHijack::event_pre_syscall(void *drcontext, int sysnum)
{
  if (sysnum == SYS_open) {

    reg_t p1 = dr_syscall_get_param(drcontext, 0);
    char *filename = (char*) p1;
    if (strlen(filename) == 0)
      return true;

    char buff[200];
    sprintf(buff, "In event pre_syscall; filename = %s\n", filename);
    WriteLog(drcontext, buff);
  }

  return true;
}

void APIHijack::event_post_syscall(void *drcontext, int sysnum)
{
  return;
}

void APIHijack::WriteLog(void *drcontext, const char *szLog) {

  Logger *logger = (Logger *) drmgr_get_cls_field(drcontext, tlsIdx);
  if (logger == NULL)
    return;
  
  logger->WriteLog(szLog);
}
