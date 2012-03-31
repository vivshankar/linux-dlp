#pragma once

#include "dr_api.h"
#include "drmgr.h"
#include "../utils/Logger.h"

class APIHijack
{
protected:

public:

  APIHijack() {

  }

  void Initialize();
  void Uninitialize();
  
public:

  static APIHijack& GetInstance() {

    static APIHijack oHijack;
    return oHijack;
  }

  static void WriteLog(void *drcontext, const char *szLog);
  static void event_exit(void);
  static bool event_filter_syscall(void *drcontext, int sysnum);
  static bool event_pre_syscall(void *drcontext, int sysnum);
  static void event_post_syscall(void *drcontext, int sysnum);
  static void event_thread_context_init(void *drcontext, bool new_depth);
  static void event_thread_context_exit(void *drcontext, bool process_exit);
};
