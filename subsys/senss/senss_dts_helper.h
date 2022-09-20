/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSS_DTS_HELPER_H
#define SENSS_DTS_HELPER_H

#include <zephyr/devicetree.h>

#define PHANDLE_ELEM_BY_IDX(idx, node, prop)				\
	DT_NODE_CHILD_IDX(DT_PHANDLE_BY_IDX(node, prop, idx)),

#define PHANDLES_IDX_NO_EMPTY_LIST(node, prop)				\
{									\
	LISTIFY(DT_PROP_LEN(node, prop),				\
			PHANDLE_ELEM_BY_IDX,				\
			(),						\
			node,						\
			prop						\
		     )							\
}

#define PHANDLES_IDX_LIST_LEN(node, prop)				\
	COND_CODE_1(DT_NODE_HAS_PROP(node, prop),			\
		    (DT_PROP_LEN(node, prop)),				\
		    (0))						\

#define PHANDLES_IDX_LIST(node, prop)					\
	COND_CODE_1(DT_NODE_HAS_PROP(node, prop),			\
		    (PHANDLES_IDX_NO_EMPTY_LIST(node, prop)),		\
		    ({}))						\

#endif /* end of include guard: SENSS_DTS_HELPER_H */
