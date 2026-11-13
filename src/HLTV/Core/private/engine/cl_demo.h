#ifndef CL_DEMO_H
#define CL_DEMO_H

#include "HLTV/common/types.h"
#include "common/ref_params.h"
#include "common/usercmd.h"
#include "pm_shared/pm_movevars.h"

#include "common/legacy_demo_records.h"

typedef cse::demo::LegacyDemoHeader demoheader_t;
typedef cse::demo::LegacyDemoEntry demoentry_t;

typedef struct demo_info_s
{
	float			timestamp;
	ref_params_t	rp;
	usercmd_t		cmd;
	movevars_t		movevars;
	vec3_t			view;
	int				viewmodel;
} demo_info_t;

#endif
