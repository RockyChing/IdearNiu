#ifndef _LINUX_KFIFO_H
#define _LINUX_KFIFO_H

typedef struct spinlock {
    unsigned int count;
} spinlock_t;

/*
 * kfifo->size的值总是2的幂，好处--对kfifo->size取模运算可以转化为与运算，如：
 * kfifo->in % kfifo->size 可以转化为 kfifo->in & (kfifo->size – 1)
 *
 * kfifo的巧妙之处在于in和out定义为无符号类型，在put和get时，in和out都是增加
 * 当达到最大值时，产生溢出，自动从0开始
 */
struct kfifo {
    unsigned char *buffer;	/* the buffer holding the data */
	unsigned int   size;	/* the size of the allocated buffer */
	unsigned int   in;	    /* data is added at offset (in % size) */
	unsigned int   out;	    /* data is extracted from off. (out % size) */
	spinlock_t    *lock;	/* protects concurrent modifications */
};

#define spin_lock(lock) do {} while (0)
#define spin_unlock(lock) do {} while (0)

#define spin_lock_irqsave(lock) do {} while (0)
#define spin_unlock_irqrestore(lock) do {} while (0)


extern struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size, spinlock_t *lock);
extern struct kfifo *kfifo_alloc(unsigned int size, spinlock_t *lock);
extern void kfifo_free(struct kfifo *fifo);
extern unsigned int __kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
extern unsigned int __kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);

/*
 * __kfifo_reset - removes the entire FIFO contents, no locking version
 * @fifo: the fifo to be emptied.
 */
static inline void __kfifo_reset(struct kfifo *fifo)
{
    fifo->in = fifo->out = 0;
}

/*
 * kfifo_reset - removes the entire FIFO contents
 * @fifo: the fifo to be emptied.
 */
static inline void kfifo_reset(struct kfifo *fifo)
{
    spin_lock_irqsave(fifo->lock);
    __kfifo_reset(fifo);
    spin_unlock_irqrestore(fifo->lock);
}

/*
 * kfifo_put - puts some data into the FIFO
 * @fifo: the fifo to be used.
 * @buffer: the data to be added.
 * @len: the length of the data to be added.
 *
 * This function copies at most 'len' bytes from the 'buffer' into
 * the FIFO depending on the free space, and returns the number of
 * bytes copied.
 */
static inline unsigned int kfifo_put(struct kfifo *fifo,
				     unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    spin_lock_irqsave(fifo->lock);
    ret = __kfifo_put(fifo, buffer, len);
    spin_unlock_irqrestore(fifo->lock);

    return ret;
}

/*
 * kfifo_get - gets some data from the FIFO
 * @fifo: the fifo to be used.
 * @buffer: where the data must be copied.
 * @len: the size of the destination buffer.
 *
 * This function copies at most 'len' bytes from the FIFO into the
 * 'buffer' and returns the number of copied bytes.
 */
static inline unsigned int kfifo_get(struct kfifo *fifo,
				     unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    spin_lock_irqsave(fifo->lock);
    ret = __kfifo_get(fifo, buffer, len);
    /*
	 * optimization: if the FIFO is empty, set the indices to 0
	 * so we don't wrap the next time
	 */
	if (fifo->in == fifo->out)
        fifo->in = fifo->out = 0;
    spin_unlock_irqrestore(fifo->lock);

    return ret;
}

/*
 * __kfifo_len - returns the number of bytes available in the FIFO, no locking version
 * @fifo: the fifo to be used.
 */
static inline unsigned int __kfifo_len(struct kfifo *fifo)
{
    return fifo->in - fifo->out;
}            

/*
 * kfifo_len - returns the number of bytes available in the FIFO
 * @fifo: the fifo to be used.
 */
static inline unsigned int kfifo_len(struct kfifo *fifo)
{
    unsigned int ret;

    spin_lock_irqsave(fifo->lock);
    ret = __kfifo_len(fifo);
    spin_unlock_irqrestore(fifo->lock);

    return ret;
}

/**
 * kfifo_is_empty - returns true if the fifo is empty
 * @fifo: address of the fifo to be used
 */
static inline int kfifo_is_empty(struct kfifo *fifo)
{
    unsigned int ret;

    spin_lock_irqsave(fifo->lock);
    ret = __kfifo_len(fifo);
    spin_unlock_irqrestore(fifo->lock);

    return ret == 0;
}


#endif
