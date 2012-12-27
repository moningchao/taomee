/*
 * =====================================================================================
 *
 *       Filename:  bench_api.cpp
 *
 *    Description:  bench_api.cpp
 *
 *        Version:  1.0
 *        Created:  2011年08月10日 15时14分16秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  henry (韩林), henry@taomee.com
 *        Company:  TaoMee.Inc, ShangHai
 *
 * =====================================================================================
 */
#include <string.h>

#include "../../common/constant.h"
#include "../../common/pack/c_pack.h"
#include "i_work_proc.h"
#include "bench_api.h"

i_work_proc *g_p_work_proc;
c_pack g_pack;

int handle_init(int argc, char ** argv, int pid_type)
{
    switch(pid_type)
    {
        case PROC_MAIN:
            DEBUG_LOG("main proc inited.");
            break;
        case PROC_WORK:
            if(create_work_proc_instance(&g_p_work_proc) != 0)
            {
                ERROR_LOG("create work_proc instance failed.");
                return -1;
            }

            if(g_p_work_proc->init(argc, argv) != 0)
            {
                ERROR_LOG("work_proc init failed.");
                return -1;
            }

            DEBUG_LOG("work proc inited.");
            break;
        case PROC_CONN:
            DEBUG_LOG("net proc inited.");
            break;
	    case PROC_TIME:
	        break;
        default:
            ERROR_LOG("unknown pid_type:%d.", pid_type);
            return -1;
    }
    return 0;
}

int handle_input(const char *buffer, int length, const skinfo_t *sk)
{
    int real_len = 0;
    if(length >= 4)
    {
        real_len = *((uint32_t*)buffer);
        if(real_len <= MAX_RECV_PKG_SIZE)
        {
            return real_len;
        }
        else
        {
            ERROR_LOG("pkg len(%u) > max_len(%d).", real_len, MAX_RECV_PKG_SIZE);
            return -1;
        }
    }
    return 0;
}

int handle_process(char *recv_buf, int recv_len, char **send_buf, int *sendlen, const skinfo_t *sk)
{
    return g_p_work_proc->process(recv_buf, recv_len, send_buf, sendlen);
}

int handle_open(char **sendptr, int *sendlen, const skinfo_t *sk)
{
    return 0;
}

int handle_close(const skinfo_t *sk)
{
    return 0;
}

int handle_timer(int *intvl)
{
    return 0;
}

void handle_fini(int pid_type)
{
    if(pid_type == PROC_MAIN)
    {
        DEBUG_LOG("main proc finished.");
    }
    else if(pid_type == PROC_CONN)
    {
       DEBUG_LOG("net proc finished.");
    }
    else if(pid_type == PROC_WORK)
    {
        g_p_work_proc->uninit();
        g_p_work_proc->release();
        g_p_work_proc = NULL;
        DEBUG_LOG("work proc finished.");
    }
}

int handle_filter_key (const char*, int, uint32_t*)
{
	return 0;
}
