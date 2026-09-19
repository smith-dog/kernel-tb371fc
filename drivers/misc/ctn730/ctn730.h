#ifndef CTN730_H
#define CTN730_H

struct ctn730_dev;
int is_stylus_attached(void);
int ctn730_notifier_register(struct notifier_block *nb);
int ctn730_notifier_unregister(struct notifier_block *nb);
#endif
