#ifndef PTIMER_H
#define PTIMER_H

struct ptimer;                  /* forward declaration; all struct
                                   members are private */

struct ptimer *ptimer_new(void);
void ptimer_destroy(struct ptimer *);

void ptimer_reset(struct ptimer *);
double ptimer_measure(struct ptimer *);
double ptimer_read(const struct ptimer *);

double ptimer_resolution(void);

#endif /* PTIMER_H */
