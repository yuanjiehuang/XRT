// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo User Function Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "common.h"
#include "kds_core.h"

int kds_mode = 0;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable (default), 1 = enable)");

int kds_echo = 0;
module_param(kds_echo, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_echo,
		 "enable KDS echo (0 = disable (default), 1 = enable)");

static int xocl_context_ioctl(struct xocl_dev *xdev, void *data,
			      struct drm_file *filp)
{
	/* TODO: implement this */
	return 0;
}

static int xocl_command_ioctl(struct xocl_dev *xdev, void *data,
			      struct drm_file *filp)
{
	struct drm_device *ddev = filp->minor->dev;
	struct kds_client *client = filp->driver_priv;
	struct drm_xocl_execbuf *args = data;
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;
	struct ert_packet *ecmd;
	struct kds_command *xcmd;
	int ret = 0;

	obj = xocl_gem_object_lookup(ddev, filp, args->exec_bo_handle);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO %d\n",
		args->exec_bo_handle);
		return -ENOENT;
	}

	xobj = to_xocl_bo(obj);
	if (!xocl_bo_execbuf(xobj)) {
		ret = -EINVAL;
		goto out;
	}

	ecmd = (struct ert_packet *)xobj->vmapping;

	/* only the user command knows the real size of the payload.
	 * count is more than enough!
	 */
	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		userpf_err(xdev, "Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;

	/* TODO: one ecmd to one xcmd now. Maybe we will need
	 * one ecmd to multiple xcmds
	 */
	if (ecmd->opcode == ERT_CONFIGURE) {
		cfg_ecmd2xcmd(to_cfg_pkg(ecmd), xcmd);
	} else if (ecmd->opcode == ERT_START_CU)
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);

	/* Now, we could forget execbuf */
	ret = kds_add_command(xcmd);
	if (ret)
		kds_free_command(xcmd);

out:
	XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&xobj->base);

	return ret;
}

int xocl_create_client(struct xocl_dev *xdev, void **priv)
{
	struct	kds_client	*client;
	int	ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->dev = XDEV2DEV(xdev);
	client->pid = get_pid(task_pid(current));
	client->ctrl = XDEV(xdev)->kds.ctrl;
	ret = kds_init_client(client);
	if (ret) {
		kfree(client);
		goto out;
	}
	mutex_lock(&xdev->dev_lock);
	list_add_tail(&client->link, &xdev->ctx_list);
	mutex_unlock(&xdev->dev_lock);
	*priv = client;

out:
	userpf_info(xdev, "created KDS client for pid(%d), ret: %d\n",
		    pid_nr(task_tgid(current)), ret);
	return ret;
}

void xocl_destroy_client(struct xocl_dev *xdev, void **priv)
{
	struct kds_client *client = *priv;
	int pid = pid_nr(client->pid);

	list_del(&client->link);
	kds_fini_client(client);
	kfree(client);
	userpf_info(xdev, "client exits pid(%d)\n", pid);
}

int xocl_poll_client(struct xocl_dev *xdev, struct file *filp,
	    poll_table *wait, void *priv)
{
	struct kds_client *client = (struct kds_client *)priv;
	int event;

	poll_wait(filp, &client->waitq, wait);

	event = atomic_dec_if_positive(&client->event);
	if (event == -1)
		return 0;

	/* If only return POLLIN, I could get 100K IOPS more.
	 * With above wait, the IOPS is more unstable (+/-100K).
	 */
	return POLLIN;
}

int xocl_client_ioctl(struct xocl_dev *xdev, int op, void *data,
		      struct drm_file *filp)
{
	int ret = 0;

	switch (op) {
	case DRM_XOCL_CTX:
		ret = xocl_context_ioctl(xdev, data, filp);
		break;
	case DRM_XOCL_EXECBUF:
		ret = xocl_command_ioctl(xdev, data, filp);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int xocl_kds_stop(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_kds_reset(struct xocl_dev *xdev, const xuid_t *xclbin_id)
{
	/* plact holder */
	return 0;
}

int xocl_kds_reconfig(struct xocl_dev *xdev)
{
	/* plact holder */
	return 0;
}

int xocl_cu_map_addr(struct xocl_dev *xdev, u32 cu_idx,
		     void *drm_filp, u32 *addrp)
{
	/* plact holder */
	return 0;
}

