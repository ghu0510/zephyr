/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSS_DTS_HELPER_H
#define SENSS_DTS_HELPER_H

#include <zephyr/devicetree.h>

#define PHANDLE_DEVICE_BY_IDX(idx, node, prop)				\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx))

#define PHANDLE_DEVICE_LIST(node, prop)					\
{									\
	LISTIFY(DT_PROP_LEN_OR(node, prop, 0),				\
			PHANDLE_DEVICE_BY_IDX,				\
			(,),						\
			node,						\
			prop)						\
}

#endif /* end of include guard: SENSS_DTS_HELPER_H */
