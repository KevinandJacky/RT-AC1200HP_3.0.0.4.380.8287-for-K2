#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/seq_file.h>
#include <linux/rcupdate.h>
#include <net/protocol.h>
#include <net/netfilter/nf_queue.h>
#include <net/dst.h>

#include "nf_internals.h"

/*
 * A queue handler may be registered for each protocol.  Each is protected by
 * long term mutex.  The handler must provide an an outfn() to accept packets
 * for queueing and must reinject all packets it receives, no matter what.
 */
static const struct nf_queue_handler *queue_handler[NFPROTO_NUMPROTO] __read_mostly;

static DEFINE_MUTEX(queue_handler_mutex);

#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
static const struct nf_queue_handler *queue_imq_handler;

void nf_register_queue_imq_handler(const struct nf_queue_handler *qh)
{
	mutex_lock(&queue_handler_mutex);
	rcu_assign_pointer(queue_imq_handler, qh);
	mutex_unlock(&queue_handler_mutex);
}
EXPORT_SYMBOL_GPL(nf_register_queue_imq_handler);

void nf_unregister_queue_imq_handler(void)
{
	mutex_lock(&queue_handler_mutex);
	rcu_assign_pointer(queue_imq_handler, NULL);
	mutex_unlock(&queue_handler_mutex);
}
EXPORT_SYMBOL_GPL(nf_unregister_queue_imq_handler);

#endif

/* return EBUSY when somebody else is registered, return EEXIST if the
 * same handler is registered, return 0 in case of success. */
int nf_register_queue_handler(u_int8_t pf, const struct nf_queue_handler *qh)
{
	int ret;

	if (pf >= ARRAY_SIZE(queue_handler))
		return -EINVAL;

	mutex_lock(&queue_handler_mutex);
	if (queue_handler[pf] == qh)
		ret = -EEXIST;
	else if (queue_handler[pf])
		ret = -EBUSY;
	else {
		rcu_assign_pointer(queue_handler[pf], qh);
		ret = 0;
	}
	mutex_unlock(&queue_handler_mutex);

	return ret;
}
EXPORT_SYMBOL(nf_register_queue_handler);

/* The caller must flush their queue before this */
int nf_unregister_queue_handler(u_int8_t pf, const struct nf_queue_handler *qh)
{
	if (pf >= ARRAY_SIZE(queue_handler))
		return -EINVAL;

	mutex_lock(&queue_handler_mutex);
	if (queue_handler[pf] && queue_handler[pf] != qh) {
		mutex_unlock(&queue_handler_mutex);
		return -EINVAL;
	}

	rcu_assign_pointer(queue_handler[pf], NULL);
	mutex_unlock(&queue_handler_mutex);

	synchronize_rcu();

	return 0;
}
EXPORT_SYMBOL(nf_unregister_queue_handler);

void nf_unregister_queue_handlers(const struct nf_queue_handler *qh)
{
	u_int8_t pf;

	mutex_lock(&queue_handler_mutex);
	for (pf = 0; pf < ARRAY_SIZE(queue_handler); pf++)  {
		if (queue_handler[pf] == qh)
			rcu_assign_pointer(queue_handler[pf], NULL);
	}
	mutex_unlock(&queue_handler_mutex);

	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(nf_unregister_queue_handlers);

void nf_queue_entry_release_refs(struct nf_queue_entry *entry)
{
	/* Release those devices we held, or Alexey will kill me. */
	if (entry->indev)
		dev_put(entry->indev);
	if (entry->outdev)
		dev_put(entry->outdev);
#ifdef CONFIG_BRIDGE_NETFILTER
	if (entry->skb->nf_bridge) {
		struct nf_bridge_info *nf_bridge = entry->skb->nf_bridge;

		if (nf_bridge->physindev)
			dev_put(nf_bridge->physindev);
		if (nf_bridge->physoutdev)
			dev_put(nf_bridge->physoutdev);
	}
#endif
	/* Drop reference to owner of hook which queued us. */
	module_put(entry->elem->owner);
}
EXPORT_SYMBOL_GPL(nf_queue_entry_release_refs);

/*
 * Any packet that leaves via this function must come back
 * through nf_reinject().
 */
static int __nf_queue(struct sk_buff *skb,
		      struct list_head *elem,
		      u_int8_t pf, unsigned int hook,
		      struct net_device *indev,
		      struct net_device *outdev,
		      int (*okfn)(struct sk_buff *),
		      unsigned int queuenum,
		      bool imq_queue)
{
	int status;
	struct nf_queue_entry *entry = NULL;
#ifdef CONFIG_BRIDGE_NETFILTER
	struct net_device *physindev;
	struct net_device *physoutdev;
#endif
	const struct nf_afinfo *afinfo;
	const struct nf_queue_handler *qh;
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
	const struct nf_queue_handler *qih = NULL;
#endif

	/* QUEUE == DROP if noone is waiting, to be safe. */
	rcu_read_lock();

	qh = rcu_dereference(queue_handler[pf]);
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	if (pf == PF_INET || pf == PF_INET6)
#else
	if (pf == PF_INET)
#endif
		qih = rcu_dereference(queue_imq_handler);

	if (!qh && !qih)
#else /* !IMQ */
	if (!qh)
#endif		
		goto err_unlock;

	afinfo = nf_get_afinfo(pf);
	if (!afinfo)
		goto err_unlock;

	entry = kmalloc(sizeof(*entry) + afinfo->route_key_size, GFP_ATOMIC);
	if (!entry)
		goto err_unlock;

	*entry = (struct nf_queue_entry) {
		.skb	= skb,
		.elem	= list_entry(elem, struct nf_hook_ops, list),
		.pf	= pf,
		.hook	= hook,
		.indev	= indev,
		.outdev	= outdev,
		.okfn	= okfn,
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
	.next_outfn = qh ? qh->outfn : NULL,
	//.next_outfn = NULL,
 	.next_queuenum = queuenum,
#endif		
	};
	
	/* If it's going away, ignore hook. */
	if (!try_module_get(entry->elem->owner)) {
		rcu_read_unlock();
		kfree(entry);
		return 0;
	}
	/* Bump dev refs so they don't vanish while packet is out */
	if (indev)
		dev_hold(indev);
	if (outdev)
		dev_hold(outdev);
#ifdef CONFIG_BRIDGE_NETFILTER
	if (skb->nf_bridge) {
		physindev = skb->nf_bridge->physindev;
		if (physindev)
			dev_hold(physindev);
		physoutdev = skb->nf_bridge->physoutdev;
		if (physoutdev)
			dev_hold(physoutdev);
	}
#endif
	skb_dst_force(skb);
	afinfo->saveroute(skb, entry);
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
	if (qih) {
		status = qih->outfn(entry, queuenum);
		goto imq_skip_queue;
	}
#endif

	status = qh->outfn(entry, queuenum);
	
#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
imq_skip_queue:
#endif
	rcu_read_unlock();

	if (status < 0) {
		nf_queue_entry_release_refs(entry);
		goto err;
	}
	return 1;

err_unlock:
	rcu_read_unlock();
err:
	kfree_skb(skb);
	kfree(entry);
	return 1;
}

static int _nf_queue(struct sk_buff *skb,
	     struct list_head *elem,
	     u_int8_t pf, unsigned int hook,
	     struct net_device *indev,
	     struct net_device *outdev,
	     int (*okfn)(struct sk_buff *),
	     unsigned int queuenum,
	     bool imq_queue)
{
	struct sk_buff *segs;

	/*
	 * Seems kernel bug here.
	 *
	 * When LRO enabled, the ingress packets would contain GSO data
	 * for tcp_measure_recv_mss() purpose only, but not for TSO.
	 *
	 * However the code here always do TSO segmentation by skb_gso_segment()
	 * for the packets which is with GSO enabled, and leads to kernel panic
	 * null pointer access returned from skb_gso_segment(). 
	 *
	 * Currently we add the hook point check here to prevent from such situation.
	 * especially for superdmz web access. -- MTK/Ralink yyhuang
	 */

	if(	!skb_is_gso(skb) ||
		(skb_is_gso(skb) && (hook == NF_INET_PRE_ROUTING || hook == NF_INET_LOCAL_IN)) )
		return __nf_queue(skb, elem, pf, hook, indev, outdev, okfn,
				  queuenum, imq_queue);

	switch (pf) {
	case NFPROTO_IPV4:
		skb->protocol = htons(ETH_P_IP);
		break;
	case NFPROTO_IPV6:
		skb->protocol = htons(ETH_P_IPV6);
		break;
	}

	segs = skb_gso_segment(skb, 0);
	kfree_skb(skb);
	if (IS_ERR(segs))
		return 1;

	do {
		struct sk_buff *nskb = segs->next;

		segs->next = NULL;
		if (!__nf_queue(segs, elem, pf, hook, indev, outdev, okfn,
				queuenum, imq_queue))
			kfree_skb(segs);
		segs = nskb;
	} while (segs);
	return 1;
}

int nf_queue(struct sk_buff *skb,
	     struct list_head *elem,
	     u_int8_t pf, unsigned int hook,
	     struct net_device *indev,
	     struct net_device *outdev,
	     int (*okfn)(struct sk_buff *),
	     unsigned int queuenum)
{
	return _nf_queue(skb, elem, pf, hook, indev, outdev, okfn, queuenum,
			 false);
}

#if defined(CONFIG_IMQ) || defined(CONFIG_IMQ_MODULE)
int nf_imq_queue(struct sk_buff *skb,
	     struct list_head *elem,
	     u_int8_t pf, unsigned int hook,
	     struct net_device *indev,
	     struct net_device *outdev,
	     int (*okfn)(struct sk_buff *),
	     unsigned int queuenum)
{
	return _nf_queue(skb, elem, pf, hook, indev, outdev, okfn, queuenum,
			 true);
}
#endif

void nf_reinject(struct nf_queue_entry *entry, unsigned int verdict)
{
	struct sk_buff *skb = entry->skb;
	struct list_head *elem = &entry->elem->list;
	const struct nf_afinfo *afinfo;
 
	rcu_read_lock();

	nf_queue_entry_release_refs(entry);

	/* Continue traversal iff userspace said ok... */
	if (verdict == NF_REPEAT) {
		elem = elem->prev;
		verdict = NF_ACCEPT;
	}

	if (verdict == NF_ACCEPT) {
		afinfo = nf_get_afinfo(entry->pf);
		if (!afinfo || afinfo->reroute(skb, entry) < 0)
			verdict = NF_DROP;
	}

	if (verdict == NF_ACCEPT) {
	next_hook:
		verdict = nf_iterate(&nf_hooks[entry->pf][entry->hook],
				     skb, entry->hook,
				     entry->indev, entry->outdev, &elem,
				     entry->okfn, INT_MIN);
	}

	switch (verdict & NF_VERDICT_MASK) {
	case NF_ACCEPT:
	case NF_STOP:
		local_bh_disable();
		entry->okfn(skb);
		local_bh_enable();
		break;
	case NF_QUEUE:
		if (!__nf_queue(skb, elem, entry->pf, entry->hook,
				entry->indev, entry->outdev, entry->okfn,
				verdict >> NF_VERDICT_BITS, false))
			goto next_hook;
		break;
	case NF_IMQ_QUEUE:
		if (!__nf_queue(skb, elem, entry->pf, entry->hook,
				entry->indev, entry->outdev, entry->okfn,
				verdict >> NF_VERDICT_BITS, true))
			goto next_hook;
		break;
	case NF_STOLEN:
	default:
		kfree_skb(skb);
	}
	rcu_read_unlock();
	kfree(entry);
}
EXPORT_SYMBOL(nf_reinject);

#ifdef CONFIG_PROC_FS
static void *seq_start(struct seq_file *seq, loff_t *pos)
{
	if (*pos >= ARRAY_SIZE(queue_handler))
		return NULL;

	return pos;
}

static void *seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;

	if (*pos >= ARRAY_SIZE(queue_handler))
		return NULL;

	return pos;
}

static void seq_stop(struct seq_file *s, void *v)
{

}

static int seq_show(struct seq_file *s, void *v)
{
	int ret;
	loff_t *pos = v;
	const struct nf_queue_handler *qh;

	rcu_read_lock();
	qh = rcu_dereference(queue_handler[*pos]);
	if (!qh)
		ret = seq_printf(s, "%2lld NONE\n", *pos);
	else
		ret = seq_printf(s, "%2lld %s\n", *pos, qh->name);
	rcu_read_unlock();

	return ret;
}

static const struct seq_operations nfqueue_seq_ops = {
	.start	= seq_start,
	.next	= seq_next,
	.stop	= seq_stop,
	.show	= seq_show,
};

static int nfqueue_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nfqueue_seq_ops);
}

static const struct file_operations nfqueue_file_ops = {
	.owner	 = THIS_MODULE,
	.open	 = nfqueue_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release,
};
#endif /* PROC_FS */


int __init netfilter_queue_init(void)
{
#ifdef CONFIG_PROC_FS
	if (!proc_create("nf_queue", S_IRUGO,
			 proc_net_netfilter, &nfqueue_file_ops))
		return -1;
#endif
	return 0;
}

