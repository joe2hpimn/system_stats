/*------------------------------------------------------------------------
 * os_info.c
 *              Operating system information
 *
 * Copyright (c) 2020, EnterpriseDB Corporation. All Rights Reserved.
 *
 *------------------------------------------------------------------------
 */

#include "postgres.h"
#include "system_stats.h"

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>

#include <libproc.h>
#include <sys/proc_info.h>

extern int get_process_list(struct kinfo_proc **proc_list, size_t *proc_count);

void ReadOSInformations(Tuplestorestate *tupstore, TupleDesc tupdesc)
{
	struct     utsname uts;
	Datum      values[Natts_os_info];
	bool       nulls[Natts_os_info];
	char       host_name[MAXPGPATH];
	char       domain_name[MAXPGPATH];
	char       os_name[MAXPGPATH];
	char       os_version_level[MAXPGPATH];
	char       architecture[MAXPGPATH];
	int        ret_val;
	size_t     num_processes = 0;
	struct     kinfo_proc *proc_list = NULL;
	int        os_process_count = 0;
	struct     timespec uptime;

	memset(nulls, 0, sizeof(nulls));
	memset(host_name, 0, MAXPGPATH);
	memset(domain_name, 0, MAXPGPATH);
	memset(os_name, 0, MAXPGPATH);
	memset(architecture, 0, MAXPGPATH);
	memset(os_version_level, 0, MAXPGPATH);

	if (0 != clock_gettime(CLOCK_MONOTONIC_RAW, &uptime))
		nulls[Anum_os_up_since_seconds] = true;

	if (get_process_list(&proc_list, &num_processes) != 0)
	{
		ereport(DEBUG1, (errmsg("Error while getting process information list from proc")));
		return;
	}

	os_process_count = (int)num_processes;

	ret_val = uname(&uts);
	/* if it returns not zero means it fails so set null values */
	if (ret_val != 0)
	{
		nulls[Anum_os_name]  = true;
		nulls[Anum_os_version]  = true;
		nulls[Anum_os_architecture] = true;
	}
	else
	{
		memcpy(os_name, uts.sysname, strlen(uts.sysname));
		memcpy(os_version_level, uts.version, strlen(uts.version));
		memcpy(architecture, uts.machine, strlen(uts.machine));
	}

	/* Function used to get the host name of the system */
	if (gethostname(host_name, sizeof(host_name)) != 0)
		ereport(DEBUG1,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("error while getting host name")));

	/* Function used to get the domain name of the system */
	if (getdomainname(domain_name, sizeof(domain_name)) != 0)
		ereport(DEBUG1,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("error while getting domain name")));

	/*If hostname or domain name is empty, set the value to NULL */
	if (strlen(host_name) == 0)
		nulls[Anum_host_name] = true;
	if (strlen(domain_name) == 0)
		nulls[Anum_domain_name] = true;

	values[Anum_host_name]           = CStringGetTextDatum(host_name);
	values[Anum_domain_name]         = CStringGetTextDatum(domain_name);
	values[Anum_os_name]             = CStringGetTextDatum(os_name);
	values[Anum_os_version]          = CStringGetTextDatum(os_version_level);
	values[Anum_os_architecture]     = CStringGetTextDatum(architecture);
	values[Anum_os_process_count]    = Int32GetDatum(os_process_count);
	values[Anum_os_up_since_seconds] = Int32GetDatum((int)uptime.tv_sec);

	nulls[Anum_os_handle_count] = true;
	nulls[Anum_os_thread_count] = true;
	nulls[Anum_os_boot_time] = true;

	tuplestore_putvalues(tupstore, tupdesc, values, nulls);
}
