/*
 * timer.c - Linux basic library function support for ktap
 *
 * Copyright 2013 The ktap Project Developers.
 * See the COPYRIGHT file at the top-level directory of this distribution.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "../../include/ktap.h"

struct hrtimer_ktap {
	struct hrtimer timer;
	ktap_state *ks;
	ktap_closure *cl;
	u64 ns;
	struct list_head list;
};

/*
 * Currently ktap disallow tracing event in timer callback closure,
 * that will corrupt ktap_state and ktap stack, because timer closure
 * and event closure use same irq percpu ktap_state and stack.
 * We can use a different percpu ktap_state and stack for timer purpuse,
 * but that's don't bring any big value with cost on memory consuming.
 *
 * So just simply disable tracing in timer closure
 *
 * todo: export perf_swevent_put_recursion_context to slove this issue.
 */
DEFINE_PER_CPU(bool, kp_in_timer_closure);

static enum hrtimer_restart hrtimer_ktap_fn(struct hrtimer *timer)
{
	ktap_state *ks;
	struct hrtimer_ktap *t;

	rcu_read_lock_sched_notrace();

	__this_cpu_write(kp_in_timer_closure, true);

	t = container_of(timer, struct hrtimer_ktap, timer);

	ks = kp_newthread(t->ks);
	setcllvalue(ks->top, t->cl);
	incr_top(ks);
	kp_call(ks, ks->top - 1, 0);
	kp_exitthread(ks);

	hrtimer_add_expires_ns(timer, t->ns);

	__this_cpu_write(kp_in_timer_closure, false);

	rcu_read_unlock_sched_notrace();

	return HRTIMER_RESTART;
}

static void set_timer(ktap_state *ks, int factor, int n, ktap_closure *cl)
{
	struct hrtimer_ktap *t;
	u64 period = (u64)factor * n;

	t = kp_malloc(ks, sizeof(*t));
	t->ks = ks;
	t->cl = cl;
	t->ns = period;

	INIT_LIST_HEAD(&t->list);
	list_add(&t->list, &(G(ks)->timers));

	hrtimer_init(&t->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	t->timer.function = hrtimer_ktap_fn;
	hrtimer_start(&t->timer, ns_to_ktime(period), HRTIMER_MODE_REL);
}

static int do_tick_profile(ktap_state *ks, int tick)
{
	const char *str, *tmp;
	char interval_str[32] = {0};
	char suffix[10] = {0};
	int n, i = 0;
	int factor;

	kp_arg_check(ks, 1, KTAP_TSTRING);
	kp_arg_check(ks, 2, KTAP_TFUNCTION);

	str = svalue(kp_arg(ks, 1));
	tmp = str;
	while (isdigit(*tmp))
		tmp++;

	strncpy(interval_str, str, tmp - str);
	if (kstrtoint(interval_str, 10, &n))
		goto error;

	strncpy(suffix, tmp, 9);
	while (suffix[i] != ' ' && suffix[i] != '\0')
		i++;

	suffix[i] = '\0';

	if (!strcmp(suffix, "s") || !strcmp(suffix, "sec"))
		factor = NSEC_PER_SEC;
	else if (!strcmp(suffix, "ms") || !strcmp(suffix, "msec"))
		factor = NSEC_PER_MSEC;
	else if (!strcmp(suffix, "us") || !strcmp(suffix, "usec"))
		factor = NSEC_PER_USEC;
	else
		goto error;

	set_timer(ks, factor, n, clvalue(kp_arg(ks, 2)));
	return 0;

 error:
	kp_error(ks, "cannot parse timer interval: %s\n", str);
	return -1;
}

/*
 * tick: timer timeout in one cpu
 * Ns, Nsec, Nms, Nmsec, Nus, Nusec
 */
static int ktap_lib_tick(ktap_state *ks)
{
	return do_tick_profile(ks, 1);
}

static int ktap_lib_profile(ktap_state *ks)
{
	kp_printf(ks, "timer profile not supported\n");
	return 0;
}

void kp_exit_timers(ktap_state *ks)
{
	struct hrtimer_ktap *t, *tmp;
	struct list_head *timers_list = &(G(ks)->timers);

	list_for_each_entry_safe(t, tmp, timers_list, list) {
		hrtimer_cancel(&t->timer);
		kp_free(ks, t);
	}
}

static const ktap_Reg timerlib_funcs[] = {
	{"profile",	ktap_lib_profile},
	{"tick",	ktap_lib_tick},
	{NULL}
};

void kp_init_timerlib(ktap_state *ks)
{
	kp_register_lib(ks, "timer", timerlib_funcs);
}

