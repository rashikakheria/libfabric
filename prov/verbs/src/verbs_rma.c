/*
 * Copyright (c) 2013-2018 Intel Corporation, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include "fi_verbs.h"


#define VERBS_COMP_READ_FLAGS(ep, flags)			\
	((ep->util_ep.tx_op_flags | flags) &			\
	 (FI_COMPLETION | FI_TRANSMIT_COMPLETE |		\
	  FI_DELIVERY_COMPLETE) ? IBV_SEND_SIGNALED : 0)

#define VERBS_COMP_READ(ep)					\
	((ep->util_ep.tx_op_flags) &				\
	 (FI_COMPLETION | FI_TRANSMIT_COMPLETE |		\
	  FI_DELIVERY_COMPLETE) ? IBV_SEND_SIGNALED : 0)

static ssize_t
fi_ibv_msg_ep_rma_write(struct fid_ep *ep_fid, const void *buf, size_t len,
		     void *desc, fi_addr_t dest_addr,
		     uint64_t addr, uint64_t key, void *context)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)context,
		.opcode = IBV_WR_RDMA_WRITE,
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = VERBS_INJECT(ep, len) | VERBS_COMP(ep),
	};

	return fi_ibv_send_buf(ep, &wr, buf, len, desc);
}

static ssize_t
fi_ibv_msg_ep_rma_writev(struct fid_ep *ep_fid, const struct iovec *iov, void **desc,
		      size_t count, fi_addr_t dest_addr,
		      uint64_t addr, uint64_t key, void *context)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)context,
		.opcode = IBV_WR_RDMA_WRITE,
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
	};

	return fi_ibv_send_iov(ep, &wr, iov, desc, count);
}

static ssize_t
fi_ibv_msg_ep_rma_writemsg(struct fid_ep *ep_fid, const struct fi_msg_rma *msg,
			uint64_t flags)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)msg->context,
		.wr.rdma.remote_addr = msg->rma_iov->addr,
		.wr.rdma.rkey = (uint32_t)msg->rma_iov->key,
	};

	if (flags & FI_REMOTE_CQ_DATA) {
		wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
		wr.imm_data = htonl((uint32_t)msg->data);
	} else {
		wr.opcode = IBV_WR_RDMA_WRITE;
	}

	return fi_ibv_send_msg(ep, &wr, msg, flags);
}

static ssize_t
fi_ibv_msg_ep_rma_read(struct fid_ep *ep_fid, void *buf, size_t len,
		    void *desc, fi_addr_t src_addr,
		    uint64_t addr, uint64_t key, void *context)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)context,
		.opcode = IBV_WR_RDMA_READ,
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = VERBS_COMP_READ(ep),
	};

	return fi_ibv_send_buf(ep, &wr, buf, len, desc);
}

static ssize_t
fi_ibv_msg_ep_rma_readv(struct fid_ep *ep_fid, const struct iovec *iov, void **desc,
		     size_t count, fi_addr_t src_addr,
		     uint64_t addr, uint64_t key, void *context)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)context,
		.opcode = IBV_WR_RDMA_READ,
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = VERBS_COMP_READ(ep),
		.num_sge = count,
	};

	fi_ibv_set_sge_iov(wr.sg_list, iov, count, desc);

	return fi_ibv_send_poll_cq_if_needed(ep, &wr);
}

static ssize_t
fi_ibv_msg_ep_rma_readmsg(struct fid_ep *ep_fid, const struct fi_msg_rma *msg,
			uint64_t flags)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)msg->context,
		.opcode = IBV_WR_RDMA_READ,
		.wr.rdma.remote_addr = msg->rma_iov->addr,
		.wr.rdma.rkey = (uint32_t)msg->rma_iov->key,
		.send_flags = VERBS_COMP_READ_FLAGS(ep, flags),
		.num_sge = msg->iov_count,
	};

	fi_ibv_set_sge_iov(wr.sg_list, msg->msg_iov, msg->iov_count, msg->desc);

	return fi_ibv_send_poll_cq_if_needed(ep, &wr);
}

static ssize_t
fi_ibv_msg_ep_rma_writedata(struct fid_ep *ep_fid, const void *buf, size_t len,
			void *desc, uint64_t data, fi_addr_t dest_addr,
			uint64_t addr, uint64_t key, void *context)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)context,
		.opcode = IBV_WR_RDMA_WRITE_WITH_IMM,
		.imm_data = htonl((uint32_t)data),
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = VERBS_INJECT(ep, len) | VERBS_COMP(ep),
	};

	return fi_ibv_send_buf(ep, &wr, buf, len, desc);
}

static ssize_t
fi_ibv_msg_ep_rma_inject_write(struct fid_ep *ep_fid, const void *buf, size_t len,
		     fi_addr_t dest_addr, uint64_t addr, uint64_t key)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = VERBS_INJECT_FLAG,
		.opcode = IBV_WR_RDMA_WRITE,
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = IBV_SEND_INLINE,
	};

	return fi_ibv_send_buf_inline(ep, &wr, buf, len);
}

static ssize_t
fi_ibv_rma_write_fast(struct fid_ep *ep_fid, const void *buf, size_t len,
		      fi_addr_t dest_addr, uint64_t addr, uint64_t key)
{
	struct fi_ibv_ep *ep;

	ep = container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);

	ep->rma_wr.wr.rdma.remote_addr = addr;
	ep->rma_wr.wr.rdma.rkey = (uint32_t) key;

	ep->sge.addr = (uintptr_t) buf;
	ep->sge.length = (uint32_t) len;

	return fi_ibv_send_poll_cq_if_needed(ep, &ep->rma_wr);
}

static ssize_t
fi_ibv_msg_ep_rma_inject_writedata(struct fid_ep *ep_fid, const void *buf, size_t len,
			uint64_t data, fi_addr_t dest_addr, uint64_t addr,
			uint64_t key)
{
	struct fi_ibv_ep *ep =
		container_of(ep_fid, struct fi_ibv_ep, util_ep.ep_fid);
	struct ibv_send_wr wr = {
		.wr_id = VERBS_INJECT_FLAG,
		.opcode = IBV_WR_RDMA_WRITE_WITH_IMM,
		.imm_data = htonl((uint32_t)data),
		.wr.rdma.remote_addr = addr,
		.wr.rdma.rkey = (uint32_t)key,
		.send_flags = IBV_SEND_INLINE,
	};

	return fi_ibv_send_buf_inline(ep, &wr, buf, len);
}

struct fi_ops_rma fi_ibv_msg_ep_rma_ops_ts = {
	.size = sizeof(struct fi_ops_rma),
	.read = fi_ibv_msg_ep_rma_read,
	.readv = fi_ibv_msg_ep_rma_readv,
	.readmsg = fi_ibv_msg_ep_rma_readmsg,
	.write = fi_ibv_msg_ep_rma_write,
	.writev = fi_ibv_msg_ep_rma_writev,
	.writemsg = fi_ibv_msg_ep_rma_writemsg,
	.inject = fi_ibv_msg_ep_rma_inject_write,
	.writedata = fi_ibv_msg_ep_rma_writedata,
	.injectdata = fi_ibv_msg_ep_rma_inject_writedata,
};

struct fi_ops_rma fi_ibv_msg_ep_rma_ops = {
	.size = sizeof(struct fi_ops_rma),
	.read = fi_ibv_msg_ep_rma_read,
	.readv = fi_ibv_msg_ep_rma_readv,
	.readmsg = fi_ibv_msg_ep_rma_readmsg,
	.write = fi_ibv_msg_ep_rma_write,
	.writev = fi_ibv_msg_ep_rma_writev,
	.writemsg = fi_ibv_msg_ep_rma_writemsg,
	.inject = fi_ibv_rma_write_fast,
	.writedata = fi_ibv_msg_ep_rma_writedata,
	.injectdata = fi_ibv_msg_ep_rma_inject_writedata,
};
