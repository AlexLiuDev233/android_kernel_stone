/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SCHED_H
#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

/* common forward declarations used by several hooks */
struct task_struct;
struct rq;
struct cpumask;
struct sched_group;
struct cfs_rq;
struct sched_entity;
struct rq_flags;
struct em_perf_domain;

DECLARE_RESTRICTED_HOOK(android_rvh_select_task_rq_fair,
	TP_PROTO(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags, int *new_cpu),
	TP_ARGS(p, prev_cpu, sd_flag, wake_flags, new_cpu), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_select_task_rq_rt,
	TP_PROTO(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags, int *new_cpu),
	TP_ARGS(p, prev_cpu, sd_flag, wake_flags, new_cpu), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_select_fallback_rq,
	TP_PROTO(int cpu, struct task_struct *p, int *new_cpu),
	TP_ARGS(cpu, p, new_cpu), 1);

DECLARE_HOOK(android_vh_scheduler_tick,
	TP_PROTO(struct rq *rq),
	TP_ARGS(rq));

DECLARE_RESTRICTED_HOOK(android_rvh_enqueue_task,
	TP_PROTO(struct rq *rq, struct task_struct *p),
	TP_ARGS(rq, p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_dequeue_task,
	TP_PROTO(struct rq *rq, struct task_struct *p),
	TP_ARGS(rq, p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_can_migrate_task,
	TP_PROTO(struct task_struct *p, int dst_cpu, int *can_migrate),
	TP_ARGS(p, dst_cpu, can_migrate), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_find_lowest_rq,
	TP_PROTO(struct task_struct *p, struct cpumask *local_cpu_mask,
			int *lowest_cpu),
	TP_ARGS(p, local_cpu_mask, lowest_cpu), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_prepare_prio_fork,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_finish_prio_fork,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_rtmutex_prepare_setprio,
	TP_PROTO(struct task_struct *p, struct task_struct *pi_task),
	TP_ARGS(p, pi_task), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_set_user_nice,
	TP_PROTO(struct task_struct *p, long *nice, bool *allowed),
	TP_ARGS(p, nice, allowed), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_setscheduler,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_find_busiest_group,
	TP_PROTO(struct sched_group *busiest, struct rq *dst_rq, int *out_balance),
		TP_ARGS(busiest, dst_rq, out_balance), 1);

DECLARE_HOOK(android_vh_map_util_freq,
	TP_PROTO(unsigned long util, unsigned long freq,
		unsigned long cap, unsigned long *next_freq),
	TP_ARGS(util, freq, cap, next_freq));

DECLARE_HOOK(android_vh_em_pd_energy,
	TP_PROTO(struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util,
		unsigned long *energy),
	TP_ARGS(pd, max_util, sum_util, energy));

DECLARE_HOOK(android_vh_dup_task_struct,
	TP_PROTO(struct task_struct *tsk, struct task_struct *orig),
	TP_ARGS(tsk, orig));

/* macro versions of hooks are no longer required */
DECLARE_RESTRICTED_HOOK(android_rvh_check_preempt_tick,
	TP_PROTO(struct task_struct *p, unsigned long *ideal_runtime, bool *skip_preempt,
			unsigned long delta_exec, struct cfs_rq *cfs_rq, struct sched_entity *curr,
			unsigned int granularity),
	TP_ARGS(p, ideal_runtime, skip_preempt, delta_exec, cfs_rq, curr, granularity), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_replace_next_task_fair,
	TP_PROTO(struct rq *rq, struct task_struct **p, struct sched_entity **se, bool *repick,
			bool simple, struct task_struct *prev),
	TP_ARGS(rq, p, se, repick, simple, prev), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_check_preempt_wakeup,
	TP_PROTO(struct rq *rq, struct task_struct *p, bool *preempt, bool *nopreempt,
			int wake_flags, struct sched_entity *se, struct sched_entity *pse,
			int next_buddy_marked, unsigned int granularity),
	TP_ARGS(rq, p, preempt, nopreempt, wake_flags, se, pse, next_buddy_marked,
			granularity), 1);

DECLARE_HOOK(android_vh_free_task,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

DECLARE_RESTRICTED_HOOK(android_rvh_after_enqueue_task,
	TP_PROTO(struct rq *rq, struct task_struct *p),
	TP_ARGS(rq, p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_after_dequeue_task,
	TP_PROTO(struct rq *rq, struct task_struct *p),
	TP_ARGS(rq, p), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_enqueue_entity,
	TP_PROTO(struct cfs_rq *cfs, struct sched_entity *se),
	TP_ARGS(cfs, se), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_dequeue_entity,
	TP_PROTO(struct cfs_rq *cfs, struct sched_entity *se),
	TP_ARGS(cfs, se), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_entity_tick,
	TP_PROTO(struct cfs_rq *cfs_rq, struct sched_entity *se),
	TP_ARGS(cfs_rq, se), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_enqueue_task_fair,
	TP_PROTO(struct rq *rq, struct task_struct *p, int flags),
	TP_ARGS(rq, p, flags), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_dequeue_task_fair,
	TP_PROTO(struct rq *rq, struct task_struct *p, int flags),
	TP_ARGS(rq, p, flags), 1);
#endif /* _TRACE_HOOK_SCHED_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
